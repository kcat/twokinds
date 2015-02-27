#include "chunk.hpp"

#include <osg/BufferObject>

#if TERRAIN_USE_SHADER
#include <extern/shiny/Main/Factory.hpp>
#endif


namespace Terrain
{

#if 0
Chunk::Chunk(osg::VertexBufferObject *uvBuffer, const osg::BoundingBoxf& bounds,
             const std::vector<float>& positions, const std::vector<float>& normals,
             const std::vector<unsigned char>& colours)
    : mBounds(bounds)
{


    mVertexData = OGRE_NEW Ogre::VertexData;
    mVertexData->vertexStart = 0;
    mVertexData->vertexCount = positions.size()/3;

    // Set up the vertex declaration, which specifies the info for each vertex (normals, colors, UVs, etc)
    Ogre::VertexDeclaration* vertexDecl = mVertexData->vertexDeclaration;

    Ogre::HardwareBufferManager* mgr = Ogre::HardwareBufferManager::getSingletonPtr();
    size_t nextBuffer = 0;

    // Positions
    vertexDecl->addElement(nextBuffer++, 0, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
    Ogre::HardwareVertexBufferSharedPtr vertexBuffer = mgr->createVertexBuffer(Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3),
                                            mVertexData->vertexCount, Ogre::HardwareBuffer::HBU_STATIC);

    // Normals
    vertexDecl->addElement(nextBuffer++, 0, Ogre::VET_FLOAT3, Ogre::VES_NORMAL);
    Ogre::HardwareVertexBufferSharedPtr normalBuffer = mgr->createVertexBuffer(Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3),
                                            mVertexData->vertexCount, Ogre::HardwareBuffer::HBU_STATIC);


    // UV texture coordinates
    vertexDecl->addElement(nextBuffer++, 0, Ogre::VET_FLOAT2,
                            Ogre::VES_TEXTURE_COORDINATES, 0);

    // Colours
    vertexDecl->addElement(nextBuffer++, 0, Ogre::VET_COLOUR, Ogre::VES_DIFFUSE);
    Ogre::HardwareVertexBufferSharedPtr colourBuffer = mgr->createVertexBuffer(Ogre::VertexElement::getTypeSize(Ogre::VET_COLOUR),
                                            mVertexData->vertexCount, Ogre::HardwareBuffer::HBU_STATIC);

    vertexBuffer->writeData(0, vertexBuffer->getSizeInBytes(), &positions[0], true);
    normalBuffer->writeData(0, normalBuffer->getSizeInBytes(), &normals[0], true);
    colourBuffer->writeData(0, colourBuffer->getSizeInBytes(), &colours[0], true);

    mVertexData->vertexBufferBinding->setBinding(0, vertexBuffer);
    mVertexData->vertexBufferBinding->setBinding(1, normalBuffer);
    mVertexData->vertexBufferBinding->setBinding(2, uvBuffer);
    mVertexData->vertexBufferBinding->setBinding(3, colourBuffer);

    // Assign a default material in case terrain material fails to be created
    mMaterial = Ogre::MaterialManager::getSingleton().getByName("BaseWhite");

    mIndexData = OGRE_NEW Ogre::IndexData();
    mIndexData->indexStart = 0;
}

void Chunk::setIndexBuffer(osg::ElementBufferObject *buffer)
{
    mIndexData->indexBuffer = buffer;
    mIndexData->indexCount = buffer->getNumIndexes();
}

Chunk::~Chunk()
{
    OGRE_DELETE mVertexData;
    OGRE_DELETE mIndexData;
}

void Chunk::setMaterial(osg::StateSet *material)
{
    mMaterial = material;
}

#if 0
const osg::BoundingBoxf& Chunk::getBoundingBox(void) const
{
    return mBounds;
}

float Chunk::getBoundingRadius(void) const
{
    return mBounds.radius();
}

void Chunk::_updateRenderQueue(Ogre::RenderQueue* queue)
{
    queue->addRenderable(this, mRenderQueueID);
}

void Chunk::visitRenderables(Ogre::Renderable::Visitor* visitor,
    bool debugRenderables)
{
    visitor->visit(this, 0, false);
}

const Ogre::MaterialPtr& Chunk::getMaterial(void) const
{
    return mMaterial;
}

void Chunk::getRenderOperation(Ogre::RenderOperation& op)
{
    assert (!mIndexData->indexBuffer.isNull() && "Trying to render, but no index buffer set!");
    assert(!mMaterial.isNull() && "Trying to render, but no material set!");
    op.useIndexes = true;
    op.operationType = Ogre::RenderOperation::OT_TRIANGLE_LIST;
    op.vertexData = mVertexData;
    op.indexData = mIndexData;
}

void Chunk::getWorldTransforms(Ogre::Matrix4* xform) const
{
    *xform = getParentSceneNode()->_getFullTransform();
}

Ogre::Real Chunk::getSquaredViewDepth(const Ogre::Camera* cam) const
{
    return getParentSceneNode()->getSquaredViewDepth(cam);
}

const Ogre::LightList& Chunk::getLights(void) const
{
    return queryLights();
}
#endif

#endif
}
