#ifndef ARCHIVES_PHYSFS_HPP
#define ARCHIVES_PHYSFS_HPP

#include <OgreArchiveFactory.h>
#include <OgreSingleton.h>

namespace TK
{

// An archive factory for PhysFS. Only one archive is made here, and it has
// access to the whole PhysFS file hierarchy.
class PhysFSFactory : public Ogre::ArchiveFactory, public Ogre::Singleton<PhysFSFactory>
{
public:
    PhysFSFactory();
    virtual ~PhysFSFactory();

    virtual const Ogre::String& getType() const;

    virtual Ogre::Archive *createInstance(const Ogre::String &name, bool readOnly);
    virtual void destroyInstance(Ogre::Archive *inst);

    void addPath(const char *path, const char *mountPoint=nullptr, bool append=false) const;
};

} // namespace TK

#endif /* ARCHIVES_PHYSFS_HPP */
