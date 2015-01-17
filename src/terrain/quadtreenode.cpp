#include "quadtreenode.hpp"

#include <OgreSceneManager.h>
#include <OgreManualObject.h>
#include <OgreSceneNode.h>
#include <OgreMaterialManager.h>
#include <OgreTextureManager.h>
#include <OgreRoot.h>

#include "defaultworld.hpp"
#include "chunk.hpp"
#include "storage.hpp"
#include "buffercache.hpp"
#include "material.hpp"

using namespace Terrain;

namespace
{
    int Log2( int n )
    {
        assert(n > 0);
        int targetlevel = 0;
        while (n >>= 1) ++targetlevel;
        return targetlevel;
    }

    // Utility functions for neighbour finding algorithm
    ChildDirection reflect(ChildDirection dir, Direction dir2)
    {
        assert(dir != Root);

        const ChildDirection lookupTable[4][4] =
        {
            // NW  NE  SW  SE
            {  SW, SE, NW, NE }, // N
            {  NE, NW, SE, SW }, // E
            {  SW, SE, NW, NE }, // S
            {  NE, NW, SE, SW }  // W
        };
        return lookupTable[dir2][dir];
    }

    bool adjacent(ChildDirection dir, Direction dir2)
    {
        assert(dir != Root);
        const bool lookupTable[4][4] =
        {
            // NW    NE    SW     SE
            {  true, true, false, false }, // N
            {  false, true, false, true }, // E
            {  false, false, true, true }, // S
            {  true, false, true, false }  // W
        };
        return lookupTable[dir2][dir];
    }

    // Algorithm described by Hanan Samet - 'Neighbour Finding in Quadtrees'
    // http://www.cs.umd.edu/~hjs/pubs/SametPRIP81.pdf
    QuadTreeNode* searchNeighbourRecursive (QuadTreeNode* currentNode, Direction dir)
    {
        if (!currentNode->getParent())
            return NULL; // Arrived at root node, the root node does not have neighbours

        QuadTreeNode* nextNode;
        if (adjacent(currentNode->getDirection(), dir))
        {
            nextNode = searchNeighbourRecursive(currentNode->getParent(), dir);
            if (nextNode && nextNode->hasChildren())
                return nextNode->getChild(reflect(currentNode->getDirection(), dir));
            return nextNode;
        }

        nextNode = currentNode->getParent();
        return nextNode->getChild(reflect(currentNode->getDirection(), dir));
    }

    // Create a 2D quad
    void makeQuad(Ogre::SceneManager* sceneMgr, float left, float top, float right, float bottom, Ogre::MaterialPtr material)
    {
        Ogre::ManualObject* manual = sceneMgr->createManualObject();

        // Use identity view/projection matrices to get a 2d quad
        manual->setUseIdentityProjection(true);
        manual->setUseIdentityView(true);

        manual->begin(material->getName());

        float normLeft = left*2-1;
        float normTop = top*2-1;
        float normRight = right*2-1;
        float normBottom = bottom*2-1;

        manual->position(normLeft, normTop, 0.0);
        manual->textureCoord(0, 1);
        manual->position(normRight, normTop, 0.0);
        manual->textureCoord(1, 1);
        manual->position(normRight, normBottom, 0.0);
        manual->textureCoord(1, 0);
        manual->position(normLeft, normBottom, 0.0);
        manual->textureCoord(0, 0);

        manual->quad(0,1,2,3);

        manual->end();

        Ogre::AxisAlignedBox aabInf;
        aabInf.setInfinite();
        manual->setBoundingBox(aabInf);

        sceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(manual);
    }
}

