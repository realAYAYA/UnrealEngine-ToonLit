// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"

#include "PCGEditorSettings.generated.h"

class UPCGSettings;

struct FEdGraphPinType;

UCLASS(config=EditorPerProjectUserSettings)
class PCGEDITOR_API UPCGEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPCGEditorSettings(const FObjectInitializer& ObjectInitializer);

	FLinearColor GetColor(UPCGSettings* InSettings) const;
	FLinearColor GetPinColor(const FEdGraphPinType& InPinType) const;

	/** Default node color */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor DefaultNodeColor;

	/** Instanced node body tint color */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor InstancedNodeBodyTintColor;

	/** Color used for input & output nodes */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor InputOutputNodeColor;

	/** Color used for Difference, Intersection, Projection, Union */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor SetOperationNodeColor;

	/** Color used for density remap */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor DensityOperationNodeColor;

	/** Color used for blueprints */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor BlueprintNodeColor;

	/** Color used for metadata operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor MetadataNodeColor;

	/** Color used for filter-like operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor FilterNodeColor;

	/** Color used for sampler operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor SamplerNodeColor;

	/** Color used for artifact-generating operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor SpawnerNodeColor;

	/** Color used for subgraph-like operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor SubgraphNodeColor;

	/** Color used for Attribute Set operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel, DisplayName = "Attribute Set Node Color"))
	FLinearColor ParamDataNodeColor;

	/** Color used for debug operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor DebugNodeColor;

	/** Color used for control flow operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor ControlFlowNodeColor;
	
	/** Color used for point operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor PointOpsNodeColor;

	/** Color used for hierarchical generation operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor HierarchicalGenerationNodeColor;

	/** Color used for graph parameters operations */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor GraphParametersNodeColor;

	/** Color used for reroute nodes */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor RerouteNodeColor;

	/** Default pin color */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor DefaultPinColor;

	/** Color used for spatial data pins */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor SpatialDataPinColor;
	
	/** Color used for concrete/simple spatial data pins */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor ConcreteDataPinColor;

	/** Color used for data pins of type Point */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor PointDataPinColor;

	/** Color used for data pins of type Spline */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor PolyLineDataPinColor;

	/** Color used for data pins of type Surface */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor SurfaceDataPinColor;

	/** Color used for data pins of type Landscape */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor LandscapeDataPinColor;

	/** Color used for data pins of type Texture */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor TextureDataPinColor;

	/** Color used for data pins of type Render Target */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor RenderTargetDataPinColor;

	/** Color used for data pins of type Volume */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor VolumeDataPinColor;

	/** Color used for data pins of type Primitive */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor PrimitiveDataPinColor;

	/** Color used for data pins of type Attribute Set */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel, DisplayName = "Attribute Set Pin Color"))
	FLinearColor ParamDataPinColor;

	/** Color used for other/unknown data types */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	FLinearColor UnknownDataPinColor;

	/** User-driven color overrides */
	UPROPERTY(EditAnywhere, config, Category = Node, meta = (HideAlphaChannel))
	TMap<TSubclassOf<UPCGSettings>, FLinearColor> OverrideNodeColorByClass;

	/** Default wire thickness */
	UPROPERTY(EditAnywhere, config, Category = Wire, meta = (ClampMin="1", ClampMap="9"))
	float DefaultWireThickness = 1.5f;

	/** Hover edge size emphasis */
	UPROPERTY(EditAnywhere, config, Category = Wire, meta = (ClampMin="1", ClampMax="5"))
	float HoverEdgeEmphasis = 1.5f;

	/** Debug mode edge emphasis on multi-data */
	UPROPERTY(EditAnywhere, config, Category = Wire, meta = (ClampMin="1", ClampMax="5"))
	float MultiDataEdgeDebugEmphasis = 2.5f;

	/** Debug mode edge desaturation with empty data */
	UPROPERTY(EditAnywhere, config, Category = Wire, meta = (ClampMin="0", ClampMax="1"))
	float EmptyEdgeDebugDesaturateFactor = 0.5f;

	/** Specify if we want to jump to definition in case of double click on native PCG Nodes */
	UPROPERTY(EditAnywhere, config, Category = Workflow)
	bool bEnableNavigateToNativeNodes = true;

	/** Specify if we want to disable CPU Throttling when a PCG Graph is executing, this will improve execution time when app is out of focus/minimized */
	UPROPERTY(EditAnywhere, config, Category = "Editor Performance", meta = (DisplayName = "Disable CPU throttling during graph execution"))
	bool bDisableCPUThrottlingDuringGraphExecution = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGSettings.h"
#endif
