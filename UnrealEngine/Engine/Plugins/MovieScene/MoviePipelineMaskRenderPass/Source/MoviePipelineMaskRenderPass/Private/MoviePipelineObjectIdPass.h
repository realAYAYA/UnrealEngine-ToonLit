// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineDeferredPasses.h"
#include "MovieRenderOverlappedMask.h"
#include "MoviePipelineObjectIdPass.generated.h"

UENUM(BlueprintType)
enum class EMoviePipelineObjectIdPassIdType : uint8
{
	/** As much information as the renderer can provide - unique per material per primitive in the world. */
	Full,
	/** Grouped by material name. This means different objects that use the same material will be merged. */
	Material,
	/** Grouped by Actor Name, all materials for a given actor are merged together, and all actors with that name are merged together as well. */
	Actor,
	/** Grouped by Actor Name and Folder Hierarchy. This means actors with the same name in different folders will not be merged together. */
	ActorWithHierarchy,
	/** Grouped by Folder Name. All actors within a given folder hierarchy in the World Outliner are merged together. */
	Folder,
	/** Primary Layer. This is the first layer found in the AActor::Layers array. May not do what you expect if an actor belongs to multiple layers. */
	Layer
};

UCLASS(BlueprintType)
class UMoviePipelineObjectIdRenderPass : public UMoviePipelineImagePassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineObjectIdRenderPass();

	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "ObjectIdRenderPassSetting_DisplayName", "Object Ids (Limited)"); }
	virtual void GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const override;
	virtual void RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState) override;
	virtual void TeardownImpl() override;
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) override;
	virtual void GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses) override;
	virtual bool IsScreenPercentageSupported() const override { return false; }
	virtual int32 GetOutputFileSortingOrder() const override { return 10; }
	
	virtual TWeakObjectPtr<UTextureRenderTarget2D> CreateViewRenderTargetImpl(const FIntPoint& InSize, IViewCalcPayload* OptPayload = nullptr) const override;
	virtual TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> CreateSurfaceQueueImpl(const FIntPoint& InSize, IViewCalcPayload* OptPayload = nullptr) const override;

protected:
	void PostRendererSubmission(const FMoviePipelineRenderPassMetrics& InSampleState);
	FString ResolveProxyIdGroup(const AActor* InActor, const UPrimitiveComponent* InPrimComponent, const int32 InMaterialIndex, const int32 InSectionIndex) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EMoviePipelineObjectIdPassIdType IdType;

private:
	TSharedPtr<FAccumulatorPool, ESPMode::ThreadSafe> AccumulatorPool;
	TArray<FMoviePipelinePassIdentifier> ExpectedPassIdentifiers;
};