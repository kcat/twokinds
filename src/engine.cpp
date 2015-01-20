
#include "engine.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreRenderWindow.h>
#include <OgreCamera.h>
#include <OgreViewport.h>
#include <OgreArchiveManager.h>
#include <OgreMaterialManager.h>
#include <OgreEntity.h>
#include <OgreSceneNode.h>
#include <OgreTechnique.h>
#include <OgreConfigFile.h>

#include <OgreRTShaderSystem.h>

#if OGRE_VERSION >= ((2<<16) | (0<<8) | 0)
#include <Compositor/OgreCompositorManager2.h>
#endif
#include <OgreLogManager.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include "archives/physfs.hpp"
#include "gui/gui.hpp"
#include "terrain.hpp"


namespace
{

// Lifted from SampleBrowser.h
class ShaderGeneratorTechniqueResolverListener : public Ogre::MaterialManager::Listener
{
protected:
    Ogre::RTShader::ShaderGenerator &mShaderGenerator;

public:
    ShaderGeneratorTechniqueResolverListener()
      : mShaderGenerator(Ogre::RTShader::ShaderGenerator::getSingleton())
    {
        Ogre::MaterialManager::getSingleton().addListener(this);
    }
    virtual ~ShaderGeneratorTechniqueResolverListener()
    {
        Ogre::MaterialManager::getSingleton().removeListener(this);
    }

    /**
     * This is the hook point where shader based technique will be created. It
     * will be called whenever the material manager won't find appropriate
     * technique that satisfy the target scheme name. If the scheme name is out
     * target RT Shader System scheme name we will try to create shader
     * generated technique for it.
     */
    virtual Ogre::Technique *handleSchemeNotFound(unsigned short schemeIndex,
        const Ogre::String &schemeName, Ogre::Material *originalMaterial,
        unsigned short lodIndex, const Ogre::Renderable *rend)
    {
        Ogre::Technique *generatedTech = nullptr;

        // Case this is the default shader generator scheme.
        if(schemeName == Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME)
        {
            bool techniqueCreated;

            // Create shader generated technique for this material.
            techniqueCreated = mShaderGenerator.createShaderBasedTechnique(
                originalMaterial->getName(), Ogre::MaterialManager::DEFAULT_SCHEME_NAME,
                schemeName
            );

            // Case technique registration succeeded.
            if(techniqueCreated)
            {
                // Force creating the shaders for the generated technique.
                mShaderGenerator.validateMaterial(schemeName, originalMaterial->getName());

                // Grab the generated technique.
                Ogre::Material::TechniqueIterator itTech = originalMaterial->getTechniqueIterator();
                while(itTech.hasMoreElements())
                {
                    Ogre::Technique *curTech = itTech.getNext();
                    if(curTech->getSchemeName() == schemeName)
                    {
                        generatedTech = curTech;
                        break;
                    }
                }
            }
        }

        return generatedTech;
    }
};

} // namespace


