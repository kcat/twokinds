#ifndef CVARS_HPP
#define CVARS_HPP

#include <string>
#include <map>

namespace TK
{

class CVar {
public:
    CVar(std::string&& name);
    virtual ~CVar() { }

    virtual bool set(const std::string &value) = 0;
    virtual std::string get() const = 0;

    static void setByName(const std::string &name, const std::string &value);
    static std::map<std::string,std::string> getAll();
    static void registerAll();
};

class CVarString : public CVar {
    std::string mValue;

public:
    CVarString(std::string&& name, std::string value);

    virtual bool set(const std::string &value) final;
    virtual std::string get() const final;
};

class CVarBool : public CVar {
    bool mValue;

public:
    CVarBool(std::string&& name, bool value);

    virtual bool set(const std::string &value) final;
    virtual std::string get() const final;
};

class CVarInt : public CVar {
    int mValue;

public:
    CVarInt(std::string&& name, int value);

    virtual bool set(const std::string &value) final;
    virtual std::string get() const final;
};

} // namespace TK

#define EXTERN_CVAR(T, name) extern ::TK::T name

#define CVAR(T, name, value) ::TK::T name(#name, value)

#endif /* CVARS_HPP */
