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
    size_t lodDeltas[4];
    for (int i=0; i<4; ++i)
        lodDeltas[i] = (flags >> (4*i)) & (0xf);

    bool anyDeltas = (lodDeltas[Terrain::North] || lodDeltas[Terrain::South] || lodDeltas[Terrain::West] || lodDeltas[Terrain::East]);

    std::vector<IndexType> indices;
    indices.reserve((verts-1) * (verts-1) * 2 * 3);

    size_t rowStart = 0, colStart = 0, rowEnd = verts-1, colEnd = verts-1;
    // If any edge needs stitching we'll skip all edges at this point,
    // mainly because stitching one edge would have an effect on corners and on the adjacent edges
    if (anyDeltas)
    {
        ++colStart;
        --colEnd;
        --rowEnd;
        ++rowStart;
    }
    for (size_t row = rowStart; row < rowEnd; ++row)
    {
        for (size_t col = colStart; col < colEnd; ++col)
        {
            indices.push_back(verts* col    + row);
            indices.push_back(verts*(col+1) + row+1);
            indices.push_back(verts* col    + row+1);

            indices.push_back(verts* col    + row);
            indices.push_back(verts*(col+1) + row);
            indices.push_back(verts*(col+1) + row+1);
        }
    }

    if (anyDeltas)
    {
        // Now configure LOD transitions at the edges - this is pretty tedious,
        // and some very long and boring code, but it works great

        // South
        size_t row = 0;
        size_t outerStep = 1 << lodDeltas[Terrain::South];
        for (size_t col = 0; col < verts-1; col += outerStep)
        {
            indices.push_back(verts*col+row);
            indices.push_back(verts*(col+outerStep)+row);
            // Make sure not to touch the right edge
            if (col+outerStep == verts-1)
                indices.push_back(verts*(col+outerStep-1) + row+1);
            else
                indices.push_back(verts*(col+outerStep)   + row+1);

            for (size_t i = 0; i < outerStep; ++i)
            {
                // Make sure not to touch the left or right edges
                if (col+i == 0 || col+i == verts-1-1)
                    continue;
                indices.push_back(verts*(col)     + row);
                indices.push_back(verts*(col+i+1) + row+1);
                indices.push_back(verts*(col+i)   + row+1);
            }
        }

        // North
        row = verts-1;
        outerStep = 1 << lodDeltas[Terrain::North];
        for (size_t col = 0; col < verts-1; col += outerStep)
        {
            indices.push_back(verts*(col+outerStep) + row);
            indices.push_back(verts* col            + row);
            // Make sure not to touch the left edge
            if (col == 0)
                indices.push_back(verts*(col+1) + row-1);
            else
                indices.push_back(verts*(col)   + row-1);

            for (size_t i = 0; i < outerStep; ++i)
            {
                // Make sure not to touch the left or right edges
                if (col+i == 0 || col+i == verts-1-1)
                    continue;
                indices.push_back(verts*(col+i)         + row-1);
                indices.push_back(verts*(col+i+1)       + row-1);
                indices.push_back(verts*(col+outerStep) + row);
            }
        }

        // West
        size_t col = 0;
        outerStep = 1 << lodDeltas[Terrain::West];
        for (size_t row = 0; row < verts-1; row += outerStep)
        {
            indices.push_back(verts*col + row+outerStep);
            indices.push_back(verts*col + row);
            // Make sure not to touch the top edge
            if (row+outerStep == verts-1)
                indices.push_back(verts*(col+1) + row+outerStep-1);
            else
                indices.push_back(verts*(col+1) + row+outerStep);

            for (size_t i = 0; i < outerStep; ++i)
            {
                // Make sure not to touch the top or bottom edges
                if (row+i == 0 || row+i == verts-1-1)
                    continue;
                indices.push_back(verts* col    + row);
                indices.push_back(verts*(col+1) + row+i);
                indices.push_back(verts*(col+1) + row+i+1);
            }
        }

        // East
        col = verts-1;
        outerStep = 1 << lodDeltas[Terrain::East];
        for (size_t row = 0; row < verts-1; row += outerStep)
        {
            indices.push_back(verts*col + row);
            indices.push_back(verts*col + row+outerStep);
            // Make sure not to touch the bottom edge
            if (row == 0)
                indices.push_back(verts*(col-1) + row+1);
            else
                indices.push_back(verts*(col-1) + row);

            for (size_t i = 0; i < outerStep; ++i)
            {
                // Make sure not to touch the top or bottom edges
                if (row+i == 0 || row+i == verts-1-1)
                    continue;
                indices.push_back(verts* col    + row+outerStep);
                indices.push_back(verts*(col-1) + row+i+1);
                indices.push_back(verts*(col-1) + row+i);
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

std::map<unsigned int,osg::ref_ptr<osg::PrimitiveSet>> BufferCache::mPrimitiveMap;
std::map<unsigned int,osg::ref_ptr<osg::Vec2Array>> BufferCache::mUvBuffers;


BufferCache::~BufferCache()
{
}

osg::Vec2Array *BufferCache::getUVBuffer()
{
    osg::ref_ptr<osg::Vec2Array> &buffer = mUvBuffers[mNumVerts];
    if(buffer.valid()) return buffer.get();

    int vertexCount = mNumVerts * mNumVerts;

    buffer = new osg::Vec2Array();
    buffer->reserveArray(vertexCount);
    for(unsigned int col = 0;col < mNumVerts;++col)
    {
        for(unsigned int row = 0;row < mNumVerts;++row)
        {
            buffer->push_back(
                osg::Vec2(col / static_cast<float>(mNumVerts-1), // U
                          row / static_cast<float>(mNumVerts-1)) // V
            );
        }
    }

    return buffer.get();
}

osg::PrimitiveSet *BufferCache::getPrimitive(unsigned int flags)
{
    unsigned int verts = mNumVerts;

    osg::ref_ptr<osg::PrimitiveSet> &prim = mPrimitiveMap[flags];
    if(prim.valid()) return prim.get();

    if(verts > 255)
        prim = createIndexBuffer<unsigned int>((flags&0xffff), verts);
    else
        prim = createIndexBuffer<unsigned short>((flags&0xffff), verts);

    return prim.get();
}

}
