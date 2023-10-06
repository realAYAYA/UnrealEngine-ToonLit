// Copyright Epic Games, Inc. All Rights Reserved.

#include "Object/TextureShareCoreObject.h"
#include "Object/TextureShareCoreObjectContainers.h"

#include "IPC/TextureShareCoreInterprocessMemoryRegion.h"
#include "IPC/TextureShareCoreInterprocessMutex.h"
#include "IPC/TextureShareCoreInterprocessEvent.h"
#include "IPC/Containers/TextureShareCoreInterprocessMemory.h"
#include "IPC/TextureShareCoreInterprocessHelpers.h"

#include "Module/TextureShareCoreModule.h"
#include "Module/TextureShareCoreLog.h"

#include "Misc/ScopeLock.h"

using namespace UE::TextureShareCore;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreObject
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::LockThreadMutex(const ETextureShareThreadMutex InThreadMutex, bool bForceLockNoWait)
{
	TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> ThreadMutex = GetThreadMutex(InThreadMutex);
	if (!ThreadMutex.IsValid())
	{
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:LockThreadMutex(%s) Mutex object not exist"), *GetName(), GetTEXT(InThreadMutex));

		return false;
	}

	if (bForceLockNoWait)
	{
		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:LockThreadMutex(%s) ForceLock"), *GetName(), GetTEXT(InThreadMutex));

		return ThreadMutex->LockMutex(0);
	}

	UE_TS_LOG(LogTextureShareCoreObjectSync, VeryVerbose, TEXT("%s:LockThreadMutex(%s) try"), *GetName(), GetTEXT(InThreadMutex));

	if (ThreadMutex->LockMutex(SyncSettings.TimeoutSettings.ThreadMutexTimeout))
	{
		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:LockThreadMutex(%s)"), *GetName(), GetTEXT(InThreadMutex));

		return true;
	}

	// mutex deadlock
	UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:LockThreadMutex(%s) DEADLOCK"), *GetName(), GetTEXT(InThreadMutex));

	// Release mutex
	ThreadMutex->UnlockMutex();

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
void FTextureShareCoreObject::InitializeThreadMutexes()
{
	for (int32 Index = 0; Index < (uint8)ETextureShareThreadMutex::COUNT; Index++)
	{
		ThreadMutexes.Add(MakeShared<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe>());
		if (ThreadMutexes.Last().IsValid())
		{
			ThreadMutexes.Last()->Initialize();
		}
	}
}

TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> FTextureShareCoreObject::GetThreadMutex(const ETextureShareThreadMutex InThreadMutex)
{
	const uint8 ThreadMutexIndex = (uint8)InThreadMutex;
	if (ThreadMutexes.IsValidIndex(ThreadMutexIndex))
	{
		if (ThreadMutexes[ThreadMutexIndex].IsValid() && ThreadMutexes[ThreadMutexIndex]->IsValid())
		{
			return ThreadMutexes[ThreadMutexIndex];
		}
	}

	return nullptr;
}

void FTextureShareCoreObject::ResetThreadMutex(const ETextureShareThreadMutex InThreadMutex)
{
	TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> ThreadMutex = GetThreadMutex(InThreadMutex);
	if (ThreadMutex.IsValid())
	{
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:ResetThreadMutex(%s)"), *GetName(), GetTEXT(InThreadMutex));

		ThreadMutex->TryUnlockMutex();
	}
}

void FTextureShareCoreObject::ReleaseThreadMutexes()
{
	ThreadMutexes.Empty();
}
