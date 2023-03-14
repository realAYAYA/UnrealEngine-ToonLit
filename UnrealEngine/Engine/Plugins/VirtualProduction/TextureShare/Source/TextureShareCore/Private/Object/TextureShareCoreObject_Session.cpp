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

#include "ITextureShareCoreCallbacks.h"

#include "Misc/ScopeLock.h"

//////////////////////////////////////////////////////////////////////////////////////////////
using namespace TextureShareCoreHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreObject
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::BeginSession()
{
	if (!IsSessionActive() && IsActive() && Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
	{
		if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
		{
			// Release dirty data for this share\this process
			InterprocessMemory->ReleaseDirtyObjects(GetObjectDesc());

			// Get ptr on a new IPC object memory region
			if (FTextureShareCoreInterprocessObject* InterprocessObject = InterprocessMemory->FindEmptyObject())
			{
				InterprocessObject->Initialize(GetObjectDesc(), SyncSettings);

				// New IPC object created
				bSessionActive = true;

				if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreBeginSession().IsBound())
				{
					ITextureShareCoreCallbacks::Get().OnTextureShareCoreBeginSession().Broadcast(*this);
				}

				UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:BeginSession()"), *GetName());
			}
		}

		Owner.UnlockInterprocessMemory();
	}

	return bSessionActive;
}

bool FTextureShareCoreObject::EndSession()
{
	bool bResult = false;

	if (IsSessionActive() && IsActive())
	{
		bSessionActive = false;

		// Release IPC
		if (Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
		{
			if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
			{
				// Get existing IPC object memory region
				if (FTextureShareCoreInterprocessObject* InterprocessObject = InterprocessMemory->FindObject(GetObjectDesc()))
				{
					// Reset IPC object to defaults
					InterprocessObject->Release();

					if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreEndSession().IsBound())
					{
						ITextureShareCoreCallbacks::Get().OnTextureShareCoreEndSession().Broadcast(*this);
					}

					UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:EndSession()"), *GetName());

					bResult = true;
				}
			}

			Owner.UnlockInterprocessMemory();
		}

		// Release resources and handles
		Owner.RemoveCachedResources(ObjectDesc);

		// Unlock and release thread mutexes
		ReleaseThreadMutexes();

		ReleaseSyncData();
		ReleaseData();
	}

	return bResult;
}

bool FTextureShareCoreObject::IsSessionActive() const
{
	return bSessionActive;
}
