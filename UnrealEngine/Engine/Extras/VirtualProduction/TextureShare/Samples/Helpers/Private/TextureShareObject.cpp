// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureShareObject.h"
#include "Misc/TextureShareLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// ITextureShareObject
//////////////////////////////////////////////////////////////////////////////////////////////
ITextureShareObject* ITextureShareObject::CreateInstance(const FTextureShareObjectDesc& InObjectDesc)
{
	const wchar_t* InShareName = *InObjectDesc.ShareName;

	if (ITextureShareSDK::IsObjectExist(InShareName))
	{
		DEBUG_LOG(TEXT("Error: TextureShare with name '%s' already exist"), InShareName);

		return nullptr;
	}

	// Create new SDK object
	if (ITextureShareSDKObject* SDKObject = ITextureShareSDK::GetOrCreateObject(InShareName))
	{
		return new FTextureShareObject(SDKObject, InObjectDesc);
	}

	DEBUG_LOG(TEXT("Error: TextureShare '%s' SDK object not created"), InShareName);

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObject
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareObject::FTextureShareObject(ITextureShareSDKObject* InTextureShareSDKObject, const FTextureShareObjectDesc& InObjectDesc)
	: ObjectDesc(InObjectDesc), TextureShareSDKObject(InTextureShareSDKObject)
{
	// Set local process name
	TextureShareSDKObject->SetProcessId(*ObjectDesc.ProcessName);

	// Set render device type
	TextureShareSDKObject->SetDeviceType(ObjectDesc.DeviceType);

	// Initialize sync settings:
	{
		// control required min cnt of remote processes required for each frame
		SyncSettings.FrameConnectionSettings.MinValue = ObjectDesc.MinConnectionsCnt;

		// Load template sync template for UE BP-object
		TextureShareSDKObject->GetFrameSyncSettings(ETextureShareFrameSyncTemplate::SDK, *TDataOutput<FTextureShareCoreFrameSyncSettings>(SyncSettings.FrameSyncSettings));

		// Set sync settings
		TextureShareSDKObject->SetSyncSetting(TDataInput<FTextureShareCoreSyncSettings>(SyncSettings));
	}

	// Readback object desc
	TextureShareSDKObject->GetObjectDesc(*TDataOutput<FTextureShareCoreObjectDesc>(CoreObjectDesc));

	// And begin TS session for this object right now
	TextureShareSDKObject->BeginSession();

	DEBUG_LOG(TEXT("Created TextureShare '%s'"), TextureShareSDKObject->GetName());
}

FTextureShareObject::~FTextureShareObject()
{
	if (!ITextureShareSDK::RemoveObject(*ObjectDesc.ShareName))
	{
		DEBUG_LOG(TEXT("Error: TextureShare '%s' SDK object not deleted"), *ObjectDesc.ShareName);
	}
	else
	{
		DEBUG_LOG(TEXT("Removed TextureShare '%s'"), *ObjectDesc.ShareName);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareObject::UpdateSyncSettings()
{
	// Update sync from  resources request
	{
		TArraySerializable<ETextureShareSyncStep> UsedSyncSteps;
		for (const FTextureShareCoreResourceRequest& ResourceRequestIt : Data.ResourceRequests)
		{
			if (ResourceRequestIt.ResourceDesc.SyncStep != ETextureShareSyncStep::Undefined)
			{
				UsedSyncSteps.AddUnique(ResourceRequestIt.ResourceDesc.SyncStep);
			}
		}

		FTextureShareCoreFrameSyncSettings NewFrameSyncSettings = SyncSettings.FrameSyncSettings;
		NewFrameSyncSettings.Append(UsedSyncSteps);

		const bool bSyncStepsEqual = SyncSettings.FrameSyncSettings.EqualsFunc(NewFrameSyncSettings);
		if (!bSyncStepsEqual)
		{
			// custom sync setting. Auto call required syncs if needed
			SyncSettings.FrameSyncSettings = NewFrameSyncSettings;
			bUpdateSyncSettings = true;
		}
	}

	if (bUpdateSyncSettings)
	{
		bUpdateSyncSettings = false;
		TextureShareSDKObject->SetSyncSetting(TDataInput<FTextureShareCoreSyncSettings>(SyncSettings));
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
#if _DEBUG && TEXTURESHARESDK_DEBUG
#pragma comment( lib, "TextureShareSDK-Win64-Debug.lib" )
#else
#pragma comment( lib, "TextureShareSDK.lib" )
#endif

FIntPoint FIntPoint::ZeroValue;
FVector   FVector::ZeroVector;
FMatrix   FMatrix::Identity;
FRotator  FRotator::ZeroRotator;
//////////////////////////////////////////////////////////////////////////////////////////////
