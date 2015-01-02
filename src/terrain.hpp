#ifndef TERRAIN_HPP
#define TERRAIN_HPP

#include <OgrePageManager.h>
#include <OgreTerrainPagedWorldSection.h>

namespace Ogre
{
    class TerrainPaging;
}

namespace TK
{

class SimpleTerrainDefiner : public Ogre::TerrainPagedWorldSection::TerrainDefiner
{
public:
    virtual void define(Ogre::TerrainGroup *terrainGroup, long x, long y);
};

class TerrainPageProvider : public Ogre::PageProvider
{
public:
    virtual bool prepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section);
    virtual bool loadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section);
    virtual bool unloadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section);
    virtual bool unprepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section);
};

class Terrain
{
    static Terrain sTerrain;

    Ogre::TerrainGroup *mTerrainGroup;
    Ogre::TerrainGlobalOptions *mTerrainGlobals;
    Ogre::PageManager *mPageManager;
    Ogre::TerrainPaging *mTerrainPaging;
    Ogre::TerrainPagedWorldSection *mTerrainSection;
    TerrainPageProvider *mPageProvider;

    void configureTerrainDefaults(Ogre::SceneManager *sceneMgr, Ogre::Light *light);

    Terrain();
public:
    void initialize(Ogre::Camera *camera, Ogre::Light *l);
    void deinitialize();

    Ogre::TerrainGroup *getTerrainGroup() { return mTerrainGroup; }

    static Terrain &get() { return sTerrain; }
    static Terrain *getPtr() { return &sTerrain; }
};

} // namespace TK

#endif /* TERRAIN_HPP */
