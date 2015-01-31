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

public:
    // NOTE: SparseArray iterators operate sequentially over the deque, rather
    // than the user index map. This means it's possible for index n to be
    // encountered before index n-1. It's done this way to reduce cache misses,
    // since following the user index ordering could cause data lookups to jump
    // forward and backward within the deque.
    //
    // The iterator acts as a pair, with first=index, second=element
    class iterator {
        typedef typename list_type::iterator iter_type;

        list_type *mContainer;
        iter_type mIter;

    public:
        iterator() { }
        iterator(list_type *cont, iter_type&& iter)
          : mContainer(cont), mIter(std::move(iter))
        {
            while(mIter != mContainer->end() && mIter->first == std::numeric_limits<size_t>::max())
                ++mIter;
        }

        bool operator==(const iterator &rhs) const
        { return mIter == rhs.mIter; }
        bool operator!=(const iterator &rhs) const
        { return mIter != rhs.mIter; }

        pair_type& operator*() { return *mIter; }
        pair_type* operator->() { return &*mIter; }

        iterator& operator++()
        {
            do {
                ++mIter;
            } while(mIter != mContainer->end() && mIter->first == std::numeric_limits<size_t>::max());
            return *this;
        }
        iterator operator++(int)
        {
            iterator old = *this;
            do {
                ++mIter;
            } while(mIter != mContainer->end() && mIter->first == std::numeric_limits<size_t>::max());
            return old;
        }
    };
    class const_iterator {
        typedef typename list_type::const_iterator iter_type;

        const list_type *mContainer;
        iter_type mIter;

    public:
        const_iterator() { }
        const_iterator(const list_type *cont, iter_type&& iter)
          : mContainer(cont), mIter(std::move(iter))
        {
            while(mIter != mContainer->cend() && mIter->first == std::numeric_limits<size_t>::max())
                ++mIter;
        }

        bool operator==(const const_iterator &rhs) const
        { return mIter == rhs.mIter; }
        bool operator!=(const const_iterator &rhs) const
        { return mIter != rhs.mIter; }

        const pair_type& operator*() { return *mIter; }
        const pair_type* operator->() { return &*mIter; }

        const_iterator& operator++()
        {
            do {
                ++mIter;
            } while(mIter != mContainer->cend() && mIter->first == std::numeric_limits<size_t>::max());
            return *this;
        }
        const_iterator operator++(int)
        {
            const_iterator old = *this;
            do {
                ++mIter;
            } while(mIter != mContainer->cend() && mIter->first == std::numeric_limits<size_t>::max());
            return old;
        }
    };


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
};

} // namespace TK

#endif /* SPARSEARRAY_HPP */
