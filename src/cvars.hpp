#ifndef CVARS_HPP
#define CVARS_HPP

#include <string>
#include <map>
#include <limits>

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
protected:
    std::string mValue;

public:
    CVarString(std::string&& name, std::string&& value);

    const std::string& operator*() const { return mValue; }
    const std::string* operator->() const { return &mValue; }

    virtual bool set(const std::string &value) final;
    virtual std::string get() const final;
};

class CVarBool : public CVar {
protected:
    bool mValue;

public:
    CVarBool(std::string&& name, bool value);

    bool operator*() const { return mValue; }

    virtual bool set(const std::string &value) final;
    virtual std::string get() const final;
};

class CVarInt : public CVar {
protected:
    const int mMinValue, mMaxValue;
    int mValue;

public:
    CVarInt(std::string&& name, int value, int minval=std::numeric_limits<int>::min(),
                                           int maxval=std::numeric_limits<int>::max());

    int operator*() const { return mValue; }

    virtual bool set(const std::string &value) final;
    virtual std::string get() const final;
};

} // namespace TK

#define CVAR(T, name, ...) ::TK::T name(#name, __VA_ARGS__)

#define EXTERN_CVAR(T, name) extern ::TK::T name

#endif /* CVARS_HPP */