namespace TK
{

Engine::Engine(void)
  : mSDLWindow(nullptr)
  , mRoot(nullptr)
  , mWindow(nullptr)
  , mSceneMgr(nullptr)
  , mCamera(nullptr)
  , mViewport(nullptr)
  , mGui(nullptr)
  , mDisplayDebugStats(false)
{
}

Engine::~Engine(void)
{
    World::get().deinitialize();

    delete mGui;
    mGui = nullptr;

    if(mRoot)
    {
        mRoot->removeFrameListener(this);

        if(mSceneMgr)
        {
            if(mCamera)
                mSceneMgr->destroyCamera(mCamera);
            mCamera = nullptr;

            mRoot->destroySceneManager(mSceneMgr);
            mSceneMgr = nullptr;
        }

        if(mWindow)
        {
            if(mViewport)
                mWindow->removeViewport(mViewport->getZOrder());
            mViewport = nullptr;

            mWindow->destroy();
            mWindow = nullptr;
        }

        delete mRoot;
        mRoot = nullptr;
    }

    if(mSDLWindow)
    {
        // If we don't do this, the desktop resolution is not restored on exit
        SDL_SetWindowFullscreen(mSDLWindow, 0);
        SDL_DestroyWindow(mSDLWindow);
        mSDLWindow = nullptr;
    }
    SDL_Quit();
}


bool Engine::parseOptions(int argc, char *argv[])
{
    for(int i = 1;i < argc;i++)
    {
        {
            std::stringstream str;
            str<< "Unrecognized option: "<<argv[i];
            throw std::runtime_error(str.str());
        }
    }

    return true;
}


Ogre::RenderWindow *Engine::createRenderWindow(SDL_Window *win)
{
    // Get the native window handle
    struct SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if(SDL_GetWindowWMInfo(win, &wmInfo) == SDL_FALSE)
        throw std::runtime_error("Couldn't get WM Info!");

    Ogre::NameValuePairList params;
    Ogre::String winHandle;
    switch(wmInfo.subsystem)
    {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
    case SDL_SYSWM_WINDOWS:
        winHandle = Ogre::StringConverter::toString((DWORD_PTR)wmInfo.info.win.window);
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_COCOA)
    case SDL_SYSWM_COCOA:
        // Required to make OGRE play nice with our window
        params.insert(std::make_pair("macAPI", "cocoa"));
        params.insert(std::make_pair("macAPICocoaUseNSView", "true"));

        winHandle = Ogre::StringConverter::toString(WindowContentViewHandle(wmInfo));
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
    case SDL_SYSWM_X11:
        winHandle = Ogre::StringConverter::toString((unsigned long)wmInfo.info.x11.window);
        break;
#endif
    default:
        throw std::runtime_error("Unsupported windowing subsystem: " +
                                 Ogre::StringConverter::toString(wmInfo.subsystem));
    }
    /* TODO: externalWindowHandle is deprecated according to the source code.
     * Figure out a way to get parentWindowHandle to work properly. On Linux/
     * X11 it causes an occasional GLXBadDrawable error. */
    params.insert(std::make_pair("externalWindowHandle", std::move(winHandle)));

    int width, height;
    Ogre::String title = SDL_GetWindowTitle(win);
    SDL_GetWindowSize(win, &width, &height);
    bool fs = (SDL_GetWindowFlags(win)&SDL_WINDOW_FULLSCREEN);
    return mRoot->createRenderWindow(title, width, height, fs, &params);
}


void Engine::handleWindowEvent(const SDL_WindowEvent &evt)
{
    switch(evt.event)
    {
        case SDL_WINDOWEVENT_SHOWN:
            mWindow->setVisible(true);
            break;

        case SDL_WINDOWEVENT_HIDDEN:
            mWindow->setVisible(false);
            break;

        case SDL_WINDOWEVENT_EXPOSED:
            // Needs redraw
            break;

        case SDL_WINDOWEVENT_ENTER:
        case SDL_WINDOWEVENT_LEAVE:
        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST:
            break;

        case SDL_WINDOWEVENT_CLOSE:
            // FIXME: Inject an SDL_QUIT event? Seems to happen anyway...
            break;

        default:
            std::cerr<< "Unhandled window event: "<<(int)evt.event <<std::endl;
    }
}

void Engine::handleMouseMotionEvent(const SDL_MouseMotionEvent &evt)
{
    mGui->mouseMoved(evt.x, evt.y, 0);

    /* HACK: mouse moves the camera around */
    if((SDL_GetMouseState(NULL, NULL)&SDL_BUTTON_LMASK))
    {
        static float x=0.0f, y=0.0f;
        /* Rotation (x motion rotates around y, y motion rotates around x) */
        x += evt.yrel * 0.1f;
        y += evt.xrel * 0.1f;

        Ogre::Matrix3 mat3;
        mat3.FromEulerAnglesZYX(Ogre::Degree(0.0f), Ogre::Degree(-y), Ogre::Degree(x));
        mCamera->setOrientation(mat3);
    }
}

void Engine::handleMouseButtonEvent(const SDL_MouseButtonEvent &evt)
{
    if(evt.state == SDL_PRESSED)
        mGui->mousePressed(evt.x, evt.y, evt.button);
    else if(evt.state == SDL_RELEASED)
        mGui->mouseReleased(evt.x, evt.y, evt.button);
}

void Engine::handleKeyboardEvent(const SDL_KeyboardEvent &evt)
{
    if(evt.state == SDL_PRESSED)
    {
        if(!evt.repeat)
            mGui->injectKeyPress(evt.keysym.sym, "");
        if(evt.keysym.scancode == SDLK_RETURN)
            mDisplayDebugStats = !mDisplayDebugStats;
    }
    else if(evt.state == SDL_RELEASED)
        mGui->injectKeyRelease(evt.keysym.sym);
}

