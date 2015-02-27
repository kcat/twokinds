#include "buffercache.hpp"

#include <cassert>

#include <osg/BufferObject>
#include <osg/PrimitiveSet>
#include <osg/Array>

#include "defs.hpp"

namespace
{

template<typename IndexType>
struct ArrayType {
    typedef void type;
};
template<>
struct ArrayType<GLubyte> {
    typedef osg::DrawElementsUByte type;
};
template<>
struct ArrayType<GLushort> {
    typedef osg::DrawElementsUShort type;
};
template<>
struct ArrayType<GLuint> {
    typedef osg::DrawElementsUInt type;
};

template<typename IndexType>
osg::PrimitiveSet *createIndexBuffer(unsigned int flags, unsigned int verts)
{
    // LOD level n means every 2^n-th vertex is kept
    size_t lodLevel = (flags >> (4*4));

    size_t lodDeltas[4];
    for (int i=0; i<4; ++i)
        lodDeltas[i] = (flags >> (4*i)) & (0xf);

    bool anyDeltas = (lodDeltas[Terrain::North] || lodDeltas[Terrain::South] || lodDeltas[Terrain::West] || lodDeltas[Terrain::East]);

    size_t increment = 1 << lodLevel;
    assert(increment < verts);
    std::vector<IndexType> indices;
    indices.reserve((verts-1)*(verts-1)*2*3 / increment);

    size_t rowStart = 0, colStart = 0, rowEnd = verts-1, colEnd = verts-1;
    // If any edge needs stitching we'll skip all edges at this point,
    // mainly because stitching one edge would have an effect on corners and on the adjacent edges
    if (anyDeltas)
    {
        colStart += increment;
        colEnd -= increment;
        rowEnd -= increment;
        rowStart += increment;
    }
    for (size_t row = rowStart; row < rowEnd; row += increment)
    {
        for (size_t col = colStart; col < colEnd; col += increment)
        {
            indices.push_back(verts*col+row);
            indices.push_back(verts*(col+increment)+row+increment);
            indices.push_back(verts*col+row+increment);

            indices.push_back(verts*col+row);
            indices.push_back(verts*(col+increment)+row);
            indices.push_back(verts*(col+increment)+row+increment);
        }
    }

    size_t innerStep = increment;
    if (anyDeltas)
    {
        // Now configure LOD transitions at the edges - this is pretty tedious,
        // and some very long and boring code, but it works great

        // South
        size_t row = 0;
        size_t outerStep = 1 << (lodDeltas[Terrain::South] + lodLevel);
        for (size_t col = 0; col < verts-1; col += outerStep)
        {
            indices.push_back(verts*col+row);
            indices.push_back(verts*(col+outerStep)+row);
            // Make sure not to touch the right edge
            if (col+outerStep == verts-1)
                indices.push_back(verts*(col+outerStep-innerStep)+row+innerStep);
            else
                indices.push_back(verts*(col+outerStep)+row+innerStep);

            for (size_t i = 0; i < outerStep; i += innerStep)
            {
                // Make sure not to touch the left or right edges
                if (col+i == 0 || col+i == verts-1-innerStep)
                    continue;
                indices.push_back(verts*(col)+row);
                indices.push_back(verts*(col+i+innerStep)+row+innerStep);
                indices.push_back(verts*(col+i)+row+innerStep);
            }
        }

        // North
        row = verts-1;
        outerStep = size_t(1) << (lodDeltas[Terrain::North] + lodLevel);
        for (size_t col = 0; col < verts-1; col += outerStep)
        {
            indices.push_back(verts*(col+outerStep)+row);
            indices.push_back(verts*col+row);
            // Make sure not to touch the left edge
            if (col == 0)
                indices.push_back(verts*(col+innerStep)+row-innerStep);
            else
                indices.push_back(verts*col+row-innerStep);

            for (size_t i = 0; i < outerStep; i += innerStep)
            {
                // Make sure not to touch the left or right edges
                if (col+i == 0 || col+i == verts-1-innerStep)
                    continue;
                indices.push_back(verts*(col+i)+row-innerStep);
                indices.push_back(verts*(col+i+innerStep)+row-innerStep);
                indices.push_back(verts*(col+outerStep)+row);
            }
        }

        // West
        size_t col = 0;
        outerStep = size_t(1) << (lodDeltas[Terrain::West] + lodLevel);
        for (size_t row = 0; row < verts-1; row += outerStep)
        {
            indices.push_back(verts*col+row+outerStep);
            indices.push_back(verts*col+row);
            // Make sure not to touch the top edge
            if (row+outerStep == verts-1)
                indices.push_back(verts*(col+innerStep)+row+outerStep-innerStep);
            else
                indices.push_back(verts*(col+innerStep)+row+outerStep);

            for (size_t i = 0; i < outerStep; i += innerStep)
            {
                // Make sure not to touch the top or bottom edges
                if (row+i == 0 || row+i == verts-1-innerStep)
                    continue;
                indices.push_back(verts*col+row);
                indices.push_back(verts*(col+innerStep)+row+i);
                indices.push_back(verts*(col+innerStep)+row+i+innerStep);
            }
        }

        // East
        col = verts-1;
        outerStep = size_t(1) << (lodDeltas[Terrain::East] + lodLevel);
        for (size_t row = 0; row < verts-1; row += outerStep)
        {
            indices.push_back(verts*col+row);
            indices.push_back(verts*col+row+outerStep);
            // Make sure not to touch the bottom edge
            if (row == 0)
                indices.push_back(verts*(col-innerStep)+row+innerStep);
            else
                indices.push_back(verts*(col-innerStep)+row);

            for (size_t i = 0; i < outerStep; i += innerStep)
            {
                // Make sure not to touch the top or bottom edges
                if (row+i == 0 || row+i == verts-1-innerStep)
                    continue;
                indices.push_back(verts*col+row+outerStep);
                indices.push_back(verts*(col-innerStep)+row+i+innerStep);
                indices.push_back(verts*(col-innerStep)+row+i);
            }
        }
    }

    return new typename ArrayType<IndexType>::type(
        osg::PrimitiveSet::TRIANGLES, indices.size(), indices.data()
    );
}

}

namespace Terrain
{

BufferCache::~BufferCache()
{
}

osg::Vec2Array *BufferCache::getUVBuffer()
{
    if(mUvBuffer.valid())
        return mUvBuffer.get();

    int vertexCount = mNumVerts * mNumVerts;

    mUvBuffer = new osg::Vec2Array();
    mUvBuffer->reserveArray(vertexCount);
    for(unsigned int col = 0;col < mNumVerts;++col)
    {
        for(unsigned int row = 0;row < mNumVerts;++row)
        {
            mUvBuffer->push_back(
                osg::Vec2(col / static_cast<float>(mNumVerts-1), // U
                          row / static_cast<float>(mNumVerts-1)) // V
            );
        }
    }

    return mUvBuffer.get();
}

osg::PrimitiveSet *BufferCache::getIndexBuffer(unsigned int flags)
{
    unsigned int verts = mNumVerts;

    auto it = mIndexBufferMap.find(flags);
    if(it != mIndexBufferMap.end())
        return it->second.get();

    osg::ref_ptr<osg::PrimitiveSet> buffer;
    if(verts*verts > (0xffffu))
        buffer = createIndexBuffer<unsigned int>(flags, verts);
    else
        buffer = createIndexBuffer<unsigned short>(flags, verts);

    mIndexBufferMap.insert(std::make_pair(flags, buffer));
    return buffer.release();
}

}
