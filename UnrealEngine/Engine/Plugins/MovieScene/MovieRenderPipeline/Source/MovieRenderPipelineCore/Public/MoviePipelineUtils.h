// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "Engine/Engine.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Evaluation/MovieSceneTimeTransform.h"
#include "Misc/FrameNumber.h"
#include "Math/Range.h"
#include "Misc/FrameRate.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "MovieSceneSequenceID.h"
#include "CineCameraComponent.h"
#include "MoviePipelineQueue.h"

// Forward Declare
class UClass;
class UMoviePipelineAntiAliasingSetting;
class UMoviePipelineExecutorShot;
class UMovieSceneSequence;

namespace MoviePipeline
{
static UWorld* FindCurrentWorld()
{
	UWorld* World = nullptr;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::Game)
		{
			World = WorldContext.World();
		}
#if WITH_EDITOR
		else if (GIsEditor && WorldContext.WorldType == EWorldType::PIE)
		{
			World = WorldContext.World();
			if (World)
			{
				return World;
			}
		}
#endif
	}

	return World;
}

MOVIERENDERPIPELINECORE_API void GetPassCompositeData(FMoviePipelineMergerOutputFrame* InMergedOutputFrame, TArray<MoviePipeline::FCompositePassInfo>& OutCompositedPasses);

}

