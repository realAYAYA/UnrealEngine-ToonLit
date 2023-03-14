// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRAssetFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h" // for LogHMD
#include "IXRSystemAssets.h"
#include "Components/PrimitiveComponent.h"
#include "Features/IModularFeatures.h"
#include "XRMotionControllerBase.h" // for GetHandEnumForSourceName()

#include UE_INLINE_GENERATED_CPP_BY_NAME(XRAssetFunctionLibrary)

/* XRAssetFunctionLibrary_Impl
 *****************************************************************************/

namespace XRAssetFunctionLibrary_Impl
{
	static UPrimitiveComponent* AddDeviceVisualizationComponent(AActor* Target, const FXRDeviceId& XRDeviceId, bool bManualAttachment, const FTransform& RelativeTransformbool, const bool bForceSynchronous = false, const FXRComponentLoadComplete& OnLoadComplete = FXRComponentLoadComplete());
	static UPrimitiveComponent* AddNamedDeviceVisualizationComponent(AActor* Target, const FName SystemName, const FName DeviceName, bool bManualAttachment, const FTransform& RelativeTransform, FXRDeviceId& XRDeviceIdOut, const bool bForceSynchronous = false, const FXRComponentLoadComplete& OnLoadComplete = FXRComponentLoadComplete());
	static UPrimitiveComponent* SpawnDeviceComponent(AActor* Target, IXRSystemAssets* AssetSystem, int32 DeviceId, bool bManualAttachment, const FTransform& RelativeTransform, const bool bForceSynchronous, const FXRComponentLoadComplete& OnLoadComplete);
}

static UPrimitiveComponent* XRAssetFunctionLibrary_Impl::AddDeviceVisualizationComponent(
	AActor*							Target, 
	const FXRDeviceId&				XRDeviceId, 
	bool							bManualAttachment, 
	const FTransform&				RelativeTransform,
	const bool						bForceSynchronous,
	const FXRComponentLoadComplete& OnLoadComplete)
{
	UPrimitiveComponent* NewComponent = nullptr;

	if (!IsValid(Target))
	{
		UE_LOG(LogHMD, Warning, TEXT("The target actor is invalid. Therefore you're unable to add a device render component to it."));
		return nullptr;
	}

	TArray<IXRSystemAssets*> XRAssetSystems = IModularFeatures::Get().GetModularFeatureImplementations<IXRSystemAssets>(IXRSystemAssets::GetModularFeatureName());
	for (IXRSystemAssets* AssetSys : XRAssetSystems)
	{
		if (!XRDeviceId.IsOwnedBy(AssetSys))
		{
			continue;
		}

		NewComponent = SpawnDeviceComponent(Target, AssetSys, XRDeviceId.DeviceId, bManualAttachment, RelativeTransform, bForceSynchronous, OnLoadComplete);
		if (NewComponent == nullptr)
		{
			UE_LOG(LogHMD, Warning, TEXT("The specified XR device does not have an associated render model."));
		}
		break;
	}

	if (NewComponent == nullptr)
	{
		UE_LOG(LogHMD, Warning, TEXT("Failed to find an active XR system with a model for the requested device."));
	}

	return NewComponent;
}

static UPrimitiveComponent* XRAssetFunctionLibrary_Impl::AddNamedDeviceVisualizationComponent(
	AActor*							Target, 
	const FName						SystemName, 
	const FName						DeviceName, 
	bool							bManualAttachment, 
	const FTransform&				RelativeTransform,
	FXRDeviceId&					XRDeviceIdOut,
	const bool						bForceSynchronous, 
	const FXRComponentLoadComplete& OnLoadComplete)
{
	UPrimitiveComponent* NewComponent = nullptr;

	EControllerHand HandId = EControllerHand::Special_11;
	if (FXRMotionControllerBase::GetHandEnumForSourceName(DeviceName, HandId))
	{
		TArray<IXRSystemAssets*> XRAssetSystems = IModularFeatures::Get().GetModularFeatureImplementations<IXRSystemAssets>(IXRSystemAssets::GetModularFeatureName());
		for (IXRSystemAssets* AssetSys : XRAssetSystems)
		{
			if (SystemName.IsNone() || AssetSys->GetSystemName() == SystemName)
			{
				const int32 DeviceId = AssetSys->GetDeviceId(HandId);

				NewComponent = SpawnDeviceComponent(Target, AssetSys, DeviceId, bManualAttachment, RelativeTransform, bForceSynchronous, OnLoadComplete);
				if (NewComponent)
				{
					XRDeviceIdOut = FXRDeviceId(AssetSys, DeviceId);
					break;
				}
			}
		}
	}

	if (NewComponent == nullptr)
	{
		UE_LOG(LogHMD, Warning, TEXT("Failed to find an active XR system with a model for the requested device: %s."), *DeviceName.ToString());
	}
	return NewComponent;
}

