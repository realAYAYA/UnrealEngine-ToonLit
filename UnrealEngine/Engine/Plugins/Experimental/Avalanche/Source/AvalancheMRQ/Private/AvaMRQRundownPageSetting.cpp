// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMRQRundownPageSetting.h"
#include "AvaRemoteControlRebind.h"
#include "AvaRemoteControlUtils.h"
#include "AvaScene.h"
#include "AvaSceneSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "IAvaSceneInterface.h"
#include "LevelSequence.h"
#include "Misc/CoreMiscDefines.h"
#include "Misc/PackageName.h"
#include "MoviePipeline.h"
#include "Rundown/AvaRundown.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaMRQSequenceData, Log, All);

namespace UE::AvaMRQ::Private
{
	int32 FindPieInstanceId(UWorld* InWorld)
	{
		check(GEngine);
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			// Return the first PIE world that we can find
			if (WorldContext.WorldType == EWorldType::PIE && InWorld == WorldContext.World())
			{
				return WorldContext.PIEInstance;
			}
		}
		return INDEX_NONE;
	}

	FSoftObjectPath RemovePiePrefix(const FSoftObjectPath& InPiePath, int32 InPieInstanceId)
	{
		FString NewPath = InPiePath.ToString();
		if (FPackageName::GetLongPackageAssetName(NewPath).StartsWith(PLAYWORLD_PACKAGE_PREFIX))
		{
			FString PiePrefix = FString::Printf(TEXT("%s_%d_"), PLAYWORLD_PACKAGE_PREFIX, InPieInstanceId);
			NewPath = NewPath.Replace(*PiePrefix, TEXT(""), ESearchCase::CaseSensitive);
		}
		return FSoftObjectPath(NewPath);
	}
}

void UAvaMRQRundownPageSetting::SetupForPipelineImpl(UMoviePipeline* InPipeline)
{
	UWorld* World = GetWorld();
	if (!InPipeline || !World)
	{
		return;
	}

	ULevelSequence* LevelSequence = InPipeline->GetTargetSequence();
	if (!LevelSequence)
	{
		return;
	}

	UAvaRundown* Rundown = RundownPage.Rundown.LoadSynchronous();
	if (!Rundown)
	{
		UE_LOG(LogAvaMRQSequenceData, Error
			, TEXT("Motion Design Rundown '%s' was not valid and could not be loaded.")
			, *RundownPage.Rundown.ToString());
		return;
	}

	const FAvaRundownPage& Page = Rundown->GetPage(RundownPage.PageId);
	if (!Page.IsValidPage())
	{
		UE_LOG(LogAvaMRQSequenceData, Error
			, TEXT("Motion Design Rundown '%s' did not have a valid page in %d.")
			, *Rundown->GetFullName()
			, RundownPage.PageId);
		return;
	}

	// Ensure Page Source Asset matches
	int32 PieInstanceId = UE::AvaMRQ::Private::FindPieInstanceId(World);
	if (PieInstanceId != INDEX_NONE)
	{
		FSoftObjectPath SourcePath  = Page.GetAssetPath(Rundown);
		FSoftObjectPath CurrentPath = UE::AvaMRQ::Private::RemovePiePrefix(World, PieInstanceId);

		if (CurrentPath != SourcePath)
		{
			UE_LOG(LogAvaMRQSequenceData, Error
				, TEXT("Asset path '%s' in Page '%d' of Motion Design Rundown '%s' did not match the provided PIE sanitized world path '%s'")
				, *SourcePath.ToString()
				, Page.GetPageId()
				, *Rundown->GetFullName()
				, *CurrentPath.ToString());
			return;
		}
	}

	ULevel* const Level = World->PersistentLevel;

	IAvaSceneInterface* Scene = UAvaSceneSubsystem::FindSceneInterface(Level);
	if (!Scene)
	{
		UE_LOG(LogAvaMRQSequenceData, Error
			, TEXT("Motion Design Scene Interface was not found in the provided World '%s'.")
			, *World->GetFullName());
		return;
	}

	URemoteControlPreset* RemoteControlPreset = Scene->GetRemoteControlPreset();
	if (!RemoteControlPreset)
	{
		UE_LOG(LogAvaMRQSequenceData, Error
			, TEXT("A Remote Control Preset was not found in the provided World '%s'.")
			, *World->GetFullName());
		return;
	}

	FAvaRemoteControlRebind::RebindUnboundEntities(RemoteControlPreset, Level);
	Page.GetRemoteControlValues().ApplyEntityValuesToRemoteControlPreset(RemoteControlPreset);

	// todo: ideally, _getUObject should not be used here, and instead the Viewport Data Provider gotten directly through an interface call
	if (IAvaViewportDataProvider* ViewportDataProvider = Cast<IAvaViewportDataProvider>(Scene->_getUObject()))
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FName StartupCameraName = ViewportDataProvider->GetStartupCameraName();

			TObjectPtr<AActor>* ViewTarget = World->PersistentLevel->Actors.FindByPredicate([StartupCameraName](TObjectPtr<AActor> InActor)
			{
				return InActor && InActor->GetFName() == StartupCameraName;
			});

			if (ViewTarget)
			{
				PlayerController->SetViewTarget(*ViewTarget);
			}
		}
	}
}
