// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "IndexTypes.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h" // IInteractiveToolNestedAcceptCancelAPI
#include "GeometryBase.h"
#include "UVEditorToolAnalyticsUtils.h"

#include "UVEditorSeamTool.generated.h"

//class ULocalSingleClickInputBehavior;
class UInputRouter;
class ULocalInputBehaviorSource;
class UPreviewGeometry;
class UUVEditorToolMeshInput;
class UUVToolEmitChangeAPI;
class UUVToolLivePreviewAPI;

PREDECLARE_GEOMETRY(class FDynamicMesh);

UCLASS()
class UVEDITORTOOLS_API UUVEditorSeamToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	// These are pointers so that they can be updated under the builder without
	// having to reset them after things are reinitialized.
	const TArray<TObjectPtr<UUVEditorToolMeshInput>>* Targets = nullptr;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorSeamTool : public UInteractiveTool,
	public IInteractiveToolNestedAcceptCancelAPI
{
	GENERATED_BODY()

public:
	using FDynamicMeshAABBTree3 = UE::Geometry::FDynamicMeshAABBTree3;

	virtual void SetTargets(const TArray<TObjectPtr<UUVEditorToolMeshInput>>& TargetsIn);

	// For use by undo/redo
	void EditLockedPath(
		TUniqueFunction<void(TArray<int32>& LockedPathInOut)> EditFunction, int32 MeshIndex);

	// UInteractiveTool
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }

	// IInteractiveToolNestedAcceptCancelAPI
	virtual bool SupportsNestedCancelCommand() override { return true; }
	virtual bool CanCurrentlyNestedCancel() override;
	virtual bool ExecuteNestedCancelCommand() override;
	virtual bool SupportsNestedAcceptCommand() override { return true; }
	virtual bool CanCurrentlyNestedAccept() override;
	virtual bool ExecuteNestedAcceptCommand() override;
protected:
	void ReconstructExistingSeamsVisualization();
	void ReconstructLockedPathVisualization();

	int32 Get2DHitVertex(const FRay& WorldRayIn, int32* IndexOf2DSpatialOut = nullptr);
	int32 Get3DHitVertex(const FRay& WorldRayIn, int32* IndexOf3DSpatialOut = nullptr);

	void OnMeshVertexClicked(int32 Vid, int32 IndexOfMesh, bool bVidIsFromUnwrap);
	void OnMeshVertexHovered(int32 Vid, int32 IndexOfMesh, bool bVidIsFromUnwrap);
	void UpdateHover();
	/** @param bClearHoverInfo If true, also clears HoverVid, etc in addition to just clearing display. */
	void ClearHover(bool bClearHoverInfo = true);
	void ResetPreviewColors();
	void ApplyClick();
	/** Clears LockedAppliedVids, but builds seam off of AppliedVidsIn */
	void ApplySeam(const TArray<int32>& AppliedVidsIn);
	void ClearLockedPath(bool bEmitChange = true);

	void UpdateToolMessage();

	enum class EState
	{
		WaitingToStart,
		SeamInProgress
	};

	EState State = EState::WaitingToStart;

	UPROPERTY()
	TArray<TObjectPtr<UUVEditorToolMeshInput>> Targets;

	UPROPERTY()
	TObjectPtr<UUVToolLivePreviewAPI> LivePreviewAPI = nullptr;

	UPROPERTY()
	TObjectPtr<UUVToolEmitChangeAPI> EmitChangeAPI = nullptr;

	UPROPERTY()
	TObjectPtr<UInputBehaviorSet> LivePreviewBehaviorSet = nullptr;

	UPROPERTY()
	TObjectPtr<ULocalInputBehaviorSource> LivePreviewBehaviorSource = nullptr;

	UPROPERTY()
	TObjectPtr<UPreviewGeometry> UnwrapGeometry = nullptr;

	UPROPERTY()
	TObjectPtr<UPreviewGeometry> LivePreviewGeometry = nullptr;

	TWeakObjectPtr<UInputRouter> LivePreviewInputRouter = nullptr;

	TArray<TSharedPtr<FDynamicMeshAABBTree3>> Spatials2D; // 1:1 with targets
	TArray<TSharedPtr<FDynamicMeshAABBTree3>> Spatials3D; // 1:1 with targets

	FViewCameraState LivePreviewCameraState;

	// Used to remember click info to apply on tick
	int32 ClickedVid = IndexConstants::InvalidID;
	int32 ClickedMeshIndex = -1;
	bool bClickWasInUnwrap = false;

	// Used to remember hover info to apply on tick
	int32 HoverVid = IndexConstants::InvalidID;
	int32 HoverMeshIndex = IndexConstants::InvalidID;
	bool bHoverVidIsFromUnwrap = false;
	int32 LastHoverVid = IndexConstants::InvalidID;
	int32 LastHoverMeshIndex = IndexConstants::InvalidID;
	bool bLastHoverVidWasFromUnwrap = false;

	// Used to know when to end the seam.
	int32 SeamStartAppliedVid = IndexConstants::InvalidID;
	int32 LastLockedAppliedVid = IndexConstants::InvalidID;

	TArray<int32> LockedPath;
	TArray<int32> PreviewPath;

	// When true, the entire path is changed to the "completion" color to show
	// that the next click will complete the path.
	bool bCompletionColorOverride = false;
	
	//
	// Analytics
	//
	
	UE::Geometry::UVEditorAnalytics::FTargetAnalytics InputTargetAnalytics;
	FDateTime ToolStartTimeAnalytics;
	void RecordAnalytics();
};
