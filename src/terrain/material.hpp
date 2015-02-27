#ifndef COMPONENTS_TERRAIN_MATERIAL_H
#define COMPONENTS_TERRAIN_MATERIAL_H

#include "storage.hpp"

#include <osg/ref_ptr>
#include <osg/Image>

namespace osg
{
    class StateSet;
    class Texture2D;
}

namespace Terrain
{

class MaterialGenerator
{
public:
    MaterialGenerator(Storage *storage);

    void setLayerList(const std::vector<LayerInfo>& layerList) { mLayerList = layerList; }
    bool hasLayers() const { return !mLayerList.empty(); }
    void setBlendmapList(const std::vector<osg::ref_ptr<osg::Image>>& blendmapList) { mBlendmapList = blendmapList; }
    const std::vector<osg::ref_ptr<osg::Image>>& getBlendmapList() const { return mBlendmapList; }

    void enableShaders(bool shaders) { mShaders = shaders; }
    void enableShadows(bool shadows) { mShadows = shadows; }
    void enableNormalMapping(bool normalMapping) { mNormalMapping = normalMapping; }
    void enableParallaxMapping(bool parallaxMapping) { mParallaxMapping = parallaxMapping; }
    void enableSplitShadows(bool splitShadows) { mSplitShadows = splitShadows; }

    /// Creates a material suitable for displaying a chunk of terrain using alpha-blending.
    osg::StateSet *generate();

    /// Creates a material suitable for displaying a chunk of terrain using a ready-made composite map.
    osg::StateSet *generateForCompositeMap(osg::Texture2D *compositeMap);

    /// Creates a material suitable for rendering composite maps, i.e. for "baking" several layer textures
    /// into one. The main difference compared to a normal material is that no shading is applied at this point.
    osg::StateSet *generateForCompositeMapRTT(int lodLevel);

private:
    osg::StateSet *create(bool renderCompositeMap, bool displayCompositeMap, osg::Texture2D *compositeMap, int lodLevel);

    std::vector<LayerInfo> mLayerList;
    std::vector<osg::ref_ptr<osg::Image>> mBlendmapList;
    bool mShaders;
    bool mShadows;
    bool mSplitShadows;
    bool mNormalMapping;
    bool mParallaxMapping;

    Storage *mStorage;
};

}

#endif
