#ifndef TERRAIN_HPP
#define TERRAIN_HPP

#include <iostream>

namespace osg
{
    class Vec3f;
    class Group;
}

namespace osgViewer
{
    class Viewer;
}

namespace Terrain
{
    class World;
}

namespace TK
{

class ImageSrcModule;

class World
{
    static World sWorld;

    Terrain::World *mTerrain;

    World();
public:
    void initialize(osgViewer::Viewer *viewer, osg::Group *rootNode, const osg::Vec3f &cameraPos);
    void deinitialize();

    void rebuildCompositeMaps();

    float getHeightAt(const osg::Vec3f &pos) const;
    void update(const osg::Vec3f &cameraPos);

    void getStatus(std::ostream &status) const;

    static World &get() { return sWorld; }
    static World *getPtr() { return &sWorld; }
};

} // namespace TK

#endif /* TERRAIN_HPP */
