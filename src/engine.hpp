#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string>
#include <map>

#include <osg/ref_ptr>
#include <osg/Vec3f>
#include <osg/Quat>

struct SDL_Window;
struct SDL_WindowEvent;
struct SDL_MouseMotionEvent;
struct SDL_MouseWheelEvent;
struct SDL_MouseButtonEvent;
struct SDL_KeyboardEvent;
struct SDL_TextInputEvent;

namespace osg
{
    class Group;
    class Camera;
}

namespace TK
{

class Input;
class Gui;

class Engine
{
    typedef void (Engine::*CmdFuncT)(const std::string&);
    typedef std::map<std::string,CmdFuncT> CommandFuncMap;

    SDL_Window *mSDLWindow;

    Input *mInput;
    Gui *mGui;

    bool mDisplayDebugStats;

    osg::ref_ptr<osg::Camera> mCamera;
    osg::Quat mCameraRot;
    osg::Vec3f mCameraPos;

    // Root node for the world display
    osg::ref_ptr<osg::Group> mSceneRoot;

    const CommandFuncMap mCommandFuncs;

    void handleWindowEvent(const SDL_WindowEvent &evt);
    bool pumpEvents();

    void toggleWireframeCmd(const std::string &value);
    void toggleDebugDisplayCmd(const std::string &value);
    void rebuildCompositeMapsCmd(const std::string &value);
    void internalCommand(const std::string &key, const std::string &value);

public:
    Engine(void);
    virtual ~Engine(void);

    bool parseOptions(int argc, char *argv[]);

    bool go(void);
};

} // namespace TK

#endif /* ENGINE_HPP */
