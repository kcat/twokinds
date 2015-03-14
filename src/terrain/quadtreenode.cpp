#include "quadtreenode.hpp"

#include <cassert>

#include <osg/MatrixTransform>
#include <osg/Drawable>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/FrameBufferObject>
#include <osg/Texture2D>

#include "defaultworld.hpp"
#include "storage.hpp"
#include "buffercache.hpp"
#include "material.hpp"


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
    Terrain::ChildDirection reflect(Terrain::ChildDirection dir, Terrain::Direction dir2)
    {
        assert(dir != Root);

        const Terrain::ChildDirection lookupTable[4][4] =
        {
            // NW  NE  SW  SE
            {  Terrain::SW, Terrain::SE, Terrain::NW, Terrain::NE }, // N
            {  Terrain::NE, Terrain::NW, Terrain::SE, Terrain::SW }, // E
            {  Terrain::SW, Terrain::SE, Terrain::NW, Terrain::NE }, // S
            {  Terrain::NE, Terrain::NW, Terrain::SE, Terrain::SW }  // W
        };
        return lookupTable[dir2][dir];
    }

    bool adjacent(Terrain::ChildDirection dir, Terrain::Direction dir2)
    {
        assert(dir != Terrain::Root);
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
    Terrain::QuadTreeNode* searchNeighbourRecursive(Terrain::QuadTreeNode* currentNode, Terrain::Direction dir)
    {
        if(!currentNode->getParent())
            return NULL; // Arrived at root node, the root node does not have neighbours

        Terrain::QuadTreeNode* nextNode;
        if(adjacent(currentNode->getDirection(), dir))
        {
            nextNode = searchNeighbourRecursive(currentNode->getParent(), dir);
            if (nextNode && nextNode->hasChildren())
                return nextNode->getChild(reflect(currentNode->getDirection(), dir));
            return nextNode;
        }

        nextNode = currentNode->getParent();
        return nextNode->getChild(reflect(currentNode->getDirection(), dir));
    }

    float distanceBetween(const osg::BoundingBoxf &bbox, const osg::Vec3f &pos)
    {
        if(bbox.contains(pos))
            return 0.0f;

        osg::Vec3f vec(0.0f, 0.0f, 0.0f);
        if(pos.x() > bbox.xMax()) vec[0] = pos.x()-bbox.xMax();
        else if(pos.x() < bbox.xMin()) vec[0] = bbox.xMin()-pos.x();
        if(pos.y() > bbox.yMax()) vec[1] = pos.y()-bbox.yMax();
        else if(pos.y() < bbox.yMin()) vec[1] = bbox.yMin()-pos.y();
        if(pos.z() > bbox.zMax()) vec[2] = pos.z()-bbox.zMax();
        else if(pos.z() < bbox.zMin()) vec[2] = bbox.zMin()-pos.z();

        return vec.length();
    }

    // Create a 2D quad
    osg::Geometry *makeQuad(float left, float top, float right, float bottom, osg::StateSet *state)
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        geom->setUseDisplayList(false);

        float normLeft = left*2.0f - 1.0f;
        float normTop = top*2.0f - 1.0f;
        float normRight = right*2.0f - 1.0f;
        float normBottom = bottom*2.0f - 1.0f;

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        vertices->push_back(osg::Vec3(normLeft,  normBottom, 0.0f));
        vertices->push_back(osg::Vec3(normRight, normBottom, 0.0f));
        vertices->push_back(osg::Vec3(normRight, normTop,    0.0f));
        vertices->push_back(osg::Vec3(normLeft,  normTop,    0.0f));
        osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array();
        texcoords->push_back(osg::Vec2(0.0f, 1.0f));
        texcoords->push_back(osg::Vec2(1.0f, 1.0f));
        texcoords->push_back(osg::Vec2(1.0f, 0.0f));
        texcoords->push_back(osg::Vec2(0.0f, 0.0f));
        osg::ref_ptr<osg::Vec4ubArray> colors = new osg::Vec4ubArray();
        colors->push_back(osg::Vec4ub(255, 255, 255, 255));
        colors->push_back(osg::Vec4ub(255, 255, 255, 255));
        colors->push_back(osg::Vec4ub(255, 255, 255, 255));
        colors->push_back(osg::Vec4ub(255, 255, 255, 255));
        colors->setNormalize(true);
        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
        normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
        normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
        normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
        normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));

        geom->setVertexArray(vertices.get());
        geom->setNormalArray(normals.get(), osg::Array::BIND_PER_VERTEX);
        geom->setTexCoordArray(0, texcoords.get(), osg::Array::BIND_PER_VERTEX);
        geom->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
        geom->addPrimitiveSet(new osg::DrawArrays(
            osg::PrimitiveSet::QUADS, 0, vertices->size()
        ));
        geom->setStateSet(state);

        return geom.release();
    }
}

