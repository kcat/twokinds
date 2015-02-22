#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string>
#include <map>

#include <osg/ref_ptr>
#include <osg/Vec3f>

struct SDL_Window;
struct SDL_WindowEvent;
struct SDL_MouseMotionEvent;
struct SDL_MouseWheelEvent;
struct SDL_MouseButtonEvent;
struct SDL_KeyboardEvent;
struct SDL_TextInputEvent;

namespace osg
{
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
    osg::Vec3f mCameraPos;

    const CommandFuncMap mCommandFuncs;

    void handleWindowEvent(const SDL_WindowEvent &evt);
    bool pumpEvents();

    void quitCmd(const std::string &value);
    void toggleBoundingBoxCmd(const std::string &value);
    void toggleDebugDisplayCmd(const std::string &value);
    void saveCfgCmd(const std::string &value);
    void internalCommand(const std::string &key, const std::string &value);

public:
    Engine(void);
    virtual ~Engine(void);

    bool parseOptions(int argc, char *argv[]);

    bool go(void);
};

} // namespace TK

#endif /* ENGINE_HPP */
