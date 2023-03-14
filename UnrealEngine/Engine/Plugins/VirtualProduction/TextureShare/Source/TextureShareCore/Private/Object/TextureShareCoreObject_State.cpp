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
const FString& FTextureShareCoreObject::GetName() const
{
	return ObjectDesc.ShareName;
}

const FTextureShareCoreObjectDesc& FTextureShareCoreObject::GetObjectDesc() const
{
	return ObjectDesc;
}

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::IsActive() const
{
	return NotificationEvent.IsValid() && Owner.IsActive();
}

bool FTextureShareCoreObject::IsFrameSyncActive() const
{
	return IsSessionActive() && IsActive() && !FrameConnections.IsEmpty();
}


//////////////////////////////////////////////////////////////////////////////////////////////
const TArraySerializable<FTextureShareCoreObjectDesc>& FTextureShareCoreObject::GetConnectedInterprocessObjects() const
{
	return FrameConnections;
}
