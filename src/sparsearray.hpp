#ifndef SPARSEARRAY_HPP
#define SPARSEARRAY_HPP

#include <deque>
#include <map>
#include <stack>
#include <limits>

namespace TK
{

/* A sparse array implementation, to handle contiguous objects in a non-
 * contiguous array (i.e. objects are (sort of) stored contiguous in memory,
 * but have array lookup indices that contain many gaps).
 *
 * To do this, we store objects in a deque. Deques generally store objects in
 * static-sized "runs", or segments, which allows for fast insertion at the
 * front or back, while not storing each object separately like a list would.
 * They also have the property that pointers and references to individual
 * elements remain valid as long as insertions and deletions happen at the
 * front and back.
 *
 * A map is used to translate externally-visible indices into deque indices.
 *
 * To ensure pointers are never invalidated, an object entry that gets erased
 * will have its destructor run manually and its deque index stored in a stack,
 * so that it may be reused for the next allocation.
 */
template<typename T>
class SparseArray {
    typedef T value_type;

    // Objects are stored with the user index. A max index value is used to
    // indicate an unallocated object.
    typedef std::pair<const size_t,value_type> pair_type;
    typedef std::deque<pair_type> list_type;

    list_type mData;
    std::map<size_t,size_t> mIdxLookup;
    std::stack<size_t> mFreeIdx;

    // No copying, only moving
    SparseArray(const SparseArray<value_type>&) = delete;
    SparseArray<T>& operator=(const SparseArray<value_type>&) = delete;

    template<typename _PT, typename _LT, typename _IT>
    class iterator_base : public std::iterator<std::bidirectional_iterator_tag, _PT> {
        typedef iterator_base<_PT,_LT,_IT> self_type;

        _LT mContainer;
        _IT mIter;

    public:
        iterator_base() { }
        iterator_base(_LT *cont, _IT&& iter)
          : mContainer(cont), mIter(std::move(iter))
        {
            while(mIter != mContainer->end() && mIter->first == std::numeric_limits<size_t>::max())
                ++mIter;
        }

        bool operator==(const self_type &rhs) const
        { return mIter == rhs.mIter; }
        bool operator!=(const self_type &rhs) const
        { return mIter != rhs.mIter; }

        _PT& operator*() { return *mIter; }
        _PT* operator->() { return &*mIter; }

        self_type& operator++()
        {
            do {
                ++mIter;
            } while(mIter != mContainer->end() && mIter->first == std::numeric_limits<size_t>::max());
            return *this;
        }
        self_type& operator--()
        {
            do {
                --mIter;
            } while(mIter != mContainer->rend() && mIter->first == std::numeric_limits<size_t>::max());
            return *this;
        }

        self_type operator++(int)
        {
            self_type old = *this;
            ++*this;
            return old;
        }
        self_type operator--(int)
        {
            self_type old = *this;
            --*this;
            return old;
        }
    };

public:
    // NOTE: SparseArray iterators operate sequentially over the deque, rather
    // than the user index map. This means it's possible for index n to be
    // encountered before index n-1. It's done this way to reduce cache misses,
    // since following the user index ordering could cause data lookups to jump
    // forward and backward within the deque.
    //
    // The iterator references a pair, with first=index, second=element
    typedef iterator_base<pair_type,list_type*,typename list_type::iterator> iterator;
    typedef iterator_base<const pair_type,const list_type*,typename list_type::const_iterator> const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    SparseArray() { }
    ~SparseArray()
    {
        // Freed objects have been manually destructed, but the deque will
        // attempt to destruct them again as it goes out of scope. We can't
        // rely on the destructor to safely run multiple times, so we construct
        // them before that happens.
        while(!mFreeIdx.empty())
        {
            new(&mData[mFreeIdx.top()]) pair_type(0, value_type());
            mFreeIdx.pop();
        }
    }

    bool exists(size_t idx) const
    { return (mIdxLookup.find(idx) != mIdxLookup.end()); }

    // Look up the object for the given index. If an object at the given index
    // doesn't exist, it will be allocated.
    T& operator[](size_t idx)
    {
        auto iter = mIdxLookup.find(idx);
        if(iter == mIdxLookup.end())
        {
            size_t dq_idx;
            if(!mFreeIdx.empty())
            {
                // Take the next free deque index, and placement-new to
                // (re)initialize the pair.
                dq_idx = mFreeIdx.top();
                new(&mData[dq_idx]) pair_type(idx, value_type());
                mFreeIdx.pop();
            }
            else
            {
                dq_idx = mData.size();
                mData.push_back(std::make_pair(idx, value_type()));
            }
            iter = mIdxLookup.emplace(idx, dq_idx).first;
        }

        return mData[iter->second].second;
    }

    void erase(size_t idx)
    {
        auto lookup = mIdxLookup.find(idx);
        if(lookup == mIdxLookup.end())
            return;

        // To "erase" an entry, we manually destruct the pair it's stored with,
        // then force in a dummy index value so it gets skipped when iterating.
        pair_type &ptr = mData[lookup->second];
        ptr.~pair_type();
        const_cast<size_t&>(ptr.first) = std::numeric_limits<size_t>::max();

        mFreeIdx.push(lookup->second);
        mIdxLookup.erase(lookup);
    }

    iterator begin() { return iterator(&mData, mData.begin()); }
    iterator end() { return iterator(&mData, mData.end()); }
    const_iterator begin() const { return const_iterator(&mData, mData.begin()); }
    const_iterator end() const { return const_iterator(&mData, mData.end()); }

    const_iterator cbegin() const { return const_iterator(&mData, mData.cbegin()); }
    const_iterator cend() const { return const_iterator(&mData, mData.cend()); }

    reverse_iterator rbegin() { return reverse_iterator(&mData, mData.rbegin()); }
    reverse_iterator rend() { return reverse_iterator(&mData, mData.rend()); }
    const_reverse_iterator rbegin() const { return  const_reverse_iterator(&mData, mData.rbegin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(&mData, mData.rend()); }

    const_reverse_iterator crbegin() const { return const_reverse_iterator(&mData, mData.crbegin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(&mData, mData.crend()); }
};

} // namespace TK

#endif /* SPARSEARRAY_HPP */
