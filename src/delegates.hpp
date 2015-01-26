#ifndef DELEGATES_HPP
#define DELEGATES_HPP

#include <typeinfo>
#include <list>

#include <MyGUI_Delegate.h>


namespace TK
{

template<typename ...Args>
class IDelegate
{
public:
    virtual ~IDelegate() { }
    virtual bool isType(const std::type_info& _type) = 0;
    virtual void invoke(Args...) = 0;
    virtual bool compare(IDelegate<Args...> *_delegate) const = 0;
    virtual bool compare(MyGUI::delegates::IDelegateUnlink* _unlink) const
    { return false; }
};

template<typename ...Args>
class CStaticDelegate : public IDelegate<Args...>
{
    typedef void (*Func)(Args...);

    Func mFunc;

public:
    CStaticDelegate(Func _func) : mFunc(_func) { }

    virtual bool isType(const std::type_info& _type)
    {
        return typeid(CStaticDelegate<Args...>) == _type;
    }

    virtual void invoke(Args...args)
    {
        mFunc(args...);
    }

    virtual bool compare(IDelegate<Args...> *_delegate) const
    {
        if(nullptr == _delegate || !_delegate->isType(typeid(CStaticDelegate<Args...>)))
            return false;
        CStaticDelegate<Args...> *cast = static_cast<CStaticDelegate<Args...>*>(_delegate);
        return cast->mFunc == mFunc;
    }
    virtual bool compare(MyGUI::delegates::IDelegateUnlink *_unlink) const
    {
        return false;
    }
};

template<typename T, typename ...Args>
class CMethodDelegate : public IDelegate<Args...>
{
    typedef void (T::*Method)(Args...);

    MyGUI::delegates::IDelegateUnlink *mUnlink;
    T *mObject;
    Method mMethod;

public:
    CMethodDelegate(MyGUI::delegates::IDelegateUnlink *_unlink, T *_object, Method _method)
      : mUnlink(_unlink), mObject(_object), mMethod(_method)
    { }

    virtual bool isType(const std::type_info &_type)
    {
        return typeid(CMethodDelegate<T,Args...>) == _type;
    }

    virtual void invoke(Args...args)
    {
        (mObject->*mMethod)(args...);
    }

    virtual bool compare(IDelegate<Args...> *_delegate) const
    {
        if (nullptr == _delegate || !_delegate->isType(typeid(CMethodDelegate<T,Args...>)))
            return false;
        CMethodDelegate<T,Args...> *cast = static_cast<CMethodDelegate<T,Args...>*>(_delegate);
        return cast->mObject == mObject && cast->mMethod == mMethod;
    }

    virtual bool compare(MyGUI::delegates::IDelegateUnlink *_unlink) const
    {
        return mUnlink == _unlink;
    }
};

template<typename ...Args>
class CDelegate
{
    typedef IDelegate<Args...> DelegateT;

    // No copying (only moving)
    CDelegate(const CDelegate<Args...> &_event) = delete;
    CDelegate<Args...>& operator=(const CDelegate<Args...>& _event) = delete;

    DelegateT *mDelegate;

public:
    CDelegate(DelegateT *_delegate=nullptr) : mDelegate(_delegate) { }
    CDelegate(CDelegate<Args...> &&_event) : mDelegate(nullptr)
    {
        // take ownership
        std::swap(mDelegate, _event.mDelegate);
    }

    ~CDelegate()
    {
        clear();
    }

    bool empty() const
    {
        return mDelegate == nullptr;
    }

    void clear()
    {
        delete mDelegate;
        mDelegate = nullptr;
    }

    CDelegate<Args...>& operator=(CDelegate<Args...>&& _event)
    {
        // take ownership
        std::swap(mDelegate, _event.mDelegate);
        return *this;
    }

    void operator()(Args...args)
    {
        if (mDelegate == nullptr) return;
        mDelegate->invoke(args...);
    }
};

template<typename ...Args>
class CMultiDelegate
{
    typedef IDelegate<Args...> DelegateT;
    typedef typename std::list<DelegateT* /*, Allocator<DelegateT*>*/ > ListDelegate;
    typedef typename ListDelegate::iterator ListDelegateIterator;
    typedef typename ListDelegate::const_iterator ConstListDelegateIterator;

