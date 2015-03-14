
#include "physfs.hpp"

#include <stdexcept>
#include <sstream>
#include <memory>

#include <fnmatch.h>

#include <osgDB/ReaderWriter>
#include <osgDB/Options>
#include <osgDB/FileNameUtils>
#include <osgDB/Registry>

#include <MyGUI_DataManager.h>

#include "physfs.h"

#include "log.hpp"


namespace
{

// Inherit from std::streambuf to handle custom I/O
class PhysFSStreamBuf : public std::streambuf {
    static const size_t sBufferSize = 4096;

    PHYSFS_File *mFile;
    char mBuffer[sBufferSize];

    virtual int_type underflow()
    {
        if(mFile && gptr() == egptr())
        {
            // Read in the next chunk of data, and set the read pointers on
            // success
            PHYSFS_sint64 got = PHYSFS_read(mFile, mBuffer, sizeof(mBuffer[0]), sBufferSize);
            if(got != -1) setg(mBuffer, mBuffer, mBuffer+got);
        }
        if(gptr() == egptr())
            return traits_type::eof();
        return (*gptr())&0xFF;
    }

    virtual pos_type seekoff(off_type offset, std::ios_base::seekdir whence, std::ios_base::openmode mode)
    {
        if(!mFile || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        // PhysFS only seeks using absolute offsets, so we have to convert cur-
        // and end-relative offsets.
        PHYSFS_sint64 fpos;
        switch(whence)
        {
            case std::ios_base::beg:
                break;

            case std::ios_base::cur:
                // Need to offset for the read pointers with std::ios_base::cur
                // regardless
                offset -= off_type(egptr()-gptr());
                if((fpos=PHYSFS_tell(mFile)) == -1)
                    return traits_type::eof();
                offset += fpos;
                break;

            case std::ios_base::end:
                if((fpos=PHYSFS_fileLength(mFile)) == -1)
                    return traits_type::eof();
                offset += fpos;
                break;

            default:
                return traits_type::eof();
        }
        // Range check - absolute offsets cannot be less than 0, nor be greater
        // than PhysFS's offset type.
        if(offset < 0 || offset >= std::numeric_limits<PHYSFS_sint64>::max())
            return traits_type::eof();
        if(PHYSFS_seek(mFile, PHYSFS_sint64(offset)) == 0)
            return traits_type::eof();
        // Clear read pointers so underflow() gets called on the next read
        // attempt.
        setg(0, 0, 0);
        return offset;
    }

    virtual pos_type seekpos(pos_type pos, std::ios_base::openmode mode)
    {
        // Simplified version of seekoff
        if(!mFile || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        if(pos < 0 || pos >= std::numeric_limits<PHYSFS_sint64>::max())
            return traits_type::eof();
        if(PHYSFS_seek(mFile, PHYSFS_sint64(pos)) == 0)
            return traits_type::eof();
        setg(0, 0, 0);
        return pos;
    }

public:
    PhysFSStreamBuf() : mFile(nullptr)
    { }
    virtual ~PhysFSStreamBuf()
    {
        PHYSFS_close(mFile);
        mFile = nullptr;
    }

    bool open(const char *filename)
    {
        PHYSFS_File *file = PHYSFS_openRead(filename);
        if(!file) return false;

        PHYSFS_close(mFile);
        mFile = file;

        setg(0, 0, 0);
        return true;
    }
};

// Inherit from std::istream to use our custom streambuf
class PhysFSStream : public std::istream {
public:
    PhysFSStream(const char *filename=nullptr) : std::istream(new PhysFSStreamBuf())
    {
        if(filename)
            open(filename);
    }
    virtual ~PhysFSStream()
    { delete rdbuf(); }

    PhysFSStream& open(const char *fname)
    {
        // Set the failbit if the file failed to open.
        if(!(static_cast<PhysFSStreamBuf*>(rdbuf())->open(fname)))
            clear(failbit);
        return *this;
    }
};


class PhysFSDataStream : public MyGUI::IDataStream {
    PHYSFS_File *mFile;

public:
    PhysFSDataStream() : mFile(nullptr)
    { }
    virtual ~PhysFSDataStream()
    {
        PHYSFS_close(mFile);
        mFile = nullptr;
    }

    bool open(const char *fname)
    {
        PHYSFS_File *file = PHYSFS_openRead(fname);
        if(!file) return false;

        PHYSFS_close(mFile);
        mFile = file;

        return true;
    }

    virtual bool eof() { return PHYSFS_eof(mFile); }

    virtual size_t size() { return PHYSFS_fileLength(mFile); }

    virtual void readline(std::string &_source, MyGUI::Char _delim)
    {
        std::string().swap(_source);

        unsigned char val;
        while(PHYSFS_read(mFile, &val, 1, 1) == 1 && val != _delim)
            _source += (char)val;
    }

    virtual size_t read(void *_buf, size_t _count)
    {
        PHYSFS_sint64 ret = PHYSFS_read(mFile, _buf, 1, _count);
        return std::max<PHYSFS_sint64>(0, ret);
    }
};

} // namespace


namespace TK
{

class PhysFSReadCallback : public osgDB::ReadFileCallback {
    typedef osgDB::ReaderWriter::ReadResult ReadResult;