QuadTreeNode::QuadTreeNode(DefaultWorld* terrain, ChildDirection dir, int size, const Ogre::Vector2 &center, QuadTreeNode* parent)
    : mMaterialGenerator(NULL)
    , mChunkLoadState(LS_Unloaded)
    , mLayerLoadState(LS_Unloaded)
    , mIsDummy(false)
    , mSize(size)
    , mLodLevel(Log2(mSize))
    , mBounds(Ogre::AxisAlignedBox::BOX_NULL)
    , mWorldBounds(Ogre::AxisAlignedBox::BOX_NULL)
    , mDirection(dir)
    , mCenter(center)
    , mSceneNode(NULL)
    , mParent(parent)
    , mChunk(NULL)
    , mTerrain(terrain)
{
    for (int i=0; i<4; ++i)
        mChildren[i] = NULL;
    for (int i=0; i<4; ++i)
        mNeighbours[i] = NULL;

    if (mDirection == Root)
        mSceneNode = mTerrain->getRootSceneNode();
    else
        mSceneNode = mTerrain->getSceneManager()->createSceneNode();
    Ogre::Vector2 pos (0,0);
    if (mParent)
        pos = mParent->getCenter();
    pos = mCenter - pos;
    float cellWorldSize = mTerrain->getStorage()->getCellWorldSize();

    Ogre::Vector3 sceneNodePos (pos.x*cellWorldSize, pos.y*cellWorldSize, 0);
    mTerrain->convertPosition(sceneNodePos);

    mSceneNode->setPosition(sceneNodePos);

    mMaterialGenerator = new MaterialGenerator();
    mMaterialGenerator->enableShaders(mTerrain->getShadersEnabled());
}

void QuadTreeNode::reset(ChildDirection dir, int size, const Ogre::Vector2& center, QuadTreeNode* parent)
{
    mSize = size;
    mLodLevel = Log2(mSize);
    mDirection = dir;
    mCenter = center;
    mParent = parent;

    mChunkLoadState = LS_Unloaded;
    mLayerLoadState = LS_Unloaded;
    mIsDummy = false;
    mBounds.setNull();
    mWorldBounds.setNull();
    for (int i=0; i<4; ++i)
        mChildren[i] = NULL;
    for (int i=0; i<4; ++i)
        mNeighbours[i] = NULL;

    Ogre::Vector2 pos (0,0);
    if (mParent)
        pos = mParent->getCenter();
    pos = mCenter - pos;
    float cellWorldSize = mTerrain->getStorage()->getCellWorldSize();

    Ogre::Vector3 sceneNodePos (pos.x*cellWorldSize, pos.y*cellWorldSize, 0);
    mTerrain->convertPosition(sceneNodePos);

    mSceneNode->setPosition(sceneNodePos);
}

void QuadTreeNode::free()
{
    // Need to make sure all pending loads on this node are finished before we can unload
    syncLoad();

    unload();
    unloadLayers();

    mSceneNode->removeAllChildren();
    Ogre::SceneNode *parent = mSceneNode->getParentSceneNode();
    if(parent) parent->removeChild(mSceneNode);
    if(hasChildren())
    {
        for (int i=0; i<4; ++i)
        {
            mChildren[i]->free();
            mChildren[i] = 0;
        }
    }
    mTerrain->freeNode(this);
}

QuadTreeNode::~QuadTreeNode()
{
    for (int i=0; i<4; ++i)
        delete mChildren[i];

    unload();
    unloadLayers();

    delete mMaterialGenerator;

    mTerrain->getSceneManager()->destroySceneNode(mSceneNode);
}


void QuadTreeNode::createChild(ChildDirection id, int size, const Ogre::Vector2 &center)
{
    mChildren[id] = mTerrain->createNode(id, size, center, this);
}

void QuadTreeNode::initNeighbours(bool childrenOnly)
{
    if (!childrenOnly)
    {
        for (int i=0; i<4; ++i)
            mNeighbours[i] = searchNeighbourRecursive(this, (Direction)i);
    }
    if (hasChildren())
    {
        for (int i=0; i<4; ++i)
            mChildren[i]->initNeighbours();
    }
}

void QuadTreeNode::updateNeighbour(Direction dir, ChildDirection id1, ChildDirection id2, QuadTreeNode *node)
{
    mNeighbours[dir] = node;
    if(hasChildren())
    {
        mChildren[id1]->updateNeighbour(dir, id1, id2, node);
        mChildren[id2]->updateNeighbour(dir, id1, id2, node);
    }
}