inline std::ostream& operator<<(std::ostream& out, const osg::Vec3f &vec)
{
    out<<"osg::Vec3f("<<vec.x()<<", "<<vec.y()<<", "<<vec.z()<<")";
    return out;
}
inline std::ostream& operator<<(std::ostream& out, const osg::BoundingBoxf &bbox)
{
    out<<"osg::BoundingBoxf("<<bbox._min<<", "<<bbox._max<<")";
    return out;
}

namespace Terrain
{

QuadTreeNode::QuadTreeNode(DefaultWorld* terrain, ChildDirection dir, int size, const osg::Vec2f &center, QuadTreeNode* parent)
    : mMaterialGenerator(nullptr)
    , mChunkLoadState(LS_Unloaded)
    , mLayerLoadState(LS_Unloaded)
    , mIsDummy(false)
    , mSize(size)
    , mLodLevel(Log2(mSize))
    , mDirection(dir)
    , mCenter(center)
    , mParent(parent)
    , mTerrain(terrain)
{
    for(int i=0; i<4; ++i)
        mChildren[i] = nullptr;
    for(int i=0; i<4; ++i)
        mNeighbours[i] = nullptr;

    mSceneNode = new osg::MatrixTransform();

    osg::Vec2f pos = mCenter;
    if(mParent)
        pos -= mParent->getCenter();

    float cellWorldSize = mTerrain->getStorage()->getCellWorldSize();
    osg::Vec3f sceneNodePos(pos*cellWorldSize, 0.0f);
    mTerrain->convertPosition(sceneNodePos);

    mSceneNode->setReferenceFrame(osg::Transform::RELATIVE_RF);
    mSceneNode->setMatrix(osg::Matrix::translate(sceneNodePos));

    mMaterialGenerator = new MaterialGenerator(mTerrain->getStorage());
    mMaterialGenerator->enableShaders(mTerrain->getShadersEnabled());

    (mParent ? mParent->getSceneNode() : mTerrain->getRootSceneNode())->addChild(mSceneNode.get());

    initAabb();
}

QuadTreeNode::~QuadTreeNode()
{
    for(int i = 0;i < 4;++i)
    {
        delete mChildren[i];
        mChildren[i] = nullptr;
    }

    unload();
    unloadLayers();

    delete mMaterialGenerator;
    mMaterialGenerator = nullptr;
}


void QuadTreeNode::getInfo(std::map<size_t,size_t> &chunks, size_t &nodes) const
{
    ++nodes;

    if(mGeode.valid())
        chunks[mLodLevel] += 1;
    if(hasChildren())
    {
        for(int i = 0;i < 4;++i)
            mChildren[i]->getInfo(chunks, nodes);
    }
}


void QuadTreeNode::createChild(ChildDirection id, int size, const osg::Vec2f &center)
{
    mChildren[id] = new QuadTreeNode(mTerrain, id, size, center, this);
}

void QuadTreeNode::initNeighbours(bool childrenOnly)
{
    if(!childrenOnly)
    {
        for(int i = 0;i < 4;++i)
            mNeighbours[i] = searchNeighbourRecursive(this, (Direction)i);
    }
    if(hasChildren())
    {
        for(int i = 0;i < 4;++i)
            mChildren[i]->initNeighbours();
    }
}

void QuadTreeNode::updateNeighbour(Direction dir, QuadTreeNode *node)
{
    mNeighbours[dir] = node;
    if(hasChildren())
    {
        const ChildDirection child_dirs[][2] = {
            { NW, NE }, // North
            { NE, SE }, // East
            { SE, SW }, // South
            { SW, NW }, // West
        };
        mChildren[child_dirs[dir][0]]->updateNeighbour(dir, node);
        mChildren[child_dirs[dir][1]]->updateNeighbour(dir, node);
    }
}

void QuadTreeNode::initAabb()
{
    float cellWorldSize = mTerrain->getStorage()->getCellWorldSize();

    float minZ, maxZ;
    if(mTerrain->getStorage()->getMinMaxHeights(mSize, mCenter, minZ, maxZ))
    {
        float halfSize = mSize/2.f;
        osg::BoundingBoxf bounds(-halfSize*cellWorldSize, -halfSize*cellWorldSize, minZ,
                                  halfSize*cellWorldSize,  halfSize*cellWorldSize, maxZ);
        mTerrain->convertBounds(bounds);
        mBounds = bounds;
    }

    osg::Vec3f offset(mCenter*cellWorldSize, 0.0f);
    mTerrain->convertPosition(offset);
    mWorldBounds = osg::BoundingBoxf(mBounds._min+offset, mBounds._max+offset);
}

void QuadTreeNode::syncLoad()
{
    //while(mChunkLoadState == LS_Loading || mLayerLoadState == LS_Loading)
    //{
    //    OGRE_THREAD_SLEEP(0);
    //    Ogre::Root::getSingleton().getWorkQueue()->processResponses();
    //}
}

void QuadTreeNode::buildQuadTree(const osg::Vec3f &cameraPos, float cellWorldSize)
{
    // Simple LOD selection
    /// \todo use error metrics?
    size_t wantedLod = 0;

    float dist = distanceBetween(mWorldBounds, cameraPos) - cellWorldSize*0.25f;
    if(dist > cellWorldSize)
        wantedLod = Log2(dist/cellWorldSize)+1;

    bool isLeaf = mSize <= 1 || (mSize <= mTerrain->getMaxBatchSize() && mLodLevel <= wantedLod);
    if(isLeaf)
    {
        // We arrived at a leaf
        Terrain::Storage *storage = mTerrain->getStorage();
        float minZ,maxZ;
        if(!storage->getMinMaxHeights(mSize, mCenter, minZ, maxZ))
            markAsDummy(); // no data available for this node, skip it
        else
        {
            float halfSize = mSize/2.f;
            osg::BoundingBoxf bounds(-halfSize*cellWorldSize, -halfSize*cellWorldSize, minZ,
                                      halfSize*cellWorldSize,  halfSize*cellWorldSize, maxZ);
            mTerrain->convertBounds(bounds);
            mBounds = bounds;
            if(mLayerLoadState == LS_Unloaded)
            {
                mLayerLoadState = LS_Loading;
                mTerrain->queueLayerLoad(this);
            }
        }
        return;
    }

    float halfSize = mSize/2.f;
    if(mCenter.x() - halfSize > mTerrain->getMaxX()
            || mCenter.x() + halfSize < mTerrain->getMinX()
            || mCenter.y() - halfSize > mTerrain->getMaxY()
            || mCenter.y() + halfSize < mTerrain->getMinY())
    {
        // Out of bounds of the actual terrain - this will happen because
        // we rounded the size up to the next power of two
        markAsDummy();
        return;
    }

    // Not a leaf, create its children
    int childSize = mSize>>1;
    createChild(SW, childSize, mCenter + osg::Vec2f(-halfSize/2.f, -halfSize/2.f));
    createChild(SE, childSize, mCenter + osg::Vec2f( halfSize/2.f, -halfSize/2.f));
    createChild(NW, childSize, mCenter + osg::Vec2f(-halfSize/2.f,  halfSize/2.f));
    createChild(NE, childSize, mCenter + osg::Vec2f( halfSize/2.f,  halfSize/2.f));
    mChildren[SW]->buildQuadTree(cameraPos, cellWorldSize);
    mChildren[SE]->buildQuadTree(cameraPos, cellWorldSize);
    mChildren[NW]->buildQuadTree(cameraPos, cellWorldSize);
    mChildren[NE]->buildQuadTree(cameraPos, cellWorldSize);

    for(int i = 0;i < 4;++i)
    {
        if(!getChild((ChildDirection)i)->isDummy())
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
    for(int i = 0;i < 4;++i)
    {
        delete mChildren[i];
        mChildren[i] = nullptr;
    }
}


bool QuadTreeNode::update(const osg::Vec3f &cameraPos, float cellWorldSize)
{
    if(isDummy())
        return true;
    if(!mBounds.valid())
        return true;

    // Simple LOD selection
    /// \todo use error metrics?
    size_t wantedLod = 0;

    float dist = distanceBetween(mWorldBounds, cameraPos) - cellWorldSize*0.25f;
    if(dist > cellWorldSize)
        wantedLod = Log2(dist/cellWorldSize)+1;

    bool wantToDisplay = mSize <= mTerrain->getMaxBatchSize() && mLodLevel <= wantedLod;
    if(wantToDisplay)
    {
        // Wanted LOD is small enough to render this node in one chunk
        if(mChunkLoadState == LS_Unloaded)
        {
            mChunkLoadState = LS_Loading;
            mTerrain->queueChunkLoad(this);
        }

        if(mChunkLoadState == LS_Loaded)
        {
            if(hasChildren())
            {
                for(int i = 0;i < 4;++i)
                {
                    delete mChildren[i];
                    mChildren[i] = nullptr;
                }
                // Children went away. Make sure our neighbours' children know
                if(mNeighbours[North]) mNeighbours[North]->updateNeighbour(South, this);
                if(mNeighbours[East ]) mNeighbours[East ]->updateNeighbour(West,  this);
                if(mNeighbours[South]) mNeighbours[South]->updateNeighbour(North, this);
                if(mNeighbours[West ]) mNeighbours[West ]->updateNeighbour(East,  this);

                mSceneNode->removeChildren(0, mSceneNode->getNumChildren());
                mSceneNode->addChild(mGeode.get());
            }

            return true;
        }
        return false; // LS_Loading
    }

    // We do not want to display this node - delegate to children if they are already loaded
    if(!hasChildren())
    {
        buildQuadTree(cameraPos, cellWorldSize);
        if(!hasChildren())
        {
            markAsDummy();
            return false;
        }
        initNeighbours(true);
        // Inform our neighbours that their children may have new neighbours
        for(int i=0; i<4; ++i)
        {
            if(mNeighbours[i])
                mNeighbours[i]->initNeighbours(true);
        }
    }

    if(mGeode.valid())
    {
        // Are children already loaded?
        bool childrenLoaded = true;
        for(int i = 0;i < 4;++i)
        {
            if(!mChildren[i]->update(cameraPos, cellWorldSize))
                childrenLoaded = false;
        }

        if(childrenLoaded)
        {
            // Delegation went well, we can unload now
            unload();
        }
        else
        {
            //mChunk->setVisible(true);
            // Make sure child scene nodes are detached until all children are loaded
            //mSceneNode->removeAllChildren();
        }
        return true;
    }

    bool success = true;
    for(int i = 0;i < 4;++i)
        success = mChildren[i]->update(cameraPos, cellWorldSize) && success;
    return success;
}

void QuadTreeNode::load(const LoadResponseData &data)
{
    assert(!mGeode.valid());

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setVertexArray(new osg::Vec3Array(data.mPositions.size(), data.mPositions.data()));
    geom->setNormalArray(new osg::Vec3Array(data.mNormals.size(), data.mNormals.data()), osg::Array::BIND_PER_VERTEX);
    geom->setColorArray(new osg::Vec4ubArray(data.mColours.size(), data.mColours.data()), osg::Array::BIND_PER_VERTEX);
    geom->setTexCoordArray(0, mTerrain->getBufferCache().getUVBuffer(), osg::Array::BIND_PER_VERTEX);
    geom->getColorArray()->setNormalize(true);
    geom->addPrimitiveSet(getPrimitive());
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);

    mGeode = new osg::Geode();
    mGeode->addDrawable(geom.get());
    mSceneNode->addChild(mGeode.get());

    mMaterialGenerator->enableShadows(mTerrain->getShadowsEnabled());
    mMaterialGenerator->enableSplitShadows(mTerrain->getSplitShadowsEnabled());

    loadMaterials();

    mChunkLoadState = LS_Loaded;
    mTerrain->setUpdateIndexBuffers();
}

void QuadTreeNode::unload()
{
    if(mGeode.valid())
    {
        mSceneNode->removeChild(mGeode.get());
        mGeode = nullptr;

        mCompositeMap = nullptr;
        mNormalMap = nullptr;

        // Do *not* set this when we are still loading!
        mChunkLoadState = LS_Unloaded;
    }
}

void QuadTreeNode::updateIndexBuffers()
{
    if(hasChunk())
    {
        osg::Geometry *geom = mGeode->getDrawable(0)->asGeometry();
        geom->removePrimitiveSet(0, geom->getNumPrimitiveSets());
        geom->addPrimitiveSet(getPrimitive());
    }
    else if(hasChildren())
    {
        for(int i = 0;i < 4;++i)
            mChildren[i]->updateIndexBuffers();
    }
}

osg::PrimitiveSet *QuadTreeNode::getPrimitive() const
{
    // Fetch a suitable Primitive for drawing (which may be shared)
    unsigned int flags = mLodLevel << (4*4);
    for(int i = 0;i < 4;++i)
    {
        QuadTreeNode* neighbour = mNeighbours[i];

        // If the neighbour isn't currently rendering itself,
        // go up until we find one. NOTE: We don't need to go down,
        // because in that case neighbour's detail would be higher than
        // our detail and the neighbour would handle stitching by itself.
        // FIXME: If an immediate neighbour isn't rendering, none of its
        // parents should be either. Could just skip?
        while(neighbour && !neighbour->hasChunk())
            neighbour = neighbour->getParent();
        size_t lod = 0;
        if(neighbour)
            lod = neighbour->getNativeLodLevel();
        // We only need to worry about neighbours less detailed than we are -
        // neighbours with more detail will do the stitching themselves
        if(lod > mLodLevel)
        {
            // Use 4 bits for each LOD delta
            assert(lod-mLodLevel < (1 << 4));
            lod = std::min<size_t>((1<<4)-1, lod - mLodLevel);
            flags |= static_cast<unsigned int>(lod) << (4*i);
        }
    }

    return mTerrain->getBufferCache().getPrimitive(flags);
}


bool QuadTreeNode::hasChunk() const
{
    return mGeode.valid();
}


void QuadTreeNode::loadLayers(const std::vector<osg::ref_ptr<osg::Image>> &blendmaps, const std::vector<LayerInfo> &layerList)
{
    assert(!mMaterialGenerator->hasLayers());

    mMaterialGenerator->setLayerList(layerList);
    mMaterialGenerator->setBlendmapList(blendmaps);

    loadMaterials();

    mLayerLoadState = LS_Loaded;
}

void QuadTreeNode::unloadLayers()
{
    mMaterialGenerator->setBlendmapList(std::vector<osg::ref_ptr<osg::Image>>());
    mMaterialGenerator->setLayerList(std::vector<LayerInfo>());

    mLayerLoadState = LS_Unloaded;
}

void QuadTreeNode::loadMaterials()
{
    if(mGeode.valid() && mMaterialGenerator->hasLayers())
    {
        if(mSize <= 1)
            mGeode->setStateSet(mMaterialGenerator->generate());
        else
        {
            ensureCompositeMap();
            mGeode->setStateSet(mMaterialGenerator->generateForCompositeMap(mCompositeMap.get(), mNormalMap.get()));
        }
    }
}

void QuadTreeNode::prepareForCompositeMap(osg::Geode *geode, osg::Vec4f area)
{
    if(mIsDummy)
    {
        // TODO - store this default material somewhere instead of creating one for each empty cell
        MaterialGenerator matGen(mTerrain->getStorage());
        matGen.enableShaders(mTerrain->getShadersEnabled());
        std::vector<LayerInfo> layer;
        layer.push_back(mTerrain->getStorage()->getDefaultLayer());
        matGen.setLayerList(layer);

        mMaterial = matGen.generateForCompositeMapRTT(mLodLevel);
        geode->addDrawable(makeQuad(area[0], area[1], area[2], area[3], mMaterial.get()));
        return;
    }
    if(hasChildren())
    {
        // 0,0 -------- 1,0
        //  |     |      |
        //  |-----|------|
        //  |     |      |
        // 0,1 -------- 1,1

        float halfW = (area[2]-area[0])/2.f;
        float halfH = (area[3]-area[1])/2.f;
        mChildren[NW]->prepareForCompositeMap(geode, osg::Vec4f(area[0]      , area[1]      , area[2]-halfW, area[3]-halfH));
        mChildren[NE]->prepareForCompositeMap(geode, osg::Vec4f(area[0]+halfW, area[1]      , area[2]      , area[3]-halfH));
        mChildren[SW]->prepareForCompositeMap(geode, osg::Vec4f(area[0]      , area[1]+halfH, area[2]-halfW, area[3]));
        mChildren[SE]->prepareForCompositeMap(geode, osg::Vec4f(area[0]+halfW, area[1]+halfH, area[2]      , area[3]));
    }
    else
    {
        mMaterial = mMaterialGenerator->generateForCompositeMapRTT(mLodLevel);
        geode->addDrawable(makeQuad(area[0], area[1], area[2], area[3], mMaterial.get()));
    }
}

void QuadTreeNode::ensureCompositeMap()
{
    if(mCompositeMap.valid())
        return;

    mCompositeMap = new osg::Texture2D();
    mCompositeMap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    mCompositeMap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

    mNormalMap = new osg::Texture2D();
    mNormalMap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    mNormalMap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

    // Create quads for each cell part of this node
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    prepareForCompositeMap(geode.get(), osg::Vec4f(0.0f, 0.0f, 1.0f, 1.0f));
    if(geode->getNumDrawables() > 0)
        mTerrain->renderCompositeMap(mCompositeMap.get(), mNormalMap.get(), geode.get());
}

void QuadTreeNode::applyMaterials()
{
    if(mGeode.valid())
    {
        mMaterialGenerator->enableShadows(mTerrain->getShadowsEnabled());
        mMaterialGenerator->enableSplitShadows(mTerrain->getSplitShadowsEnabled());
        if(mSize <= 1)
            mGeode->setStateSet(mMaterialGenerator->generate());
        else
        {
            ensureCompositeMap();
            mGeode->setStateSet(mMaterialGenerator->generateForCompositeMap(mCompositeMap.get(), mNormalMap.get()));
        }
    }
    if(hasChildren())
    {
        for(int i = 0;i < 4;++i)
            mChildren[i]->applyMaterials();
    }
}

void QuadTreeNode::clearCompositeMaps()
{
    mCompositeMap = nullptr;
    if(hasChildren())
    {
        for(int i = 0;i < 4;++i)
            mChildren[i]->clearCompositeMaps();
    }
}

} // namespace Terrain
