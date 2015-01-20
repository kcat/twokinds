
#include "terrain.hpp"

#include <OgreRoot.h>
#include <OgreLogManager.h>
#include <OgreCamera.h>

#include "noiseutils/noiseutils.h"

#include "terrain/defaultworld.hpp"
#include "terrain/storage.hpp"
#include "terrain/quadtreenode.hpp"


namespace TK
{

// This custom module allows us to provide image data as a source for other
// libnoise modules (such as selectors).
class ImageSrcModule : public noise::module::Module
{
protected:
    Ogre::Image mImage;
    double mFrequency;

public:
    ImageSrcModule()
      : noise::module::Module(0), mFrequency(1.0)
    { }

    // Retrieves the source Image object.
    Ogre::Image &GetImage() { return mImage; }

    // Retrieves the source Image object.
    const Ogre::Image &GetImage() const { return mImage; }

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

// Same as above, except applies linear interpolation to the calculated height.
class ImageInterpSrcModule : public ImageSrcModule
{
public:
    ImageInterpSrcModule() { }

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


/* World size/height is just a placeholder for now. */
#define TERRAIN_WORLD_SIZE 1500.0f
#define TERRAIN_WORLD_HEIGHT 600.0f
#define TERRAIN_SIZE 65

class TerrainStorage : public Terrain::Storage
{
    ImageInterpSrcModule mHeightmapModule;

    noise::module::Perlin mBaseFieldsTerrain;
    noise::module::ScaleBias mFieldsTerrain;
    noise::module::Billow mBaseSeaTerrain;
    noise::module::ScaleBias mSeaTerrain;

    noise::module::Select mFinalTerrain;

public:
    TerrainStorage();

    virtual void getBounds(float& minX, float& maxX, float& minY, float& maxY);

    virtual bool getMinMaxHeights(float size, const Ogre::Vector2 &center, float& min, float& max);

    virtual void fillVertexBuffers(int lodLevel, float size, const Ogre::Vector2 &center, Terrain::Alignment align,
                                   std::vector<float> &positions, std::vector<float> &normals,
                                   std::vector<Ogre::uint8> &colours);

    virtual void getBlendmaps(float chunkSize, const Ogre::Vector2 &chunkCenter, bool pack,
                              std::vector<Ogre::PixelBox> &blendmaps,
                              std::vector<Terrain::LayerInfo> &layerList);

    virtual void getBlendmaps(const std::vector<Terrain::QuadTreeNode*> &nodes, std::vector<Terrain::LayerCollection> &out, bool pack);

    virtual float getHeightAt(const Ogre::Vector3 &worldPos);

    virtual Terrain::LayerInfo getDefaultLayer()
    {
        return Terrain::LayerInfo{"dirt_grayrocky_diffusespecular.dds", "dirt_grayrocky_normalheight.dds", false, false};
    }

    virtual float getCellWorldSize() { return TERRAIN_WORLD_SIZE; }

