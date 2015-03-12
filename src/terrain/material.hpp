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

class LayerIdentifier;

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

    /// Creates a StateSet suitable for displaying a chunk of terrain.
    osg::StateSet *generate();

    /// Creates a StateSet suitable for displaying a chunk of terrain using a ready-made composite map and normal map.
    osg::StateSet *generateForCompositeMap(osg::Texture2D *compositeMap, osg::Texture2D *normalMap);

    /// Creates a StateSet suitable for rendering composite maps, i.e. for "baking" several layer textures
    /// into one. The main difference compared to a normal StateSet is that no shading is applied at this point.
    osg::StateSet *generateForCompositeMapRTT(int lodLevel);

private:
    osg::StateSet *create(bool renderCompositeMap, osg::Texture2D *compositeMap, osg::Texture2D *normalMap, int lodLevel);

    std::vector<LayerInfo> mLayerList;
    std::vector<osg::ref_ptr<osg::Image>> mBlendmapList;
    bool mShaders;
    bool mShadows;
    bool mSplitShadows;
    bool mNormalMapping;
    bool mParallaxMapping;

    Storage *mStorage;

    static std::map<LayerIdentifier,osg::ref_ptr<osg::Program>> mPrograms;
};

}

#endif
