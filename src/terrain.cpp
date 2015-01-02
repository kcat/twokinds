
#include "terrain.hpp"

#include <OgrePage.h>


namespace TK
{

void SimpleTerrainDefiner::define(Ogre::TerrainGroup *terrainGroup, long x, long y)
{
    Ogre::Image img;
    img.load("terrain.png", "Textures");
    if((x&1) != 0) img.flipAroundY();
    if((y&1) != 0) img.flipAroundX();
    terrainGroup->defineTerrain(x, y, &img);
}

bool TerrainPageProvider::prepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::loadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::unloadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::unprepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }

} // namespace TK
