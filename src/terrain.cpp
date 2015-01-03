
#include "terrain.hpp"

#include <OgreRoot.h>
#include <OgrePageManager.h>
#include <OgrePage.h>

#include <OgreTerrain.h>
#include <OgreTerrainGroup.h>
#include <OgreTerrainPaging.h>
#include <OgreTerrainPagedWorldSection.h>
#include <OgreLogManager.h>

namespace TK
{

#define TERRAIN_WORLD_SIZE 6000.0f
#define TERRAIN_SIZE 257

void Terrain::configureTerrainDefaults(Ogre::SceneManager *sceneMgr, Ogre::Light *light)
{
    // Configure global
    mTerrainGlobals->setMaxPixelError(4);
    // testing composite map
    mTerrainGlobals->setCompositeMapDistance(3000);

    // Important to set these so that the terrain knows what to use for derived (non-realtime) data
    mTerrainGlobals->setLightMapDirection(light->getDerivedDirection());
    mTerrainGlobals->setCompositeMapAmbient(sceneMgr->getAmbientLight());
    mTerrainGlobals->setCompositeMapDiffuse(light->getDiffuseColour());

    // Configure default import settings for if we use imported image
    Ogre::Terrain::ImportData &defaultimp = mTerrainGroup->getDefaultImportSettings();
    defaultimp.terrainSize = TERRAIN_SIZE;
    defaultimp.worldSize = TERRAIN_WORLD_SIZE;
    defaultimp.inputScale = 600;
    defaultimp.minBatchSize = 33;
    defaultimp.maxBatchSize = 65;
    // textures
    defaultimp.layerList.resize(3);
    defaultimp.layerList[0].worldSize = 100;
    defaultimp.layerList[0].textureNames.push_back("dirt_grayrocky_diffusespecular.dds");
    defaultimp.layerList[0].textureNames.push_back("dirt_grayrocky_normalheight.dds");
    defaultimp.layerList[1].worldSize = 30;
    defaultimp.layerList[1].textureNames.push_back("grass_green-01_diffusespecular.dds");
    defaultimp.layerList[1].textureNames.push_back("grass_green-01_normalheight.dds");
    defaultimp.layerList[2].worldSize = 200;
    defaultimp.layerList[2].textureNames.push_back("growth_weirdfungus-03_diffusespecular.dds");
    defaultimp.layerList[2].textureNames.push_back("growth_weirdfungus-03_normalheight.dds");
}

void Terrain::initialize(Ogre::Camera *camera, Ogre::Light *l)
{
    Ogre::SceneManager *sceneMgr = camera->getSceneManager();

    mTerrainGlobals = OGRE_NEW Ogre::TerrainGlobalOptions();

    // Bugfix for D3D11 Render System because of pixel format incompatibility when using
    // vertex compression
    if(Ogre::Root::getSingleton().getRenderSystem()->getName() == "Direct3D11 Rendering Subsystem")
        mTerrainGlobals->setUseVertexCompressionWhenAvailable(false);

    mTerrainGroup = OGRE_NEW Ogre::TerrainGroup(sceneMgr, Ogre::Terrain::ALIGN_X_Z, TERRAIN_SIZE, TERRAIN_WORLD_SIZE);
    mTerrainGroup->setFilenameConvention("tkworld", "terrain");
    mTerrainGroup->setOrigin(Ogre::Vector3::ZERO);
    mTerrainGroup->setResourceGroup("Terrain");

#if OGRE_VERSION >= ((2<<16) | (0<<8) | 0)
    sceneMgr->updateSceneGraph();
#endif

    configureTerrainDefaults(sceneMgr, l);

    // Paging setup
    mPageManager = OGRE_NEW Ogre::PageManager();
    mPageProvider = OGRE_NEW TerrainPageProvider();
    // Since we're not loading any pages from .page files, we need a way just
    // to say we've loaded them without them actually being loaded
    mPageManager->setPageProvider(mPageProvider);
    mPageManager->addCamera(camera);
    mTerrainPaging = OGRE_NEW Ogre::TerrainPaging(mPageManager);
    Ogre::PagedWorld *world = mPageManager->createWorld();
    mTerrainSection = mTerrainPaging->createWorldSection(world, mTerrainGroup, 2000, 3000,
        -2, -2, 1, 1
    );
    mTerrainSection->setDefiner(OGRE_NEW TerrainDefiner());

    mTerrainGroup->freeTemporaryResources();
}

void Terrain::deinitialize()
{
    if(mTerrainPaging)
    {
        OGRE_DELETE mTerrainPaging;
        mTerrainPaging = nullptr;
        OGRE_DELETE mPageManager;
        mPageManager = nullptr;
    }
    else
        OGRE_DELETE mTerrainGroup;
    mTerrainGroup = nullptr;
    mTerrainSection = nullptr;

    OGRE_DELETE mTerrainGlobals;
    mTerrainGlobals = nullptr;

    OGRE_DELETE mPageProvider;
    mPageProvider = nullptr;
}

Terrain::Terrain()
  : mTerrainGroup(nullptr)
  , mTerrainGlobals(nullptr)
  , mPageManager(nullptr)
  , mTerrainPaging(nullptr)
  , mTerrainSection(nullptr)
  , mPageProvider(nullptr)
{
}
Terrain Terrain::sTerrain;


TerrainDefiner::TerrainDefiner()
{
    mNoiseModule.SetFrequency(4);
    mHeightMapBuilder.SetSourceModule(mNoiseModule);
    mHeightMapBuilder.SetDestNoiseMap(mHeightMap);
    mHeightMapBuilder.SetDestSize(TERRAIN_SIZE, TERRAIN_SIZE);
}

void TerrainDefiner::define(Ogre::TerrainGroup *terrainGroup, long x, long y)
{
    mHeightMapBuilder.SetBounds(
        x, (x+1) + 1.0f/(TERRAIN_SIZE-1),
        y, (y+1) + 1.0f/(TERRAIN_SIZE-1)
    );
    mHeightMapBuilder.Build();

    std::vector<float> pixels(mHeightMap.GetWidth() * mHeightMap.GetHeight());
    for(int py = 0;py < mHeightMap.GetHeight();++py)
    {
        const float *src = mHeightMap.GetSlabPtr(py);
        float *dst = &pixels[py*mHeightMap.GetWidth()];
        for(int px = 0;px < mHeightMap.GetWidth();++px)
            dst[px] = src[px]*0.5f + 0.5f;
    }
    terrainGroup->defineTerrain(x, y, pixels.data());
}


bool TerrainPageProvider::prepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::loadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::unloadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::unprepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }

} // namespace TK
