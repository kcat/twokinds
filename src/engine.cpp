
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
#include "input/input.hpp"
#include "gui/gui.hpp"
#include "terrain.hpp"
#include "delegates.hpp"
#include "sparsearray.hpp"
#include "cvars.hpp"
#include "log.hpp"


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

CVAR(CVarInt, vid_width, 1280);
CVAR(CVarInt, vid_height, 720);
CVAR(CVarBool, vid_fullscreen, false);
CVAR(CVarBool, vid_showfps, false);

CVAR(CVarString, r_rendersystem, "");

Engine::Engine(void)
  : mSDLWindow(nullptr)
  , mRoot(nullptr)
  , mWindow(nullptr)
  , mSceneMgr(nullptr)
  , mCamera(nullptr)
  , mViewport(nullptr)
  , mInput(nullptr)
  , mGui(nullptr)
  , mDisplayDebugStats(false)
  , mCommandFuncs{
      { "savecfg", &Engine::saveCfgCmd },
      { "tbb", &Engine::toggleBoundingBoxCmd },
      { "tdd", &Engine::toggleDebugDisplayCmd },
      { "qqq", &Engine::quitCmd },
    }
{
    Ogre::LogManager *logMgr = OGRE_NEW Ogre::LogManager();
    logMgr->createLog("twokinds.log", true);

    new Log(Log::Level_Normal, logMgr->getDefaultLog());
    Log::get().message("--- Starting log ---");
}

Engine::~Engine(void)
{
    World::get().deinitialize();

    Log::get().setGuiIface(nullptr);

    delete mGui;
    mGui = nullptr;

    delete mInput;
    mInput = nullptr;

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
    Log::get().setLog(nullptr);
    OGRE_DELETE Ogre::LogManager::getSingletonPtr();

    if(mSDLWindow)
    {
        // If we don't do this, the desktop resolution is not restored on exit
        SDL_SetWindowFullscreen(mSDLWindow, 0);
        SDL_DestroyWindow(mSDLWindow);
        mSDLWindow = nullptr;
    }
    SDL_Quit();

    delete Log::getPtr();
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
            mInput->handleMouseMotionEvent(evt.motion);
            if(mGui->getMode() == Gui::Mode_Game)
            {
                /* HACK: mouse moves the camera around */
                static float x=0.0f, y=0.0f;
                /* Rotation (x motion rotates around y, y motion rotates around x) */
                x += evt.motion.yrel * 0.1f;
                y += evt.motion.xrel * 0.1f;
                x = Ogre::Math::Clamp(x, -89.0f, 89.0f);

                Ogre::Matrix3 mat3;
                mat3.FromEulerAnglesZYX(Ogre::Degree(0.0f), Ogre::Degree(-y), Ogre::Degree(x));
                mCamera->setOrientation(mat3);
            }
            break;
        case SDL_MOUSEWHEEL:
            mInput->handleMouseWheelEvent(evt.wheel);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            mInput->handleMouseButtonEvent(evt.button);
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            mInput->handleKeyboardEvent(evt.key);
            break;
        case SDL_TEXTINPUT:
            mInput->handleTextInputEvent(evt.text);
            break;

        case SDL_QUIT:
            return false;
        }
    }

    return true;
}


void Engine::quitCmd(const std::string&)
{
    SDL_Event evt{};
    evt.quit.type = SDL_QUIT;
    SDL_PushEvent(&evt);
}

void Engine::toggleBoundingBoxCmd(const std::string&)
{
    mSceneMgr->showBoundingBoxes(!mSceneMgr->getShowBoundingBoxes());
}

void Engine::toggleDebugDisplayCmd(const std::string&)
{
    mDisplayDebugStats = !mDisplayDebugStats;
}

void Engine::saveCfgCmd(const std::string &value)
{
    static const std::string default_cfg("twokinds.cfg");
    const std::string &cfg_name = (value.empty() ? default_cfg : value);
    auto cvars = CVar::getAll();

    Log::get().stream()<< "Saving config "<<cfg_name<<"...";
    std::ofstream ocfg(cfg_name, std::ios::binary);
    if(!ocfg.is_open())
        throw std::runtime_error("Failed to open "+cfg_name+" for writing");

    ocfg<< "[CVars]" <<std::endl;
    for(const auto &cvar : cvars)
        ocfg<< cvar.first<<" = "<<cvar.second <<std::endl;
}