void QuadTreeNode::initAabb()
{
    float cellWorldSize = mTerrain->getStorage()->getCellWorldSize();
    if (hasChildren())
    {
        for (int i=0; i<4; ++i)
        {
            mChildren[i]->initAabb();
            mBounds.merge(mChildren[i]->getBoundingBox());
        }
        float minH=0.0f, maxH=0.0f;
        switch (mTerrain->getAlign())
        {
            case Terrain::Align_XY:
                minH = mBounds.getMinimum().z;
                maxH = mBounds.getMaximum().z;
                break;
            case Terrain::Align_XZ:
                minH = mBounds.getMinimum().y;
                maxH = mBounds.getMaximum().y;
                break;
            case Terrain::Align_YZ:
                minH = mBounds.getMinimum().x;
                maxH = mBounds.getMaximum().x;
                break;
        }
        float halfSize = mSize/2.f;
        Ogre::Vector3 min(-halfSize*cellWorldSize, -halfSize*cellWorldSize, minH);
        Ogre::Vector3 max(Ogre::Vector3(halfSize*cellWorldSize, halfSize*cellWorldSize, maxH));
        mBounds = Ogre::AxisAlignedBox (min, max);
        mTerrain->convertBounds(mBounds);
    }
    else
    {
        float minZ, maxZ;
        if (mTerrain->getStorage()->getMinMaxHeights(mSize, mCenter, minZ, maxZ))
        {
            float halfSize = mSize/2.f;
            Ogre::AxisAlignedBox bounds(Ogre::Vector3(-halfSize*cellWorldSize, -halfSize*cellWorldSize, minZ),
                                        Ogre::Vector3( halfSize*cellWorldSize,  halfSize*cellWorldSize, maxZ));
            mTerrain->convertBounds(bounds);
            mBounds = bounds;
        }
    }
    Ogre::Vector3 offset(mCenter.x*cellWorldSize, mCenter.y*cellWorldSize, 0);
    mTerrain->convertPosition(offset);
    mWorldBounds = Ogre::AxisAlignedBox(mBounds.getMinimum() + offset,
                                        mBounds.getMaximum() + offset);
}

void QuadTreeNode::syncLoad()
{
    while(mChunkLoadState == LS_Loading || mLayerLoadState == LS_Loading)
    {
        OGRE_THREAD_SLEEP(0);
        Ogre::Root::getSingleton().getWorkQueue()->processResponses();
    }
}

void QuadTreeNode::buildQuadTree(const Ogre::Vector3 &cameraPos)
{
    if(mWorldBounds.isNull())
        initAabb();
    float dist = mWorldBounds.distance(cameraPos);

    // Simple LOD selection
    /// \todo use error metrics?
    size_t wantedLod = 0;
    float cellWorldSize = mTerrain->getStorage()->getCellWorldSize();

    if(dist > cellWorldSize*1.42) // < sqrt2 so the 3x3 grid around player is always highest lod
        wantedLod = Log2(dist/cellWorldSize)+1;

    bool isLeaf = mSize <= 1 || (mSize <= mTerrain->getMaxBatchSize() && mLodLevel <= wantedLod);
    if (isLeaf)
    {
        // We arrived at a leaf
        Terrain::Storage *storage = mTerrain->getStorage();
        float minZ,maxZ;
        if (storage->getMinMaxHeights(mSize, mCenter, minZ, maxZ))
        {
            float halfSize = mSize/2.f;
            Ogre::AxisAlignedBox bounds(Ogre::Vector3(-halfSize*cellWorldSize, -halfSize*cellWorldSize, minZ),
                                        Ogre::Vector3( halfSize*cellWorldSize,  halfSize*cellWorldSize, maxZ));
            mTerrain->convertBounds(bounds);
            mBounds = bounds;
            if(mLayerLoadState == LS_Unloaded)
            {
                mLayerLoadState = LS_Loading;
                mTerrain->queueLayerLoad(this);
            }
        }
        else
            markAsDummy(); // no data available for this node, skip it
        return;
    }

    float halfSize = mSize/2.f;
    if (mCenter.x - halfSize > mTerrain->getMaxX()
            || mCenter.x + halfSize < mTerrain->getMinX()
            || mCenter.y - halfSize > mTerrain->getMaxY()
            || mCenter.y + halfSize < mTerrain->getMinY() )
    {
        // Out of bounds of the actual terrain - this will happen because
        // we rounded the size up to the next power of two
        markAsDummy();
        return;
    }

    // Not a leaf, create its children
    int childSize = mSize>>1;
    createChild(SW, childSize, mCenter - halfSize/2.f);
    createChild(SE, childSize, mCenter + Ogre::Vector2(halfSize/2.f, -halfSize/2.f));
    createChild(NW, childSize, mCenter + Ogre::Vector2(-halfSize/2.f, halfSize/2.f));
    createChild(NE, childSize, mCenter + halfSize/2.f);
    mChildren[SW]->buildQuadTree(cameraPos);
    mChildren[SE]->buildQuadTree(cameraPos);
    mChildren[NW]->buildQuadTree(cameraPos);
    mChildren[NE]->buildQuadTree(cameraPos);

    for (int i=0; i<4; ++i)
    {
        if (!getChild((ChildDirection)i)->isDummy())
        {
            // Only valid if we have at least one non-dummy child
            if(mLayerLoadState == LS_Unloaded)
            {
                mLayerLoadState = LS_Loading;
                mTerrain->queueLayerLoad(this);
            }
            return;
        }
    }
    // if all children are dummy, we are also dummy and have no need for children
    markAsDummy();
    for (int i=0; i<4; ++i)
    {
        mChildren[i]->free();
        mChildren[i] = 0;
    }
}


