
#include "mygui_osgrendermanager.h"

#include <MyGUI_VertexData.h>
#include <MyGUI_Gui.h>
#include <MyGUI_Timer.h>
#include <MyGUI_ITexture.h>
#include <MyGUI_RenderManager.h>

#include <osgViewer/Viewer>
#include <osgDB/ReadFile>
#include <osg/Texture2D>
#include <osg/PolygonMode>
#include <osg/BlendFunc>
#include <osg/Depth>

#include "mygui_osgvertexbuffer.h"
#include "mygui_osgtexture.h"
#include "mygui_osgdiagnostic.h"


namespace
{

// Proxy to forward a Drawable's draw call to OSGRenderManager::drawFrame
class Renderable : public osg::Drawable {
    TK::OSGRenderManager *mParent;

    virtual void drawImplementation(osg::RenderInfo &renderInfo) const
    { mParent->drawFrame(renderInfo); }

public:
    Renderable(TK::OSGRenderManager *parent=nullptr) : mParent(parent) { }
    Renderable(const Renderable &rhs, const osg::CopyOp &copyop=osg::CopyOp::SHALLOW_COPY)
        : osg::Drawable(rhs, copyop)
        , mParent(rhs.mParent)
    { }

    META_Object(osg, Renderable)
};

// Proxy to forward an OSG resize event to OSGRenderManager::setViewSize
class ResizeHandler : public osgGA::GUIEventHandler {
    TK::OSGRenderManager *mParent;

    virtual bool handle(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
    {
        if(ea.getEventType() == osgGA::GUIEventAdapter::RESIZE)
        {
            int width = ea.getWindowWidth();
            int height = ea.getWindowHeight();
            mParent->setViewSize(width, height);
        }
        return false;
    }

public:
    ResizeHandler(TK::OSGRenderManager *parent=nullptr) : mParent(parent) { }
    ResizeHandler(const ResizeHandler &rhs, const osg::CopyOp &copyop=osg::CopyOp::SHALLOW_COPY)
        : osgGA::GUIEventHandler(rhs, copyop)
        , mParent(rhs.mParent)
    { }

    META_Object(osgGA, ResizeHandler)
};

osg::StateSet *setShaderProgram(osg::Node *node, std::string vert, std::string frag)
{
    osg::ref_ptr<osg::Program> program = new osg::Program();
    program->addShader(osgDB::readShaderFile(osg::Shader::VERTEX, vert));
    program->addShader(osgDB::readShaderFile(osg::Shader::FRAGMENT, frag));

    osg::StateSet *ss = node->getOrCreateStateSet();
    ss->setAttributeAndModes(program.get(),
        osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE
    );
    return ss;
}

} // namespace

namespace TK
{

OSGRenderManager::OSGRenderManager(osgViewer::Viewer *viewer, osg::Group *sceneroot)
  : mViewer(viewer)
  , mSceneRoot(sceneroot)
  , mUpdate(false)
  , mIsInitialise(false)
{
}

OSGRenderManager::~OSGRenderManager()
{
    MYGUI_PLATFORM_LOG(Info, "* Shutdown: "<<getClassTypeName());

    if(mGuiRoot.valid())
        mSceneRoot->removeChild(mGuiRoot.get());
    mGuiRoot = nullptr;
    mSceneRoot = nullptr;
    mViewer = nullptr;

    destroyAllResources();

    MYGUI_PLATFORM_LOG(Info, getClassTypeName()<<" successfully shutdown");
    mIsInitialise = false;
}


void OSGRenderManager::initialise()
{
    MYGUI_PLATFORM_ASSERT(!mIsInitialise, getClassTypeName()<<" initialised twice");
    MYGUI_PLATFORM_LOG(Info, "* Initialise: "<<getClassTypeName());

    mVertexFormat = MyGUI::VertexColourType::ColourABGR;

    mUpdate = false;

    osg::ref_ptr<osg::Drawable> drawable = new Renderable(this);
    drawable->setSupportsDisplayList(false);
    drawable->setUseVertexBufferObjects(true);
    drawable->setDataVariance(osg::Object::DYNAMIC);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(drawable.get());

    osg::ref_ptr<osg::Camera> camera = new osg::Camera();
    camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    camera->setProjectionResizePolicy(osg::Camera::FIXED);
    camera->setProjectionMatrix(osg::Matrix::identity());
    camera->setViewMatrix(osg::Matrix::identity());
    camera->setRenderOrder(osg::Camera::POST_RENDER);
    camera->setClearMask(GL_NONE);
    osg::StateSet *state = setShaderProgram(camera.get(), "shaders/quad_2d.vert", "shaders/quad_2d.frag");
    state->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    state->setAttributeAndModes(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL));
    state->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    state->setAttribute(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false));
    state->addUniform(new osg::Uniform("TexImage", 0));
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state->setRenderBinDetails(11, "RenderBin");
    camera->addChild(geode.get());

