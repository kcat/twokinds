#include <iostream>

#include <sstream>

#include "SDL_main.h"
#include "physfs.h"

#include "engine.hpp"


static void DoErrorMessage(const char *msg, const char *title)
{
#if _WIN32
    MessageBoxA(NULL, msg, title, MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
    std::stringstream cmd;
    const char *str;

    if((str=getenv("KDE_FULL_SESSION")) && strcmp(str, "true") == 0)
    {
        cmd<< "kdialog --geometry 640x240";
        cmd<< " --title '"<<title<<"'";
        cmd<< " --error '"<<msg<<"'";
    }
    else if((str=getenv("GNOME_DESKTOP_SESSION_ID")) && str[0] != '\0')
    {
        cmd<< "gxmessage -buttons 'Okay:0' -geometry 640x240";
        cmd<< " -title '"<<title<<"'";
        cmd<< " -center '"<<msg<<"'";
    }
    else
    {
        cmd<< "xmessage -buttons 'Okay:0' ";
        cmd<< " -center '"<<msg<<"'";
    }

    int err = system(cmd.str().c_str());
    if(WIFEXITED(err) == 0 || WEXITSTATUS(err) != 0)
        std::cerr<< "*** "<<title<<" ***" <<std::endl
                 << msg <<std::endl
                 << "***" <<std::endl;
#endif
}


int main(int argc, char *argv[])
{
    TK::Engine app;

    try {
        if(!app.parseOptions(argc, argv))
            return 0;
        app.go();
    }
    catch(std::exception &e) {
        DoErrorMessage(e.what(), "An exception has occured!");
        return 1;
    }

    return 0;
}