#define MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(InOutVariable, CVarName, OverrideValue, bUseOverride) \
{ \
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName); \
	if(ensureMsgf(CVar, TEXT("Failed to find CVar " #CVarName " to override."))) \
	{ \
		if(bUseOverride) \
		{ \
			InOutVariable = CVar->GetInt(); \
			CVar->SetWithCurrentPriority(OverrideValue); \
		} \
		else \
		{ \
			CVar->SetWithCurrentPriority(InOutVariable); \
		} \
	} \
}

#define MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_FLOAT(InOutVariable, CVarName, OverrideValue, bUseOverride) \
{ \
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName); \
	if(ensureMsgf(CVar, TEXT("Failed to find CVar " #CVarName " to override."))) \
	{ \
		if(bUseOverride) \
		{ \
			InOutVariable = CVar->GetFloat(); \
			CVar->SetWithCurrentPriority(OverrideValue); \
		} \
		else \
		{ \
			CVar->SetWithCurrentPriority(InOutVariable); \
		} \
	} \
}

#define MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT_IF_EXIST(InOutVariable, CVarName, OverrideValue, bUseOverride) \
{ \
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName); \
	if(CVar) \
	{ \
		if(bUseOverride) \
		{ \
			InOutVariable = CVar->GetInt(); \
			CVar->SetWithCurrentPriority(OverrideValue); \
		} \
		else \
		{ \
			CVar->SetWithCurrentPriority(InOutVariable); \
		} \
	} \
}

#define MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_FLOAT_IF_EXIST(InOutVariable, CVarName, OverrideValue, bUseOverride) \
{ \
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName); \
	if(CVar) \
	{ \
		if(bUseOverride) \
		{ \
			InOutVariable = CVar->GetFloat(); \
			CVar->SetWithCurrentPriority(OverrideValue); \
		} \
		else \
		{ \
			CVar->SetWithCurrentPriority(InOutVariablee); \
		} \
	} \
}

namespace UE
{
	namespace MovieRenderPipeline
	{
		MOVIERENDERPIPELINECORE_API TArray<UClass*> FindMoviePipelineSettingClasses(UClass* InBaseClass, const bool bIncludeBlueprints = true);
		MOVIERENDERPIPELINECORE_API EAntiAliasingMethod GetEffectiveAntiAliasingMethod(const UMoviePipelineAntiAliasingSetting* InSetting);
		
		UE_DEPRECATED(5.3, "Do not use, this is here as a temporary workaround for another issue.")
		MOVIERENDERPIPELINECORE_API uint64 GetRendererFrameCount();
	}

	namespace MoviePipeline
	{
		MOVIERENDERPIPELINECORE_API void ConformOutputFormatStringToken(FString& InOutFilenameFormatString, const FStringView InToken, const FName& InNodeName, const FName& InBranchName);
		MOVIERENDERPIPELINECORE_API void ValidateOutputFormatString(FString& InOutFilenameFormatString, const bool bTestRenderPass, const bool bTestFrameNumber, const bool bIncludeCameraName = false);
		MOVIERENDERPIPELINECORE_API void RemoveFrameNumberFormatStrings(FString& InOutFilenameFormatString, const bool bIncludeShots);
		/** De-duplicates the provided array of strings by appending (1), (2), etc to the end of duplicates. */
		MOVIERENDERPIPELINECORE_API void DeduplicateNameArray(TArray<FString>& InOutNames);

		MOVIERENDERPIPELINECORE_API FString GetJobAuthor(const UMoviePipelineExecutorJob* InJob);
		MOVIERENDERPIPELINECORE_API void GetSharedFormatArguments(TMap<FString, FString>& InFilenameArguments, TMap<FString, FString>& InFileMetadata, const FDateTime& InDateTime, const int32 InVersionNumber, const UMoviePipelineExecutorJob* InJob, const FTimespan& InInitializationTimeOffset = FTimespan());
		MOVIERENDERPIPELINECORE_API void GetHardwareUsageMetadata(TMap<FString, FString>& InFileMetadata, const FString& InOutputDir);
		MOVIERENDERPIPELINECORE_API void GetMetadataFromCineCamera(class UCineCameraComponent* InComponent, const FString& InCameraName, const FString& InRenderPassName, TMap<FString, FString>& InOutMetadata);
		MOVIERENDERPIPELINECORE_API void GetMetadataFromCameraLocRot(const FString& InCameraName, const FString& InRenderPassName, const FVector& InCurLoc, const FRotator& InCurRot, const FVector& InPrevLoc, const FRotator& InPrevRot, TMap<FString, FString>& InOutMetadata);
		MOVIERENDERPIPELINECORE_API FMoviePipelineRenderPassMetrics GetRenderPassMetrics(UMoviePipelinePrimaryConfig* InPrimaryConfig, UMoviePipelineExecutorShot* InPipelineExecutorShot, const FMoviePipelineRenderPassMetrics& InRenderPassMetrics, const FIntPoint& InEffectiveOutputResolution);
		MOVIERENDERPIPELINECORE_API bool CanWriteToFile(const TCHAR* InFilename, bool bOverwriteExisting);
		MOVIERENDERPIPELINECORE_API FString GetPaddingFormatString(int32 InZeroPadCount, const int32 InFrameNumber);

		/** When using spatial/temporal samples without anti-aliasing, get the sub-pixel jitter for the given frame index. FrameIndex is modded by InSamplesPerFrame so that the aa jitter pattern repeats every output frame. */
		MOVIERENDERPIPELINECORE_API FVector2f GetSubPixelJitter(int32 InFrameIndex, int32 InSamplesPerFrame);

	}
}

namespace MoviePipeline
{
	MOVIERENDERPIPELINECORE_API void GetOutputStateFormatArgs(TMap<FString, FString>& InFilenameArguments, TMap<FString, FString>& InFileMetadata, const FString FrameNumber, const FString FrameNumberShot, const FString FrameNumberRel, const FString FrameNumberShotRel, const FString CameraName, const FString ShotName);
	
	/** Iterate root-to-tails and generate a HierarchyNode for each level. Caches the sub-section range, playback range, camera cut range, etc. */
	void CacheCompleteSequenceHierarchy(UMovieSceneSequence* InSequence, TSharedPtr<FCameraCutSubSectionHierarchyNode> InRootNode);
	/** Matching function to Cache. Restores the sequence properties when given the root node. */
	void RestoreCompleteSequenceHierarchy(UMovieSceneSequence* InSequence, TSharedPtr<FCameraCutSubSectionHierarchyNode> InRootNode);
	/** Iterates tail to root building Hierarchy Nodes while correctly keeping track of which sub-section by GUID for correct enabling/disabling later. */
	void BuildSectionHierarchyRecursive(const FMovieSceneSequenceHierarchy& InHierarchy, UMovieSceneSequence* InRootSequence, const FMovieSceneSequenceID InSequenceId, const FMovieSceneSequenceID InChildId, TSharedPtr<FCameraCutSubSectionHierarchyNode> OutSubsectionHierarchy);
	/** Gets the inner and outer names for the shot by resolving camera bindings/shot names, etc. Not neccessairly the final names. */
	TTuple<FString, FString> GetNameForShot(const FMovieSceneSequenceHierarchy& InHierarchy, UMovieSceneSequence* InRootSequence, TSharedPtr<FCameraCutSubSectionHierarchyNode> InSubSectionHierarch);
	/** Given a leaf node, either caches the value of the hierarchy or restores it. Used to save the state before soloing a shot. */
	void SaveOrRestoreSubSectionHierarchy(TSharedPtr<FCameraCutSubSectionHierarchyNode> InLeaf, const bool bInSave);
	/** Given a leaf node, appropriately sets the IsActive flags for the whole hierarchy chain up to root for soloing a shot. */
	void SetSubSectionHierarchyActive(TSharedPtr<FCameraCutSubSectionHierarchyNode> InRoot, bool bInActive);
	/** Given a leaf node, searches for sections that will be partially evaluated when using temporal sub-sampling and prints a warning. */
	void CheckPartialSectionEvaluationAndWarn(const FFrameNumber& LeftDeltaTicks, TSharedPtr<FCameraCutSubSectionHierarchyNode> Node, UMoviePipelineExecutorShot* InShot, const FFrameRate& InRootDisplayRate);
}