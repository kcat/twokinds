#ifndef SINGLETON_HPP
#define SINGLETON_HPP

#include <cassert>


namespace TK
{

template<typename T>
class Singleton
{
    Singleton(const Singleton<T>&) = delete;
    Singleton<T>& operator=(const Singleton<T>&) = delete;

protected:
    static T* sInstance;

public:
    Singleton()
    {
        assert(!sInstance);
        sInstance = static_cast<T*>(this);
    }
    virtual ~Singleton()
    {
        sInstance = nullptr;
    }

    static T& get() { assert(sInstance); return *sInstance; }
    static T* getPtr() { return sInstance; }
};

} // namspace TK

#endif /* SINGLETON_HPP */