    // No copying (only moving)
    CMultiDelegate(const CMultiDelegate<Args...>& _event) = delete;
    CMultiDelegate<Args...>& operator=(const CMultiDelegate<Args...>& _event) = delete;

    void safe_clear(ListDelegate& _delegates)
    {
        for(ListDelegateIterator iter = mListDelegates.begin();iter != mListDelegates.end();++iter)
        {
            if(*iter)
            {
                DelegateT *del = *iter;
                *iter = nullptr;
                delete_if_not_found(del, _delegates);
            }
        }
    }

    static void delete_if_not_found(DelegateT *_del, ListDelegate &_delegates)
    {
        for(ListDelegateIterator iter = _delegates.begin();iter != _delegates.end();++iter)
        {
            if(*iter && (*iter)->compare(_del))
                return;
        }
        delete _del;
    }

    ListDelegate mListDelegates;

public:
    CMultiDelegate() { }
    CMultiDelegate(CMultiDelegate<Args...>&& _event)
    {
        // take ownership
        std::swap(mListDelegates, _event.mListDelegates);
        safe_clear(mListDelegates);
    }

    ~CMultiDelegate()
    {
        clear();
    }

    bool empty() const
    {
        for(ConstListDelegateIterator iter = mListDelegates.begin();iter != mListDelegates.end();++iter)
        {
            if(*iter)
                return false;
        }
        return true;
    }

    void clear()
    {
        for(ListDelegateIterator iter = mListDelegates.begin();iter != mListDelegates.end();++iter)
        {
            delete *iter;
            *iter = nullptr;
        }
    }

    void clear(MyGUI::delegates::IDelegateUnlink *_unlink)
    {
        for(ListDelegateIterator iter = mListDelegates.begin();iter != mListDelegates.end();++iter)
        {
            if(*iter && (*iter)->compare(_unlink))
            {
                delete *iter;
                *iter = nullptr;
            }
        }
    }

    CMultiDelegate<Args...>& operator+=(DelegateT *_delegate)
    {
        for(ListDelegateIterator iter = mListDelegates.begin();iter != mListDelegates.end();++iter)
        {
            if(*iter && (*iter)->compare(_delegate))
                MYGUI_EXCEPT("Trying to add same delegate twice.");
        }
        mListDelegates.push_back(_delegate);
        return *this;
    }

    CMultiDelegate<Args...>& operator-=(DelegateT *_delegate)
    {
        for(ListDelegateIterator iter = mListDelegates.begin();iter != mListDelegates.end();++iter)
        {
            if(*iter && (*iter)->compare(_delegate))
            {
                // проверяем на идентичность делегатов
                if(*iter != _delegate)
                    delete *iter;
                *iter = nullptr;
                break;
            }
        }
        delete _delegate;
        return *this;
    }

    void operator()(Args...args)
    {
        ListDelegateIterator iter = mListDelegates.begin();
        while(iter != mListDelegates.end())
        {
            if(nullptr == *iter)
                iter = mListDelegates.erase(iter);
            else
            {
                (*iter)->invoke(args...);
                ++iter;
            }
        }
    }

    CMultiDelegate<Args...>& operator=(CMultiDelegate<Args...>&& _event)
    {
        // take ownership
        std::swap(mListDelegates, _event.mListDelegates);
        safe_clear(mListDelegates);
        return *this;
    }
};


template<typename ...Args>
inline IDelegate<Args...>* makeDelegate(void (*func)(Args...))
{
    return new CStaticDelegate<Args...>(func);
}

template<typename T, typename ...Args>
inline IDelegate<Args...>* makeDelegate(T *obj, void (T::*Func)(Args...))
{
    return new CMethodDelegate<T,Args...>(MyGUI::delegates::GetDelegateUnlink(obj), obj, Func);
}

} // namespace TK

#endif /* DELEGATES_HPP */