void Engine::handleTextInputEvent(const SDL_TextInputEvent &evt)
{
    mGui->injectKeyPress(SDLK_UNKNOWN, evt.text);
}

bool Engine::pumpEvents()
{
    SDL_PumpEvents();

    SDL_Event evt;
    while(SDL_PollEvent(&evt))
    {
        switch(evt.type)
        {
        case SDL_WINDOWEVENT:
            handleWindowEvent(evt.window);
            break;

        case SDL_MOUSEMOTION:
            handleMouseMotionEvent(evt.motion);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            handleMouseButtonEvent(evt.button);
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            handleKeyboardEvent(evt.key);
            break;
        case SDL_TEXTINPUT:
            handleTextInputEvent(evt.text);
            break;

        case SDL_QUIT:
            return false;
        }
    }

    return true;
}


bool Engine::go(void)
{
    // Kindly ask SDL not to trash our OGL context
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

    // Init everything except audio (we will use OpenAL for that)
    if(SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO) != 0)
    {
        std::stringstream sstr;
        sstr<< "SDL_Init Error: "<<SDL_GetError();
        throw std::runtime_error(sstr.str());
    }

    // Construct Ogre::Root
    mRoot = new Ogre::Root("plugins.cfg", "ogre.cfg", "ogre.log");

    Ogre::ArchiveManager::getSingleton().addArchiveFactory(new PhysFSFactory);

    // Configure
    {
        // Show the configuration dialog and initialise the system
        if(!(mRoot->restoreConfig() || mRoot->showConfigDialog()))
            return false;

        int width = 1280;
        int height = 720;
        int xpos = SDL_WINDOWPOS_CENTERED;
        int ypos = SDL_WINDOWPOS_CENTERED;
        Uint32 flags = SDL_WINDOW_SHOWN;

        mSDLWindow = SDL_CreateWindow("Twokinds", xpos, ypos, width, height, flags);
        if(mSDLWindow == nullptr)
        {
            std::cerr<< "SDL_CreateWindow Error: "<<SDL_GetError() <<std::endl;
            return false;
        }

        mRoot->initialise(false);
        mWindow = createRenderWindow(mSDLWindow);
    }

    // Setup resources
    {
        auto &resGrpMgr = Ogre::ResourceGroupManager::getSingleton();
        resGrpMgr.createResourceGroup("Shaders");
        resGrpMgr.createResourceGroup("Materials");
        resGrpMgr.createResourceGroup("Textures");
        resGrpMgr.createResourceGroup("Meshes");
        resGrpMgr.createResourceGroup("Terrain");
        resGrpMgr.createResourceGroup("GUI");

        // Load resource paths from config file
        Ogre::ConfigFile cf;
        cf.load("resources.cfg");

        Ogre::StringVector paths = cf.getMultiSetting("source", "General");
        for(const auto &path : paths)
            PhysFSFactory::getSingleton().Mount(path.c_str());

        // Go through all sections & settings in the file
        Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();
        while(seci.hasMoreElements())
        {
            Ogre::String secName = seci.peekNextKey();
            Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
            for(const auto &i : *settings)
            {
                if(i.first != "path")
                    continue;
                resGrpMgr.addResourceLocation(
                    i.second, PhysFSFactory::getSingleton().getType(),
                    secName, true
                );
            }
        }
        resGrpMgr.addResourceLocation("", PhysFSFactory::getSingleton().getType());
    }

    /* Necessary for D3D11/GL3+. They have no fixed function pipeline. Must be
     * initialized before resources! */
    Ogre::RTShader::ShaderGenerator::initialize();

    ShaderGeneratorTechniqueResolverListener sgtrl;

    /* Initialize all resource groups. Ogre is sensitive to the shaders being
     * initialized after the materials, so ensure the shaders get initialized
     * first. */
    Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup("Shaders");
    Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup("Materials");
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

#if OGRE_VERSION >= ((2<<16) | (0<<8) | 0)
    const size_t numThreads = std::max(1u, Ogre::PlatformInformation::getNumLogicalCores());
    Ogre::InstancingTheadedCullingMethod threadedCullingMethod = Ogre::INSTANCING_CULLING_SINGLETHREAD;
    if(numThreads > 1)
        threadedCullingMethod = Ogre::INSTANCING_CULLING_THREADED;
    mSceneMgr = mRoot->createSceneManager(Ogre::ST_GENERIC, numThreads, threadedCullingMethod);
