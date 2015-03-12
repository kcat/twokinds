#include "defaultworld.hpp"

#include <iostream>
#include <cassert>

#include <osgViewer/Viewer>
#include <osg/MatrixTransform>
#include <osg/Texture2D>
#include <osg/Depth>
#include <osg/Material>

#include "storage.hpp"
#include "quadtreenode.hpp"

namespace
{

int nextPowerOfTwo(int v)
{
    if(v != 0)
    {
        --v;
        v |= v>>1;
        v |= v>>2;
        v |= v>>4;
        v |= v>>8;
        v |= v>>16;
    }
    return v+1;
}

Terrain::QuadTreeNode* findNode (const osg::Vec2f& center, Terrain::QuadTreeNode* node)
{
    if(center == node->getCenter())
        return node;

    if(center.x() > node->getCenter().x() && center.y() > node->getCenter().y())
        return findNode(center, node->getChild(Terrain::NE));
    else if(center.x() > node->getCenter().x() && center.y() < node->getCenter().y())
        return findNode(center, node->getChild(Terrain::SE));
    else if(center.x() < node->getCenter().x() && center.y() > node->getCenter().y())
        return findNode(center, node->getChild(Terrain::NW));
    else //if(center.x() < node->getCenter().x() && center.y() < node->getCenter().y())
        return findNode(center, node->getChild(Terrain::SW));
}

class CompositorRanCallback : public osg::Camera::DrawCallback {
    Terrain::DefaultWorld *mTerrain;

public:
    CompositorRanCallback(Terrain::DefaultWorld *terrain)
      : mTerrain(terrain)
    { }

    virtual void operator()(osg::RenderInfo&/*info*/) const
    {
        mTerrain->setCompositorRan();
    }
};

}

namespace Terrain
{

    //const Ogre::uint REQ_ID_CHUNK = 1;
    //const Ogre::uint REQ_ID_LAYER = 2;

    DefaultWorld::DefaultWorld(osgViewer::Viewer *viewer, osg::Group *rootNode, Storage *storage,
                               int visibilityFlags, bool shaders, Alignment align, int maxBatchSize)
      : World(viewer, storage, visibilityFlags, shaders, align)
      , mVisible(true)
      , mChunksLoading(0)
      , mLayersLoading(0)
      , mCompositorRan(false)
      , mUpdateIndexBuffers(false)
      , mMinX(0)
      , mMaxX(0)
      , mMinY(0)
      , mMaxY(0)
      , mMaxBatchSize(maxBatchSize)
    {
        mRootSceneNode = new osg::Group();
        {
            osg::StateSet *state = mRootSceneNode->getOrCreateStateSet();

            state->setMode(GL_BLEND, osg::StateAttribute::OFF);
            state->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
            state->setAttribute(new osg::Depth(osg::Depth::LESS));
        }

        mCompositorRootSceneNode = new osg::Group();
        {
            osg::StateSet *state = mCompositorRootSceneNode->getOrCreateStateSet();

            state->setMode(GL_BLEND, osg::StateAttribute::OFF);
            state->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
            state->setAttribute(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false));
        }

        storage->getBounds(mMinX, mMaxX, mMinY, mMaxY);

        int origSizeX = mMaxX-mMinX;
        int origSizeY = mMaxY-mMinY;

        // Dividing a quad tree only works well for powers of two, so round up to the nearest one
        int size = nextPowerOfTwo(std::max(origSizeX, origSizeY));

        // Adjust the center according to the new size
        float centerX = (mMinX+mMaxX)/2.f + (size-origSizeX)/2.f;
        float centerY = (mMinY+mMaxY)/2.f + (size-origSizeY)/2.f;

        mRootNode = new QuadTreeNode(this, Root, size, osg::Vec2f(centerX, centerY), nullptr);
        mRootNode->initNeighbours();

        //Ogre::WorkQueue* wq = Ogre::Root::getSingleton().getWorkQueue();
        //mWorkQueueChannel = wq->getChannel("LargeTerrain");
        //wq->addRequestHandler(mWorkQueueChannel, this);
        //wq->addResponseHandler(mWorkQueueChannel, this);

        queueLayerLoad(mRootNode);