    mGuiRoot = camera;
    mSceneRoot->addChild(mGuiRoot.get());
    mViewer->addEventHandler(new ResizeHandler(this));

    osg::ref_ptr<osg::Viewport> vp = mViewer->getCamera()->getViewport();
    setViewSize(vp->width(), vp->height());

    MYGUI_PLATFORM_LOG(Info, getClassTypeName()<<" successfully initialized");
    mIsInitialise = true;
}

MyGUI::IVertexBuffer* OSGRenderManager::createVertexBuffer()
{
    return new OSGVertexBuffer();
}

void OSGRenderManager::destroyVertexBuffer(MyGUI::IVertexBuffer *buffer)
{
    delete buffer;
}


void OSGRenderManager::begin()
{
    osg::State *state = mRenderInfo->getState();
    state->disableAllVertexArrays();
}

void OSGRenderManager::doRender(MyGUI::IVertexBuffer *buffer, MyGUI::ITexture *texture, size_t count)
{
    osg::State *state = mRenderInfo->getState();
    osg::VertexBufferObject *vbo = static_cast<OSGVertexBuffer*>(buffer)->getBuffer();
    MYGUI_PLATFORM_ASSERT(vbo, "Vertex buffer is not created");

    if(texture)
    {
        osg::Texture2D *tex = static_cast<OSGTexture*>(texture)->getTexture();
        MYGUI_PLATFORM_ASSERT(tex, "Texture is not created");
        state->applyTextureAttribute(0, tex);
    }

    state->setVertexPointer(vbo->getArray(0));
    state->setColorPointer(vbo->getArray(1));
    state->setTexCoordPointer(0, vbo->getArray(2));

    glDrawArrays(GL_TRIANGLES, 0, count);
}

void OSGRenderManager::end()
{
    osg::State *state = mRenderInfo->getState();
    state->disableTexCoordPointer(0);
    state->disableColorPointer();
    state->disableVertexPointer();
    state->unbindVertexBufferObject();
}

void OSGRenderManager::drawFrame(osg::RenderInfo &renderInfo)
{
    MyGUI::Gui *gui = MyGUI::Gui::getInstancePtr();
    if(gui == nullptr) return;

    mRenderInfo = &renderInfo;

    static MyGUI::Timer timer;
    static unsigned long last_time = timer.getMilliseconds();
    unsigned long now_time = timer.getMilliseconds();
    unsigned long time = now_time - last_time;

    onFrameEvent((float)((double)(time) / (double)1000));

    last_time = now_time;

    begin();
    onRenderToTarget(this, mUpdate);
    end();

    mUpdate = false;
}

void OSGRenderManager::setViewSize(int width, int height)
{
    if(width < 1) width = 1;
    if(height < 1) height = 1;

    mGuiRoot->setViewport(0, 0, width, height);
    mViewSize.set(width, height);

    mInfo.maximumDepth = 1;
    mInfo.hOffset = 0;
    mInfo.vOffset = 0;
    mInfo.aspectCoef = float(mViewSize.height) / float(mViewSize.width);
    mInfo.pixScaleX = 1.0f / float(mViewSize.width);
    mInfo.pixScaleY = 1.0f / float(mViewSize.height);

    onResizeView(mViewSize);
    mUpdate = true;
}


bool OSGRenderManager::isFormatSupported(MyGUI::PixelFormat /*format*/, MyGUI::TextureUsage /*usage*/)
{
    return true;
}

MyGUI::ITexture* OSGRenderManager::createTexture(const std::string &name)
{
    MapTexture::const_iterator item = mTextures.find(name);
    MYGUI_PLATFORM_ASSERT(item == mTextures.end(), "Texture '"<<name<<"' already exist");

    OSGTexture* texture = new OSGTexture(name);
    mTextures.insert(std::make_pair(name, texture));
    return texture;
}

void OSGRenderManager::destroyTexture(MyGUI::ITexture *texture)
{
    if(texture == nullptr)
        return;

    MapTexture::iterator item = mTextures.find(texture->getName());
    MYGUI_PLATFORM_ASSERT(item != mTextures.end(), "Texture '"<<texture->getName()<<"' not found");

    mTextures.erase(item);
    delete texture;
}

MyGUI::ITexture* OSGRenderManager::getTexture(const std::string &name)
{
    MapTexture::const_iterator item = mTextures.find(name);
    if(item == mTextures.end()) return nullptr;
    return item->second;
}

void OSGRenderManager::destroyAllResources()
{
    for(const auto &item : mTextures)
        delete item.second;
    mTextures.clear();
}

} // namespace TK