#else
    mSceneMgr = mRoot->createSceneManager(Ogre::ST_GENERIC);
#endif

    {
        auto &shaderGenerator = Ogre::RTShader::ShaderGenerator::getSingleton();
        shaderGenerator.addSceneManager(mSceneMgr);

        // Add a specialized sub-render (per-pixel lighting) state to the default scheme render state
        Ogre::RTShader::RenderState *rstate = shaderGenerator.createOrRetrieveRenderState(
            Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME
        ).first;
        rstate->reset();

        //shaderGenerator.addSubRenderStateFactory(new Ogre::RTShader::PerPixelLightingFactory);
        rstate->addTemplateSubRenderState(shaderGenerator.createSubRenderState(Ogre::RTShader::PerPixelLighting::Type));
    }

    mCamera = mSceneMgr->createCamera("PlayerCam");
#if OGRE_VERSION >= ((2<<16) | (0<<8) | 0)
    mRoot->getCompositorManager2()->addWorkspace(
        mSceneMgr, mWindow, mCamera, "MainWorkspace", true
    );
#else
    mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(mCamera);
    mViewport = mWindow->addViewport(mCamera);
    mViewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
#endif

    // Setup GUI subsystem
    mGui = new Gui(mWindow, mSceneMgr);

    // Alter the camera aspect ratio to match the window
    mCamera->setAspectRatio(Ogre::Real(mWindow->getWidth()) / Ogre::Real(mWindow->getHeight()));
    mCamera->setPosition(Ogre::Vector3(0.0f, 0.0f, 0.0f));
    mCamera->setNearClipDistance(1.0f);
    mCamera->setFarClipDistance(50000.0f);
    if(mRoot->getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_INFINITE_FAR_PLANE))
        mCamera->setFarClipDistance(0.0f);   // enable infinite far clip distance if we can

    /* Make a light so we can see things */
    mSceneMgr->setAmbientLight(Ogre::ColourValue(137.0f/255.0f, 140.0f/255.0f, 160.0f/255.0f));
    Ogre::SceneNode *lightNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
    Ogre::Light *l = mSceneMgr->createLight();
    lightNode->attachObject(l);
    l->setType(Ogre::Light::LT_DIRECTIONAL);
    l->setDirection(Ogre::Vector3(0.55f, -0.3f, 0.75f).normalisedCopy());
    l->setDiffuseColour(Ogre::ColourValue(1.0f, 252.0f/255.0f, 238.0f/255.0f));

    // Set up the terrain
    World::get().initialize(mCamera, l);

    // And away we go!
    mRoot->addFrameListener(this);
    mRoot->startRendering();

    return true;
}


bool Engine::frameStarted(const Ogre::FrameEvent &evt)
{
    World::get().update(mCamera->getPosition());
    return true;
}

bool Engine::frameRenderingQueued(const Ogre::FrameEvent &evt)
{
    if(!pumpEvents() || mWindow->isClosed())
        return false;

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);
    if(keystate[SDL_SCANCODE_ESCAPE])
        return false;

    float frametime = std::min<Ogre::Real>(1.0f/20.0f, evt.timeSinceLastFrame);
    Ogre::Vector3 movedir(0.0f);
    Ogre::Real speed = 60.0f * frametime;
    if(keystate[SDL_SCANCODE_LSHIFT])
        speed *= 2.0f;

    if(keystate[SDL_SCANCODE_W])
        movedir.z += -1.0f;
    if(keystate[SDL_SCANCODE_A])
        movedir.x += -1.0f;
    if(keystate[SDL_SCANCODE_S])
        movedir.z += +1.0f;
    if(keystate[SDL_SCANCODE_D])
        movedir.x += +1.0f;

    Ogre::Vector3 pos = mCamera->getPosition();
    Ogre::Quaternion ori = mCamera->getOrientation();
    pos += (ori*movedir)*speed;
    pos.y = std::max(pos.y, World::get().getHeightAt(pos)+60.0f);
    mCamera->setPosition(pos);

    if(!mDisplayDebugStats)
        mGui->updateStatus(std::string());
    else
    {
        std::stringstream status;
        status<< "Average FPS: "<<mWindow->getAverageFPS() <<std::endl;
        status<< "Camera pos: "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<pos <<std::endl;
        World::get().getStatus(status);
        mGui->updateStatus(status.str());
    }

    return true;
}

} // namespace TK
