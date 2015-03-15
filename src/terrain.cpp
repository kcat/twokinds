
#include "terrain.hpp"

#include <osg/Image>
#include <osg/Texture2D>
#include <osgDB/ReadFile>

#include "noiseutils/noiseutils.h"

#include "terrain/defaultworld.hpp"
#include "terrain/storage.hpp"
#include "terrain/quadtreenode.hpp"

#include "cvars.hpp"
#include "log.hpp"


namespace TK
{

// This custom module allows us to provide image data as a source for other
// libnoise modules (such as selectors).
class ImageSrcModule : public noise::module::Module
{
protected:
    osg::ref_ptr<osg::Image> mImage;
    double mFrequency;

public:
    ImageSrcModule()
      : noise::module::Module(0), mFrequency(1.0)
    { }
    virtual ~ImageSrcModule() { }

    // Sets the source Image object.
    void SetImage(osg::Image *image) { mImage = image; }

    // Retrieves the source Image object.
    osg::Image *GetImage() { return mImage.get(); }

    // Retrieves the source Image object.
    const osg::Image *GetImage() const { return mImage.get(); }

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
        const size_t width = mImage->s();
        const size_t height = mImage->t();

        // NOTE: X is west/east and Z is north/south (Y is up/down, but we
        // don't bother with depth)
        x *= mFrequency;
        z *= mFrequency;

        // Offset the image so 0 is the center
        x += width/2.0;
        z += height/2.0;

        x = std::min<double>(std::max(x, 0.0), width-1);
        z = std::min<double>(std::max(z, 0.0), height-1);

        size_t sx = size_t(x);
        size_t sy = size_t(z);

        // The components are normalized to 0...1, while libnoise expects -1...+1.
        osg::Vec4 clr = mImage->getColor(sx, sy);
        return clr.r()*2.0 - 1.0;
    }
};

// Same as above, except applies linear interpolation to the calculated height.
class ImageInterpSrcModule : public ImageSrcModule
{
public:
    ImageInterpSrcModule() { }

    virtual double GetValue(double x, double /*y*/, double z) const
    {
        size_t width = mImage->s();
        size_t height = mImage->t();

        x = x*mFrequency + width/2.0;
        z = z*mFrequency + height/2.0;

        x = std::min<double>(std::max(x, 0.0), width-1);
        z = std::min<double>(std::max(z, 0.0), height-1);

        size_t sx = size_t(x);
        size_t sy = size_t(z);

        x = x - sx;
        z = z - sy;

        float b00 = (1.0-x) * (1.0-z);
        float b01 = (    x) * (1.0-z);
        float b10 = (1.0-x) * (    z);
        float b11 = (    x) * (    z);

        // Bilinear interpolate using the four nearest colors
        osg::Vec4 clr00 = mImage->getColor(sx, sy);
        osg::Vec4 clr01 = mImage->getColor(std::min(sx+1, width-1), sy);
        osg::Vec4 clr10 = mImage->getColor(sx, std::min(sy+1,height-1));
        osg::Vec4 clr11 = mImage->getColor(std::min(sx+1, width-1), std::min(sy+1,height-1));
        // The components are normalized to 0...1, while libnoise expects -1...+1.
        return (clr00.r()*b00 + clr01.r()*b01 + clr10.r()*b10 + clr11.r()*b11)*2.0 - 1.0;
    }
};


/* World size/height is just a placeholder for now. */
#define TERRAIN_WORLD_SIZE 2048.0f
#define TERRAIN_WORLD_HEIGHT 2400.0f
#define TERRAIN_SIZE 65

class TerrainStorage : public Terrain::Storage
{
    ImageInterpSrcModule mHeightmapModule;

    noise::module::Perlin mBaseFieldsTerrain;
    noise::module::ScaleBias mFieldsTerrain;
    noise::module::Billow mBaseSeaTerrain;
    noise::module::ScaleBias mSeaTerrain;
    noise::module::Select mCombinedTerrain;

    noise::module::Add mFinalTerrain;

public:
    TerrainStorage();

    virtual void getBounds(float& minX, float& maxX, float& minY, float& maxY);

    virtual bool getMinMaxHeights(float size, const osg::Vec2f &center, float& min, float& max);

