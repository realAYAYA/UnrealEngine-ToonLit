// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "Changes/TransformChange.h"
#include "FrameTypes.h"
#include "BoxTypes.h"
#include "TransformTypes.h"
#include "PatternTool.generated.h"

class UBaseAxisTranslationGizmo;
class UAxisAngleGizmo;
class UDragAlignmentMechanic;
class UCombinedTransformGizmo;
class UTransformProxy;
class UPreviewGeometry;
class UDynamicMesh;
class UDynamicMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UConstructionPlaneMechanic;


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

	virtual void InitializeNewTool(UMultiSelectionMeshEditingTool* NewTool, const FToolBuilderState& SceneState) const override;
	
protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};




UENUM()
enum class EPatternToolShape : uint8
{
	/** Arrange pattern elements along a Line */
	Line = 0,

	/** Arrange pattern elements in a 2D Grid */
	Grid = 1,

	/** Arrange pattern elements in a Circle */
	Circle = 2
};


UENUM()
enum class EPatternToolSingleAxis : uint8
{
	XAxis = 0,
	YAxis = 1,
	ZAxis = 2
};

UENUM()
enum class EPatternToolSinglePlane : uint8
{
	XYPlane = 0,
	XZPlane = 1,
	YZPlane = 2
};



UENUM()
enum class EPatternToolAxisSpacingMode : uint8
{
	/** Place a specific number of Pattern Elements along the pattern geometry */
	ByCount = 0,

	/** Place Pattern Elements at regular increments along the Pattern Geometry (on-center) */
	StepSize = 1,

	/** Pack in as many Pattern Elements as fits in the available space */
	Packed = 2
};


/**
 * Settings for the Pattern Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Shape of the underlying Pattern */
	UPROPERTY(EditAnywhere, Category = Shape)
	EPatternToolShape Shape = EPatternToolShape::Line;

	/** Axis direction used for the Pattern geometry */
	UPROPERTY(EditAnywhere, Category = Shape, meta = (DisplayName = "Direction", EditCondition = "Shape == EPatternToolShape::Line", EditConditionHides))
	EPatternToolSingleAxis SingleAxis = EPatternToolSingleAxis::XAxis;

	/** Plane used for the Pattern geometry */
	UPROPERTY(EditAnywhere, Category = Shape, meta = (DisplayName = "Plane", EditCondition = "Shape != EPatternToolShape::Line", EditConditionHides))
	EPatternToolSinglePlane SinglePlane = EPatternToolSinglePlane::XYPlane;

	/** Shape of the underlying Pattern */
	UPROPERTY(EditAnywhere, Category = Shape)
	bool bHideSources = true;

	/** The seed used to introduce random transform variations when enabled */
	UPROPERTY(EditAnywhere, Category = Shape)
	int32 Seed = FMath::Rand();
};


/**
 * Settings for Linear Patterns in the Pattern Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool_LinearSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Spacing Technique used to distribute Pattern Elements */
	UPROPERTY(EditAnywhere, Category = LinearPattern)
	EPatternToolAxisSpacingMode SpacingMode = EPatternToolAxisSpacingMode::ByCount;

	/** Number of Pattern Elements to place */
	UPROPERTY(EditAnywhere, Category = LinearPattern, meta = (ClampMin = 1, UIMax = 25, EditCondition = "SpacingMode == EPatternToolAxisSpacingMode::ByCount"))
	int32 Count = 10;

	/** Fixed Increment used to place Pattern Elements */
	UPROPERTY(EditAnywhere, Category = LinearPattern, meta = (ClampMin = 0, EditCondition = "SpacingMode == EPatternToolAxisSpacingMode::StepSize"))
	double StepSize = 100.0;

	/** Length of Pattern along the Axis */
	UPROPERTY(EditAnywhere, Category = LinearPattern, meta = (ClampMin = 0))
	double Extent = 1000.0;

	/** If true, Pattern is centered at the Origin, otherwise Pattern starts at the Origin */
	UPROPERTY(EditAnywhere, Category = LinearPattern)
	bool bCentered = true;
};




