#include "defaultworld.hpp"

#include <iostream>

#include <OgreAxisAlignedBox.h>
#include <OgreCamera.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreTextureManager.h>
#include <OgreRenderTexture.h>
#include <OgreSceneNode.h>
#include <OgreRoot.h>

#include "storage.hpp"
#include "quadtreenode.hpp"

namespace
{

    bool isPowerOfTwo(int x)
    {
        return ( (x > 0) && ((x & (x - 1)) == 0) );
    }

    int nextPowerOfTwo (int v)
    {
        if (isPowerOfTwo(v)) return v;
        int depth=0;
        while(v)
        {
            v >>= 1;
            depth++;
        }
        return 1 << depth;
    }

    Terrain::QuadTreeNode* findNode (const Ogre::Vector2& center, Terrain::QuadTreeNode* node)
    {
        if (center == node->getCenter())
            return node;

        if (center.x > node->getCenter().x && center.y > node->getCenter().y)
            return findNode(center, node->getChild(Terrain::NE));
        else if (center.x > node->getCenter().x && center.y < node->getCenter().y)
            return findNode(center, node->getChild(Terrain::SE));
        else if (center.x < node->getCenter().x && center.y > node->getCenter().y)
            return findNode(center, node->getChild(Terrain::NW));
        else //if (center.x < node->getCenter().x && center.y < node->getCenter().y)
            return findNode(center, node->getChild(Terrain::SW));
    }

}

namespace Terrain
{

    const Ogre::uint REQ_ID_CHUNK = 1;
    const Ogre::uint REQ_ID_LAYER = 2;

    DefaultWorld::DefaultWorld(Ogre::SceneManager* sceneMgr,
                     Storage* storage, int visibilityFlags, bool shaders, Alignment align, int maxBatchSize)
        : World(sceneMgr, storage, visibilityFlags, shaders, align)
        , mWorkQueueChannel(0)
        , mVisible(true)
        , mChunksLoading(0)
        , mLayersLoading(0)
        , mMinX(0)
        , mMaxX(0)
        , mMinY(0)
        , mMaxY(0)
        , mMaxBatchSize(maxBatchSize)
    {
#if TERRAIN_USE_SHADER == 0
        if (mShaders)
            std::cerr << "Compiled Terrain without shader support, disabling..." << std::endl;
        mShaders = false;
#endif

        mCompositeMapSceneMgr = Ogre::Root::getSingleton().createSceneManager(Ogre::ST_GENERIC);

        /// \todo make composite map size configurable
        Ogre::Camera* compositeMapCam = mCompositeMapSceneMgr->createCamera("a");
        mCompositeMapRenderTexture = Ogre::TextureManager::getSingleton().createManual(
                    "terrain/comp/rt", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D, 128, 128, 0, Ogre::PF_A8B8G8R8, Ogre::TU_RENDERTARGET);
        mCompositeMapRenderTarget = mCompositeMapRenderTexture->getBuffer()->getRenderTarget();
        mCompositeMapRenderTarget->setAutoUpdated(false);
        mCompositeMapRenderTarget->addViewport(compositeMapCam);

        storage->getBounds(mMinX, mMaxX, mMinY, mMaxY);

        int origSizeX = mMaxX-mMinX;
        int origSizeY = mMaxY-mMinY;

        // Dividing a quad tree only works well for powers of two, so round up to the nearest one
        int size = nextPowerOfTwo(std::max(origSizeX, origSizeY));

        // Adjust the center according to the new size
        float centerX = (mMinX+mMaxX)/2.f + (size-origSizeX)/2.f;
        float centerY = (mMinY+mMaxY)/2.f + (size-origSizeY)/2.f;

        mRootSceneNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();

        mRootNode = new QuadTreeNode(this, Root, size, Ogre::Vector2(centerX, centerY), NULL);
        mRootNode->initAabb();
        mRootNode->initNeighbours();

        Ogre::WorkQueue* wq = Ogre::Root::getSingleton().getWorkQueue();
        mWorkQueueChannel = wq->getChannel("LargeTerrain");
        wq->addRequestHandler(mWorkQueueChannel, this);
        wq->addResponseHandler(mWorkQueueChannel, this);

        queueLayerLoad(mRootNode);
    }

