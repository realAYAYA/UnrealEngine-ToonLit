// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/ParkingLot.h"

namespace UE
{

/**
 * A one-byte portable condition variable. Gives the same decent performance everywhere.
 */
class FConditionVariable final
{
public:
    constexpr FConditionVariable() = default;

    FConditionVariable(const FConditionVariable&) = delete;
    FConditionVariable& operator=(const FConditionVariable&) = delete;

    void NotifyOne()
    {
        if (bHasWaiters)
        {
            ParkingLot::WakeOne(
                &bHasWaiters,
                [this] (ParkingLot::FWakeState WakeState) -> uint64
                {
                    if (!WakeState.bHasWaitingThreads)
                        bHasWaiters = false;
                    return 0;
                });
        }
    }
    
    void NotifyAll()
    {
        if (bHasWaiters)
        {
            bHasWaiters = false;
            ParkingLot::WakeAll(&bHasWaiters);
        }
    }

    template<typename TLock>
    void Wait(TLock& Lock)
    {
        ParkingLot::Wait(
            &bHasWaiters,
            [this] () -> bool
            {
                bHasWaiters = true;
                return true;
            },
            [&Lock] ()
            {
                Lock.Unlock();
            });
        Lock.Lock();
    }
    
private:
    std::atomic<bool> bHasWaiters = false;
};

} // namespace UE


