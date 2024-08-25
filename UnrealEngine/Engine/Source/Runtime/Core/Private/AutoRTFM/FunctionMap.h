// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalData.h"

namespace AutoRTFM
{

class FContext;

// The compiler has to agree to this.
inline uintptr_t FunctionPtrHash(void* functionPtr)
{
    // FIXME: We'll need something better.
    return reinterpret_cast<uintptr_t>(functionPtr) / 4;
}

struct FFunctionEntry
{
    FFunctionEntry() = default;

    FFunctionEntry(void* OldFunction, void* NewFunction)
        : OldFunction(OldFunction), NewFunction(NewFunction) { }
    
    void* OldFunction{nullptr};
    void* NewFunction{nullptr};
};

struct FFunctionMap {
    size_t KeyCount;
    size_t Size;
    size_t IndexMask;
    FFunctionEntry Entries[1];
};

void FunctionMapAdd(void* OldFunction, void* NewFuncton);

inline void* FunctionMapTryLookup(void* OldFunction)
{
    FFunctionMap* Map = GlobalData->FunctionMap;

    if (!Map)
        return nullptr;

    for (size_t Hash = FunctionPtrHash(OldFunction); ; ++Hash) {
        size_t Index = Hash & Map->IndexMask;
        FFunctionEntry* Entry = Map->Entries + Index;

        if (Entry->OldFunction == OldFunction)
        {
            // This races with Add, but in a benign way. If _OldFunction is set and matches our OldFunction but the _NewFunction
            // is nullptr, then it means that an Add is happening right now, and before the Add, this entry would have been nullptr.
            return Entry->NewFunction;
        }

        if (!Entry->OldFunction)
        {
            return nullptr;
        }
    }
}

template<typename TReturnType, typename... TParameterTypes>
auto FunctionMapTryLookup(TReturnType (*Function)(TParameterTypes...)) -> TReturnType (*)(TParameterTypes...)
{
    return reinterpret_cast<TReturnType (*)(TParameterTypes...)>(FunctionMapTryLookup(reinterpret_cast<void*>(Function)));
}

} // namespace AutoRTFM
