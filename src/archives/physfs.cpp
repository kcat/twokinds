
#include "physfs.hpp"

#include <stdexcept>
#include <sstream>

#include <OgreArchive.h>
#include <OgreLogManager.h>

#include <osgDB/ReaderWriter>
#include <osgDB/Options>
#include <osgDB/FileNameUtils>
#include <osgDB/Registry>

#include "physfs.h"

#include "log.hpp"


#if OGRE_VERSION >= ((2<<16) | (0<<8) | 0)
#define const_OGRE2 const
#else
#define const_OGRE2
#endif

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


class PhysFileStream : public Ogre::DataStream {
    PHYSFS_File *mFile;

public:
    PhysFileStream(PHYSFS_File *file) : mFile(file)
    {
        PHYSFS_sint64 len = PHYSFS_fileLength(mFile);
        if(len > 0) mSize = len;
    }
    virtual ~PhysFileStream()
    { close(); }

    virtual size_t read(void *buf, size_t count)
    {
        PHYSFS_sint64 got = PHYSFS_read(mFile, buf, 1, count);
        if(got >= 0) return got;
        throw std::runtime_error("Failed to read from file");
    }

    virtual size_t write(const void *buf, size_t count)
    {
        PHYSFS_sint64 got = PHYSFS_write(mFile, buf, 1, count);
        if(got >= 0) return got;
        throw std::runtime_error("Failed to write to file");
    }

    virtual void skip(long count)
    {
        PHYSFS_sint64 pos = PHYSFS_tell(mFile);
        if(pos < 0 || std::numeric_limits<PHYSFS_sint64>::max()-pos < count)
        {
            std::stringstream sstr;
            sstr<< "Cannot skip "<<count<<" bytes";
            throw std::runtime_error(sstr.str());
        }
        if(!PHYSFS_seek(mFile, pos + count))
        {
            std::stringstream sstr;
            sstr<< "Failed to skip "<<count<<" bytes";
            throw std::runtime_error(sstr.str());
        }
    }

    virtual void seek(size_t pos)
    {
        if(!PHYSFS_seek(mFile, pos))
        {
            std::stringstream sstr;
            sstr<< "Failed to seek to offset "<<pos;
            throw std::runtime_error(sstr.str());
        }
    }

    virtual size_t tell() const
    {
        PHYSFS_sint64 pos = PHYSFS_tell(mFile);
        if(pos == -1)
            throw std::runtime_error("PHYSFS_tell failed");
        return pos;
     }

    virtual bool eof() const
    {
        return PHYSFS_eof(mFile);
    }

    virtual void close()
    {
        if(mFile)
            PHYSFS_close(mFile);
        mFile = nullptr;
    }
};
typedef Ogre::SharedPtr<PhysFileStream> PhysFileStreamPtr;


class PhysFSArchive : public Ogre::Archive {
    const Ogre::String mBasePath;

    void enumerateNames(const Ogre::String &dirpath, Ogre::StringVectorPtr &names, const Ogre::String &pattern, bool recurse, bool dirs) const
    {
        char **list = PHYSFS_enumerateFiles((mBasePath+dirpath).c_str());
        for(size_t i = 0;list[i];i++)
        {
            std::string fullName = dirpath+list[i];
            if(PHYSFS_isDirectory((mBasePath+fullName).c_str()))
            {
                if(dirs && (pattern.empty() || Ogre::StringUtil::match(fullName, pattern)))
                    names->push_back(fullName);
                if(recurse)
                {
                    fullName += "/";
                    enumerateNames(fullName, names, pattern, recurse, dirs);
                }
            }
            else if(!dirs && (pattern.empty() || Ogre::StringUtil::match(fullName, pattern)))
                names->push_back(fullName);
        }
        PHYSFS_freeList(list);
    }

    void enumerateInfo(const Ogre::String &dirpath, Ogre::FileInfoListPtr &infos, const Ogre::String &pattern, bool recurse, bool dirs) const_OGRE2
    {
        char **list = PHYSFS_enumerateFiles((mBasePath+dirpath).c_str());
        for(size_t i = 0;list[i];i++)
        {
            std::string fullName = dirpath+list[i];
            if(PHYSFS_isDirectory((mBasePath+fullName).c_str()))
            {
                if(dirs && (pattern.empty() || Ogre::StringUtil::match(fullName, pattern)))
                {
                    Ogre::FileInfo info;
                    info.archive = this;
                    info.filename = fullName;
                    info.basename = list[i];
                    info.path = dirpath;
                    info.compressedSize = 0;
                    info.uncompressedSize = 0;
                    infos->push_back(info);
                }
                if(recurse)
                {
                    fullName += "/";
                    enumerateInfo(fullName, infos, pattern, recurse, dirs);
                }
            }
            else if(!dirs && (pattern.empty() || Ogre::StringUtil::match(fullName, pattern)))
            {
                Ogre::FileInfo info;
                info.archive = this;
                info.filename = fullName;
                info.basename = list[i];
                info.path = dirpath;
                info.compressedSize = 0;
                info.uncompressedSize = 0;
                infos->push_back(info);
            }
        }
        PHYSFS_freeList(list);
    }

public:
    PhysFSArchive(const Ogre::String &name, const Ogre::String &archType, const Ogre::String &base, bool readOnly)
      : Archive(name, archType), mBasePath(base)
    { mReadOnly = readOnly; }

    virtual bool isCaseSensitive() const { return true; }

    virtual void load() { }
    virtual void unload() { }

