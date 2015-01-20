#ifndef TERRAIN_HPP
#define TERRAIN_HPP

#include <sstream>

namespace Ogre
{
    class Vector3;
    class Camera;
    class Light;
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
    void initialize(Ogre::Camera *camera, Ogre::Light *l);
    void deinitialize();

    float getHeightAt(const Ogre::Vector3 &pos) const;
    void update(const Ogre::Vector3 &cameraPos);

    void getStatus(std::stringstream &status) const;

    static World &get() { return sWorld; }
    static World *getPtr() { return &sWorld; }
};

} // namespace TK

#endif /* TERRAIN_HPP */
