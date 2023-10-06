// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace AutoRTFM
{

template<typename TLockType>
class TLockGuard
{
public:
    TLockGuard(TLockType& Lock)
        : Lock(Lock)
    {
        Lock.Lock();
    }

    ~TLockGuard()
    {
        Lock.Unlock();
    }

private:
    TLockType& Lock;
};

} // namespace AutoRTFM
