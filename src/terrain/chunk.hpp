#ifndef COMPONENTS_TERRAIN_TERRAINBATCH_H
#define COMPONENTS_TERRAIN_TERRAINBATCH_H

#include <vector>

#include <osg/ref_ptr>
#include <osg/BoundingBox>

namespace osg
{
    class VertexBufferObject;
    class ElementBufferObject;
    class StateSet;
}

namespace Terrain
{
#if 0
/**
 * @brief A movable object representing a chunk of terrain.
 */
class Chunk// : public Ogre::Renderable, public Ogre::MovableObject
{
public:
    Chunk(osg::VertexBufferObject *uvBuffer, const osg::BoundingBoxf& bounds,
          const std::vector<float>& positions,
          const std::vector<float>& normals,
          const std::vector<unsigned char>& colours);
    virtual ~Chunk();

    /// @param own Should we take ownership of the material?
    void setMaterial(osg::StateSet *material);

    void setIndexBuffer(osg::ElementBufferObject *buffer);

#if 0
    // Inherited from MovableObject
    virtual const Ogre::String& getMovableType(void) const { static Ogre::String t = "MW_TERRAIN"; return t; }
    virtual const Ogre::AxisAlignedBox& getBoundingBox(void) const;
    virtual Ogre::Real getBoundingRadius(void) const;
    virtual void _updateRenderQueue(Ogre::RenderQueue* queue);
    virtual void visitRenderables(Renderable::Visitor* visitor, bool debugRenderables = false);

    // Inherited from Renderable
    virtual const Ogre::MaterialPtr& getMaterial(void) const;
    virtual void getRenderOperation(Ogre::RenderOperation& op);
    virtual void getWorldTransforms(Ogre::Matrix4* xform) const;
    virtual Ogre::Real getSquaredViewDepth(const Ogre::Camera* cam) const;
    virtual const Ogre::LightList& getLights(void) const;
#endif

private:
    osg::BoundingBoxf mBounds;
    osg::ref_ptr<osg::StateSet> mMaterial;

    //Ogre::VertexData* mVertexData;
    //Ogre::IndexData* mIndexData;
};
#endif

}

#endif