bool QuadTreeNode::update(const Ogre::Vector3 &cameraPos)
{
    if (isDummy())
        return true;

    if (mBounds.isNull())
        return true;

    float dist = mWorldBounds.distance(cameraPos);

    // Make sure our scene node is attached
    if (!mSceneNode->isInSceneGraph())
    {
        mParent->getSceneNode()->addChild(mSceneNode);
    }

    // Simple LOD selection
    /// \todo use error metrics?
    size_t wantedLod = 0;
    float cellWorldSize = mTerrain->getStorage()->getCellWorldSize();

    if(dist > cellWorldSize*1.42) // < sqrt2 so the 3x3 grid around player is always highest lod
        wantedLod = Log2(dist/cellWorldSize)+1;

    bool wantToDisplay = mSize <= mTerrain->getMaxBatchSize() && mLodLevel <= wantedLod;
    if (wantToDisplay)
    {
        // Wanted LOD is small enough to render this node in one chunk
        if (mChunkLoadState == LS_Unloaded)
        {
            mChunkLoadState = LS_Loading;
            mTerrain->queueChunkLoad(this);
            return false;
        }

        if (mChunkLoadState == LS_Loaded)
        {
            // Additional (index buffer) LOD is currently disabled.
            // This is due to a problem with the LOD selection when a node splits.
            // After splitting, the distance is measured from the children's bounding boxes, which are possibly
            // further away than the original node's bounding box, possibly causing a child to switch to a *lower* LOD
            // than the original node.
            // In short, we'd sometimes get a switch to a lesser detail when actually moving closer.
            // This wouldn't be so bad, but unfortunately it also breaks LOD edge connections if a neighbour
            // node hasn't split yet, and has a higher LOD than our node's child:
            //  ----- ----- ------------
            // | LOD | LOD |            |
            // |  1  |  1  |            |
            // |-----|-----|   LOD 0    |
            // | LOD | LOD |            |
            // |  0  |  0  |            |
            //  ----- ----- ------------
            // To prevent this, nodes of the same size need to always select the same LOD, which is basically what we're
            // doing here.
            // But this "solution" does increase triangle overhead, so eventually we need to find a more clever way.
            //mChunk->setAdditionalLod(wantedLod - mLodLevel);

            if (!mChunk->getVisible() && hasChildren())
            {
                for (int i=0; i<4; ++i)
                {
                    mChildren[i]->free();
                    mChildren[i] = 0;
                }
                // Children went away. Make sure our neighbours' children know
                if(mNeighbours[North]) mNeighbours[North]->updateNeighbour(South, SE, SW, this);
                if(mNeighbours[East ]) mNeighbours[East ]->updateNeighbour(West,  SW, NW, this);
                if(mNeighbours[South]) mNeighbours[South]->updateNeighbour(North, NW, NE, this);
                if(mNeighbours[West ]) mNeighbours[West ]->updateNeighbour(East,  NE, SE, this);
            }
            mChunk->setVisible(true);

            return true;
        }
        return false; // LS_Loading
    }

    // We do not want to display this node - delegate to children if they are already loaded
    if(!hasChildren())
    {
        buildQuadTree(cameraPos);
        if(!hasChildren())
        {
            markAsDummy();
            return false;
        }
        initAabb();
        initNeighbours(true);
        // Inform our neighbours that their children may have new neighbours
        for(int i=0; i<4; ++i)
        {
            if(mNeighbours[i])
                mNeighbours[i]->initNeighbours(true);
        }
    }

    if (mChunk)
    {
        // Are children already loaded?
        bool childrenLoaded = true;
        for (int i=0; i<4; ++i)
            if (!mChildren[i]->update(cameraPos))
                childrenLoaded = false;

        if (!childrenLoaded)
        {
            mChunk->setVisible(true);
            // Make sure child scene nodes are detached until all children are loaded
            mSceneNode->removeAllChildren();
        }
        else
        {
            // Delegation went well, we can unload now
            unload();

            for (int i=0; i<4; ++i)
            {
                if (!mChildren[i]->getSceneNode()->isInSceneGraph())
                    mSceneNode->addChild(mChildren[i]->getSceneNode());
            }
        }
        return true;
    }

    bool success = true;
    for (int i=0; i<4; ++i)
        success = mChildren[i]->update(cameraPos) & success;
    return success;
}

