#ifndef COMPONENTS_TERRAIN_QUADTREENODE_H
#define COMPONENTS_TERRAIN_QUADTREENODE_H

#include <map>

#include <osg/ref_ptr>
#include <osg/BoundingBox>

#include "defs.hpp"

namespace osg
{
    class Vec2f;
    class Vec3f;
    class Vec4f;

    class Node;
    class MatrixTransform;
    class Geode;

    class StateSet;
    class Texture2D;

    class Geometry;
    class PrimitiveSet;
}

namespace Terrain
{
    class DefaultWorld;
    class Chunk;
    class MaterialGenerator;
    struct LoadResponseData;

    enum LoadState
    {
        LS_Unloaded,
        LS_Loading,
        LS_Loaded
    };

    /**
     * @brief A node in the quad tree for our terrain. Depending on LOD,
     *        a node can either choose to render itself in one batch (merging its children),
     *        or delegate the render process to its children, rendering each child in at least one batch.
     */
    class QuadTreeNode
    {
    public:
        /// @param terrain
        /// @param dir relative to parent, or Root if we are the root node
        /// @param size size (in *cell* units!)
        /// @param center center (in *cell* units!)
        /// @param parent parent node
        QuadTreeNode (DefaultWorld* terrain, ChildDirection dir, int size, const osg::Vec2f& center, QuadTreeNode* parent);
        ~QuadTreeNode();

        /// Rebuild all materials
        void applyMaterials();

        /// Initialize neighbours - do this after the quadtree is (re)constructed
        void initNeighbours(bool childrenOnly=false);
        /// Initialize bounding boxes of non-leafs by merging children bounding boxes.
        /// Do this after the quadtree is (re)constructed
        void initAabb();

        /// Updates the neighbour bordering the given direction with the given
        /// node, and updates the descendants on the same border (e.g. if
        /// dir=East, all NE and SE children will be updated too).
        void updateNeighbour(Direction dir, QuadTreeNode *node);

        /// @note takes ownership of \a child
        void createChild(ChildDirection id, int size, const osg::Vec2f& center);

        /// Mark this node as a dummy node. This can happen if the terrain size isn't a power of two.
        /// For the QuadTree to work, we need to round the size up to a power of two, which means we'll
        /// end up with empty nodes that don't actually render anything.
        void markAsDummy() { mIsDummy = true; }
        bool isDummy() { return mIsDummy; }

        QuadTreeNode* getParent() { return mParent; }

        osg::MatrixTransform* getSceneNode() { return mSceneNode.get(); }

        int getSize() const { return mSize; }
        const osg::Vec2f& getCenter() const { return mCenter; }

        bool hasChildren() const { return mChildren[0] != 0; }
        QuadTreeNode* getChild(ChildDirection dir) const { return mChildren[dir]; }

        /// Returns our direction relative to the parent node, or Root if we are the root node.
        ChildDirection getDirection() const { return mDirection; }

        /// Get bounding box in local coordinates
        const osg::BoundingBoxf& getBoundingBox() const { return mBounds; }

        const osg::BoundingBoxf& getWorldBoundingBox() const { return mWorldBounds; }

        DefaultWorld* getTerrain() const { return mTerrain; }

        void buildQuadTree(const osg::Vec3f &cameraPos, float cellWorldSize);

        /// Adjust LODs for the given camera position, possibly splitting up chunks or merging them.
        /// @return Did we (or all of our children) choose to render?
        bool update(const osg::Vec3f &cameraPos, float cellWorldSize);

        /// Adjust index buffers of chunks to stitch together chunks of different LOD, so that cracks are avoided.
        /// Call after QuadTreeNode::update!
        void updateIndexBuffers();

        /// Get the effective LOD level if this node was rendered in one chunk
        /// with Storage::getCellVertices^2 vertices
        size_t getNativeLodLevel() const { return mLodLevel; }

        /// Is this node currently configured to render itself?
        bool hasChunk() const;

        /// Add a textured quad to a specific 2d area in the composite map scenemanager.
        /// Only nodes with size <= 1 can be rendered with alpha blending, so larger nodes will simply
        /// call this method on their children.
        /// @note Do not call this before World::areLayersLoaded() == true
        /// @param area area in image space to put the quad
        void prepareForCompositeMap(osg::Geode *geode, osg::Vec4f area);

        void clearCompositeMaps();

        /// Create a chunk for this node from the given data.
        void load(const LoadResponseData& data);
        void unload();
        void loadLayers(const std::vector<osg::ref_ptr<osg::Image>> &blendmaps, const std::vector<LayerInfo> &layerList);
        void unloadLayers();

        void getInfo(std::map<size_t,size_t> &chunks, size_t &nodes) const;

    private:
        // Stored here for convenience in case we need layer list again
        MaterialGenerator* mMaterialGenerator;

        LoadState mChunkLoadState;
        LoadState mLayerLoadState;

        bool mIsDummy;
        int mSize;
        size_t mLodLevel; // LOD if we were to render this node in one chunk
        osg::BoundingBoxf mBounds;
        osg::BoundingBoxf mWorldBounds;
        ChildDirection mDirection;
        osg::Vec2f mCenter;

        osg::ref_ptr<osg::MatrixTransform> mSceneNode;

        QuadTreeNode* mParent;
        QuadTreeNode* mChildren[4];
        QuadTreeNode* mNeighbours[4];

        osg::ref_ptr<osg::Geode> mGeode;//Chunk* mChunk;

        DefaultWorld* mTerrain;

        osg::ref_ptr<osg::StateSet> mMaterial;
        osg::ref_ptr<osg::Texture2D> mCompositeMap;

        osg::PrimitiveSet *getIndexBuffer() const;

        void ensureCompositeMap();

        void loadMaterials();

        /// Waits for any pending loads on this cell to complete
        void syncLoad();
    };

}

#endif