void Engine::internalCommand(const std::string &key, const std::string &value)
{
    auto cmd = mCommandFuncs.find(key);
    if(cmd != mCommandFuncs.end())
        return (this->*cmd->second)(value);
    throw std::runtime_error("Unexpected engine command: "+key);
}


bool Engine::go(void)
{
    Log::get().message("Initializing SDL...");
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
    Log::get().message("Initializing Ogre Root...");
    mRoot = new Ogre::Root("plugins.cfg", Ogre::String(), Ogre::String());

    Ogre::ArchiveManager::getSingleton().addArchiveFactory(new PhysFSFactory);

    // Configure
    {
        try {
            Ogre::ConfigFile cf;
            cf.load("twokinds.cfg");

            Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();
            while(seci.hasMoreElements())
            {
                Ogre::String secName = seci.peekNextKey();
                Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
                if(secName == "CVars")
                {
                    Log::get().message("Loading cvar values...");
                    for(const auto &i : *settings)
                        CVar::setByName(i.first, i.second);
                }
            }
        }
        catch(Ogre::FileNotFoundException&) {
            // Ignore if config file not found
        }

        Log::get().message("Setting up RenderSystem...");
        Ogre::RenderSystem *rsys = nullptr;
        if(!r_rendersystem->empty())
        {
            if(!(rsys=mRoot->getRenderSystemByName(*r_rendersystem)))
                Log::get().stream(Log::Level_Error)<< "  Render system \""<<*r_rendersystem<<"\" not found";
        }
        if(!rsys)
        {
            const Ogre::RenderSystemList &renderers = mRoot->getAvailableRenderers();
            for(Ogre::RenderSystem *renderer : renderers)
            {
                if(!rsys) rsys = renderer;
                Log::get().stream()<< "  Found renderer \""<<renderer->getName()<<"\"";
            }
            if(!rsys)
                throw std::runtime_error("Failed to find a usable RenderSystem");
        }
        // Force the fixed-function pipeline off
        rsys->setConfigOption("Fixed Pipeline Enabled", "No");
        const Ogre::String &err = rsys->validateConfigOptions();
        if(!err.empty())
            throw std::runtime_error("RenderSystem config error: "+err);
        mRoot->setRenderSystem(rsys);
        Log::get().stream()<< "  Initialized "<<rsys->getName();


        int width = *vid_width;
        int height = *vid_height;
        int xpos = SDL_WINDOWPOS_CENTERED;
        int ypos = SDL_WINDOWPOS_CENTERED;
        Uint32 flags = SDL_WINDOW_SHOWN;
        if(*vid_fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;

        Log::get().stream()<< "Creating window "<<width<<"x"<<height<<", flags 0x"<<std::hex<<flags;
        mSDLWindow = SDL_CreateWindow("Twokinds", xpos, ypos, width, height, flags);
        if(mSDLWindow == nullptr)
        {
            std::stringstream sstr;
            sstr<< "SDL_CreateWindow Error: "<<SDL_GetError();
            throw std::runtime_error(sstr.str());
        }

        mRoot->initialise(false);
        mWindow = createRenderWindow(mSDLWindow);
    }
    SDL_ShowCursor(0);

    // Setup resources
    Log::get().message("Initializing resources...");
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
        {
            Log::get().stream()<< "  Adding source path "<<path;
            PhysFSFactory::getSingleton().addPath(path.c_str());
        }

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

    // Setup Input subsystem
    Log::get().message("Initializing input...");
    mInput = new Input();

    // Setup GUI subsystem
    Log::get().message("Initializing GUI...");
    mGui = new Gui(mWindow, mSceneMgr);
    Log::get().setGuiIface(mGui);
    for(const auto &cmd : mCommandFuncs)
        mGui->addConsoleCallback(cmd.first.c_str(), makeDelegate(this, &Engine::internalCommand));
    CVar::registerAll();

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

    saveCfgCmd(std::string());

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
    if(mGui->getMode() == Gui::Mode_Game)
    {
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
    }

    if(!mDisplayDebugStats)
    {
        if(!*vid_showfps)
            mGui->updateStatus(std::string());
        else
        {
            std::stringstream status;
            status<< "Average FPS: "<<mWindow->getAverageFPS() <<std::endl;
            mGui->updateStatus(status.str());
        }
    }
    else
    {
        std::stringstream status;
        status<< "Average FPS: "<<mWindow->getAverageFPS() <<std::endl;
        status<< "Camera pos: "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<mCamera->getPosition() <<std::endl;
        World::get().getStatus(status);
        mGui->updateStatus(status.str());
    }

    return true;
}

} // namespace TK
