// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <map>
#include <mutex>
#include <vector>

namespace gte
{

template <typename Key, typename Value>
class ThreadSafeMap
{
public:
    // Construction and destruction.
    virtual ~ThreadSafeMap();
    ThreadSafeMap();

    // All the operations are thread-safe.
    bool HasElements() const;
    bool Exists(Key key) const;
    void Insert(Key key, Value value);
    bool Remove(Key key, Value& value);
    void RemoveAll();
    bool Get(Key key, Value& value) const;
    void GatherAll(std::vector<Value>& values) const;

protected:
    std::map<Key,Value> mMap;
    mutable std::mutex mMutex;
};


template <typename Key, typename Value>
ThreadSafeMap<Key, Value>::~ThreadSafeMap()
{
}

template <typename Key, typename Value>
ThreadSafeMap<Key, Value>::ThreadSafeMap()
{
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::HasElements() const
{
    bool hasElements;
    mMutex.lock();
    {
        hasElements = (mMap.size() > 0);
    }
    mMutex.unlock();
    return hasElements;
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::Exists(Key key) const
{
    bool exists;
    mMutex.lock();
    {
        exists = (mMap.find(key) != mMap.end());
    }
    mMutex.unlock();
    return exists;
}

template <typename Key, typename Value>
void ThreadSafeMap<Key, Value>::Insert(Key key, Value value)
{
    mMutex.lock();
    {
        mMap[key] = value;
    }
    mMutex.unlock();
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::Remove(Key key, Value& value)
{
    bool exists;
    mMutex.lock();
    {
        auto iter = mMap.find(key);
        if (iter != mMap.end())
        {
            value = iter->second;
            mMap.erase(iter);
            exists = true;
        }
        else
        {
            exists = false;
        }
    }
    mMutex.unlock();
    return exists;
}

template <typename Key, typename Value>
void ThreadSafeMap<Key, Value>::RemoveAll()
{
    mMutex.lock();
    {
        mMap.clear();
    }
    mMutex.unlock();
}

template <typename Key, typename Value>
bool ThreadSafeMap<Key, Value>::Get(Key key, Value& value) const
{
    bool exists;
    mMutex.lock();
    {
        auto iter = mMap.find(key);
        if (iter != mMap.end())
        {
            value = iter->second;
            exists = true;
        }
        else
        {
            exists = false;
        }
    }
    mMutex.unlock();
    return exists;
}

template <typename Key, typename Value>
void ThreadSafeMap<Key, Value>::GatherAll(std::vector<Value>& values) const
{
    mMutex.lock();
    {
        if (mMap.size() > 0)
        {
            values.resize(mMap.size());
            auto viter = values.begin();
            for (auto const& m : mMap)
            {
                *viter++ = m.second;
            }
        }
        else
        {
            values.clear();
        }
    }
    mMutex.unlock();
}


}
