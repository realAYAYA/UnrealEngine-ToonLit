// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)
#include "FunctionMap.h"
#include "Atomic.h"
#include "FastLock.h"
#include "LockGuard.h"
#include "Utils.h"
#include <stdlib.h>
#include <string.h>

#include "Containers/Array.h"

namespace AutoRTFM
{

FFunctionMap* AllocateFunctionMap(size_t Size)
{
    ASSERT(Size && !(Size & (Size - 1)));
    size_t TotalSize = offsetof(FFunctionMap, Entries) + sizeof(FFunctionEntry) * Size;
    FFunctionMap* Result = static_cast<FFunctionMap*>(malloc(TotalSize));
    Result->KeyCount = 0;
    Result->Size = Size;
    Result->IndexMask = Size - 1;
    memset(Result->Entries, 0, sizeof(FFunctionEntry) * Size);
    Fence(); // Really we just need a store-store fence...
    return Result;
}

void FunctionMapAddImpl(void* OldFunction, void* NewFunction)
{
    FFunctionMap* Map = GlobalData->FunctionMap;
    if (!Map)
    {
        Map = AllocateFunctionMap(1024);
        GlobalData->FunctionMap = Map;
    }
    else if (Map->KeyCount * 2 >= Map->Size)
    {
        size_t NewSize = Map->Size * 2;
        FFunctionMap* NewMap = AllocateFunctionMap(NewSize);
        for (size_t OldIndex = 0; OldIndex < Map->Size; OldIndex++)
        {
            FFunctionEntry* OldEntry = Map->Entries + OldIndex;
            if (!OldEntry->OldFunction)
            {
                continue;
            }
            // FIXME: Could have a secondary stronger hash here that is used on the second iteration
            // followed by +1 for the subsequent iterations.
            for (size_t Hash = FunctionPtrHash(OldEntry->OldFunction); ; ++Hash)
            {
                size_t NewIndex = Hash & NewMap->IndexMask;
                FFunctionEntry* NewEntry = NewMap->Entries + NewIndex;
                if (!NewEntry->OldFunction)
                {
                    *NewEntry = *OldEntry;
                    break;
                }
            }
        }
        NewMap->KeyCount = Map->KeyCount;
        GlobalData->FunctionMap = NewMap;
        Map = NewMap;
    }

    for (size_t Hash = FunctionPtrHash(OldFunction); ; ++Hash) {
        size_t Index = Hash & Map->IndexMask;
        FFunctionEntry* Entry = Map->Entries + Index;

        if (!Entry->OldFunction) {
            Entry->OldFunction = OldFunction;
            Entry->NewFunction = NewFunction;
            Map->KeyCount++;
            return;
        }
        
        if (Entry->OldFunction == OldFunction) {
            Entry->NewFunction = NewFunction;
            return;
        }
    }
}

void FunctionMapAdd(void* OldFunction, void* NewFunction)
{
    if (!OldFunction)
    {
        // Silently ignore null OldFunction to make weak linking easy.
        return;
    }

    InitializeGlobalDataIfNecessary();
    
    TLockGuard<FFastLock> LockGuard(GlobalData->FunctionMapLock);
    TArray<void*> Functions;
    Functions.Push(OldFunction);
    while (!Functions.IsEmpty())
    {
        void* Function = Functions.Last();
        Functions.Pop();
        FunctionMapAddImpl(Function, NewFunction);
    }
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