        rootNode->addChild(mCompositorRootSceneNode.get());
        rootNode->addChild(mRootSceneNode.get());
    }

    DefaultWorld::~DefaultWorld()
    {
        //Ogre::WorkQueue* wq = Ogre::Root::getSingleton().getWorkQueue();
        //wq->removeRequestHandler(mWorkQueueChannel, this);
        //wq->removeResponseHandler(mWorkQueueChannel, this);
        if(mCompositorRootSceneNode.valid())
        {
            while(mCompositorRootSceneNode->getNumParents())
                mCompositorRootSceneNode->getParent(0)->removeChild(mCompositorRootSceneNode.get());
            mCompositorRootSceneNode = nullptr;
        }
        if(mRootSceneNode.valid())
        {
            while(mRootSceneNode->getNumParents())
                mRootSceneNode->getParent(0)->removeChild(mRootSceneNode.get());
            mRootSceneNode = nullptr;
        }

        delete mRootNode;
    }

    void DefaultWorld::update(const osg::Vec3f &cameraPos)
    {
        // FIXME: Only remove children when a render has occured (or better,
        // make OSG auto-remove compositor cameras right after their children
        // have finished rendering)
        if(mCompositorRan)
        {
            mCompositorRan = false;
            mCompositorRootSceneNode->removeChildren(
                0, mCompositorRootSceneNode->getNumChildren()
            );
        }
        if(!mVisible) return;
        mRootNode->update(cameraPos, mStorage->getCellWorldSize());
        if(mUpdateIndexBuffers)
        {
            mUpdateIndexBuffers = false;
            mRootNode->updateIndexBuffers();
        }
    }

    osg::BoundingBoxf DefaultWorld::getWorldBoundingBox(const osg::Vec2f& center)
    {
        if(center.x() > mMaxX || center.x() < mMinX ||
           center.y() > mMaxY || center.y() < mMinY)
            return osg::BoundingBoxf();
        QuadTreeNode* node = findNode(center, mRootNode);
        return node->getWorldBoundingBox();
    }

    void DefaultWorld::rebuildCompositeMaps()
    {
        mRootNode->clearCompositeMaps();
        mRootNode->applyMaterials();
    }

    // FIXME
    void DefaultWorld::renderCompositeMap(osg::Texture2D *target, osg::Texture2D *normal, osg::Geode *geode)
    {
        const int size = 128;

        target->setTextureSize(size, size);
        target->setSourceFormat(GL_RGBA);
        target->setSourceType(GL_UNSIGNED_BYTE);
        target->setInternalFormat(GL_RGBA8);
        target->setUnRefImageDataAfterApply(true);

        normal->setTextureSize(size, size);
        normal->setSourceFormat(GL_RGBA);
        normal->setSourceType(GL_UNSIGNED_BYTE);
        normal->setInternalFormat(GL_RGBA8);
        normal->setUnRefImageDataAfterApply(true);

        osg::ref_ptr<osg::Camera> camera = new osg::Camera();
        camera->setClearMask(GL_NONE);

        camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        camera->setProjectionResizePolicy(osg::Camera::FIXED);
        camera->setProjectionMatrix(osg::Matrixd::identity());
        camera->setViewMatrix(osg::Matrixd::identity());
        camera->setViewport(0, 0, size, size);

        camera->setRenderOrder(osg::Camera::PRE_RENDER);

        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        camera->attach(osg::Camera::COLOR_BUFFER0, target, 0, 0, true);
        camera->attach(osg::Camera::COLOR_BUFFER1, normal, 0, 0, true);

        camera->setPostDrawCallback(new CompositorRanCallback(this));
        mCompositorRan = false;

        camera->addChild(geode);

        mCompositorRootSceneNode->addChild(camera);
    }


    void DefaultWorld::applyMaterials(bool shadows, bool splitShadows)
    {
        mShadows = shadows;
        mSplitShadows = splitShadows;
        mRootNode->applyMaterials();
    }

    void DefaultWorld::setVisible(bool visible)
    {
        if(visible)
            mRootSceneNode->addChild(mRootNode->getSceneNode());
        else
            mRootSceneNode->removeChild(mRootNode->getSceneNode());
        mVisible = visible;
    }

    bool DefaultWorld::getVisible()
    {
        return mVisible;
    }

    void DefaultWorld::getStatus(std::ostream &status) const
    {
        static std::map<size_t,size_t> chunks;

        size_t nodes = 0;
        chunks.clear();
        mRootNode->getInfo(chunks, nodes);

        size_t totalchunks = 0;
        if(!chunks.empty())
        {
            status<< "LOD:Chunks";
            for(const auto &chunklod : chunks)
            {
                status<< ", "<<chunklod.first<<":"<<chunklod.second;
                totalchunks += chunklod.second;
            }
            status<<std::endl;
        }
        status<< "Total chunks: "<<totalchunks <<std::endl;
        status<< "Loaded nodes: "<<nodes <<std::endl;
    }


    void DefaultWorld::syncLoad()
    {
        //while (mChunksLoading || mLayersLoading)
        //{
        //    OGRE_THREAD_SLEEP(0);
        //    Ogre::Root::getSingleton().getWorkQueue()->processResponses();
        //}
    }