/**
 * Settings for Grid Patterns in the Pattern Tool
 * TODO: maybe we can just re-use UPatternTool_LinearSettings for this??
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool_GridSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Spacing Technique used to distribute Pattern Elements along the Main axis */
	UPROPERTY(EditAnywhere, Category = GridPatternX)
	EPatternToolAxisSpacingMode SpacingX = EPatternToolAxisSpacingMode::ByCount;

	/** Number of  Pattern Elements to place along the Main axis */
	UPROPERTY(EditAnywhere, Category = GridPatternX, meta = (ClampMin = 1, UIMax = 25, EditCondition = "SpacingX == EPatternToolAxisSpacingMode::ByCount"))
	int32 CountX = 10;

	/** Fixed Increment used to place Pattern Elements along the Main axis */
	UPROPERTY(EditAnywhere, Category = GridPatternX, meta = (ClampMin = 0, EditCondition = "SpacingX == EPatternToolAxisSpacingMode::StepSize"))
	double StepSizeX = 100.0;

	/** Length/Extent of Pattern falong the Main Axis */
	UPROPERTY(EditAnywhere, Category = GridPatternX, meta = (ClampMin = 0))
	double ExtentX = 1000.0;

	/** If true, Pattern is centered at the Origin along the Main axis, otherwise Pattern starts at the Origin */
	UPROPERTY(EditAnywhere, Category = GridPatternX)
	bool bCenteredX = true;

	/** Spacing Technique used to distribute Pattern Elements along the Secondary axis*/
	UPROPERTY(EditAnywhere, Category = GridPatternY)
	EPatternToolAxisSpacingMode SpacingY = EPatternToolAxisSpacingMode::ByCount;

	/** Number of  Pattern Elements to place along the Secondary axis */
	UPROPERTY(EditAnywhere, Category = GridPatternY, meta = (ClampMin = 1, UIMax = 25, EditCondition = "SpacingY == EPatternToolAxisSpacingMode::ByCount"))
	int32 CountY = 10;

	/** Fixed Increment used to place Pattern Elements along the Secondary axis */
	UPROPERTY(EditAnywhere, Category = GridPatternY, meta = (ClampMin = 0, EditCondition = "SpacingY == EPatternToolAxisSpacingMode::StepSize"))
	double StepSizeY = 100.0;

	/** Length/Extent of Pattern falong the Secondary Axis */
	UPROPERTY(EditAnywhere, Category = GridPatternY, meta = (ClampMin = 0))
	double ExtentY = 1000.0;

	/** If true, Pattern is centered at the Origin along the Secondary axis, otherwise Pattern starts at the Origin */
	UPROPERTY(EditAnywhere, Category = GridPatternY)
	bool bCenteredY = true;
};





/**
 * Settings for Radial Patterns in the Pattern Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool_RadialSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Spacing Technique used to distribute Pattern Elements around the Circle/Arc */
	UPROPERTY(EditAnywhere, Category = RadialPattern)
	EPatternToolAxisSpacingMode SpacingMode = EPatternToolAxisSpacingMode::ByCount;

	/** Number of  Pattern Elements to place */
	UPROPERTY(EditAnywhere, Category = RadialPattern, meta = (ClampMin = 1, UIMax = 25, EditCondition = "SpacingMode == EPatternToolAxisSpacingMode::ByCount"))
	int32 Count = 10;

	/** Fixed Increment (in Degrees) used to position Pattern Elements around the Circle/Arc */
	UPROPERTY(EditAnywhere, Category = RadialPattern, meta = (Units = "Degrees", ClampMin = 0, EditCondition = "SpacingMode == EPatternToolAxisSpacingMode::StepSize"))
	double StepSize = 100.0;

	/** Radius of the Circle/Arc */
	UPROPERTY(EditAnywhere, Category = RadialPattern, meta = (ClampMin = 0))
	double Radius = 250;

	/** Start angle of the Circle/Arc */
	UPROPERTY(EditAnywhere, Category = RadialPattern, meta = (Units = "Degrees", UIMin = -360, UIMax = 360))
	double StartAngle = 0.0;

	/** End angle of the Circle/Arc */
	UPROPERTY(EditAnywhere, Category = RadialPattern, meta = (Units = "Degrees", UIMin = -360, UIMax = 360))
	double EndAngle = 360.0;

	/** Fixed offset added to Start/End Angles */
	UPROPERTY(EditAnywhere, Category = RadialPattern, meta = (Units = "Degrees", ClampMin = -180, ClampMax = 180))
	double AngleShift = 0.0;

	/** If true, Pattern elements are rotated to align with the Circle tangen */
	UPROPERTY(EditAnywhere, Category = RadialPattern)
	bool bOriented = true;
};




/**
 * Settings for Per Element Rotation in the Pattern Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool_RotationSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** If true, Rotation is linearly interpolated between StartRotation and Rotation values */
	UPROPERTY(EditAnywhere, Category = Rotation, meta = (InlineEditConditionToggle))
	bool bInterpolate = false;

	/** If true, Rotation at each Pattern Element is offset by a uniformly chosen random value in the range of [-RotationJitterRange, RotationJitterRange] */
	UPROPERTY(/*EditAnywhere, Category = Rotation*/)
	bool bJitter = false;

	/** Rotation at first Pattern Element */
	UPROPERTY(EditAnywhere, Category = Rotation)
	FRotator StartRotation = FRotator::ZeroRotator;

	/** Rotation applied to all Pattern Elements, or at Last Pattern Element for Interpolated rotations */
	UPROPERTY(EditAnywhere, Category = Rotation, meta = (EditCondition = "bInterpolate"))
	FRotator EndRotation = FRotator::ZeroRotator;

	/** Upper bound of the range which is sampled to randomly rotate each Pattern Element if Jitter is true */
	UPROPERTY(/*EditAnywhere, Category = Rotation, meta = (ClampMin = 0, EditCondition = "bJitter", EditConditionHides, HideEditConditionToggle)*/)
	FRotator RotationJitterRange = FRotator::ZeroRotator;
};


