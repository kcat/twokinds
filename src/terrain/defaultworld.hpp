#ifndef COMPONENTS_TERRAIN_H
#define COMPONENTS_TERRAIN_H

#include <vector>

#include "world.hpp"

namespace osg
{
    class Vec3f;
    class Vec4ub;
    class Texture2D;
    class Group;
    class Geode;
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
    class DefaultWorld : public World
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
        DefaultWorld(osgViewer::Viewer *viewer, osg::Group *rootNode, Storage* storage,
                     int visibilityFlags, bool shaders, Alignment align,
                     int maxBatchSize, int compmapsize);
        ~DefaultWorld();

        /// Update chunk LODs according to this camera position
        /// @note Calling this method might lead to composite textures being rendered, so it is best
        /// not to call it when render commands are still queued, since that would cause a flush.
        virtual void update (const osg::Vec3f& cameraPos);

        /// Get the world bounding box of a chunk of terrain centered at \a center
        virtual osg::BoundingBoxf getWorldBoundingBox (const osg::Vec2f& center);

        osg::Group *getRootSceneNode() { return mRootSceneNode.get(); }

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

        virtual void rebuildCompositeMaps(int compmapsize);

        int getMaxBatchSize() const { return mMaxBatchSize; }

        float getMinX() const { return mMinX; }
        float getMaxX() const { return mMaxX; }
        float getMinY() const { return mMinY; }
        float getMaxY() const { return mMaxY; }

        /// Wait until all background loading is complete.
        virtual void syncLoad();

        virtual void getStatus(std::ostream &status) const;

    private:
        // Called from a background worker thread
        //virtual Ogre::WorkQueue::Response* handleRequest(const Ogre::WorkQueue::Request* req, const Ogre::WorkQueue* srcQ);
        // Called from the main thread
        //virtual void handleResponse(const Ogre::WorkQueue::Response* res, const Ogre::WorkQueue* srcQ);
        //Ogre::uint16 mWorkQueueChannel;

        bool mVisible;

        QuadTreeNode* mRootNode;
        osg::ref_ptr<osg::Group> mRootSceneNode;

        /// The number of chunks currently loading in a background thread. If 0, we have finished loading!
        int mChunksLoading;

        /// The number of layer data requests. This is done when new QuadTreeNodes are created (but in a background thread)
        int mLayersLoading;

        //Ogre::SceneManager* mCompositeMapSceneMgr;
        osg::ref_ptr<osg::Group> mCompositorRootSceneNode;
        bool mCompositorRan;

        bool mUpdateIndexBuffers;

        /// Bounds in cell units
        float mMinX, mMaxX, mMinY, mMaxY;

        /// Maximum size of a terrain batch along one side (in cell units)
        int mMaxBatchSize;

        /// Composite map size
        int mCompositeMapSize;

    public:
        // ----INTERNAL----
        //Ogre::SceneManager* getCompositeMapSceneManager() { return mCompositeMapSceneMgr; }

        void renderCompositeMap(osg::Texture2D *target, osg::Texture2D *normalmap, osg::Geode *geode);
        void setCompositorRan() { mCompositorRan = true; }

        void setUpdateIndexBuffers() { mUpdateIndexBuffers = true; }

        // Adds a WorkQueue request to load a chunk for this node in the background.
        void queueChunkLoad(QuadTreeNode* node);
        // Adds a WorkQueue request to load layers for this node in the background.
        void queueLayerLoad(QuadTreeNode* leaf);

    private:
        //Ogre::RenderTarget* mCompositeMapRenderTarget;
        //Ogre::TexturePtr mCompositeMapRenderTexture;
    };

    struct LoadRequestData
    {
        QuadTreeNode* mNode;

        friend std::ostream& operator<<(std::ostream& o, const LoadRequestData& r)
        { return o; }
    };

    struct LoadResponseData
    {
        std::vector<osg::Vec3f> mPositions;
        std::vector<osg::Vec3f> mNormals;
        std::vector<osg::Vec4ub> mColours;

        friend std::ostream& operator<<(std::ostream& o, const LoadResponseData& r)
        { return o; }
    };

    struct LayerRequestData
    {
        QuadTreeNode *mNode;
        bool mPack;

        friend std::ostream& operator<<(std::ostream& o, const LayerRequestData& r)
        { return o; }
    };

    struct LayerResponseData
    {
        // Since we can't create a texture from a different thread, this only holds the raw texel data
        std::vector<osg::ref_ptr<osg::Image>> mBlendmaps;
        std::vector<LayerInfo> mLayers;

        friend std::ostream& operator<<(std::ostream& o, const LayerResponseData& r)
        { return o; }
    };

}

#endif