    DefaultWorld::~DefaultWorld()
    {
        Ogre::WorkQueue* wq = Ogre::Root::getSingleton().getWorkQueue();
        wq->removeRequestHandler(mWorkQueueChannel, this);
        wq->removeResponseHandler(mWorkQueueChannel, this);

        for(auto node : mFreeNodes)
            delete node;

        delete mRootNode;
    }

    void DefaultWorld::update(const Ogre::Vector3& cameraPos)
    {
        if (!mVisible)
            return;
        mRootNode->update(cameraPos);
        mRootNode->updateIndexBuffers();
    }

    Ogre::AxisAlignedBox DefaultWorld::getWorldBoundingBox (const Ogre::Vector2& center)
    {
        if (center.x > mMaxX
                 || center.x < mMinX
                || center.y > mMaxY
                || center.y < mMinY)
            return Ogre::AxisAlignedBox::BOX_NULL;
        QuadTreeNode* node = findNode(center, mRootNode);
        return node->getWorldBoundingBox();
    }

    void DefaultWorld::renderCompositeMap(Ogre::TexturePtr target)
    {
        mCompositeMapRenderTarget->update();
        target->getBuffer()->blit(mCompositeMapRenderTexture->getBuffer());
    }

    void DefaultWorld::clearCompositeMapSceneManager()
    {
        mCompositeMapSceneMgr->destroyAllManualObjects();
        mCompositeMapSceneMgr->clearScene();
    }

    void DefaultWorld::applyMaterials(bool shadows, bool splitShadows)
    {
        mShadows = shadows;
        mSplitShadows = splitShadows;
        mRootNode->applyMaterials();
    }

    void DefaultWorld::setVisible(bool visible)
    {
        if (visible && !mVisible)
            mSceneMgr->getRootSceneNode()->addChild(mRootSceneNode);
        else if (!visible && mVisible)
            mSceneMgr->getRootSceneNode()->removeChild(mRootSceneNode);

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
        status<< "Loaded nodes: "<<nodes<<" ("<<mFreeNodes.size()<<" free)" <<std::endl;
    }


    void DefaultWorld::freeNode(QuadTreeNode *node)
    {
        mFreeNodes.push_back(node);
    }

    QuadTreeNode *DefaultWorld::createNode(ChildDirection dir, int size, const Ogre::Vector2& center, QuadTreeNode* parent)
    {
        if(mFreeNodes.empty())
            return new QuadTreeNode(this, dir, size, center, parent);
        QuadTreeNode *node = mFreeNodes.back();
        node->reset(dir, size, center, parent);
        mFreeNodes.pop_back();
        return node;
    }

    void DefaultWorld::syncLoad()
    {
        while (mChunksLoading || mLayersLoading)
        {
            OGRE_THREAD_SLEEP(0);
            Ogre::Root::getSingleton().getWorkQueue()->processResponses();
        }
    }

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

    void DefaultWorld::queueChunkLoad(QuadTreeNode *node)
    {
        LoadRequestData data;
        data.mNode = node;

        Ogre::Root::getSingleton().getWorkQueue()->addRequest(mWorkQueueChannel, REQ_ID_CHUNK, Ogre::Any(data));
        ++mChunksLoading;
    }

    void DefaultWorld::queueLayerLoad(QuadTreeNode *node)
    {
        LayerRequestData data;
        data.mNode = node;
        data.mPack = getShadersEnabled();

        Ogre::Root::getSingleton().getWorkQueue()->addRequest(mWorkQueueChannel, REQ_ID_LAYER, Ogre::Any(data));
        ++mLayersLoading;
    }
}
