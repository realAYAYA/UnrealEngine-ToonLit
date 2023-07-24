// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "UVEditorToolAnalyticsUtils.h"
#include "Selection/UVToolSelectionAPI.h"
#include "Operators/UVEditorUVTransformOp.h"

#include "UVEditorTransformTool.generated.h"

class UUVEditorToolMeshInput;
class UUVEditorUVTransformProperties;
enum class EUVEditorUVTransformType;
class UUVEditorUVTransformOperatorFactory;
class UCanvas;
class UUVEditorTransformTool;

/**
 * Visualization settings for the TransformTool
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorTransformToolDisplayProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Draw the tool's active pivot location if needed.*/
	UPROPERTY(EditAnywhere, Category = "Tool Display Settings", meta = (DisplayName = "Display Tool Pivots"))
	bool bDrawPivots = true;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorBaseTransformToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	// This is a pointer so that it can be updated under the builder without
	// having to set it in the mode after initializing targets.
	const TArray<TObjectPtr<UUVEditorToolMeshInput>>* Targets = nullptr;

protected:
	virtual void ConfigureTool(UUVEditorTransformTool* NewTool) const;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorTransformToolBuilder : public UUVEditorBaseTransformToolBuilder
{
	GENERATED_BODY()

protected:
	virtual void ConfigureTool(UUVEditorTransformTool* NewTool) const override;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorAlignToolBuilder : public UUVEditorBaseTransformToolBuilder
{
	GENERATED_BODY()

protected:
	virtual void ConfigureTool(UUVEditorTransformTool* NewTool) const  override;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorDistributeToolBuilder : public UUVEditorBaseTransformToolBuilder
{
	GENERATED_BODY()

protected:
	virtual void ConfigureTool(UUVEditorTransformTool* NewTool) const  override;
};

/**
 * 
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorTransformTool : public UInteractiveTool, public IUVToolSupportsSelection
{
	GENERATED_BODY()

public:

	/**
	 * The tool will operate on the meshes given here.
	 */
	virtual void SetTargets(const TArray<TObjectPtr<UUVEditorToolMeshInput>>& TargetsIn)
	{
		Targets = TargetsIn;
	}

	void SetToolMode(const EUVEditorUVTransformType& Mode);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

protected:	

	TOptional<EUVEditorUVTransformType> ToolMode;

	UPROPERTY()
	TArray<TObjectPtr<UUVEditorToolMeshInput>> Targets;

	UPROPERTY()
	TObjectPtr<UUVEditorUVTransformPropertiesBase> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorTransformToolDisplayProperties> DisplaySettings = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UUVEditorUVTransformOperatorFactory>> Factories;

	UPROPERTY()
	TObjectPtr<UUVToolSelectionAPI> UVToolSelectionAPI = nullptr;

	TArray<TArray<FVector2D>> PerTargetPivotLocations;

	//
	// Analytics
	//

	UE::Geometry::UVEditorAnalytics::FTargetAnalytics InputTargetAnalytics;
	FDateTime ToolStartTimeAnalytics;
	void RecordAnalytics();
};