    static bool open(PhysFSStream &istream, const std::string &fname, const osgDB::Options *options)
    {
        // try to find the proper path in vfs
        if(istream.open(fname.c_str()))
            return true;
        if(options)
        {
            const osgDB::FilePathList &pl = options->getDatabasePathList();
            for(const auto &path : pl)
            {
                std::string searchpath = path + "/" + fname;
                istream.clear();
                if(istream.open(searchpath.c_str()))
                    return true;
            }
        }
        const osgDB::FilePathList &pl = osgDB::Registry::instance()->getDataFilePathList();
        for(const auto &path : pl)
        {
            std::string searchpath = path + "/" + fname;
            istream.clear();
            if(istream.open(searchpath.c_str()))
                return true;
        }
        return false;
    }

#define WRAP_READER(func)                                                           \
    virtual ReadResult func(const std::string &fname, const osgDB::Options *options)\
    {                                                                               \
        PhysFSStream istream;                                                       \
        if(!open(istream, fname, options))                                          \
            return ReadResult::FILE_NOT_FOUND;                                      \
                                                                                    \
        const osgDB::ReaderWriter *rw = osgDB::Registry::instance()->getReaderWriterForExtension(osgDB::getFileExtension(fname)); \
        if(rw) return rw->func(istream, options);                                   \
        return ReadResult::ERROR_IN_READING_FILE;                                   \
    }
    WRAP_READER(readObject)
    WRAP_READER(readImage)
    WRAP_READER(readHeightField)
    WRAP_READER(readNode)
    WRAP_READER(readShader)
#undef WRAP_READER

public:
    PhysFSReadCallback() { }
};


class PhysFSDataManager : public MyGUI::DataManager {
    std::string mBasePath;

    std::string findFilePath(const std::string &fname, const std::string &path=std::string()) const
    {
        std::string found;

        char **list = PHYSFS_enumerateFiles((mBasePath+path).c_str());
        for(size_t i = 0;list[i];i++)
        {
            if(fname == list[i])
            {
                found = path+"/"+list[i];
                break;
            }
        }
        for(size_t i = 0;found.empty() && list[i];i++)
        {
            std::string fullName = path+"/"+list[i];
            if(PHYSFS_isDirectory((mBasePath+fullName).c_str()))
                found = findFilePath(fname, fullName);
        }
        PHYSFS_freeList(list);

        return std::move(found);
    }

    void enumerateFiles(MyGUI::VectorString &filelist, const std::string &pattern, const std::string &path=std::string()) const
    {
        char **list = PHYSFS_enumerateFiles((mBasePath+path).c_str());
        for(size_t i = 0;list[i];i++)
        {
            std::string fullName = path+"/"+list[i];

            if(fnmatch(pattern.c_str(), fullName.c_str(), 0) == 0)
                filelist.push_back(fullName);

            if(PHYSFS_isDirectory((mBasePath+fullName).c_str()))
                enumerateFiles(filelist, pattern, fullName);
        }
        PHYSFS_freeList(list);
    }

public:
    PhysFSDataManager(std::string&& basepath)
      : mBasePath(std::move(basepath))
    { }

    /** Get data stream from specified resource name.
     *  @param _name Resource name (usually file name).
     */
    virtual MyGUI::IDataStream *getData(const std::string &fname)
    {
        std::unique_ptr<PhysFSDataStream> istream(new PhysFSDataStream());
        if(istream->open((mBasePath+"/"+fname).c_str()))
            return istream.release();
        return nullptr;
    }

    /** Free data stream.
     *  @param _data Data stream.
     */
    virtual void freeData(MyGUI::IDataStream *data)
    {
        delete data;
    }

    /** Is data with specified name exist.
     *  @param _name Resource name.
     */
    virtual bool isDataExist(const std::string &fname)
    {
        return PHYSFS_exists((mBasePath+"/"+fname).c_str()) != 0;
    }

    /** Get all data names with names that matches pattern.
     *  @param _pattern Pattern to match (for example "*.layout").
     */
    virtual const MyGUI::VectorString &getDataListNames(const std::string &pattern)
    {
        static MyGUI::VectorString name_list;
        MyGUI::VectorString().swap(name_list);
        Log::get().stream()<< "Searching for "<<pattern;
        enumerateFiles(name_list, pattern);
        return name_list;
    }

    /** Get full path to data.
     *  @param _name Resource name.
     *  @return Return full path to specified data.
     *  For example getDataPath("My.layout") might return "C:\path\to\project\data\My.layout"
     */
    virtual const std::string &getDataPath(const std::string &fname)
    {
        static std::string fullpath;
        fullpath = findFilePath(fname);
        Log::get().stream()<< "Found "<<fullpath<<" for "<<fname;
        return fullpath;
    }
};


template<>
PhysFSFactory* Singleton<PhysFSFactory>::sInstance = nullptr;

PhysFSFactory::PhysFSFactory()
{
    if(!PHYSFS_init(nullptr))
    {
        std::stringstream sstr;
        sstr<< "Failed to initialize PhysFS: "<<PHYSFS_getLastError();
        throw std::runtime_error(sstr.str());
    }

    osgDB::Registry::instance()->setReadFileCallback(new PhysFSReadCallback());
}

PhysFSFactory::~PhysFSFactory()
{
    osgDB::Registry::instance()->setReadFileCallback(nullptr);
    PHYSFS_deinit();
}

void PhysFSFactory::addPath(const char *path, const char *mountPoint, bool append) const
{
    if(PHYSFS_mount(path, mountPoint, append) == 0)
        Log::get().stream(Log::Level_Error)<< "Failed to add "<<path<<": "<<PHYSFS_getLastError();
}

MyGUI::DataManager *PhysFSFactory::createDataManager(std::string&& base) const
{
    return new PhysFSDataManager(std::move(base));
}

} // namespace TK