    virtual void fillVertexBuffers(int lodLevel, float size, const osg::Vec2f &center, Terrain::Alignment align,
                                   std::vector<osg::Vec3f> &positions, std::vector<osg::Vec3f> &normals,
                                   std::vector<osg::Vec4ub> &colours);

    virtual void getBlendmaps(float chunkSize, const osg::Vec2f &chunkCenter, bool pack,
                              std::vector<osg::ref_ptr<osg::Image>> &blendmaps,
                              std::vector<Terrain::LayerInfo> &layerList);

    virtual void getBlendmaps(const std::vector<Terrain::QuadTreeNode*> &nodes, std::vector<Terrain::LayerCollection> &out, bool pack);

    virtual osg::Texture2D *getTextureImage(const std::string &name);

    virtual float getHeightAt(const osg::Vec3f &worldPos);

    virtual Terrain::LayerInfo getDefaultLayer()
    {
        return Terrain::LayerInfo{"dirt_grayrocky_diffusespecular.dds", "dirt_grayrocky_normalheight.dds", false, false};
    }

    virtual float getCellWorldSize() { return TERRAIN_WORLD_SIZE; }

    virtual int getCellVertices() { return TERRAIN_SIZE; }
};

TerrainStorage::TerrainStorage()
{
    mHeightmapModule.SetImage(osgDB::readImageFile("terrain/tk-heightmap.png"));
    mHeightmapModule.GetImage()->flipVertical();
    mHeightmapModule.SetFrequency(32.0);

    float fields_base = 16.0f/255.0f * 2.0f - 1.0f;
    mBaseFieldsTerrain.SetFrequency(noise::module::DEFAULT_PERLIN_FREQUENCY * 2.0);
    mFieldsTerrain.SetSourceModule(0, mBaseFieldsTerrain);
    mFieldsTerrain.SetScale(1.0 / 32.0);

    mBaseSeaTerrain.SetFrequency(2.0 * 2.0);
    mSeaTerrain.SetSourceModule(0, mBaseSeaTerrain);
    mSeaTerrain.SetScale(1.0 / 64.0);

    float edge_falloff = 8.0f/255.0f;
    mCombinedTerrain.SetSourceModule(0, mSeaTerrain);
    mCombinedTerrain.SetSourceModule(1, mFieldsTerrain);
    mCombinedTerrain.SetControlModule(mHeightmapModule);
    mCombinedTerrain.SetBounds(fields_base-edge_falloff, std::numeric_limits<double>::max());
    mCombinedTerrain.SetEdgeFalloff(edge_falloff);

    mFinalTerrain.SetSourceModule(0, mCombinedTerrain);
    mFinalTerrain.SetSourceModule(1, mHeightmapModule);
}

void TerrainStorage::getBounds(float& minX, float& maxX, float& minY, float& maxY)
{
    minX = -(int)mHeightmapModule.GetImage()->s()/2;
    minY = -(int)mHeightmapModule.GetImage()->t()/2;
    maxX =  mHeightmapModule.GetImage()->s()/2;
    maxY =  mHeightmapModule.GetImage()->t()/2;
}

bool TerrainStorage::getMinMaxHeights(float /*size*/, const osg::Vec2f& /*center*/, float& min, float& max)
{
    min = -TERRAIN_WORLD_HEIGHT*2.0f;
    max =  TERRAIN_WORLD_HEIGHT*2.0f;
    return true;
}

