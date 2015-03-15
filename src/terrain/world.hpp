#ifndef COMPONENTS_TERRAIN_WORLD_H
#define COMPONENTS_TERRAIN_WORLD_H

#include <iostream>

#include <osg/ref_ptr>
#include <osg/BoundingBox>

#include "defs.hpp"
#include "buffercache.hpp"

namespace osg
{
    class Vec2f;
    class Vec3f;
}

namespace osgViewer
{
    class Viewer;
}

namespace Terrain
{
    class Storage;

    /**
     * @brief The basic interface for a terrain world. How the terrain chunks are paged and displayed
     *  is up to the implementation.
     */
    class World
    {
    public:
        /// @note takes ownership of \a storage
        /// @param sceneMgr scene manager to use
        /// @param storage Storage instance to get terrain data from (heights, normals, colors, textures..)
        /// @param visbilityFlags visibility flags for the created meshes
        /// @param shaders Whether to use splatting shader, or multi-pass fixed function splatting. Shader is usually
        ///         faster so this is just here for compatibility.
        /// @param align The align of the terrain, see Alignment enum
        World(osgViewer::Viewer *viewer,
                Storage* storage, int visiblityFlags, bool shaders, Alignment align);
        virtual ~World();

        bool getShadersEnabled() { return mShaders; }
        bool getShadowsEnabled() { return mShadows; }
        bool getSplitShadowsEnabled() { return mSplitShadows; }

        float getHeightAt (const osg::Vec3f& worldPos);

        /// Update chunk LODs according to this camera position
        /// @note Calling this method might lead to composite textures being rendered, so it is best
        /// not to call it when render commands are still queued, since that would cause a flush.
        virtual void update (const osg::Vec3f& cameraPos) = 0;

        // This is only a hint and may be ignored by the implementation.
        virtual void loadCell(int x, int y) {}
        virtual void unloadCell(int x, int y) {}

        /// Get the world bounding box of a chunk of terrain centered at \a center
        virtual osg::BoundingBoxf getWorldBoundingBox (const osg::Vec2f& center) = 0;

        osgViewer::Viewer *getSceneManager() { return mViewer.get(); }

        Storage* getStorage() { return mStorage; }

        /// Show or hide the whole terrain
        /// @note this setting may be invalidated once you call Terrain::update, so do not call it while the terrain should be hidden
        virtual void setVisible(bool visible) = 0;
        virtual bool getVisible() = 0;

        /// Recreate materials used by terrain chunks. This should be called whenever settings of
        /// the material factory are changed. (Relying on the factory to update those materials is not
        /// enough, since turning a feature on/off can change the number of texture units available for layer/blend
        /// textures, and to properly respond to this we may need to change the structure of the material, such as
        /// adding or removing passes. This can only be achieved by a full rebuild.)
        virtual void applyMaterials(bool shadows, bool splitShadows) = 0;

        virtual void rebuildCompositeMaps(int) { }

        int getVisibilityFlags() { return mVisibilityFlags; }

        Alignment getAlign() { return mAlign; }

        /// Wait until all background loading is complete.
        virtual void syncLoad() {}

        virtual void getStatus(std::ostream&) const { }

    protected:
        bool mShaders;
        bool mShadows;
        bool mSplitShadows;
        Alignment mAlign;

        Storage* mStorage;

        int mVisibilityFlags;

        osg::ref_ptr<osgViewer::Viewer> mViewer;

        BufferCache mCache;

    public:
        // ----INTERNAL----
        BufferCache& getBufferCache() { return mCache; }

        // Convert the given position from Z-up align, i.e. Align_XY to the wanted align set in mAlign
        void convertPosition (float& x, float& y, float& z);
        void convertPosition (osg::Vec3f& pos);
        void convertBounds (osg::BoundingBoxf& bounds);
    };

}

#endif
