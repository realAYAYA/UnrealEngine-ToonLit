// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/WorldSubsystem/TextureShareWorldSubsystemObject.h"
#include "Game/WorldSubsystem/TextureShareWorldSubsystemObjectProxy.h"
#include "Game/ViewExtension//TextureShareSceneViewExtension.h"

#include "Containers/TextureShareContainers.h"

#include "Module/TextureShareLog.h"
#include "Misc/TextureShareStrings.h"

#include "ITextureShare.h"
#include "ITextureShareAPI.h"
#include "ITextureShareObject.h"
#include "ITextureShareObjectProxy.h"

#include "Blueprints/TextureShareBlueprintContainers.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareWorldSubsystemObject
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareWorldSubsystemObject::FTextureShareWorldSubsystemObject(const FString& InProcessName, UTextureShareObject* InTextureShareObject)
	: TextureShareObject(InTextureShareObject)
{
	check(IsInGameThread());
	check(InTextureShareObject);

	const FString& InShareName = InTextureShareObject->Desc.GetTextureShareObjectName();

	static ITextureShareAPI& TextureShareAPIRef = ITextureShare::Get().GetTextureShareAPI();

	Object = TextureShareAPIRef.GetOrCreateObject(InShareName);
	if (Object.IsValid())
	{
		// Update settings
		// re-Initialize setup sync logic
		const FTextureShareObjectSyncSettings InSyncSettings = InTextureShareObject->Desc.Settings;
		{
			FTextureShareCoreSyncSettings SyncSetting = Object->GetSyncSetting();

			SyncSetting.TimeoutSettings.FrameBeginTimeOut = InSyncSettings.FrameConnectTimeOut;
			SyncSetting.TimeoutSettings.FrameSyncTimeOut = InSyncSettings.FrameSyncTimeOut;

			SyncSetting.FrameSyncSettings = Object->GetFrameSyncSettings(ETextureShareFrameSyncTemplate::Default);

			Object->SetSyncSetting(SyncSetting);
		}

		// Update process name (empty or equal values will be ignored)
		Object->SetProcessId(InProcessName);

		Object->BeginSession();
	}
}

bool FTextureShareWorldSubsystemObject::Tick(FViewport* InViewport)
{
	if (Object.IsValid()
		&& Object->BeginFrameSync()
		&& Object->IsFrameSyncActive())
	{
		// handle frame:
		UpdateFrameMarker();

		// Write Custom Data
		SendCustomData();

		if (Object->FrameSync(ETextureShareSyncStep::FramePreSetupBegin)
			&& Object->IsFrameSyncActive())
		{
			UpdateResourceRequests();

			// Read Custom Data
			ReceiveCustomData();

			Object->EndFrameSync(InViewport);

			// handle proxy frame:
			FTextureShareWorldSubsystemObjectProxy ProxyAPI(Object->GetProxy());

			return ProxyAPI.Update(TextureShareObject);
		}
	}

	return false;
}

bool FTextureShareWorldSubsystemObject::UpdateFrameMarker()
{
	if (Object.IsValid())
	{
		// Update frame marker for current frame
		Object->GetCoreData().FrameMarker.NextFrame();

		return true;
	}

	return false;
}

bool FTextureShareWorldSubsystemObject::UpdateResourceRequests()
{
	if (Object.IsValid())
	{
		for (const FTextureShareCoreObjectData& ObjectDataIt : Object->GetReceivedCoreObjectData())
		{
			for (const FTextureShareCoreResourceRequest& RequestIt : ObjectDataIt.Data.ResourceRequests)
			{
				// Add mapping on UE rendering
				const FTextureShareCoreViewDesc& ViewDesc = RequestIt.ResourceDesc.ViewDesc;
				switch (ViewDesc.EyeType)
				{
				case ETextureShareEyeType::StereoLeft:
					Object->GetData().Views.Add(ViewDesc, 0, EStereoscopicPass::eSSP_PRIMARY);
					break;
				case ETextureShareEyeType::StereoRight:
					Object->GetData().Views.Add(ViewDesc, 1, EStereoscopicPass::eSSP_SECONDARY);
					break;
				case ETextureShareEyeType::Default:
				default:
					Object->GetData().Views.Add(ViewDesc, INDEX_NONE, EStereoscopicPass::eSSP_FULL);
					break;
				}
			}
		}

		return true;
	}

	return false;
}

bool FTextureShareWorldSubsystemObject::SendCustomData()
{
	bool bResult = false;
	if (TextureShareObject && Object.IsValid())
	{
		const TMap<FString, FString>& InParameters = TextureShareObject->CustomData.SendParameters;
		if (!InParameters.IsEmpty())
		{
			bResult = true;
			for (const TPair<FString, FString>& ParamIt : InParameters)
			{
				Object->GetCoreData().CustomData.Add(FTextureShareCoreCustomData(ParamIt.Key, ParamIt.Value));
			}
		}
	}

	return bResult;
}

bool FTextureShareWorldSubsystemObject::ReceiveCustomData()
{
	bool bResult = false;
	if (TextureShareObject && Object.IsValid())
	{
		TMap<FString, FString>& OutParameters = TextureShareObject->CustomData.ReceivedParameters;
		OutParameters.Empty();

		for (const FTextureShareCoreObjectData& ObjectDataIt : Object->GetReceivedCoreObjectData())
		{
			bResult = true;
			for (const FTextureShareCoreCustomData& ParamIt : ObjectDataIt.Data.CustomData)
			{
				OutParameters.Emplace(ParamIt.Key, ParamIt.Value);
			}
		}
	}

	return bResult;
}
