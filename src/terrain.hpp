#ifndef TERRAIN_HPP
#define TERRAIN_HPP

#include <OgrePageManager.h>
#include <OgreTerrainPagedWorldSection.h>

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

} // namespace TK

#endif /* TERRAIN_HPP */
