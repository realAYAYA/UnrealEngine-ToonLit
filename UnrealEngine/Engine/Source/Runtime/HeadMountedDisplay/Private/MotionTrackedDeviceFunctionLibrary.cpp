// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionTrackedDeviceFunctionLibrary.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "IMotionController.h"
#include "IMotionTrackingSystemManagement.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for GetHandEnumForSourceName()
#include "IXRTrackingSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MotionTrackedDeviceFunctionLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogMotionTracking, Log, All);

UMotionTrackedDeviceFunctionLibrary::UMotionTrackedDeviceFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMotionTrackedDeviceFunctionLibrary::IsMotionTrackedDeviceCountManagementNecessary()
{
	return IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName());
}

void UMotionTrackedDeviceFunctionLibrary::SetIsControllerMotionTrackingEnabledByDefault(bool Enable)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.SetIsControllerMotionTrackingEnabledByDefault(Enable);
	}
}

int32 UMotionTrackedDeviceFunctionLibrary::GetMaximumMotionTrackedControllerCount()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.GetMaximumMotionTrackedControllerCount();
	} 
	else
	{
		return -1;
	}
}


int32 UMotionTrackedDeviceFunctionLibrary::GetMotionTrackingEnabledControllerCount()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.GetMotionTrackingEnabledControllerCount();
	}
	else
	{
		return -1;
	}
}

bool UMotionTrackedDeviceFunctionLibrary::EnableMotionTrackingOfDevice(int32 PlayerIndex, EControllerHand Hand)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.EnableMotionTrackingOfDevice(PlayerIndex, Hand);
	}
	else
	{
		// Return true if TrackingManagement is not available.  
		// It isn't available because we don't need to manage it like this, because everything is always tracked.
		return true;
	}
}

bool UMotionTrackedDeviceFunctionLibrary::EnableMotionTrackingOfSource(int32 PlayerIndex, FName SourceName)
{
	EControllerHand Hand = EControllerHand::Special_11;
	if (FXRMotionControllerBase::GetHandEnumForSourceName(SourceName, Hand))
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
		{
			IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
			return MotionTrackingSystemManagement.EnableMotionTrackingOfDevice(PlayerIndex, Hand);
		}
	}

	// Return true if TrackingManagement is not available.  
	// It isn't available because we don't need to manage it like this, because everything is always tracked.
	return true;
}

bool UMotionTrackedDeviceFunctionLibrary::EnableMotionTrackingForComponent(UMotionControllerComponent* MotionControllerComponent)
{
	if (MotionControllerComponent == nullptr)
	{
		return false;
	}
	return EnableMotionTrackingOfSource(MotionControllerComponent->PlayerIndex, MotionControllerComponent->MotionSource);
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingOfDevice(int32 PlayerIndex, EControllerHand Hand)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		MotionTrackingSystemManagement.DisableMotionTrackingOfDevice(PlayerIndex, Hand);
	}
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingOfSource(int32 PlayerIndex, FName SourceName)
{
	EControllerHand Hand = EControllerHand::Special_11;
	if (FXRMotionControllerBase::GetHandEnumForSourceName(SourceName, Hand))
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
		{
			IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
			MotionTrackingSystemManagement.DisableMotionTrackingOfDevice(PlayerIndex, Hand);
		}
	}
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingForComponent(const UMotionControllerComponent* MotionControllerComponent)
{
	if (MotionControllerComponent == nullptr)
	{
		return;
	}
	DisableMotionTrackingOfSource(MotionControllerComponent->PlayerIndex, MotionControllerComponent->MotionSource);
}

bool UMotionTrackedDeviceFunctionLibrary::IsMotionTrackingEnabledForDevice(int32 PlayerIndex, EControllerHand Hand)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.IsMotionTrackingEnabledForDevice(PlayerIndex, Hand);
	}
	else
	{
		// Return true if TrackingManagement is not available.  
		// It isn't available because we don't need to manage it like this, because everything is always tracked.
		return true;
	}
}

bool UMotionTrackedDeviceFunctionLibrary::IsMotionTrackingEnabledForSource(int32 PlayerIndex, FName SourceName)
{
	EControllerHand Hand = EControllerHand::Special_11;
	if (FXRMotionControllerBase::GetHandEnumForSourceName(SourceName, Hand))
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
		{
			IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
			return MotionTrackingSystemManagement.IsMotionTrackingEnabledForDevice(PlayerIndex, Hand);
		}
	}

	// Return true if TrackingManagement is not available.  
	// It isn't available because we don't need to manage it like this, because everything is always tracked.
	return true;
}

bool UMotionTrackedDeviceFunctionLibrary::IsMotionTrackingEnabledForComponent(const UMotionControllerComponent* MotionControllerComponent)
{
	if (MotionControllerComponent == nullptr)
	{
		return false;
	}
	return IsMotionTrackingEnabledForSource(MotionControllerComponent->PlayerIndex, MotionControllerComponent->MotionSource);
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingOfAllControllers()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.DisableMotionTrackingOfAllControllers();
	}
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingOfControllersForPlayer(int32 PlayerIndex)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.DisableMotionTrackingOfControllersForPlayer(PlayerIndex);
	}
}

TArray<FName> UMotionTrackedDeviceFunctionLibrary::EnumerateMotionSources()
{
	TArray<FName> SourceList;

	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (IMotionController* MotionController : MotionControllers)
	{
		if (MotionController)
		{
			TArray<FMotionControllerSource> MotionSources;
			MotionController->EnumerateSources(MotionSources);

			SourceList.Reserve(SourceList.Num() + MotionSources.Num());
			for (const FMotionControllerSource& Source : MotionSources)
			{
				SourceList.AddUnique(Source.SourceName);
			}
		}
	}

	return SourceList;
}

FName UMotionTrackedDeviceFunctionLibrary::GetActiveTrackingSystemName()
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	return TrackingSys ? TrackingSys->GetSystemName() : NAME_None;
}

bool UMotionTrackedDeviceFunctionLibrary::IsMotionSourceTracking(int32 PlayerIndex, FName SourceName)
{
	bool bIsTracking = false;

	if (SourceName == FXRMotionControllerBase::HMDSourceId)
	{
		IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
		if (TrackingSys)
		{
			bIsTracking = TrackingSys->IsTracking(IXRTrackingSystem::HMDDeviceId);
		}
	}
	else
	{
		TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
		for (IMotionController* MotionController : MotionControllers)
		{
			if (MotionController)
			{
				if (MotionController->GetControllerTrackingStatus(PlayerIndex, SourceName) != ETrackingStatus::NotTracked)
				{
					bIsTracking = true;
					break;
				}
			}
		}
	}

	return bIsTracking;
}

