#ifndef ARCHIVES_PHYSFS_HPP
#define ARCHIVES_PHYSFS_HPP

#include <string>

#include "singleton.hpp"

namespace MyGUI
{
    class DataManager;
}

namespace TK
{

// A factory for handling PhysFS.
class PhysFSFactory : public Singleton<PhysFSFactory>
{
public:
    PhysFSFactory();
    virtual ~PhysFSFactory();

    void addPath(const char *path, const char *mountPoint=nullptr, bool append=false) const;
    MyGUI::DataManager *createDataManager(std::string&& base) const;
};

} // namespace TK

#endif /* ARCHIVES_PHYSFS_HPP */
