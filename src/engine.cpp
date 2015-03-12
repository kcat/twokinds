
#include "engine.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <OgreConfigFile.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgDB/Registry>
#include <osgDB/ReadFile>
#include <osg/MatrixTransform>

#include "archives/physfs.hpp"
#include "input/input.hpp"
#include "gui/gui.hpp"
#include "terrain.hpp"
#include "delegates.hpp"
#include "sparsearray.hpp"
#include "cvars.hpp"
#include "log.hpp"

#include "render/mygui_osgrendermanager.h"
#include "render/sdl2_osggraphicswindow.h"
#include "render/pipeline.hpp"


inline std::ostream& operator<<(std::ostream &out, const osg::Vec3f &vec)
{
    out<<"osg::Vec3f("<<vec[0]<<", "<<vec[1]<<", "<<vec[2]<<")";
    return out;
}


namespace TK
{

CVAR(CVarInt, vid_width, 1280);
CVAR(CVarInt, vid_height, 720);
CVAR(CVarBool, vid_fullscreen, false);
CVAR(CVarBool, vid_showfps, false);


Engine::Engine(void)
  : mSDLWindow(nullptr)
  , mInput(nullptr)
  , mGui(nullptr)
  , mDisplayDebugStats(false)
  , mCommandFuncs{
      { "rcm", &Engine::rebuildCompositeMapsCmd },
      { "savecfg", &Engine::saveCfgCmd },
      { "tbb", &Engine::toggleBoundingBoxCmd },
      { "tdd", &Engine::toggleDebugDisplayCmd },
      { "tm", &Engine::toggleMapsCmd },
      { "qqq", &Engine::quitCmd },
    }
{
    new Log(Log::Level_Normal, "twokinds.log");
}

Engine::~Engine(void)
{
    delete Pipeline::getPtr();

    World::get().deinitialize();

    mSceneRoot = nullptr;
    mCamera = nullptr;

    Log::get().setGuiIface(nullptr);

    delete mGui;
    mGui = nullptr;

    delete mInput;
    mInput = nullptr;

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


void Engine::handleWindowEvent(const SDL_WindowEvent &evt)
{
    switch(evt.event)
    {
        case SDL_WINDOWEVENT_MOVED:
        {
            int width, height;
            SDL_GetWindowSize(mSDLWindow, &width, &height);
            mCamera->getGraphicsContext()->resized(evt.data1, evt.data2, width, height);
            break;
        }
        case SDL_WINDOWEVENT_RESIZED:
        {
            int x, y;
            SDL_GetWindowPosition(mSDLWindow, &x, &y);
            mCamera->getGraphicsContext()->resized(x, y, evt.data1, evt.data2);
            break;
        }

        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_HIDDEN:
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
                /* HACK: mouse rotates the camera around */
                static float x=0.0f, y=0.0f;
                /* Rotation (x motion rotates around y, y motion rotates around x) */
                x += evt.motion.yrel * 0.1f;
                y += evt.motion.xrel * 0.1f;
                x = std::min(std::max(x, -89.0f), 89.0f);

                mCameraRot.makeRotate(
                     x*3.14159f/180.0f, osg::Vec3f(1.0f, 0.0f, 0.0f),
                    -y*3.14159f/180.0f, osg::Vec3f(0.0f, 1.0f, 0.0f),
                                  0.0f, osg::Vec3f(0.0f, 0.0f, 1.0f)
                );
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
    Log::get().message("Cannot currently display bounding boxes");
}

void Engine::toggleDebugDisplayCmd(const std::string&)
{
    mDisplayDebugStats = !mDisplayDebugStats;
}

void Engine::toggleMapsCmd(const std::string&)
{
    Pipeline::get().toggleDebugMapDisplay();
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

void Engine::rebuildCompositeMapsCmd(const std::string&)
{
    Log::get().message("Rebuilding composite maps...");
    World::get().rebuildCompositeMaps();
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

    // Init everything except audio (we will use OpenAL for that)
    if(SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO) != 0)
    {
        std::stringstream sstr;
        sstr<< "SDL_Init Error: "<<SDL_GetError();
        throw std::runtime_error(sstr.str());
    }

    // Setup resources
    Log::get().message("Initializing resources...");
    {
        PhysFSFactory *factory = new PhysFSFactory();

        // Load resource paths from config file
        Ogre::ConfigFile cf;
        cf.load("resources.cfg");

        Ogre::StringVector paths = cf.getMultiSetting("source", "General");
        for(const auto &path : paths)
        {
            Log::get().stream()<< "  Adding source path "<<path;
            factory->addPath(path.c_str());
        }

        osgDB::FilePathList dbpaths{"/materials/textures", "/meshes", "/MyGUI_Media"};
        osgDB::Registry::instance()->setDataFilePathList(dbpaths);
    }

    // Configure
    osg::ref_ptr<osgViewer::Viewer> viewer;
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

        int width = *vid_width;
        int height = *vid_height;
        int xpos = SDL_WINDOWPOS_CENTERED;
        int ypos = SDL_WINDOWPOS_CENTERED;
        Uint32 flags = SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN;
        if(*vid_fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;

        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, SDL_TRUE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, SDL_TRUE);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        Log::get().stream()<< "Creating window "<<width<<"x"<<height<<", flags 0x"<<std::hex<<flags;
        mSDLWindow = SDL_CreateWindow("Twokinds", xpos, ypos, width, height, flags);
        if(mSDLWindow == nullptr)
        {
            std::stringstream sstr;
            sstr<< "SDL_CreateWindow Error: "<<SDL_GetError();
            throw std::runtime_error(sstr.str());
        }

        graphicswindow_SDL2();
        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
        SDL_GetWindowPosition(mSDLWindow, &traits->x, &traits->y);
        SDL_GetWindowSize(mSDLWindow, &traits->width, &traits->height);
        traits->windowName = SDL_GetWindowTitle(mSDLWindow);
        traits->windowDecoration = !(SDL_GetWindowFlags(mSDLWindow)&SDL_WINDOW_BORDERLESS);
        traits->screenNum = SDL_GetWindowDisplayIndex(mSDLWindow);
        // FIXME: Some way to get these settings back from the SDL window?
        traits->red = 8;
        traits->green = 8;
        traits->blue = 8;
        traits->alpha = 8;
        traits->depth = 24;
        traits->stencil = 8;
        traits->doubleBuffer = true;
        traits->inheritedWindowData = new GraphicsWindowSDL2::WindowData(mSDLWindow);

        osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
        if(!gc.valid()) throw std::runtime_error("Failed to create GraphicsContext");
        gc->getState()->setUseModelViewAndProjectionUniforms(true);
        gc->getState()->setUseVertexAttributeAliasing(true);

        mCamera = new osg::Camera();
        mCamera->setGraphicsContext(gc.get());
        mCamera->setViewport(0, 0, width, height);
        mCamera->setProjectionResizePolicy(osg::Camera::FIXED);
        mCamera->setProjectionMatrix(osg::Matrix::identity());

        viewer = new osgViewer::Viewer();
        viewer->setCamera(mCamera.get());
    }
    SDL_ShowCursor(0);

    mSceneRoot = new osg::Group();

    {
        int screen_width = mCamera->getViewport()->width();
        int screen_height = mCamera->getViewport()->height();
        Pipeline *pipeline = new Pipeline(screen_width, screen_height);
        pipeline->init(mSceneRoot.get());
        pipeline->setProjectionMatrix(osg::Matrix::perspective(
            65.0, double(screen_width)/double(screen_height), 1.0, 50000.0
        ));

        // Add a light so we can see
        osg::Vec3f lightDir(70.f, -100.f, 10.f);
        lightDir.normalize();
        osg::ref_ptr<osg::Node> light = pipeline->createDirectionalLight();
        osg::StateSet *ss = light->getOrCreateStateSet();
        ss->addUniform(new osg::Uniform("light_direction", lightDir));
        ss->addUniform(new osg::Uniform("diffuse_color", osg::Vec4f(1.0f, 0.988f, 0.933f, 1.0f)));
        ss->addUniform(new osg::Uniform("specular_color", osg::Vec4f(1.0f, 1.0f, 1.0f, 1.0f)));
        pipeline->getLightingStateSet()->getUniform("ambient_color")->set(
            osg::Vec4f(0.537f, 0.549f, 0.627f, 1.0f)
        );
    }

    viewer->setSceneData(Pipeline::get().getGraphRoot());
    viewer->requestContinuousUpdate();
    viewer->setLightingMode(osg::View::NO_LIGHT);
    viewer->addEventHandler(new osgViewer::StatsHandler);
    viewer->realize();

    // Setup Input subsystem
    Log::get().message("Initializing input...");
    mInput = new Input();

    // Setup GUI subsystem
    Log::get().message("Initializing GUI...");
    mGui = new Gui(viewer.get(), viewer->getSceneData()->asGroup());

    Log::get().setGuiIface(mGui);
    for(const auto &cmd : mCommandFuncs)
        mGui->addConsoleCallback(cmd.first.c_str(), makeDelegate(this, &Engine::internalCommand));
    CVar::registerAll();

    // Set up the terrain
    World::get().initialize(viewer.get(), mSceneRoot.get(), mCameraPos);

    // Frame rate tracking...
    double last_fps_time = 0.0;
    double last_fps = 0.0;
    int frame_count = 0;

    // And away we go!
    const Uint32 base_time = SDL_GetTicks();
    double last_time = 0.0;
    while(!viewer->done() && pumpEvents())
    {
        const Uint8 *keystate = SDL_GetKeyboardState(NULL);
        if(keystate[SDL_SCANCODE_ESCAPE])
            break;

        Uint32 current_time = SDL_GetTicks() - base_time;
        double frame_time = current_time / 1000.0;
        if(frame_time < last_time)
            throw std::runtime_error("Tick count overflow");


        double timediff = std::min(1.0/20.0, frame_time-last_time);
        if(mGui->getMode() == Gui::Mode_Game)
        {
            float speed = 60.0f * timediff;
            if(keystate[SDL_SCANCODE_LSHIFT])
                speed *= 2.0f;

            osg::Vec3f movedir;
            if(keystate[SDL_SCANCODE_W])
                movedir.z() += -1.0f;
            if(keystate[SDL_SCANCODE_A])
                movedir.x() += -1.0f;
            if(keystate[SDL_SCANCODE_S])
                movedir.z() += +1.0f;
            if(keystate[SDL_SCANCODE_D])
                movedir.x() += +1.0f;
            if(keystate[SDL_SCANCODE_PAGEUP])
                movedir.y() += +1.0f;
            if(keystate[SDL_SCANCODE_PAGEDOWN])
                movedir.y() += -1.0f;


            mCameraPos += (mCameraRot*movedir)*speed;
            mCameraPos.y() = std::max(mCameraPos.y(), World::get().getHeightAt(mCameraPos)+60.0f);

            osg::Matrixf matf(mCameraRot.inverse());
            matf.preMultTranslate(-mCameraPos);
            mCamera->setViewMatrix(matf);
        }

        World::get().update(mCameraPos);

        if(frame_time-last_fps_time >= 1.0)
        {
            last_fps = frame_count / (frame_time-last_fps_time);
            last_fps_time = frame_time;
            frame_count = 0;
        }

        if(!mDisplayDebugStats)
        {
            if(!*vid_showfps)
                mGui->updateStatus(std::string());
            else
            {
                std::stringstream status;
                status<< "Average FPS: "<<std::setiosflags(std::ios::fixed)<<std::setprecision(1)<<last_fps <<std::endl;
                mGui->updateStatus(status.str());
            }
        }
        else
        {
            std::stringstream status;
            status<< "Average FPS: "<<std::setiosflags(std::ios::fixed)<<std::setprecision(1)<<last_fps <<std::endl;
            status<< "Camera pos: "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<mCameraPos <<std::endl;
            World::get().getStatus(status);
            mGui->updateStatus(status.str());
        }

        viewer->frame(timediff);
        last_time = frame_time;
        ++frame_count;
    }

    saveCfgCmd(std::string());

    return true;
}

} // namespace TK