    virtual int getCellVertices() { return TERRAIN_SIZE; }
};

TerrainStorage::TerrainStorage()
{
    mHeightmapModule.GetImage().load("tk-heightmap.png", "Terrain");
    mHeightmapModule.GetImage().flipAroundX();
    mHeightmapModule.SetFrequency(32.0);

    float fields_base = 16.0f/255.0f * 2.0f - 1.0f;
    mBaseFieldsTerrain.SetFrequency(noise::module::DEFAULT_PERLIN_FREQUENCY * 2.0);
    mFieldsTerrain.SetSourceModule(0, mBaseFieldsTerrain);
    mFieldsTerrain.SetScale(0.25);
    mFieldsTerrain.SetBias(0.25 + fields_base);

    mBaseSeaTerrain.SetFrequency(2.0 * 2.0);
    mSeaTerrain.SetSourceModule(0, mBaseSeaTerrain);
    mSeaTerrain.SetScale(0.0625);
    mSeaTerrain.SetBias(-1.5);

    float edge_falloff = 8.0f/255.0f;
    mFinalTerrain.SetSourceModule(0, mSeaTerrain);
    mFinalTerrain.SetSourceModule(1, mFieldsTerrain);
    mFinalTerrain.SetControlModule(mHeightmapModule);
    mFinalTerrain.SetBounds(fields_base-edge_falloff, std::numeric_limits<double>::max());
    mFinalTerrain.SetEdgeFalloff(edge_falloff);
}

void TerrainStorage::getBounds(float& minX, float& maxX, float& minY, float& maxY)
{
    minX = -(int)mHeightmapModule.GetImage().getWidth()/2;
    minY = -(int)mHeightmapModule.GetImage().getHeight()/2;
    maxX =  mHeightmapModule.GetImage().getWidth()/2;
    maxY =  mHeightmapModule.GetImage().getHeight()/2;
}

bool TerrainStorage::getMinMaxHeights(float /*size*/, const Ogre::Vector2& /*center*/, float& min, float& max)
{
    min = -TERRAIN_WORLD_HEIGHT;
    max =  TERRAIN_WORLD_HEIGHT;
    return true;
}

void TerrainStorage::fillVertexBuffers(int lodLevel, float size, const Ogre::Vector2& center, Terrain::Alignment align,
                                       std::vector<float>& positions, std::vector<float>& normals,
                                       std::vector<Ogre::uint8>& colours)
{
    assert(size == 1<<lodLevel);

    const float cell_vtx = size / (TERRAIN_SIZE-1);
    noise::utils::NoiseMap output;
    noise::utils::NoiseMapBuilderPlane builder;
    builder.SetSourceModule(mFinalTerrain);
    builder.SetDestNoiseMap(output);
    // We need an extra rows and columns on the sides to calculate proper normals
    builder.SetDestSize(TERRAIN_SIZE+2, TERRAIN_SIZE+2);
    builder.SetBounds(
        center.x-(size/2.0f) - cell_vtx, center.x+(size/2.0f) + 2.0f*cell_vtx,
        center.y-(size/2.0f) - cell_vtx, center.y+(size/2.0f) + 2.0f*cell_vtx
    );
    builder.Build();

    noise::utils::Image normalmap(output.GetWidth(), output.GetHeight());
    noise::utils::RendererNormalMap normrender;
    /* *0.5f since libnoise goes from -1..+1, rather than 0..1 */
    normrender.SetBumpHeight(TERRAIN_WORLD_HEIGHT*0.5f / (TERRAIN_WORLD_SIZE / (TERRAIN_SIZE-1)) / size);
    normrender.SetSourceNoiseMap(output);
    normrender.SetDestImage(normalmap);
    normrender.Render();

    positions.resize(TERRAIN_SIZE*TERRAIN_SIZE * 3);
    normals.resize(TERRAIN_SIZE*TERRAIN_SIZE * 3);
    colours.resize(TERRAIN_SIZE*TERRAIN_SIZE * 4);

    for(int py = 0;py < TERRAIN_SIZE;++py)
    {
        const float *src = output.GetConstSlabPtr(py+1)+1;
        const noise::utils::Color *norms = normalmap.GetConstSlabPtr(py+1)+1;
        for(int px = 0;px < TERRAIN_SIZE;++px)
        {
            size_t idx = (px*TERRAIN_SIZE) + py;

            positions[idx*3 + 0] = (px/float(TERRAIN_SIZE-1) - 0.5f) * size * TERRAIN_WORLD_SIZE;
            positions[idx*3 + 1] = (py/float(TERRAIN_SIZE-1) - 0.5f) * size * TERRAIN_WORLD_SIZE;
            positions[idx*3 + 2] = (src[px]*0.5f + 0.5f) * TERRAIN_WORLD_HEIGHT;
            Terrain::convertPosition(align, positions[idx*3 + 0], positions[idx*3 + 1], positions[idx*3 + 2]);

            normals[idx*3 + 0] = norms[px].red/127.5f - 1.0f;
            normals[idx*3 + 1] = norms[px].green/127.5f - 1.0f;
            normals[idx*3 + 2] = norms[px].blue/127.5f - 1.0f;
            Terrain::convertPosition(align, normals[idx*3 + 0], normals[idx*3 + 1], normals[idx*3 + 2]);

            Ogre::ColourValue col(1.0f, 1.0f, 1.0f, 1.0f);
            union {
                Ogre::uint32 val32;
                Ogre::uint8 val8[4];
            } cvt_clr;
            Ogre::Root::getSingleton().getRenderSystem()->convertColourValue(col, &cvt_clr.val32);
            colours[idx*4 + 0] = cvt_clr.val8[0];
            colours[idx*4 + 1] = cvt_clr.val8[1];
            colours[idx*4 + 2] = cvt_clr.val8[2];
            colours[idx*4 + 3] = cvt_clr.val8[3];
        }
    }
}

void TerrainStorage::getBlendmaps(float chunkSize, const Ogre::Vector2& chunkCenter, bool pack,
                                  std::vector<Ogre::PixelBox>& blendmaps,
                                  std::vector<Terrain::LayerInfo>& layerList)
{
    layerList.push_back(Terrain::LayerInfo{"dirt_grayrocky_diffusespecular.dds","dirt_grayrocky_normalheight.dds", false, false});
}

void TerrainStorage::getBlendmaps(const std::vector<Terrain::QuadTreeNode*>& nodes, std::vector<Terrain::LayerCollection>& out, bool pack)
{
    for(Terrain::QuadTreeNode *node : nodes)
    {
        Terrain::LayerCollection layers;
        layers.mTarget = node;

        getBlendmaps(node->getSize(), node->getCenter(), pack, layers.mBlendmaps, layers.mLayers);

        out.push_back(std::move(layers));
    }
}

float TerrainStorage::getHeightAt(const Ogre::Vector3 &worldPos)
{
    float val = mFinalTerrain.GetValue(worldPos.x / TERRAIN_WORLD_SIZE, 0.0f, worldPos.z / -TERRAIN_WORLD_SIZE);
    return (val*0.5f + 0.5f) * TERRAIN_WORLD_HEIGHT;
}



void World::initialize(Ogre::Camera *camera, Ogre::Light *l)
{
    Ogre::SceneManager *sceneMgr = camera->getSceneManager();

    mTerrain = new Terrain::DefaultWorld(sceneMgr, new TerrainStorage(), 1, true, Terrain::Align_XZ, 65536);
    mTerrain->applyMaterials(false/*Settings::Manager::getBool("enabled", "Shadows")*/,
                             false/*Settings::Manager::getBool("split", "Shadows")*/);
    mTerrain->update(camera->getRealPosition());
    mTerrain->syncLoad();
    // need to update again so the chunks that were just loaded can be made visible
    mTerrain->update(camera->getRealPosition());
}

void World::deinitialize()
{
    delete mTerrain;
    mTerrain = nullptr;
}


float World::getHeightAt(const Ogre::Vector3 &pos) const
{
    return mTerrain->getHeightAt(pos);
}

void World::update(const Ogre::Vector3 &cameraPos)
{
    mTerrain->update(cameraPos);
}


void World::getStatus(std::ostream &status) const
{
    mTerrain->getStatus(status);
}


World::World()
  : mTerrain(nullptr)
{
}
World World::sWorld;

} // namespace TK
