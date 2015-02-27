#include "world.hpp"

#include <osgViewer/Viewer>
#include <osg/Vec3f>
#include <osg/BoundingBox>

#include "storage.hpp"

namespace Terrain
{

World::World(osgViewer::Viewer *viewer, Storage* storage, int visibilityFlags, bool shaders, Alignment align)
    : mShaders(shaders)
    , mShadows(false)
    , mSplitShadows(false)
    , mAlign(align)
    , mStorage(storage)
    , mVisibilityFlags(visibilityFlags)
    , mViewer(viewer)
    , mCache(storage->getCellVertices())
{
}

World::~World()
{
    delete mStorage;
}

float World::getHeightAt(const osg::Vec3f &worldPos)
{
    return mStorage->getHeightAt(worldPos);
}

void World::convertPosition(float &x, float &y, float &z)
{
    Terrain::convertPosition(mAlign, x, y, z);
}

void World::convertPosition(osg::Vec3f &pos)
{
    convertPosition(pos.x(), pos.y(), pos.z());
}

void World::convertBounds(osg::BoundingBoxf &bounds)
{
    switch (mAlign)
    {
    case Align_XY:
        return;
    case Align_XZ:
        convertPosition(bounds._min);
        convertPosition(bounds._max);
        // Because we changed sign of Z
        std::swap(bounds._min.z(), bounds._max.z());
        return;
    case Align_YZ:
        convertPosition(bounds._min);
        convertPosition(bounds._max);
        return;
    }
}

}
