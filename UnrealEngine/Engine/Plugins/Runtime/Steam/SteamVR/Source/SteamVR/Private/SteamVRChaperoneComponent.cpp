// Copyright Epic Games, Inc. All Rights Reserved.
//
#include "SteamVRChaperoneComponent.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"
#include "Engine/Engine.h"

USteamVRChaperoneComponent::USteamVRChaperoneComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;

	bTickInEditor = true;
	bAutoActivate = true;

	bWasInsideBounds = true;
}

void USteamVRChaperoneComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if STEAMVR_SUPPORTED_PLATFORMS
	IXRTrackingSystem* ActiveXRSystem = GEngine->XRSystem.Get();

	if (ActiveXRSystem && ActiveXRSystem->GetSystemName() == FSteamVRHMD::SteamSystemName)
	{
		FSteamVRHMD* SteamVRHMD = (FSteamVRHMD*)(ActiveXRSystem);
		if (SteamVRHMD->IsStereoEnabled())
		{
			bool bInBounds = SteamVRHMD->IsInsideBounds();

			if (bInBounds != bWasInsideBounds)
			{
				if (bInBounds)
				{
					OnReturnToBounds.Broadcast();
				}
				else
				{
					OnLeaveBounds.Broadcast();
				}
			}

			bWasInsideBounds = bInBounds;
		}
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS
}

TArray<FVector> USteamVRChaperoneComponent::GetBounds() const
{
	TArray<FVector> RetValue;

#if STEAMVR_SUPPORTED_PLATFORMS
	if (GEngine->XRSystem.IsValid())
	{
		IXRTrackingSystem* XRSystem = GEngine->XRSystem.Get();
		if (XRSystem->GetSystemName() == FSteamVRHMD::SteamSystemName)
		{
			FSteamVRHMD* SteamVRHMD = (FSteamVRHMD*)(XRSystem);
			if (SteamVRHMD->IsStereoEnabled())
			{
				RetValue = SteamVRHMD->GetBounds();
			}
		}
	}	
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetValue;
}