static UPrimitiveComponent* XRAssetFunctionLibrary_Impl::SpawnDeviceComponent(
	AActor*							Target, 
	IXRSystemAssets*				AssetSystem, 
	int32							DeviceId, 
	bool							bManualAttachment, 
	const FTransform&				RelativeTransform, 
	const bool						bForceSynchronous, 
	const FXRComponentLoadComplete& OnLoadComplete)
{
	if (!IsValid(Target))
	{
		UE_LOG(LogHMD, Warning, TEXT("The target actor is invalid. Therefore you're unable to add a device render component to it."));
		return nullptr;
	}

	UPrimitiveComponent* DeviceProxy = AssetSystem->CreateRenderComponent(DeviceId, Target, RF_StrongRefOnFrame, bForceSynchronous, OnLoadComplete);
	if (DeviceProxy)
	{
		DeviceProxy->RegisterComponent();

		if (!bManualAttachment)
		{
			USceneComponent* RootComponent = Target->GetRootComponent();
			if (RootComponent == nullptr)
			{
				Target->SetRootComponent(DeviceProxy);
			}
			else
			{
				DeviceProxy->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
		DeviceProxy->SetRelativeTransform(RelativeTransform);
	}
	return DeviceProxy;
}

/* UXRAssetFunctionLibrary
 *****************************************************************************/

UXRAssetFunctionLibrary::UXRAssetFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPrimitiveComponent* UXRAssetFunctionLibrary::AddDeviceVisualizationComponentBlocking(AActor* Target, const FXRDeviceId& XRDeviceId, bool bManualAttachment, const FTransform& RelativeTransform)
{
	UPrimitiveComponent* NewComponent = XRAssetFunctionLibrary_Impl::AddDeviceVisualizationComponent(Target, XRDeviceId, bManualAttachment, RelativeTransform, /*bForceSynchronous =*/true);
	return NewComponent;
}

UPrimitiveComponent* UXRAssetFunctionLibrary::AddNamedDeviceVisualizationComponentBlocking(AActor* Target, const FName SystemName, const FName DeviceName, bool bManualAttachment, const FTransform& RelativeTransform, FXRDeviceId& XRDeviceId)
{
	UPrimitiveComponent* NewComponent = XRAssetFunctionLibrary_Impl::AddNamedDeviceVisualizationComponent(Target, SystemName, DeviceName, bManualAttachment, RelativeTransform, XRDeviceId, /*bForceSynchronous =*/true);
	return NewComponent;
}

/* UAsyncTask_LoadXRDeviceVisComponent
 *****************************************************************************/

UAsyncTask_LoadXRDeviceVisComponent::UAsyncTask_LoadXRDeviceVisComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

UAsyncTask_LoadXRDeviceVisComponent* UAsyncTask_LoadXRDeviceVisComponent::AddDeviceVisualizationComponentAsync(AActor* Target, const FXRDeviceId& XRDeviceId, bool bManualAttachment, const FTransform& RelativeTransform, UPrimitiveComponent*& NewComponent)
{
	UAsyncTask_LoadXRDeviceVisComponent* NewTask = NewObject<UAsyncTask_LoadXRDeviceVisComponent>();

	FXRComponentLoadComplete LoadCompleteDelegate = FXRComponentLoadComplete::CreateLambda([NewTask](UPrimitiveComponent* Component)
	{
		NewTask->LoadStatus = Component ? ELoadStatus::LoadSuccess : ELoadStatus::LoadFailure;
		if (NewTask->SpawnedComponent != nullptr)
		{
			NewTask->OnLoadComplete(Component != nullptr);
		}
	});
	NewComponent = NewTask->SpawnedComponent = XRAssetFunctionLibrary_Impl::AddDeviceVisualizationComponent(Target, XRDeviceId, bManualAttachment, RelativeTransform, /*bForceSynchronous =*/false, LoadCompleteDelegate);

	return NewTask;
}

UAsyncTask_LoadXRDeviceVisComponent* UAsyncTask_LoadXRDeviceVisComponent::AddNamedDeviceVisualizationComponentAsync(AActor* Target, const FName SystemName, const FName DeviceName, bool bManualAttachment, const FTransform& RelativeTransform, FXRDeviceId& XRDeviceId, UPrimitiveComponent*& NewComponent)
{
	UAsyncTask_LoadXRDeviceVisComponent* NewTask = NewObject<UAsyncTask_LoadXRDeviceVisComponent>();

	FXRComponentLoadComplete LoadCompleteDelegate = FXRComponentLoadComplete::CreateLambda([NewTask](UPrimitiveComponent* Component)
	{
		NewTask->LoadStatus = Component ? ELoadStatus::LoadSuccess : ELoadStatus::LoadFailure;
		if (NewTask->SpawnedComponent != nullptr)
		{
			NewTask->OnLoadComplete(Component != nullptr);
		}
	});
	NewComponent = NewTask->SpawnedComponent = XRAssetFunctionLibrary_Impl::AddNamedDeviceVisualizationComponent(Target, SystemName, DeviceName, bManualAttachment, RelativeTransform, XRDeviceId, /*bForceSynchronous =*/false, LoadCompleteDelegate);

	return NewTask;
}

void UAsyncTask_LoadXRDeviceVisComponent::Activate()
{
	Super::Activate();
	if (LoadStatus != ELoadStatus::Pending || SpawnedComponent == nullptr)
	{
		OnLoadComplete(LoadStatus == ELoadStatus::LoadSuccess);
	}
}

void UAsyncTask_LoadXRDeviceVisComponent::OnLoadComplete(bool bSuccess)
{
	if (bSuccess)
	{
		OnModelLoaded.Broadcast(SpawnedComponent);
	}
	else
	{
		OnLoadFailure.Broadcast(SpawnedComponent);
	}
	SetReadyToDestroy();
}

