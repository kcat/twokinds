#ifndef ENGINE_HPP
#define ENGINE_HPP

struct SDL_Window;

namespace TK
{

class Engine
{
    SDL_Window *mSDLWindow;

public:
    Engine(void);
    virtual ~Engine(void);

    bool parseOptions(int argc, char *argv[]);

    bool go(void);
};

} // namespace TK

#endif /* ENGINE_HPP */
