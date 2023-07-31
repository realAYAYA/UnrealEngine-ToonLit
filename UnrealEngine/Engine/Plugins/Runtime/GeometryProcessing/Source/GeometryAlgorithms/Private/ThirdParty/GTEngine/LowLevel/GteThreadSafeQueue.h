// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <mutex>
#include <queue>

namespace gte
{

template <typename Element>
class ThreadSafeQueue
{
public:
    // Construction and destruction.
    virtual ~ThreadSafeQueue();
    ThreadSafeQueue(size_t maxNumElements = 0);

    // All the operations are thread-safe.
    size_t GetMaxNumElements() const;
    size_t GetNumElements() const;
    bool Push(Element const& element);
    bool Pop(Element& element);

protected:
    size_t mMaxNumElements;
    std::queue<Element> mQueue;
    mutable std::mutex mMutex;
};


template <typename Element>
ThreadSafeQueue<Element>::~ThreadSafeQueue()
{
}

template <typename Element>
ThreadSafeQueue<Element>::ThreadSafeQueue(size_t maxNumElements)
    :
    mMaxNumElements(maxNumElements)
{
}

template <typename Element>
size_t ThreadSafeQueue<Element>::GetMaxNumElements() const
{
    size_t maxNumElements;
    mMutex.lock();
    {
        maxNumElements = mMaxNumElements;
    }
    mMutex.unlock();
    return maxNumElements;
}

template <typename Element>
size_t ThreadSafeQueue<Element>::GetNumElements() const
{
    size_t numElements;
    mMutex.lock();
    {
        numElements = mQueue.size();
    }
    mMutex.unlock();
    return numElements;
}

template <typename Element>
bool ThreadSafeQueue<Element>::Push(Element const& element)
{
    bool pushed;
    mMutex.lock();
    {
        if (mQueue.size() < mMaxNumElements)
        {
            mQueue.push(element);
            pushed = true;
        }
        else
        {
            pushed = false;
        }
    }
    mMutex.unlock();
    return pushed;
}

template <typename Element>
bool ThreadSafeQueue<Element>::Pop(Element& element)
{
    bool popped;
    mMutex.lock();
    {
        if (mQueue.size() > 0)
        {
            element = mQueue.front();
            mQueue.pop();
            popped = true;
        }
        else
        {
            popped = false;
        }
    }
    mMutex.unlock();
    return popped;
}


}
