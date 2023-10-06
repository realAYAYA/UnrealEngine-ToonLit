// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoRTFM/Utils.h"

namespace AutoRTFM
{

template<typename T, size_t PoolSize>
class TObjectPool
{
public:
    TObjectPool() = default;

    T* Allocate()
    {
        if (FFreeNode* FreeNode = NextFree)
        {
            T* Result = reinterpret_cast<T*>(FreeNode);
            NextFree = FreeNode->Next;
            return Result;
        }

        ASSERT(HighWatermark < PoolSize);
        return Lines + HighWatermark++;
    }

    void Deallocate(T* Line)
    {
        FFreeNode* FreeNode = reinterpret_cast<FFreeNode*>(Line);
        FreeNode->Next = NextFree;
        NextFree = FreeNode;
    }
    
private:
    struct FFreeNode
    {
        FFreeNode* Next;
    };
    
    T Lines[PoolSize];
    size_t HighWatermark{0};
    FFreeNode* NextFree{nullptr};
};

} // namespace AutoRTFM
