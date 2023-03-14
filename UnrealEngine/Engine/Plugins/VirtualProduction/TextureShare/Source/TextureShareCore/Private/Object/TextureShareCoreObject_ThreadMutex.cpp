// Copyright Epic Games, Inc. All Rights Reserved.

#include "Object/TextureShareCoreObject.h"
#include "Object/TextureShareCoreObjectContainers.h"

#include "IPC/TextureShareCoreInterprocessMemoryRegion.h"
#include "IPC/TextureShareCoreInterprocessMutex.h"
#include "IPC/TextureShareCoreInterprocessEvent.h"
#include "IPC/Containers/TextureShareCoreInterprocessMemory.h"

#include "Core/TextureShareCoreHelpers.h"

#include "Module/TextureShareCoreModule.h"
#include "Module/TextureShareCoreLog.h"

#include "Misc/ScopeLock.h"

//////////////////////////////////////////////////////////////////////////////////////////////
using namespace TextureShareCoreHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreObject
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::LockThreadMutex(const ETextureShareThreadMutex InThreadMutex)
{
	TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> ThreadMutex = GetThreadMutex(InThreadMutex);
	if (ThreadMutex.IsValid())
	{
		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:LockThreadMutex(%s) try"), *GetName(), GetTEXT(InThreadMutex));

		if (ThreadMutex->LockMutex(SyncSettings.TimeoutSettings.ThreadMutexTimeout))
		{
			UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:LockThreadMutex(%s) enter"), *GetName(), GetTEXT(InThreadMutex));

			return true;
		}

		// mutex deadlock
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:LockThreadMutex(%s) deadlock"), *GetName(), GetTEXT(InThreadMutex));

		// Release mutex
		ThreadMutex->UnlockMutex();

		return false;
	}

	return false;

}

bool FTextureShareCoreObject::UnlockThreadMutex(const ETextureShareThreadMutex InThreadMutex)
{
	TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> ThreadMutex = GetThreadMutex(InThreadMutex);
	if (ThreadMutex.IsValid())
	{
		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:UnlockThreadMutex(%s)"), *GetName(), GetTEXT(InThreadMutex));

		ThreadMutex->UnlockMutex();

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> FTextureShareCoreObject::GetThreadMutex(const ETextureShareThreadMutex InThreadMutex)
{
	check(InThreadMutex != ETextureShareThreadMutex::COUNT);

	if (ThreadMutexMap.IsEmpty())
	{
		// Create thread mutex for local process
		for (int32 Index = 0; Index < (uint8)ETextureShareThreadMutex::COUNT; Index++)
		{
			TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> NewMemoryMutex = MakeShared<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe>();
			if (NewMemoryMutex.IsValid() && NewMemoryMutex->Initialize())
			{
				ThreadMutexMap.Emplace((ETextureShareThreadMutex)Index, NewMemoryMutex);
			}
		}
	}

	TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe>* Exist = ThreadMutexMap.Find(InThreadMutex);

	return Exist ? *Exist : nullptr;
}

void FTextureShareCoreObject::ResetThreadMutexes()
{
	for (TPair<ETextureShareThreadMutex, TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe>>& ThreadMutexIt : ThreadMutexMap)
	{
		if (ThreadMutexIt.Value.IsValid())
		{
			ThreadMutexIt.Value->TryUnlockMutex();
		}
	}
}

void FTextureShareCoreObject::ReleaseThreadMutexes()
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:ReleaseThreadMutexes"), *GetName());

	ResetThreadMutexes();
	ThreadMutexMap.Empty();
}