    // Open a file in the archive.
    virtual Ogre::DataStreamPtr open(const Ogre::String &filename, bool readOnly) const_OGRE2
    {
        PhysFileStreamPtr stream;
        PHYSFS_File *file = nullptr;
        if(readOnly)
            file = PHYSFS_openRead((mBasePath+"/"+filename).c_str());
        else if(!mReadOnly)
            file = PHYSFS_openAppend((mBasePath+"/"+filename).c_str());
        if(file)
            stream.bind(OGRE_NEW_T(PhysFileStream, Ogre::MEMCATEGORY_GENERAL)(file),
                                   Ogre::SPFM_DELETE_T);
        return stream;
    }

    virtual Ogre::StringVectorPtr list(bool recursive, bool dirs)
    {
        Ogre::StringVectorPtr names(OGRE_NEW_T(Ogre::StringVector, Ogre::MEMCATEGORY_GENERAL)(),
                                    Ogre::SPFM_DELETE_T);
        enumerateNames("/", names, Ogre::String(), recursive, dirs);
        return names;
    }
    Ogre::FileInfoListPtr listFileInfo(bool recursive, bool dirs)
    {
        Ogre::FileInfoListPtr infos(OGRE_NEW_T(Ogre::FileInfoList, Ogre::MEMCATEGORY_GENERAL)(),
                                    Ogre::SPFM_DELETE_T);
        enumerateInfo("/", infos, Ogre::String(), recursive, dirs);
        return infos;
    }

    virtual Ogre::StringVectorPtr find(const Ogre::String &pattern, bool recursive, bool dirs)
    {
        Ogre::StringVectorPtr names(OGRE_NEW_T(Ogre::StringVector, Ogre::MEMCATEGORY_GENERAL)(),
                                    Ogre::SPFM_DELETE_T);
        enumerateNames("/", names, pattern, recursive, dirs);
        return names;
    }
    virtual Ogre::FileInfoListPtr findFileInfo(const Ogre::String &pattern, bool recursive, bool dirs) const_OGRE2
    {
        Ogre::FileInfoListPtr infos(OGRE_NEW_T(Ogre::FileInfoList, Ogre::MEMCATEGORY_GENERAL)(),
                                    Ogre::SPFM_DELETE_T);
        enumerateInfo("/", infos, pattern, recursive, dirs);
        return infos;
    }

    virtual bool exists(const Ogre::String &filename)
    {
        return PHYSFS_exists((mBasePath+"/"+filename).c_str());
    }

    virtual time_t getModifiedTime(const Ogre::String &filename)
    {
        return PHYSFS_getLastModTime((mBasePath+"/"+filename).c_str());
    }
};

} // namespace


namespace TK
{

//! OSG Reader for physfs
class ReaderPhysFS : public osgDB::ReaderWriter, public Singleton<ReaderPhysFS>
{
    static thread_local bool sAcceptExtensions;

    static bool open(PhysFSStream &istream, const std::string &fname, const Options *options)
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
                if(istream.open(searchpath.c_str()))
                    return true;
            }
        }
        return false;
    }

public:

    virtual const char *className() const { return "PhysFS Reader"; }

    virtual bool acceptsExtension(const std::string &ext) const
    {
        /* HACK: If we're recursing, we won't accept any extensions so we can
         * get the appropriate ReaderWriter format handler. */
        return sAcceptExtensions;
    }

#define WRAP_READER(func)                                                           \
    virtual ReadResult func(const std::string &fname, const Options *options) const \
    {                                                                               \
        PhysFSStream istream;                                                       \
        if(!open(istream, fname, options))                                          \
            return ReadResult::FILE_NOT_FOUND;                                      \
                                                                                    \
        sAcceptExtensions = false;                                                  \
        const osgDB::ReaderWriter *rw = osgDB::Registry::instance()->getReaderWriterForExtension(osgDB::getFileExtension(fname)); \
        sAcceptExtensions = true;                                                   \
        if(rw) return rw->func(istream, options);                                   \
        return ReadResult::ERROR_IN_READING_FILE;                                   \
    }
    WRAP_READER(readObject)
    WRAP_READER(readImage)
    WRAP_READER(readHeightField)
    WRAP_READER(readNode)
    WRAP_READER(readShader)
#undef WRAP_READER
};
thread_local bool ReaderPhysFS::sAcceptExtensions = true;
template<>
ReaderPhysFS* Singleton<ReaderPhysFS>::sInstance = nullptr;


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

    ReaderPhysFS *rdr = new ReaderPhysFS();
    osgDB::Registry::instance()->addReaderWriter(rdr);
}

PhysFSFactory::~PhysFSFactory()
{
    osgDB::Registry::instance()->removeReaderWriter(ReaderPhysFS::getPtr());
    delete ReaderPhysFS::getPtr();
    PHYSFS_deinit();
}

const Ogre::String &PhysFSFactory::getType() const
{
    static const Ogre::String name = "PhysFS";
    return name;
}


Ogre::Archive *PhysFSFactory::createInstance(const Ogre::String &name, bool readOnly)
{
    Ogre::String base = name;
    if(!base.empty() && *base.rbegin() == '/')
        base.erase(base.length()-1);
    return new PhysFSArchive(name, getType(), base, readOnly);
}

void PhysFSFactory::destroyInstance(Ogre::Archive *inst)
{
    delete inst;
}

void PhysFSFactory::addPath(const char *path, const char *mountPoint, bool append) const
{
    if(PHYSFS_mount(path, mountPoint, append) == 0)
        Log::get().stream(Log::Level_Error)<< "Failed to add "<<path<<": "<<PHYSFS_getLastError();
}

} // namespace TK
