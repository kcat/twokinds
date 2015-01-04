
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


// This custom module allows us to provide image data as a source for other
// libnoise modules (such as selectors).
class ImageSrcModule : public noise::module::Module
{
protected:
    const Ogre::Image &mImage;
    double mFrequency;

public:
    ImageSrcModule(const Ogre::Image &image)
      : noise::module::Module(0), mImage(image), mFrequency(1.0)
    { }

    // Sets the number of samples per unit (default=1). Higher values
    // effectively shrink the image.
    void SetFrequency(double freq)
    {
        if(!(freq > 0.0 && freq < std::numeric_limits<double>::max()))
            throw std::runtime_error("Invalid ImageSrcModule frequency");
        mFrequency = freq;
    }

    double GetFrequency() const { return mFrequency; }

    // No source modules
    virtual int GetSourceModuleCount() const { return 0; }

    virtual double GetValue(double x, double /*y*/, double z) const
    {
        const size_t width = mImage.getWidth();
        const size_t height = mImage.getHeight();

        // NOTE: X is west/east and Z is north/south (Y is up/down, but we
        // don't bother with depth)
        x *= mFrequency;
        z *= mFrequency;

        // Offset the image so 0 is the center
        x += width/2.0;
        z += height/2.0;

        x = Ogre::Math::Clamp<double>(x, 0.0, width-1);
        z = Ogre::Math::Clamp<double>(z, 0.0, height-1);

        size_t sx = size_t(x);
        size_t sy = size_t(z);

        // The components are normalized to 0...1, while libnoise expects -1...+1.
        Ogre::ColourValue clr = mImage.getColourAt(sx, sy, 0);
        return clr.r*2.0 - 1.0;
    }
};

class ImageInterpSrcModule : public ImageSrcModule
{
public:
    ImageInterpSrcModule(const Ogre::Image &image)
      : ImageSrcModule(image)
    { }

    virtual double GetValue(double x, double /*y*/, double z) const
    {
        size_t width = mImage.getWidth();
        size_t height = mImage.getHeight();

        x = x*mFrequency + width/2.0;
        z = z*mFrequency + height/2.0;

        x = Ogre::Math::Clamp<double>(x, 0.0, width-1);
        z = Ogre::Math::Clamp<double>(z, 0.0, height-1);

        size_t sx = size_t(x);
        size_t sy = size_t(z);

        x = x - sx;
        z = z - sy;

        float b00 = (1.0-x) * (1.0-z);
        float b01 = (    x) * (1.0-z);
        float b10 = (1.0-x) * (    z);
        float b11 = (    x) * (    z);

        // Bilinear interpolate using the four nearest colors
        Ogre::ColourValue clr00 = mImage.getColourAt(sx, sy, 0);
        Ogre::ColourValue clr01 = mImage.getColourAt(std::min(sx+1, width-1), sy, 0);
        Ogre::ColourValue clr10 = mImage.getColourAt(sx, std::min(sy+1,height-1), 0);
        Ogre::ColourValue clr11 = mImage.getColourAt(std::min(sx+1, width-1), std::min(sy+1,height-1), 0);
        // The components are normalized to 0...1, while libnoise expects -1...+1.
        return (clr00.r*b00 + clr01.r*b01 + clr10.r*b10 + clr11.r*b11)*2.0 - 1.0;
    }
};


TerrainDefiner::TerrainDefiner()
{
    mHeightmap.load("tk-heightmap.png", "Terrain");
    mHeightmapModule.reset(new ImageInterpSrcModule(mHeightmap));

    mNoiseModule.SetFrequency(4);
    mHeightMapBuilder.SetSourceModule(mNoiseModule);
    mHeightMapBuilder.SetDestNoiseMap(mNoiseMap);
    mHeightMapBuilder.SetDestSize(TERRAIN_SIZE, TERRAIN_SIZE);
}

void TerrainDefiner::define(Ogre::TerrainGroup *terrainGroup, long x, long y)
{
    mHeightMapBuilder.SetBounds(
        x, (x+1) + 1.0f/(TERRAIN_SIZE-1),
        y, (y+1) + 1.0f/(TERRAIN_SIZE-1)
    );
    mHeightMapBuilder.Build();

    std::vector<float> pixels(mNoiseMap.GetWidth() * mNoiseMap.GetHeight());
    for(int py = 0;py < mNoiseMap.GetHeight();++py)
    {
        const float *src = mNoiseMap.GetConstSlabPtr(py);
        float *dst = &pixels[py*mNoiseMap.GetWidth()];
        for(int px = 0;px < mNoiseMap.GetWidth();++px)
            dst[px] = src[px]*0.5f + 0.5f;
    }
    terrainGroup->defineTerrain(x, y, pixels.data());
}


bool TerrainPageProvider::prepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::loadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::unloadProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }
bool TerrainPageProvider::unprepareProceduralPage(Ogre::Page *page, Ogre::PagedWorldSection *section) { return true; }

} // namespace TK
