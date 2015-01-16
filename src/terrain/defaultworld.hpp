#ifndef COMPONENTS_TERRAIN_H
#define COMPONENTS_TERRAIN_H

#include <OgreAxisAlignedBox.h>
#include <OgreTexture.h>
#include <OgreWorkQueue.h>

#include "world.hpp"

namespace Ogre
{
    class Camera;
}

namespace Terrain
{

    class QuadTreeNode;
    class Storage;

    /**
     * @brief A quadtree-based terrain implementation suitable for large data sets. \n
     *        Near cells are rendered with alpha splatting, distant cells are merged
     *        together in batches and have their layers pre-rendered onto a composite map. \n
     *        Cracks at LOD transitions are avoided using stitching.
     * @note  Multiple cameras are not supported yet
     */
    class DefaultWorld : public World, public Ogre::WorkQueue::RequestHandler, public Ogre::WorkQueue::ResponseHandler
    {
    public:
        /// @note takes ownership of \a storage
        /// @param sceneMgr scene manager to use
        /// @param storage Storage instance to get terrain data from (heights, normals, colors, textures..)
        /// @param visbilityFlags visibility flags for the created meshes
        /// @param shaders Whether to use splatting shader, or multi-pass fixed function splatting. Shader is usually
        ///         faster so this is just here for compatibility.
        /// @param align The align of the terrain, see Alignment enum
        /// @param maxBatchSize Maximum size of a terrain batch along one side (in cell units). Used when traversing the quad tree.
        DefaultWorld(Ogre::SceneManager* sceneMgr,
                Storage* storage, int visibilityFlags, bool shaders, Alignment align, int maxBatchSize);
        ~DefaultWorld();

        /// Update chunk LODs according to this camera position
        /// @note Calling this method might lead to composite textures being rendered, so it is best
        /// not to call it when render commands are still queued, since that would cause a flush.
        virtual void update (const Ogre::Vector3& cameraPos);

        /// Get the world bounding box of a chunk of terrain centered at \a center
        virtual Ogre::AxisAlignedBox getWorldBoundingBox (const Ogre::Vector2& center);

        Ogre::SceneNode* getRootSceneNode() { return mRootSceneNode; }

        /// Show or hide the whole terrain
        /// @note this setting will be invalidated once you call Terrain::update, so do not call it while the terrain should be hidden
        virtual void setVisible(bool visible);
        virtual bool getVisible();

        /// Recreate materials used by terrain chunks. This should be called whenever settings of
        /// the material factory are changed. (Relying on the factory to update those materials is not
        /// enough, since turning a feature on/off can change the number of texture units available for layer/blend
        /// textures, and to properly respond to this we may need to change the structure of the material, such as
        /// adding or removing passes. This can only be achieved by a full rebuild.)
        virtual void applyMaterials(bool shadows, bool splitShadows);

        int getMaxBatchSize() const { return mMaxBatchSize; }

        float getMinX() const { return mMinX; }
        float getMaxX() const { return mMaxX; }
        float getMinY() const { return mMinY; }
        float getMaxY() const { return mMaxY; }

        /// Wait until all background loading is complete.
        void syncLoad();

    private:
        // Called from a background worker thread
        virtual Ogre::WorkQueue::Response* handleRequest(const Ogre::WorkQueue::Request* req, const Ogre::WorkQueue* srcQ);
        // Called from the main thread
        virtual void handleResponse(const Ogre::WorkQueue::Response* res, const Ogre::WorkQueue* srcQ);
        Ogre::uint16 mWorkQueueChannel;

        bool mVisible;

        QuadTreeNode* mRootNode;
        Ogre::SceneNode* mRootSceneNode;

        /// The number of chunks currently loading in a background thread. If 0, we have finished loading!
        int mChunksLoading;

        /// The number of layer data requests. This is done when new QuadTreeNodes are created (but in a background thread)
        int mLayersLoading;

        Ogre::SceneManager* mCompositeMapSceneMgr;

        /// Bounds in cell units
        float mMinX, mMaxX, mMinY, mMaxY;

        /// Maximum size of a terrain batch along one side (in cell units)
        int mMaxBatchSize;

        /// Reusable nodes
        std::vector<QuadTreeNode*> mFreeNodes;

    public:
        // ----INTERNAL----
        Ogre::SceneManager* getCompositeMapSceneManager() { return mCompositeMapSceneMgr; }

        bool areLayersLoaded() { return !mLayersLoading; }

        // Delete all quads
        void clearCompositeMapSceneManager();
        void renderCompositeMap (Ogre::TexturePtr target);

        void freeNode(QuadTreeNode *node);
        QuadTreeNode *createNode(ChildDirection dir, int size, const Ogre::Vector2& center, QuadTreeNode* parent);

        // Adds a WorkQueue request to load a chunk for this node in the background.
        void queueLoad (QuadTreeNode* node);
        // Adds a WorkQueue request to load layers for these nodes in the background.
        void queueLayerLoad (std::vector<QuadTreeNode*>& leafs);

    private:
        Ogre::RenderTarget* mCompositeMapRenderTarget;
        Ogre::TexturePtr mCompositeMapRenderTexture;
    };

    struct LoadRequestData
    {
        QuadTreeNode* mNode;

        friend std::ostream& operator<<(std::ostream& o, const LoadRequestData& r)
        { return o; }
    };

    struct LoadResponseData
    {
        std::vector<float> mPositions;
        std::vector<float> mNormals;
        std::vector<Ogre::uint8> mColours;

        friend std::ostream& operator<<(std::ostream& o, const LoadResponseData& r)
        { return o; }
    };

    struct LayersRequestData
    {
        std::vector<QuadTreeNode*> mNodes;
        bool mPack;

        friend std::ostream& operator<<(std::ostream& o, const LayersRequestData& r)
        { return o; }
    };

    struct LayersResponseData
    {
        std::vector<LayerCollection> mLayerCollections;

        friend std::ostream& operator<<(std::ostream& o, const LayersResponseData& r)
        { return o; }
    };

}

#endif