void QuadTreeNode::load(const LoadResponseData &data)
{
    assert (!mChunk);

    mChunk = new Chunk(mTerrain->getBufferCache().getUVBuffer(), mBounds, data.mPositions, data.mNormals, data.mColours);
    mChunk->setVisibilityFlags(mTerrain->getVisibilityFlags());
    mChunk->setCastShadows(true);
    mSceneNode->attachObject(mChunk);

    mMaterialGenerator->enableShadows(mTerrain->getShadowsEnabled());
    mMaterialGenerator->enableSplitShadows(mTerrain->getSplitShadowsEnabled());

    loadMaterials();

    mChunk->setVisible(false);

    mChunkLoadState = LS_Loaded;
}

void QuadTreeNode::unload()
{
    if (mChunk)
    {
        mSceneNode->detachObject(mChunk);

        delete mChunk;
        mChunk = NULL;

        if (!mCompositeMap.isNull())
        {
            Ogre::TextureManager::getSingleton().remove(mCompositeMap->getName());
            mCompositeMap.setNull();
        }

        // Do *not* set this when we are still loading!
        mChunkLoadState = LS_Unloaded;
    }
}

void QuadTreeNode::updateIndexBuffers()
{
    if (hasChunk())
    {
        // Fetch a suitable index buffer (which may be shared)
        size_t ourLod = getActualLodLevel();

        unsigned int flags = 0;

        for (int i=0; i<4; ++i)
        {
            QuadTreeNode* neighbour = mNeighbours[i];

            // If the neighbour isn't currently rendering itself,
            // go up until we find one. NOTE: We don't need to go down,
            // because in that case neighbour's detail would be higher than
            // our detail and the neighbour would handle stitching by itself.
            // FIXME: If an immediate neighbour isn't rendering, none of its
            // parents should be either. Could just skip?
            while (neighbour && !neighbour->hasChunk())
                neighbour = neighbour->getParent();
            size_t lod = 0;
            if (neighbour)
                lod = neighbour->getActualLodLevel();
            // We only need to worry about neighbours less detailed than we are -
            // neighbours with more detail will do the stitching themselves
            if (lod > ourLod)
            {
                // Use 4 bits for each LOD delta
                assert (lod - ourLod < (1 << 4));
                lod = std::min<size_t>((1<<4)-1, lod - ourLod) + ourLod;
                flags |= static_cast<unsigned int>(lod - ourLod) << (4*i);
            }
        }
        flags |= 0 /*((int)mAdditionalLod)*/ << (4*4);

        mChunk->setIndexBuffer(mTerrain->getBufferCache().getIndexBuffer(flags));
    }
    else if (hasChildren())
    {
        for (int i=0; i<4; ++i)
            mChildren[i]->updateIndexBuffers();
    }
}

bool QuadTreeNode::hasChunk() const
{
    return mSceneNode->isInSceneGraph() && mChunk && mChunk->getVisible();
}

size_t QuadTreeNode::getActualLodLevel() const
{
    assert(hasChunk() && "Can't get actual LOD level if this node has no render chunk");
    return mLodLevel /* + mChunk->getAdditionalLod() */;
}

void QuadTreeNode::loadLayers(const std::vector<Ogre::PixelBox> &blendmaps, const std::vector<LayerInfo> &layerList)
{
    assert (!mMaterialGenerator->hasLayers());

    Ogre::TextureManager &texMgr = Ogre::TextureManager::getSingleton();
    std::vector<Ogre::TexturePtr> blendTextures;

    for (std::vector<Ogre::PixelBox>::const_iterator it = blendmaps.begin(); it != blendmaps.end(); ++it)
    {
        static int count=0;
        Ogre::TexturePtr map = texMgr.createManual("terrain/blend/" + Ogre::StringConverter::toString(count++),
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D, it->getWidth(), it->getHeight(), 0, it->format);

        Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(it->data, it->getWidth()*it->getHeight()*Ogre::PixelUtil::getNumElemBytes(it->format), true));
        map->loadRawData(stream, it->getWidth(), it->getHeight(), it->format);
        blendTextures.push_back(map);
    }

    mMaterialGenerator->setLayerList(layerList);
    mMaterialGenerator->setBlendmapList(blendTextures);

    loadMaterials();

    mLayerLoadState = LS_Loaded;
}

