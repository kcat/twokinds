
#include "engine.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>

#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreRenderWindow.h>
#include <OgreCamera.h>
#include <OgreArchiveManager.h>
#include <OgreMaterialManager.h>
#include <OgreEntity.h>
#include <OgreTechnique.h>
#include <OgreConfigFile.h>

#include <Compositor/OgreCompositorManager2.h>
#include <RTShaderSystem/OgreRTShaderSystem.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include "archives/physfs.hpp"


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
    { }

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
{
}

Engine::~Engine(void)
{
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

        case SDL_KEYDOWN:
        case SDL_KEYUP:
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
        std::cerr<< "SDL_Init Error: "<<SDL_GetError() <<std::endl;
        return false;
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

        // Load resource paths from config file
        Ogre::ConfigFile cf;
        cf.load("resources.cfg");

        Ogre::StringVector paths = cf.getMultiSetting("path", "General");
        for(const auto &path : paths)
            PhysFSFactory::getSingleton().Mount(path.c_str());

        // Go through all sections & settings in the file
        Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();
        while(seci.hasMoreElements())
        {
            Ogre::String secName = seci.peekNextKey();
            Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
            if(secName == "General")
                continue;
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
    Ogre::MaterialManager::getSingleton().addListener(&sgtrl);

    /* Initialise all resource groups. Ogre is sensitive to the shaders being
     * initialized after the materials, so ensure the shaders get initialized
     * first. */
    Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup("Shaders");
    Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup("Materials");
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

    const size_t numThreads = std::max(1u, Ogre::PlatformInformation::getNumLogicalCores());
    Ogre::InstancingTheadedCullingMethod threadedCullingMethod = Ogre::INSTANCING_CULLING_SINGLETHREAD;
    if(numThreads > 1)
        threadedCullingMethod = Ogre::INSTANCING_CULLING_THREADED;
    mSceneMgr = mRoot->createSceneManager(Ogre::ST_GENERIC, numThreads, threadedCullingMethod);

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
    //mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(mCamera);
    mRoot->getCompositorManager2()->addWorkspace(
        mSceneMgr, mWindow, mCamera, "MainWorkspace", true
    );

    // Alter the camera aspect ratio to match the window
    mCamera->setAspectRatio(Ogre::Real(mWindow->getWidth()) / Ogre::Real(mWindow->getHeight()));

    /* Create something simple to look at */
    Ogre::Entity *ent = mSceneMgr->createEntity("penguin.mesh");
    mSceneMgr->getRootSceneNode()->createChildSceneNode(Ogre::SCENE_DYNAMIC, Ogre::Vector3(0,0,-250))->attachObject(ent);

    /* Make a light so we can see what we're looking at */
    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.3, 0.3, 0.3));
    Ogre::SceneNode *lightNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
    lightNode->setPosition(20, 80, 50);
    lightNode->attachObject(mSceneMgr->createLight());

    // And away we go!
    mRoot->addFrameListener(this);
    mRoot->startRendering();

    return true;
}


bool Engine::frameRenderingQueued(const Ogre::FrameEvent &evt)
{
    if(!pumpEvents() || mWindow->isClosed())
        return false;

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);
    if(keystate[SDL_SCANCODE_ESCAPE])
        return false;

    return true;
}

} // namespace TK
