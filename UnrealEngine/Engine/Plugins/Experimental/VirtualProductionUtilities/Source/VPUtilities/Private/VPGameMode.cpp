// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPGameMode.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/WorldSettings.h"
#include "VPRootActor.h"


AActor* AVPGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	RootActor = nullptr;

	AWorldSettings* WorldSetting = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr;
	if (WorldSetting)
	{
		UVPWorldAssetUserData* UserData = CastChecked<UVPWorldAssetUserData>(WorldSetting->GetAssetUserDataOfClass(UVPWorldAssetUserData::StaticClass()), ECastCheckedType::NullAllowed);
		if (UserData)
		{
			RootActor = UserData->LastSelectedRootActor.Get();
		}
	}

	if (RootActor == nullptr)
	{
		for (TActorIterator<AVPRootActor> It(GetWorld()); It; ++It)
		{
			RootActor = *It;
			break;
		}
	}

	AActor* Result = RootActor;
	if (Result == nullptr)
	{
		Result = Super::ChoosePlayerStart_Implementation(Player);
	}

	return Result;
}
