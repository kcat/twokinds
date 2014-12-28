
#include "physfs.hpp"

#include <stdexcept>
#include <sstream>

#include <OgreArchive.h>
#include <OgreLogManager.h>

#include "physfs.h"


namespace
{

class PhysFileStream : public Ogre::DataStream {
    PHYSFS_File *mFile;

public:
    PhysFileStream(PHYSFS_File *file) : mFile(file)
    { }
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

    void enumerateInfo(const Ogre::String &dirpath, Ogre::FileInfoListPtr &infos, const Ogre::String &pattern, bool recurse, bool dirs) const
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
    virtual Ogre::DataStreamPtr open(const Ogre::String &filename, bool readOnly) const
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
    virtual Ogre::FileInfoListPtr findFileInfo(const Ogre::String &pattern, bool recursive, bool dirs) const
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

template<>
PhysFSFactory* Ogre::Singleton<PhysFSFactory>::msSingleton = nullptr;

PhysFSFactory::PhysFSFactory()
{
    if(!PHYSFS_init(nullptr))
    {
        std::stringstream sstr;
        sstr<< "Failed to initialize PhysFS: "<<PHYSFS_getLastError();
        throw std::runtime_error(sstr.str());
    }
}

PhysFSFactory::~PhysFSFactory()
{
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

void PhysFSFactory::Mount(const char *path, const char *mountPoint, int append) const
{
    auto &logMgr = Ogre::LogManager::getSingleton();
    logMgr.stream()<< "Adding new file source "<<path<<" to "<<(mountPoint?mountPoint:"<root>")<<"...";

    if(PHYSFS_mount(path, mountPoint, append) == 0)
        logMgr.stream(Ogre::LML_CRITICAL)<< "Failed to add "<<path<<": "<<PHYSFS_getLastError();
}

} // namespace TK
