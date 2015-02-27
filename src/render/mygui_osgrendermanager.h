#ifndef OGSRENDERMANAGER_H
#define OGSRENDERMANAGER_H

#include <MyGUI_RenderManager.h>

#include <osg/ref_ptr>

namespace osg
{
    class Group;
    class Camera;
    class RenderInfo;
}

namespace osgViewer
{
    class Viewer;
}


namespace TK
{

class OSGRenderManager : public MyGUI::RenderManager, public MyGUI::IRenderTarget
{
    osg::ref_ptr<osgViewer::Viewer> mViewer;
    osg::ref_ptr<osg::Group> mSceneRoot;

    MyGUI::IntSize mViewSize;
    bool mUpdate;
    MyGUI::VertexColourType mVertexFormat;
    MyGUI::RenderTargetInfo mInfo;

    typedef std::map<std::string, MyGUI::ITexture*> MapTexture;
    MapTexture mTextures;

    bool mIsInitialise;

    osg::ref_ptr<osg::Camera> mGuiRoot;

    // Only valid during drawFrame()!
    osg::RenderInfo *mRenderInfo;

    void destroyAllResources();

public:
    OSGRenderManager(osgViewer::Viewer *viewer, osg::Group *sceneroot);
    virtual ~OSGRenderManager();

    void initialise();

    static OSGRenderManager& getInstance() { return *getInstancePtr(); };
    static OSGRenderManager* getInstancePtr()
    { return static_cast<OSGRenderManager*>(MyGUI::RenderManager::getInstancePtr()); }

    /** @see RenderManager::getViewSize */
    virtual const MyGUI::IntSize& getViewSize() const { return mViewSize; }

    /** @see RenderManager::getVertexFormat */
    virtual MyGUI::VertexColourType getVertexFormat() { return mVertexFormat; }

    /** @see RenderManager::isFormatSupported */
    virtual bool isFormatSupported(MyGUI::PixelFormat format, MyGUI::TextureUsage usage);

    /** @see RenderManager::createVertexBuffer */
    virtual MyGUI::IVertexBuffer* createVertexBuffer();
    /** @see RenderManager::destroyVertexBuffer */
    virtual void destroyVertexBuffer(MyGUI::IVertexBuffer *buffer);

    /** @see RenderManager::createTexture */
    virtual MyGUI::ITexture* createTexture(const std::string &name);
    /** @see RenderManager::destroyTexture */
    virtual void destroyTexture(MyGUI::ITexture* _texture);
    /** @see RenderManager::getTexture */
    virtual MyGUI::ITexture* getTexture(const std::string &name);


    /** @see IRenderTarget::begin */
    virtual void begin();
    /** @see IRenderTarget::end */
    virtual void end();
    /** @see IRenderTarget::doRender */
    virtual void doRender(MyGUI::IVertexBuffer *buffer, MyGUI::ITexture *texture, size_t count);
    /** @see IRenderTarget::getInfo */
    virtual const MyGUI::RenderTargetInfo& getInfo() { return mInfo; }

/*internal:*/
    void drawFrame(osg::RenderInfo &renderInfo);
    void setViewSize(int width, int height);
};

} // namespace TK

#endif /* OGSRENDERMANAGER_H */
