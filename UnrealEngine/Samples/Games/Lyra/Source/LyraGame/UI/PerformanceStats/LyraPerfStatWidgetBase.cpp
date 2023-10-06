// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPerfStatWidgetBase.h"

#include "Engine/GameInstance.h"
#include "Performance/LyraPerformanceStatSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPerfStatWidgetBase)

//////////////////////////////////////////////////////////////////////
// ULyraPerfStatWidgetBase

ULyraPerfStatWidgetBase::ULyraPerfStatWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

double ULyraPerfStatWidgetBase::FetchStatValue()
{
	if (CachedStatSubsystem == nullptr)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				CachedStatSubsystem = GameInstance->GetSubsystem<ULyraPerformanceStatSubsystem>();
			}
		}
	}

	if (CachedStatSubsystem)
	{
		return CachedStatSubsystem->GetCachedStat(StatToDisplay);
	}
	else
	{
		return 0.0;
	}
}