void QuadTreeNode::unloadLayers()
{
    if(mMaterialGenerator->hasLayers())
    {
        std::vector<Ogre::TexturePtr> oldlist = mMaterialGenerator->getBlendmapList();
        mMaterialGenerator->setBlendmapList(std::vector<Ogre::TexturePtr>());
        mMaterialGenerator->setLayerList(std::vector<LayerInfo>());

        Ogre::TextureManager &texMgr = Ogre::TextureManager::getSingleton();
        for(Ogre::TexturePtr &tex : oldlist)
            texMgr.remove(tex->getName());

        mLayerLoadState = LS_Unloaded;
    }
}

void QuadTreeNode::loadMaterials()
{
    if (mChunk && mMaterialGenerator->hasLayers())
    {
        if (mSize <= 1)
            mChunk->setMaterial(mMaterialGenerator->generate());
        else
        {
            ensureCompositeMap();
            mMaterialGenerator->setCompositeMap(mCompositeMap->getName());
            mChunk->setMaterial(mMaterialGenerator->generateForCompositeMap());
        }
    }
}

void QuadTreeNode::prepareForCompositeMap(Ogre::TRect<float> area)
{
    Ogre::SceneManager* sceneMgr = mTerrain->getCompositeMapSceneManager();

    if (mIsDummy)
    {
        // TODO - store this default material somewhere instead of creating one for each empty cell
        MaterialGenerator matGen;
        matGen.enableShaders(mTerrain->getShadersEnabled());
        std::vector<LayerInfo> layer;
        layer.push_back(mTerrain->getStorage()->getDefaultLayer());
        matGen.setLayerList(layer);
        makeQuad(sceneMgr, area.left, area.top, area.right, area.bottom, matGen.generateForCompositeMapRTT());
        return;
    }
    if (hasChildren())
    {
        // 0,0 -------- 1,0
        //  |     |      |
        //  |-----|------|
        //  |     |      |
        // 0,1 -------- 1,1

        float halfW = area.width()/2.f;
        float halfH = area.height()/2.f;
        mChildren[NW]->prepareForCompositeMap(Ogre::TRect<float>(area.left, area.top, area.right-halfW, area.bottom-halfH));
        mChildren[NE]->prepareForCompositeMap(Ogre::TRect<float>(area.left+halfW, area.top, area.right, area.bottom-halfH));
        mChildren[SW]->prepareForCompositeMap(Ogre::TRect<float>(area.left, area.top+halfH, area.right-halfW, area.bottom));
        mChildren[SE]->prepareForCompositeMap(Ogre::TRect<float>(area.left+halfW, area.top+halfH, area.right, area.bottom));
    }
    else
    {
        // TODO: when to destroy?
        Ogre::MaterialPtr material = mMaterialGenerator->generateForCompositeMapRTT();
        makeQuad(sceneMgr, area.left, area.top, area.right, area.bottom, material);
    }
}

void QuadTreeNode::ensureCompositeMap()
{
    if (!mCompositeMap.isNull())
        return;

    static int i=0;
    std::stringstream name;
    name << "terrain/comp" << i++;

    const int size = 128;
    mCompositeMap = Ogre::TextureManager::getSingleton().createManual(
                name.str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D, size, size, Ogre::MIP_DEFAULT, Ogre::PF_A8B8G8R8);

    // Create quads for each cell
    prepareForCompositeMap(Ogre::TRect<float>(0,0,1,1));

    mTerrain->renderCompositeMap(mCompositeMap);

    mTerrain->clearCompositeMapSceneManager();

}

void QuadTreeNode::applyMaterials()
{
    if (mChunk)
    {
        mMaterialGenerator->enableShadows(mTerrain->getShadowsEnabled());
        mMaterialGenerator->enableSplitShadows(mTerrain->getSplitShadowsEnabled());
        if (mSize <= 1)
            mChunk->setMaterial(mMaterialGenerator->generate());
        else
            mChunk->setMaterial(mMaterialGenerator->generateForCompositeMap());
    }
    if (hasChildren())
        for (int i=0; i<4; ++i)
            mChildren[i]->applyMaterials();
}
