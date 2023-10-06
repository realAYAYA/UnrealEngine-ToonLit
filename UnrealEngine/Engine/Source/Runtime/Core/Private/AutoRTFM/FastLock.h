// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>

namespace AutoRTFM
{

// This should eventually be a ParkingLot-based lock.
class FFastLock
{
public:
    FFastLock() = default;

    void Lock()
    {
        while (bIsHeld.exchange(true));
    }

    bool TryLock()
    {
        return !bIsHeld.exchange(true);
    }

    void Unlock()
    {
        bIsHeld.store(false);
    }

private:
    std::atomic<bool> bIsHeld{false};
};

} // namespace AutoRTFM
