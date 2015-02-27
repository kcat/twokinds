#ifndef COMPONENTS_TERRAIN_BUFFERCACHE_H
#define COMPONENTS_TERRAIN_BUFFERCACHE_H

#include <map>

#include <osg/ref_ptr>
#include <osg/Array>

namespace osg
{
    class PrimitiveSet;
}

namespace Terrain
{

/// @brief Implements creation and caching of vertex buffers for terrain chunks.
class BufferCache
{
public:
    BufferCache(unsigned int numVerts) : mNumVerts(numVerts) {}
    ~BufferCache();

    /// @param flags first 4*4 bits are LOD deltas on each edge, respectively (4 bits each)
    ///              next 4 bits are LOD level of the index buffer (LOD 0 = don't omit any vertices)
    osg::PrimitiveSet *getIndexBuffer(unsigned int flags);

    osg::Vec2Array *getUVBuffer();

private:
    // Index buffers are shared across terrain batches where possible. There is one index buffer for each
    // combination of LOD deltas and index buffer LOD we may need.
    std::map<unsigned int, osg::ref_ptr<osg::PrimitiveSet>> mIndexBufferMap;

    osg::ref_ptr<osg::Vec2Array> mUvBuffer;

    unsigned int mNumVerts;
};

}

#endif