void TerrainStorage::fillVertexBuffers(int lodLevel, float size, const osg::Vec2f& center, Terrain::Alignment align,
                                       std::vector<osg::Vec3f>& positions, std::vector<osg::Vec3f>& normals,
                                       std::vector<osg::Vec4ub>& colours)
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
        center.x()-(size/2.0f) - cell_vtx, center.x()+(size/2.0f) + 2.0f*cell_vtx,
        center.y()-(size/2.0f) - cell_vtx, center.y()+(size/2.0f) + 2.0f*cell_vtx
    );
    builder.Build();

    noise::utils::Image normalmap(output.GetWidth(), output.GetHeight());
    noise::utils::RendererNormalMap normrender;
    normrender.SetBumpHeight(TERRAIN_WORLD_HEIGHT / (TERRAIN_WORLD_SIZE / (TERRAIN_SIZE-1)) / size);
    normrender.SetSourceNoiseMap(output);
    normrender.SetDestImage(normalmap);
    normrender.Render();

    positions.resize(TERRAIN_SIZE*TERRAIN_SIZE);
    normals.resize(TERRAIN_SIZE*TERRAIN_SIZE);
    colours.resize(TERRAIN_SIZE*TERRAIN_SIZE);

    for(int py = 0;py < TERRAIN_SIZE;++py)
    {
        const float *src = output.GetConstSlabPtr(py+1)+1;
        const noise::utils::Color *norms = normalmap.GetConstSlabPtr(py+1)+1;
        for(int px = 0;px < TERRAIN_SIZE;++px)
        {
            size_t idx = (px*TERRAIN_SIZE) + py;

            positions[idx][0] = (px/float(TERRAIN_SIZE-1) - 0.5f) * size * TERRAIN_WORLD_SIZE;
            positions[idx][1] = (py/float(TERRAIN_SIZE-1) - 0.5f) * size * TERRAIN_WORLD_SIZE;
            positions[idx][2] = src[px] * TERRAIN_WORLD_HEIGHT;
            Terrain::convertPosition(align, positions[idx][0], positions[idx][1], positions[idx][2]);

            normals[idx][0] = norms[px].red/127.5f - 1.0f;
            normals[idx][1] = norms[px].green/127.5f - 1.0f;
            normals[idx][2] = norms[px].blue/127.5f - 1.0f;
            Terrain::convertPosition(align, normals[idx][0], normals[idx][1], normals[idx][2]);

            colours[idx][0] = 255;
            colours[idx][1] = 255;
            colours[idx][2] = 255;
            colours[idx][3] = 255;
        }
    }
}

void TerrainStorage::getBlendmaps(float size, const osg::Vec2f& center, bool pack,
                                  std::vector<osg::ref_ptr<osg::Image>>& blendmaps,
                                  std::vector<Terrain::LayerInfo>& layerList)
{
    layerList.push_back(Terrain::LayerInfo{"dirt_grayrocky_diffusespecular.dds","dirt_grayrocky_normalheight.dds", true, true});
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

osg::Texture2D *TerrainStorage::getTextureImage(const std::string &name)
{
    static std::map<std::string,osg::ref_ptr<osg::Texture2D>> sTextureList;
    auto iter = sTextureList.find(name);
    if(iter != sTextureList.end())
        return iter->second.get();

    osg::ref_ptr<osg::Image> image = osgDB::readImageFile(name);
    if(!image.valid()) return nullptr;

    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(image.get());
    tex->setUnRefImageDataAfterApply(true);
    tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    sTextureList.insert(std::make_pair(name, tex));
    return tex.release();
}

float TerrainStorage::getHeightAt(const osg::Vec3f &worldPos)
{
    float val = mFinalTerrain.GetValue(worldPos.x() / TERRAIN_WORLD_SIZE, 0.0f, worldPos.z() / -TERRAIN_WORLD_SIZE);
    return val * TERRAIN_WORLD_HEIGHT;
}


CCMD(rebuildcompositemaps, "rcm")
{
    Log::get().message("Rebuilding composite maps...");
    World::get().rebuildCompositeMaps();
}


void World::initialize(osgViewer::Viewer *viewer, osg::Group *rootNode, const osg::Vec3f &cameraPos)
{
    mTerrain = new Terrain::DefaultWorld(viewer, rootNode, new TerrainStorage(), 1, true, Terrain::Align_XZ, 65536);
    mTerrain->applyMaterials(false/*Settings::Manager::getBool("enabled", "Shadows")*/,
                             false/*Settings::Manager::getBool("split", "Shadows")*/);
    mTerrain->update(cameraPos);
    mTerrain->syncLoad();
    // need to update again so the chunks that were just loaded can be made visible
    mTerrain->update(cameraPos);
}

void World::deinitialize()
{
    delete mTerrain;
    mTerrain = nullptr;
}


void World::rebuildCompositeMaps()
{
    mTerrain->rebuildCompositeMaps();
}


float World::getHeightAt(const osg::Vec3f &pos) const
{
    return mTerrain->getHeightAt(pos);
}

void World::update(const osg::Vec3f &cameraPos)
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