#if 0 // FIXME
    Ogre::WorkQueue::Response* DefaultWorld::handleRequest(const Ogre::WorkQueue::Request *req, const Ogre::WorkQueue *srcQ)
    {
        if (req->getType() == REQ_ID_CHUNK)
        {
            const LoadRequestData data = Ogre::any_cast<LoadRequestData>(req->getData());
            LoadResponseData* responseData = new LoadResponseData();

            getStorage()->fillVertexBuffers(
                data.mNode->getNativeLodLevel(), data.mNode->getSize(), data.mNode->getCenter(), getAlign(),
                responseData->mPositions, responseData->mNormals, responseData->mColours
            );
            return OGRE_NEW Ogre::WorkQueue::Response(req, true, Ogre::Any(responseData));
        }
        else // REQ_ID_LAYER
        {
            const LayerRequestData data = Ogre::any_cast<LayerRequestData>(req->getData());
            LayerResponseData* responseData = new LayerResponseData();

            getStorage()->getBlendmaps(data.mNode->getSize(), data.mNode->getCenter(), data.mPack,
                                       responseData->mBlendmaps, responseData->mLayers);

            return OGRE_NEW Ogre::WorkQueue::Response(req, true, Ogre::Any(responseData));
        }
    }

    void DefaultWorld::handleResponse(const Ogre::WorkQueue::Response *res, const Ogre::WorkQueue *srcQ)
    {
        assert(res->succeeded() && "Response failure not handled");

        if (res->getRequest()->getType() == REQ_ID_CHUNK)
        {
            LoadResponseData* data = Ogre::any_cast<LoadResponseData*>(res->getData());
            const LoadRequestData requestData = Ogre::any_cast<LoadRequestData>(res->getRequest()->getData());

            requestData.mNode->load(*data);

            delete data;

            --mChunksLoading;
        }
        else // REQ_ID_LAYER
        {
            LayerResponseData* data = Ogre::any_cast<LayerResponseData*>(res->getData());
            const LayerRequestData requestData = Ogre::any_cast<LayerRequestData>(res->getRequest()->getData());

            requestData.mNode->loadLayers(data->mBlendmaps, data->mLayers);

            delete data;

            --mLayersLoading;
        }
    }
#endif

    void DefaultWorld::queueChunkLoad(QuadTreeNode *node)
    {
        LoadResponseData responseData;

        ++mChunksLoading;
        getStorage()->fillVertexBuffers(
            node->getNativeLodLevel(), node->getSize(), node->getCenter(), getAlign(),
            responseData.mPositions, responseData.mNormals, responseData.mColours
        );

        node->load(responseData);
        --mChunksLoading;
    }

    void DefaultWorld::queueLayerLoad(QuadTreeNode *node)
    {
        LayerResponseData responseData;

        ++mLayersLoading;
        getStorage()->getBlendmaps(node->getSize(), node->getCenter(), getShadersEnabled(),
                                   responseData.mBlendmaps, responseData.mLayers);

        node->loadLayers(responseData.mBlendmaps, responseData.mLayers);
        --mLayersLoading;
    }
}