/**
 * Settings for Per Element Translation in the Pattern Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool_TranslationSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** If true, Translation is linearly interpolated between StartTranslation and Translation values */
	UPROPERTY(EditAnywhere, Category = Translation, meta = (InlineEditConditionToggle))
	bool bInterpolate = false;

	/** If true, Translation at each Pattern Element is offset by a uniformly chosen random value in the range of [-TranslationJitterRange, TranslationJitterRange] */
	UPROPERTY(/*EditAnywhere, Category = Translation*/)
	bool bJitter = false;

	/** Translation at first Pattern Element */
	UPROPERTY(EditAnywhere, Category = Translation)
	FVector StartTranslation = FVector::ZeroVector;

	/** Translation applied to all Pattern Elements, or at Last Pattern Element for Interpolated translations */
	UPROPERTY(EditAnywhere, Category = Translation, meta = (EditCondition = "bInterpolate"))
	FVector EndTranslation = FVector::ZeroVector;

	/** Upper bound of the range which is sampled to randomly translate each Pattern Element if Jitter is true */
	UPROPERTY(/*EditAnywhere, Category = Translation, meta = (ClampMin = 0, EditCondition = "bJitter", EditConditionHides, HideEditConditionToggle)*/)
	FVector TranslationJitterRange = FVector::ZeroVector;
};


/**
 * Settings for Per Element Scale in the Pattern Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool_ScaleSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** If true, Scaling is limited to Uniform Scaling */
	UPROPERTY(EditAnywhere, Category = Scale)
	bool bUniform = true;
	
	/** If true, Scale is linearly interpolated between StartScale and Scale values */
	UPROPERTY(EditAnywhere, Category = Scale, meta = (InlineEditConditionToggle))
	bool bInterpolate = false;

	/** If true, Scale at each Pattern Element is offset by a uniformly chosen random value in the range of [-ScaleJitterRange, ScaleJitterRange] */
	UPROPERTY(/*EditAnywhere, Category = Scale*/)
	bool bJitter = false;
	
	/** Uniform Scale at first Pattern Element */
	UPROPERTY(EditAnywhere, Category = Scale)
	FVector StartScale = FVector::OneVector;
	
	/** Uniform Scale applied to all Pattern Elements, or at Last Pattern Element for Interpolated scales */
	UPROPERTY(EditAnywhere, Category = Scale, meta = (EditCondition = "bInterpolate"))
	FVector EndScale = FVector::OneVector;
	
	/** Upper bound of the range which is sampled to randomly scale each Pattern Element if Jitter is true */
	UPROPERTY(/*EditAnywhere, Category = Scale, meta = (ClampMin = 0, EditCondition = "bJitter && bUniform", EditConditionHides, HideEditConditionToggle)*/)
	float ScaleJitterRange = 0.0f;

	/** Upper bound of the range which is sampled to randomly scale each Pattern Element if Jitter is true (Non-Uniform) */
	UPROPERTY(/*EditAnywhere, Category = Scale, meta = (ClampMin = 0, DisplayName = "Scale Jitter Range", EditCondition = "bJitter && bUniform == false", EditConditionHides, HideEditConditionToggle)*/)
	FVector ScaleJitterRangeNonUniform = FVector(0, 0, 0);
};




/**
 * Ouptput Settings for the Pattern Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool_OutputSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Emit a separate Actor for each pattern element */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSeparateActors = false;

	/** Emit StaticMesh pattern elements as DynamicMeshes */
	UPROPERTY(EditAnywhere, Category = Output, meta = (EditCondition = "bHaveStaticMeshes == true", HideEditConditionToggle))
	bool bConvertToDynamic = false;

	/** Create InstancedStaticMeshComponents instead multiple StaticMeshComponents, for StaticMesh pattern elements */
	UPROPERTY(EditAnywhere, Category = Output, meta = (EditCondition = "bHaveStaticMeshes == true && bSeparateActors == false && bConvertToDynamic == false", HideEditConditionToggle))
	bool bCreateISMCs = false;

	// internal, used to control state of Instance settings
	UPROPERTY(meta = (TransientToolProperty))
	bool bHaveStaticMeshes = false;
};



