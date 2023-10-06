// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"
#include "MoviePipelineGameOverrideSetting.h"
#include "MovieGraphGlobalGameOverrides.generated.h"

/** A node which configures the global game overrides. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphGlobalGameOverridesNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
	UMovieGraphGlobalGameOverridesNode() = default;

	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override { return EMovieGraphBranchRestriction::Globals; }

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_GameModeOverride : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_CinematicQualitySettings : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_TextureStreaming : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_UseLODZero : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_DisableHLODs : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_UseHighQualityShadows : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ShadowDistanceScale : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ShadowRadiusThreshold : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OverrideViewDistanceScale : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ViewDistanceScale : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_FlushGrassStreaming : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_FlushStreamingManagers : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_VirtualTextureFeedbackFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_DisableGPUTimeout : 1;
	
	/** Optional Game Mode to override the map's default game mode with. This can be useful if the game's normal mode displays UI elements or loading screens that you don't want captured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game", meta = (EditCondition = "bOverride_GameModeOverride"))
	TSubclassOf<AGameModeBase> GameModeOverride;

	/** If true, automatically set the engine to the Cinematic Scalability quality settings during render. See the Scalability Reference documentation for information on how to edit cvars to add/change default quality values.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game", meta = (EditCondition = "bOverride_CinematicQualitySettings"))
	bool bCinematicQualitySettings;

	/** Defines which If true, when using texture streaming fully load the required textures each frame instead of loading them in over time. This solves objects being blurry after camera cuts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_TextureStreaming"))
	EMoviePipelineTextureStreamingMethod TextureStreaming;

	/** Should we try to use the highest quality LOD for meshes and particle systems regardless of distance? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_UseLODZero"))
	bool bUseLODZero;

	/** Should we disable Hierarchical LODs and instead use their real meshes regardless of distance? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_DisableHLODs"))
	bool bDisableHLODs;

	/** Should we override shadow-related CVars with some high quality preset settings? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_UseHighQualityShadows"))
	bool bUseHighQualityShadows;

	// TODO: This should also be disabled if bUseHighQualityShadows is false, but EditCondition doesn't work with that *plus* the InlineEditConditionToggle
	/** Scalability option to trade shadow distance versus performance for directional lights  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_ShadowDistanceScale", ClampMin=0.1, UIMin=0.1, UIMax=10))
	int32 ShadowDistanceScale;

	// TODO: This should also be disabled if bUseHighQualityShadows is false, but EditCondition doesn't work with that *plus* the InlineEditConditionToggle
	/** Cull shadow casters if they are too small, value is the minimal screen space bounding sphere radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_ShadowRadiusThreshold", UIMin=0.001, ClampMin=0.001))
	float ShadowRadiusThreshold;

	/** Controls the view distance scale. A primitive's MaxDrawDistance is scaled by this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_ViewDistanceScale"))
	int32 ViewDistanceScale;
	
	/** Flushing grass streaming (combined with override view distance scale) prevents visible pop-in/culling of grace instances. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_FlushGrassStreaming"))
	bool bFlushGrassStreaming;

	/** Experimental. If true flush the streaming managers (Texture Streaming) each frame. Allows Texture Streaming to not have visible pop-in in final frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_FlushStreamingManagers"))
	bool bFlushStreamingManagers;

	/** The virtual texture feedback resolution factor. A lower factor will increase virtual texture feedback resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bOverride_VirtualTextureFeedbackFactor"))
	int32 VirtualTextureFeedbackFactor;
};