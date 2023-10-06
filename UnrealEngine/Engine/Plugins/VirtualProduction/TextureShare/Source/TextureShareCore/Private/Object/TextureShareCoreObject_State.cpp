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
const FString& FTextureShareCoreObject::GetName() const
{
	return ObjectDesc.ShareName;
}

const FTextureShareCoreObjectDesc& FTextureShareCoreObject::GetObjectDesc() const
{
	return ObjectDesc;
}

const FTextureShareCoreObjectDesc& FTextureShareCoreObject::GetObjectDesc_RenderThread() const
{
	return ObjectDesc;
}
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::IsActive() const
{
	return NotificationEvent.IsValid() && Owner.IsActive();
}

bool FTextureShareCoreObject::IsActive_RenderThread() const
{
	return NotificationEvent.IsValid() && Owner.IsActive();
}

bool FTextureShareCoreObject::IsFrameSyncActive() const
{
	return IsSessionActive() && IsActive() && !FrameConnections.IsEmpty();
}

bool FTextureShareCoreObject::IsFrameSyncActive_RenderThread() const
{
	return IsSessionActive() && IsActive_RenderThread() && !FrameConnections.IsEmpty();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::IsBeginFrameSyncActive() const
{
	switch (FrameSyncState)
	{
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyEnd:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameEnd:
	case ETextureShareCoreInterprocessObjectFrameSyncState::Undefined:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameSyncLost:
		break;

	default:
		// logic broken
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:BeginFrameSync() - frame logic broken = %s"), *GetName(), GetTEXT(FrameSyncState));

		return false;
	}

	return true;
}

bool FTextureShareCoreObject::IsBeginFrameSyncActive_RenderThread() const
{
	if (FrameConnections.IsEmpty())
	{
		UE_TS_LOG(LogTextureShareCoreProxyObjectSync, Log, TEXT("%s:IsBeginFrameSyncActive_RenderThread() - canceled: no connections"), *GetName());

		return false;
	}

	switch (FrameSyncState)
	{
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyEnd:
		// logic broken
		UE_TS_LOG(LogTextureShareCoreProxyObjectSync, Error, TEXT("%s:BeginFrameProxySync() - frame logic broken = %s"), *GetName(), GetTEXT(FrameSyncState));

		return false;

	default:
		break;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
const TArraySerializable<FTextureShareCoreObjectDesc>& FTextureShareCoreObject::GetConnectedInterprocessObjects() const
{
	return FrameConnections;
}