/**
 * UPatternTool takes input meshes and generates 3D Patterns of those meshes, by
 * placing repeated copies along geometric paths like lines, grids, circles, etc.
 * The output can be a single Actor per pattern Element, or combined into single
 * Actors in various ways depending on the input mesh type. 
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UPatternTool : public UMultiSelectionMeshEditingTool
{
	GENERATED_BODY()

public:
	UPatternTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return true; }

public:
	UPROPERTY()
	TObjectPtr<UPatternToolSettings> Settings;

	UPROPERTY()
	TObjectPtr<UPatternTool_LinearSettings> LinearSettings;

	UPROPERTY()
	TObjectPtr<UPatternTool_GridSettings> GridSettings;

	UPROPERTY()
	TObjectPtr<UPatternTool_RadialSettings> RadialSettings;

	UPROPERTY()
	TObjectPtr<UPatternTool_RotationSettings> RotationSettings;

	UPROPERTY()
	TObjectPtr<UPatternTool_TranslationSettings> TranslationSettings;

	UPROPERTY()
	TObjectPtr<UPatternTool_ScaleSettings> ScaleSettings;
	
	FVector StartScaleDirection;
	FVector EndScaleDirection;
	
	int32 StartScaleWatcherIdx;
	int32 EndScaleWatcherIdx;

	UPROPERTY()
	TObjectPtr<UPatternTool_OutputSettings> OutputSettings;

protected:

	UPROPERTY()
	TObjectPtr<UTransformProxy> TransformProxy_End = nullptr;

	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> TransformGizmo_End = nullptr;


	UPROPERTY()
	TObjectPtr<UDragAlignmentMechanic> DragAlignmentMechanic = nullptr;


	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;

	UE::Geometry::FFrame3d CurrentStartFrameWorld;
	TArray<UE::Geometry::FTransformSRT3d> CurrentPattern;

	bool bPatternNeedsUpdating = false;
	FDateTime LastPatternUpdateTime;
	void MarkPatternDirty();

	void OnSourceVisibilityToggled(bool bVisible);
	void OnMainFrameUpdated();
	void OnShapeUpdated();
	void OnParametersUpdated();
	void UpdatePattern();
	void GetPatternTransforms_Linear(TArray<UE::Geometry::FTransformSRT3d>& TransformsOut);
	void GetPatternTransforms_Grid(TArray<UE::Geometry::FTransformSRT3d>& TransformsOut);
	void GetPatternTransforms_Radial(TArray<UE::Geometry::FTransformSRT3d>& TransformsOut);


	struct FPatternElement
	{
		int32 TargetIndex = 0;

		// todo: This is no longer necessary now that InitializeNewTool filters out invalid targets.
		bool bValid = true;

		UPrimitiveComponent* SourceComponent = nullptr;
		TArray<UMaterialInterface*> SourceMaterials;
		UE::Geometry::FTransformSRT3d SourceTransform = UE::Geometry::FTransformSRT3d::Identity();
		UE::Geometry::FTransformSRT3d BaseRotateScale = UE::Geometry::FTransformSRT3d::Identity();

		UDynamicMesh* SourceDynamicMesh = nullptr;
		UStaticMesh* SourceStaticMesh = nullptr;

		UE::Geometry::FAxisAlignedBox3d LocalBounds = UE::Geometry::FAxisAlignedBox3d::Empty();
		UE::Geometry::FAxisAlignedBox3d PatternBounds = UE::Geometry::FAxisAlignedBox3d::Empty();
	};
	TArray<FPatternElement> Elements;

	bool bHaveNonUniformScaleElements = false;

	struct FComponentSet
	{
		TArray<UPrimitiveComponent*> Components;
	};
	TArray<FComponentSet> PreviewComponents;

	UPROPERTY()
	TSet<TObjectPtr<UPrimitiveComponent>> AllComponents;		// to keep components in FComponentSet alive

	
	TMap<int32, FComponentSet> StaticMeshPools;
	UStaticMeshComponent* GetPreviewStaticMesh(FPatternElement& Element);
	void ReturnStaticMeshes(FPatternElement& Element, FComponentSet& ComponentSet);

	TMap<int32, FComponentSet> DynamicMeshPools;
	UDynamicMeshComponent* GetPreviewDynamicMesh(FPatternElement& Element);
	void ReturnDynamicMeshes(FPatternElement& Element, FComponentSet& ComponentSet);

	void HideReturnedPreviewMeshes();


	UPROPERTY()
	TObjectPtr<UPreviewGeometry> PreviewGeometry = nullptr;	// parent actor for all preview components


	void InitializeElements();
	void ResetPreviews();
	void DestroyPreviews();

	void EmitResults();
};
