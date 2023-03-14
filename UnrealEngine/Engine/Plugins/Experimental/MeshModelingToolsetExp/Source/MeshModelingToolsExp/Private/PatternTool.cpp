// Copyright Epic Games, Inc. All Rights Reserved.

#include "PatternTool.h"
#include "InteractiveToolManager.h"
#include "InteractiveGizmoManager.h"
#include "Mechanics/DragAlignmentMechanic.h"
#include "Mechanics/ConstructionPlaneMechanic.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMeshEditor.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "ToolSceneQueriesUtil.h"
#include "ModelingToolTargetUtil.h"
#include "TransformSequence.h"
#include "Selection/ToolSelectionUtil.h"

#include "ModelingObjectsCreationAPI.h"
#include "ModelingComponentsSettings.h"

#include "Drawing/PreviewGeometryActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"

#include "BaseGizmos/GizmoComponents.h"
#include "BaseGizmos/TransformGizmoUtil.h"

#include "Engine/World.h"

#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"


using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UPatternTool"


/*
 * ToolBuilder
 */

bool UPatternToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	int32 ValidTargets = 0;

	// This tool currently only has 5 output modes, each of them either supporting either static or dynamic meshes.
	// Other targets which are UPrimitiveComponent backed are not currently supported.
	// todo: Add support for skeletal meshes and volumes
	SceneState.TargetManager->EnumerateSelectedAndTargetableComponents(SceneState, GetTargetRequirements(), [&ValidTargets](UActorComponent* Component)
	{
		if (Cast<UStaticMeshComponent>(Component) || Cast<UDynamicMeshComponent>(Component))
		{
			ValidTargets++;
		}
	});
	
	return ValidTargets > 0;
}

UMultiSelectionMeshEditingTool* UPatternToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UPatternTool>(SceneState.ToolManager);
}

void UPatternToolBuilder::InitializeNewTool(UMultiSelectionMeshEditingTool* NewTool, const FToolBuilderState& SceneState) const
{
	TArray<TObjectPtr<UToolTarget>> Targets;
	SceneState.TargetManager->EnumerateSelectedAndTargetableComponents(SceneState, GetTargetRequirements(), [this, &Targets, &SceneState](UActorComponent* Component)
	{
		if (Cast<UStaticMeshComponent>(Component) || Cast<UDynamicMeshComponent>(Component))
		{
			Targets.Add(SceneState.TargetManager->BuildTarget(Component, GetTargetRequirements()));
		}
	});

	NewTool->SetTargets(Targets);
	NewTool->SetWorld(SceneState.World);
}

const FToolTargetTypeRequirements& UPatternToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		UPrimitiveComponentBackedTarget::StaticClass()
		);
	return TypeRequirements;
}

// these pattern generators should be promoted to GeometryProcessing, however they need some
// work to clean up the API and make them more correct (ie handling of step size seems a bit
// flaky, particularly for circle patterns)

class FPatternGenerator
{
public:
	enum class ESpacingMode
	{
		ByCount = 0,
		ByStepSize = 1,
		Packed = 2
	};

public:
	// input settings
	FQuaterniond StartRotation = FQuaterniond::Identity();
	FQuaterniond EndRotation = FQuaterniond::Identity();
	bool bInterpolateRotation = false;			// if false, only StartRotation is used

	FVector3d StartTranslation = FVector3d::Zero();
	FVector3d EndTranslation = FVector3d::Zero();
	bool bInterpolateTranslation = false;		// if false, only StartTranslation is used

	FVector3d StartScale = FVector3d::One();
	FVector3d EndScale = FVector3d::One();
	bool bInterpolateScale = false;				// if false, only StartScale is used

	FAxisAlignedBox3d Dimensions = FAxisAlignedBox3d(FVector3d::Zero(), 10.0);

public:
	// current result pattern
	TArray<FTransformSRT3d> Pattern;


public:
	void ResetPattern()
	{
		Pattern.Reset();
	}

	void AddPatternElement(FTransformSRT3d NewTransform, double Alpha)
	{
		ApplyElementTransforms(NewTransform, Alpha);
		Pattern.Add(NewTransform);
	}


	void ApplyElementTransforms(FTransformSRT3d& Transform, double Alpha)
	{
		if (bInterpolateRotation)
		{
			FQuaterniond InterpRotation(StartRotation, EndRotation, Alpha);
			Transform.SetRotation(Transform.GetRotation() * InterpRotation);
		}
		else
		{
			Transform.SetRotation(Transform.GetRotation() * StartRotation);
		}

		if (bInterpolateScale)
		{
			Transform.SetScale( Transform.GetScale() * Lerp(StartScale, EndScale, Alpha) );
		}
		else
		{
			Transform.SetScale( Transform.GetScale() * StartScale );
		}

		if (bInterpolateTranslation)
		{
			Transform.SetTranslation( Transform.GetTranslation() + Lerp(StartTranslation, EndTranslation, Alpha) );
		}
		else
		{
			Transform.SetTranslation( Transform.GetTranslation() + StartTranslation );
		}
	}



	double GetAlpha(double AlphaStep, int32 k, int32 Iterations, bool bForceLastStepToOne)
	{
		if (k == 0)
		{
			return 0;
		}
		else if (bForceLastStepToOne && k == Iterations-1)
		{
			return 1.0;
		}
		else
		{
			return FMathd::Clamp((double)k * AlphaStep, 0.0, 1.0);
		}
	}

	void ComputeSteps(FPatternGenerator::ESpacingMode UseSpacingMode, int32 CountIn, double StepSizeIn, double LengthIn, int32& Iterations, double& AlphaStep, bool& bForceLastStepToOne)
	{
		Iterations = 1;
		AlphaStep = 0.5;
		bForceLastStepToOne = true;

		if (UseSpacingMode == ESpacingMode::ByCount)
		{
			Iterations = CountIn;
			AlphaStep = 1.0 / FMath::Max(Iterations-1, 1);
		}
		else if (UseSpacingMode == ESpacingMode::ByStepSize)
		{
			bForceLastStepToOne = false;
			AlphaStep = StepSizeIn / LengthIn;
			Iterations = FMath::Clamp( (int)(1.0 / (AlphaStep + FMathd::ZeroTolerance) ) + 1, 1, 1000);
		}
		else if (UseSpacingMode == ESpacingMode::Packed)
		{
			bForceLastStepToOne = false;
			AlphaStep = StepSizeIn / LengthIn;
			Iterations = FMath::Clamp( (int)(1.0 / (AlphaStep + FMathd::ZeroTolerance) ) + 1, 1, 1000);
		}
		else
		{
			check(false);
		}
	}

};



class FLinearPatternGenerator : public FPatternGenerator
{
public:

	FFrame3d StartFrame;
	FFrame3d EndFrame;

	ESpacingMode SpacingMode = ESpacingMode::ByCount;

	int Axis = 0;
	int32 Count = 5;
	double StepSize = 1.0;

	// Not used for LineFill
	ESpacingMode SpacingModeY = ESpacingMode::ByCount;
	int AxisY = 1;
	int32 CountY = 5;
	double StepSizeY = 1.0;


	enum class EFillMode
	{
		LineFill,
		RectangleFill
	};
	EFillMode FillMode = EFillMode::LineFill;


	void UpdatePattern()
	{
		ResetPattern();

		if (FillMode == EFillMode::LineFill)
		{
			UpdatePattern_LineFill();
		}
		else if (FillMode == EFillMode::RectangleFill)
		{
			UpdatePattern_RectangleFill();
		}
		else
		{
			ensure(false);
			AddPatternElement(StartFrame.ToTransform(), 1.0);
		}
	}


	void UpdatePattern_LineFill();
	void UpdatePattern_RectangleFill();

};


void FLinearPatternGenerator::UpdatePattern_LineFill()
{
	double LineLength = Distance(StartFrame.Origin, EndFrame.Origin);

	int32 Iterations = 1;
	double AlphaStep = 0.5;
	bool bForceLastStepToOne = true;
	double UseStepSize = (SpacingMode == ESpacingMode::Packed) ? Dimensions.Dimension(Axis) : StepSize;
	ComputeSteps(SpacingMode, Count, UseStepSize, LineLength, Iterations, AlphaStep, bForceLastStepToOne);

	for (int32 k = 0; k < Iterations; ++k)
	{
		FFrame3d PatternFrame = StartFrame;
		double Alpha = 0;
		if (bForceLastStepToOne && k == Iterations-1)
		{
			Alpha = 1;
			PatternFrame = EndFrame;
		}
		else if (k != 0)
		{
			Alpha = GetAlpha(AlphaStep, k, Iterations, bForceLastStepToOne);
			PatternFrame = Lerp(StartFrame, EndFrame, Alpha);
		}

		AddPatternElement(PatternFrame.ToTransform(), Alpha);
	}
}



void FLinearPatternGenerator::UpdatePattern_RectangleFill()
{
	FVector3d LocalPt = StartFrame.ToFramePoint(EndFrame.Origin);
	double ExtentX = LocalPt[Axis];
	double ExtentY = LocalPt[AxisY];

	int32 IterationsX = 1;
	double AlphaStepX = 0.5;
	bool bForceLastStepToOneX = true;
	double UseStepSizeX = (SpacingMode == ESpacingMode::Packed) ? Dimensions.Dimension(Axis) : StepSize;
	ComputeSteps(SpacingMode, Count, UseStepSizeX, ExtentX, IterationsX, AlphaStepX, bForceLastStepToOneX);

	int32 IterationsY = 1;
	double AlphaStepY = 0.5;
	bool bForceLastStepToOneY = true;
	double UseStepSizeY = (SpacingModeY == ESpacingMode::Packed) ? Dimensions.Dimension(AxisY): StepSizeY;
	ComputeSteps(SpacingModeY, CountY, UseStepSizeY, ExtentY, IterationsY, AlphaStepY, bForceLastStepToOneY);

	FFrame3d MidFrameOrientation = Lerp(StartFrame, EndFrame, 0.5);
	FFrame3d Frame00 = StartFrame;
	FFrame3d Frame11 = EndFrame;
	FFrame3d Frame10 = StartFrame;
	Frame10.Origin += ExtentX * StartFrame.GetAxis(Axis);
	Frame10.Rotation = MidFrameOrientation.Rotation;
	FFrame3d Frame01 = StartFrame;
	Frame01.Origin += ExtentY * StartFrame.GetAxis(AxisY);
	Frame01.Rotation = MidFrameOrientation.Rotation;

	for (int32 yi = 0; yi < IterationsY; ++yi)
	{
		double AlphaY = GetAlpha(AlphaStepY, yi, IterationsY, bForceLastStepToOneY);
		FFrame3d Frame0 = Lerp(Frame00, Frame01, AlphaY);
		FFrame3d Frame1 = Lerp(Frame10, Frame11, AlphaY);

		for (int32 xi = 0; xi < IterationsX; ++xi)
		{
			double AlphaX = GetAlpha(AlphaStepX, xi, IterationsX, bForceLastStepToOneX);
			FFrame3d PatternFrame = Lerp(Frame0, Frame1, AlphaX);

			AddPatternElement(PatternFrame.ToTransform(), AlphaX*AlphaY);
		}
	}
}




class FRadialPatternGenerator : public FPatternGenerator
{
public:
	FFrame3d CenterFrame;

	double StartAngleDeg = 0.0;
	double EndAngleDeg = 360.0;
	double AngleShift = 0.0;

	double Radius = 100.0;

	bool bOriented = true;
	int AxisIndexToAlign = 0;	// depends on SinglePlane of tool

	ESpacingMode SpacingMode = ESpacingMode::ByCount;

	int32 Count = 5;
	double StepSizeDeg = 1.0;

	enum class EFillMode
	{
		CircleFill,
	};
	EFillMode FillMode = EFillMode::CircleFill;


	void UpdatePattern()
	{
		ResetPattern();

		if (FillMode == EFillMode::CircleFill)
		{
			UpdatePattern_CircleFill();
		}
	}

	void UpdatePattern_CircleFill();

};


void FRadialPatternGenerator::UpdatePattern_CircleFill()
{
	double ArcLengthDeg = FMathd::Abs(EndAngleDeg - StartAngleDeg);

	int32 Iterations = 1;
	double AlphaStep = 0.5;

	if (SpacingMode == ESpacingMode::ByCount)
	{
		Iterations = Count;
		AlphaStep = 1.0 / FMath::Max(Iterations, 1);
	}
	else if (SpacingMode == ESpacingMode::ByStepSize)
	{
		AlphaStep = StepSizeDeg / ArcLengthDeg;
		Iterations = FMath::Clamp( (int)(1.0 / AlphaStep) + 1, 1, 1000);
	}
	else if (SpacingMode == ESpacingMode::Packed)
	{
		double ArcLenStepSize = Dimensions.Dimension(0);		// currently only support X axis in circle pattern?
		double CalcStepSizeRad = ArcLenStepSize / Radius;
		AlphaStep = (CalcStepSizeRad * FMathd::RadToDeg) / ArcLengthDeg;
		Iterations = FMath::Clamp( (int)(1.0 / AlphaStep) + 1, 1, 1000);
	}
	else
	{
		check(false);
	}

	for (int32 k = 0; k < Iterations; ++k)
	{
		double Alpha = (double)k * AlphaStep;
		if (k == 0)
		{
			Alpha = 0;
		}
		Alpha = FMathd::Clamp(Alpha, 0.0, 1.0);

		double Angle = (1.0-Alpha)*StartAngleDeg + (Alpha)*EndAngleDeg;
		Angle += AngleShift;
		double FrameX = Radius * FMathd::Cos(Angle * FMathd::DegToRad);
		double FrameY = Radius * FMathd::Sin(Angle * FMathd::DegToRad);

		FFrame3d PatternFrame(CenterFrame.FromPlaneUV(FVector2d(FrameX, FrameY), 2));

		if (bOriented)
		{
			FVector3d Axis = Normalized(PatternFrame.Origin - CenterFrame.Origin);
			if (IsNormalized(Axis))
			{
				PatternFrame.ConstrainedAlignAxis(AxisIndexToAlign, Axis, CenterFrame.Z());
			}
		}

		AddPatternElement(PatternFrame.ToTransform(), Alpha);
	}

}




/*
 * Tool
 * 
 * TODO: convert dynamic mesh to temporary static mesh to allow instance rendering (does it work?)
 * TODO: when outputting a merged dynamic mesh, scale/rotation will be baked in, and so there are no limits
 *       on non-uniform scaling, but it would need to be baked into the preview dynamicmesh components 
 *       (and for static meshes being converted, static preview would have to become dynamic preview)
 * TODO: investigate using temporary ISMComponents instead of multiple SMComponents
 * 
 */

UPatternTool::UPatternTool()
{
}


void UPatternTool::Setup()
{
	UInteractiveTool::Setup();

	PreviewGeometry = NewObject<UPreviewGeometry>(this);
	PreviewGeometry->CreateInWorld(GetTargetWorld(), FTransform::Identity);

	// Must be done before creating gizmos, so that we can bind the mechanic to them.
	DragAlignmentMechanic = NewObject<UDragAlignmentMechanic>(this);
	DragAlignmentMechanic->Setup(this);

	Settings = NewObject<UPatternToolSettings>();
	AddToolPropertySource(Settings);
	Settings->RestoreProperties(this);
	Settings->WatchProperty(Settings->Shape, [this](EPatternToolShape) { OnShapeUpdated(); } );
	Settings->WatchProperty(Settings->SingleAxis, [this](EPatternToolSingleAxis) { OnParametersUpdated(); } );
	Settings->WatchProperty(Settings->SinglePlane, [this](EPatternToolSinglePlane) { OnParametersUpdated(); } );
	Settings->WatchProperty(Settings->bHideSources, [this](bool bNewValue) { OnSourceVisibilityToggled(!bNewValue); } );

	LinearSettings = NewObject<UPatternTool_LinearSettings>();
	AddToolPropertySource(LinearSettings);
	LinearSettings->RestoreProperties(this);
	SetToolPropertySourceEnabled(LinearSettings, false);

	GridSettings = NewObject<UPatternTool_GridSettings>();
	AddToolPropertySource(GridSettings);
	GridSettings->RestoreProperties(this);
	SetToolPropertySourceEnabled(GridSettings, false);

	RadialSettings = NewObject<UPatternTool_RadialSettings>();
	AddToolPropertySource(RadialSettings);
	RadialSettings->RestoreProperties(this);
	SetToolPropertySourceEnabled(RadialSettings, false);

	RotationSettings = NewObject<UPatternTool_RotationSettings>();
	AddToolPropertySource(RotationSettings);
	RotationSettings->RestoreProperties(this);
	
	TranslationSettings = NewObject<UPatternTool_TranslationSettings>();
	AddToolPropertySource(TranslationSettings);
	TranslationSettings->RestoreProperties(this);

	ScaleSettings = NewObject<UPatternTool_ScaleSettings>();
	AddToolPropertySource(ScaleSettings);
	ScaleSettings->RestoreProperties(this);

	auto OnUniformChanged = [this](bool bNewValue)
	{
		if (bNewValue)
		{
			constexpr float Tolerance = 1E-08;
			const FVector DefaultDirection = FVector::OneVector.GetUnsafeNormal();
			
			StartScaleDirection = ScaleSettings->StartScale.GetSafeNormal(Tolerance, DefaultDirection);
			EndScaleDirection = ScaleSettings->EndScale.GetSafeNormal(Tolerance, DefaultDirection);
		}
	};
	ScaleSettings->WatchProperty(ScaleSettings->bUniform, OnUniformChanged);
	
	StartScaleWatcherIdx = ScaleSettings->WatchProperty(ScaleSettings->StartScale, [this](const FVector& NewStartScale)
	{
		if (ScaleSettings->bUniform)
		{
			ScaleSettings->StartScale = StartScaleDirection * NewStartScale.Size();
			ScaleSettings->SilentUpdateWatcherAtIndex(StartScaleWatcherIdx);
		}
	});
	
	EndScaleWatcherIdx = ScaleSettings->WatchProperty(ScaleSettings->EndScale, [this](const FVector& NewEndScale)
	{
		if (ScaleSettings->bUniform)
		{
			ScaleSettings->EndScale = EndScaleDirection * NewEndScale.Size();
			ScaleSettings->SilentUpdateWatcherAtIndex(EndScaleWatcherIdx);
		}
	});

	// Initialize StartScaleDirection and EndScaleDirection
	OnUniformChanged(true);
	
	OutputSettings = NewObject<UPatternTool_OutputSettings>();
	AddToolPropertySource(OutputSettings);
	OutputSettings->RestoreProperties(this);

	InitializeElements();
	for (const FPatternElement& Element : Elements)
	{
		if (Element.SourceStaticMesh != nullptr)
		{
			OutputSettings->bHaveStaticMeshes = true;
		}
	}

	CurrentStartFrameWorld = FFrame3d(Elements[0].SourceTransform);

	OnShapeUpdated();

	PlaneMechanic = NewObject<UConstructionPlaneMechanic>(this);
	PlaneMechanic->Setup(this);
	PlaneMechanic->Initialize( GetTargetWorld(), CurrentStartFrameWorld );
	PlaneMechanic->OnPlaneChanged.AddLambda([this]() { OnMainFrameUpdated(); });

	SetToolDisplayName(LOCTEXT("ToolName", "Pattern"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartPatternTool", "Create Patterns for the selected Objects"),
		EToolMessageLevel::UserNotification);

	if (bHaveNonUniformScaleElements)
	{
		// todo: this message only applies in certain contexts like if bSeparateActors=true or if 
		// emitting an ISMC. Should make it dynamic and/or smarter.
		GetToolManager()->DisplayMessage(LOCTEXT("NonUniformScaleWarning", 
			"Source Objects have Non-Uniform Scaling, which may prevent Pattern Transforms from working correctly."), EToolMessageLevel::UserWarning);
	}
}







void UPatternTool::OnShutdown(EToolShutdownType ShutdownType)
{
	PlaneMechanic->Shutdown();
	PlaneMechanic = nullptr;

	// destroy all the preview components we created
	for (UPrimitiveComponent* Component : AllComponents)
	{
		Component->UnregisterComponent();
		Component->DestroyComponent();
	}
	AllComponents.Reset();
	PreviewComponents.Reset();
	StaticMeshPools.Reset();
	DynamicMeshPools.Reset();

	PreviewGeometry->Disconnect();

	Settings->SaveProperties(this);
	LinearSettings->SaveProperties(this);
	GridSettings->SaveProperties(this);
	RadialSettings->SaveProperties(this);
	RotationSettings->SaveProperties(this);
	TranslationSettings->SaveProperties(this);
	ScaleSettings->SaveProperties(this);
	OutputSettings->SaveProperties(this);

	DragAlignmentMechanic->Shutdown();

	GetToolManager()->GetPairedGizmoManager()->DestroyAllGizmosByOwner(this);

	OnSourceVisibilityToggled(true);

	if (ShutdownType == EToolShutdownType::Accept)
	{
		EmitResults();
	}
}




void UPatternTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	DragAlignmentMechanic->Render(RenderAPI);
	PlaneMechanic->Render(RenderAPI);

	if (bPatternNeedsUpdating)
	{
		// throttle update rate as it is somewhat expensive
		double DeltaTime = (FDateTime::Now() - LastPatternUpdateTime).GetTotalSeconds();
		if (DeltaTime > 0.05)
		{
			UpdatePattern();
			bPatternNeedsUpdating = false;
			LastPatternUpdateTime = FDateTime::Now();
		}
	}
}



void UPatternTool::OnSourceVisibilityToggled(bool bVisible)
{
	for (int32 k = 0; k < Targets.Num(); ++k)
	{
		UE::ToolTarget::SetSourceObjectVisible(Targets[k], bVisible);
	}
}


void UPatternTool::MarkPatternDirty()
{
	if (bPatternNeedsUpdating == false)
	{
		bPatternNeedsUpdating = true;
		LastPatternUpdateTime = FDateTime::Now();
	}
}




void UPatternTool::InitializeElements()
{
	int32 NumElements = Targets.Num();
	Elements.SetNum(NumElements);

	for (int32 TargetIdx = 0; TargetIdx < NumElements; TargetIdx++)
	{
		FPatternElement& Element = Elements[TargetIdx];
		Element.TargetIndex = TargetIdx;

		Element.SourceComponent = UE::ToolTarget::GetTargetComponent(Targets[TargetIdx]);
		Element.SourceMaterials = UE::ToolTarget::GetMaterialSet(Targets[TargetIdx], false).Materials;
		Element.SourceTransform = UE::ToolTarget::GetLocalToWorldTransform(Targets[TargetIdx]);
		Element.BaseRotateScale = Element.SourceTransform;
		Element.BaseRotateScale.SetTranslation(FVector3d::Zero());		// clear translation from base transform
		Element.SourceTransform.SetRotation(FQuaterniond::Identity());	// clear rotate/scale from source transform, so only location is used
		Element.SourceTransform.SetScale(FVector::One());

		bHaveNonUniformScaleElements = bHaveNonUniformScaleElements || Element.BaseRotateScale.HasNonUniformScale();

		if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Element.SourceComponent))
		{
			Element.SourceStaticMesh = StaticMeshComp->GetStaticMesh();
			Element.LocalBounds = (FAxisAlignedBox3d)Element.SourceStaticMesh->GetBounds().GetBox();
			Element.PatternBounds = Element.LocalBounds;
		}
		else if (UDynamicMeshComponent* DynamicMeshComp = Cast<UDynamicMeshComponent>(Element.SourceComponent))
		{
			Element.SourceDynamicMesh = DynamicMeshComp->GetDynamicMesh();
			Element.SourceDynamicMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) {
				Element.LocalBounds = Mesh.GetBounds(true);
			});
			Element.PatternBounds = Element.LocalBounds;
		}
		else
		{
			Element.bValid = false;
		}
	}

	PreviewComponents.SetNum(NumElements);
}




void UPatternTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (PropertySet == RadialSettings 
		|| PropertySet == LinearSettings 
		|| PropertySet == GridSettings
		|| PropertySet == RotationSettings
		|| PropertySet == TranslationSettings
		|| PropertySet == ScaleSettings )
	{
		OnParametersUpdated();
	}
}


void UPatternTool::OnMainFrameUpdated()
{
	CurrentStartFrameWorld = PlaneMechanic->Plane;
	MarkPatternDirty();
}


void UPatternTool::OnShapeUpdated()
{
	bool bLinearSettings = (Settings->Shape == EPatternToolShape::Line);
	bool bGridSettings = (Settings->Shape == EPatternToolShape::Grid);
	bool bRadialSettings = (Settings->Shape == EPatternToolShape::Circle);

	SetToolPropertySourceEnabled(LinearSettings, bLinearSettings);
	SetToolPropertySourceEnabled(GridSettings, bGridSettings);
	SetToolPropertySourceEnabled(RadialSettings, bRadialSettings);

	OnParametersUpdated();
}


void UPatternTool::OnParametersUpdated()
{
	MarkPatternDirty();
}




void UPatternTool::ResetPreviews()
{
	int32 NumElements = Elements.Num();
	if (PreviewComponents.Num() != NumElements)
	{
		return;
	}

	for (int32 ElemIdx = 0; ElemIdx < NumElements; ++ElemIdx)
	{
		FPatternElement& Element = Elements[ElemIdx];
		if (Element.bValid)
		{
			FComponentSet& ElemComponents = PreviewComponents[ElemIdx];
			if (Element.SourceStaticMesh)
			{
				ReturnStaticMeshes(Element, ElemComponents);
			}
			else if (Element.SourceDynamicMesh != nullptr)
			{
				ReturnDynamicMeshes(Element, ElemComponents);
			}
		}
	}
}



static void InitializeGenerator(FPatternGenerator& Generator, UPatternTool* Tool)
{
	Generator.bInterpolateRotation = Tool->RotationSettings->bInterpolate;
	if (Generator.bInterpolateRotation)
	{
		Generator.StartRotation = FQuaterniond(Tool->RotationSettings->StartRotation);
		Generator.EndRotation = FQuaterniond(Tool->RotationSettings->EndRotation);
	}
	else
	{
		Generator.StartRotation = Generator.EndRotation = FQuaterniond(Tool->RotationSettings->StartRotation);
	}

	Generator.bInterpolateTranslation = Tool->TranslationSettings->bInterpolate;
	if (Generator.bInterpolateTranslation)
	{
		Generator.StartTranslation = Tool->TranslationSettings->StartTranslation;
		Generator.EndTranslation = Tool->TranslationSettings->EndTranslation;
	}
	else
	{
		Generator.StartTranslation = Generator.EndTranslation = Tool->TranslationSettings->StartTranslation;
	}

	Generator.bInterpolateScale = Tool->ScaleSettings->bInterpolate;
	if (Generator.bInterpolateScale)
	{
		Generator.StartScale = Tool->ScaleSettings->StartScale;
		Generator.EndScale = Tool->ScaleSettings->EndScale;
	}
	else
	{
		Generator.StartScale = Generator.EndScale = Tool->ScaleSettings->StartScale;
	}
}



void UPatternTool::GetPatternTransforms_Linear(TArray<UE::Geometry::FTransformSRT3d>& TransformsOut)
{
	FLinearPatternGenerator Generator;
	InitializeGenerator(Generator, this);
	Generator.Dimensions = this->Elements[0].PatternBounds;

	double ExtentX = LinearSettings->Extent;

	Generator.StartFrame = FFrame3d();
	Generator.Axis = (int32)Settings->SingleAxis;
	FVector3d AxisX = Generator.StartFrame.GetAxis(Generator.Axis);

	if (LinearSettings->bCentered)
	{
		Generator.StartFrame.Origin -= 0.5 * ExtentX * AxisX;
	}

	Generator.EndFrame = Generator.StartFrame;
	Generator.EndFrame.Origin += ExtentX * AxisX;

	Generator.FillMode = FLinearPatternGenerator::EFillMode::LineFill;

	Generator.SpacingMode = (FLinearPatternGenerator::ESpacingMode)(int)LinearSettings->SpacingMode;
	Generator.Count = LinearSettings->Count;
	Generator.StepSize = LinearSettings->StepSize;

	Generator.UpdatePattern();
	TransformsOut = MoveTemp(Generator.Pattern);
}



void UPatternTool::GetPatternTransforms_Grid(TArray<UE::Geometry::FTransformSRT3d>& TransformsOut)
{
	FLinearPatternGenerator Generator;
	InitializeGenerator(Generator, this);
	Generator.Dimensions = this->Elements[0].PatternBounds;

	double ExtentX = GridSettings->ExtentX;
	double ExtentY = GridSettings->ExtentY;

	Generator.StartFrame = FFrame3d();
	switch (Settings->SinglePlane)
	{
	case EPatternToolSinglePlane::XYPlane:
		Generator.Axis = 0; Generator.AxisY = 1; break;
	case EPatternToolSinglePlane::XZPlane:
		Generator.Axis = 0; Generator.AxisY = 2; break;
	case EPatternToolSinglePlane::YZPlane:
		Generator.Axis = 1; Generator.AxisY = 2; break;
	}
	FVector3d AxisX = Generator.StartFrame.GetAxis(Generator.Axis);
	FVector3d AxisY = Generator.StartFrame.GetAxis(Generator.AxisY);

	if (GridSettings->bCenteredX)
	{
		Generator.StartFrame.Origin -= 0.5 * ExtentX * AxisX;
	}
	if (GridSettings->bCenteredY)
	{
		Generator.StartFrame.Origin -= 0.5 * ExtentY * AxisY;
	}

	Generator.EndFrame = Generator.StartFrame;
	Generator.EndFrame.Origin += ExtentX * AxisX + ExtentY * AxisY;

	Generator.FillMode = FLinearPatternGenerator::EFillMode::RectangleFill;

	Generator.SpacingMode = (FLinearPatternGenerator::ESpacingMode)(int)GridSettings->SpacingX;
	Generator.Count = GridSettings->CountX;
	Generator.StepSize = GridSettings->StepSizeX;

	Generator.SpacingModeY = (FLinearPatternGenerator::ESpacingMode)(int)GridSettings->SpacingY;
	Generator.CountY = GridSettings->CountY;
	Generator.StepSizeY = GridSettings->StepSizeY;

	Generator.UpdatePattern();
	TransformsOut = MoveTemp(Generator.Pattern);
}




void UPatternTool::GetPatternTransforms_Radial(TArray<UE::Geometry::FTransformSRT3d>& TransformsOut)
{
	FRadialPatternGenerator Generator;
	InitializeGenerator(Generator, this);
	Generator.Dimensions = this->Elements[0].PatternBounds;

	// Orient the CenterFrame based on the plane setting of the tool & determine which axis is used if bOriented is true
	const FVector3d Origin(0, 0, 0);
	switch (Settings->SinglePlane)
	{
	case EPatternToolSinglePlane::XYPlane:
		Generator.CenterFrame = FFrame3d(Origin, FVector3d(0, 0, 1));
		Generator.AxisIndexToAlign = 0;
		break;
	case EPatternToolSinglePlane::XZPlane:
		Generator.CenterFrame = FFrame3d(Origin, FVector3d(0, 1, 0));
		Generator.AxisIndexToAlign = 2;
		break;
	case EPatternToolSinglePlane::YZPlane:
		Generator.CenterFrame = FFrame3d(Origin, FVector3d(1, 0, 0));
		Generator.AxisIndexToAlign = 2;
		break;
	default:
		Generator.CenterFrame = FFrame3d();
	}
	
	Generator.StartAngleDeg = RadialSettings->StartAngle;
	Generator.EndAngleDeg = RadialSettings->EndAngle;
	Generator.AngleShift = RadialSettings->AngleShift;

	Generator.Radius = RadialSettings->Radius;

	Generator.bOriented = RadialSettings->bOriented;

	Generator.SpacingMode = (FRadialPatternGenerator::ESpacingMode)(int)RadialSettings->SpacingMode;

	Generator.FillMode = FRadialPatternGenerator::EFillMode::CircleFill;
	
	Generator.Count = RadialSettings->Count;
	Generator.StepSizeDeg = RadialSettings->StepSize;

	Generator.UpdatePattern();
	TransformsOut = MoveTemp(Generator.Pattern);
}



void UPatternTool::UpdatePattern()
{
	TArray<FTransformSRT3d> Pattern;

	if (Settings->Shape == EPatternToolShape::Line)
	{
		GetPatternTransforms_Linear(Pattern);
	}
	if (Settings->Shape == EPatternToolShape::Grid)
	{
		GetPatternTransforms_Grid(Pattern);
	}
	else if (Settings->Shape == EPatternToolShape::Circle)
	{
		GetPatternTransforms_Radial(Pattern);
	}

	// Return all current preview components in use to the preview component pool
	ResetPreviews();

	int32 NumPatternItems = Pattern.Num();
	int32 NumElements = Elements.Num();
	check(PreviewComponents.Num() == NumElements);

	for (int32 ElemIdx = 0; ElemIdx < NumElements; ++ElemIdx)
	{
		FPatternElement& Element = Elements[ElemIdx];
		FTransformSRT3d ElementTransform = Element.BaseRotateScale;
		FComponentSet& ElemComponents = PreviewComponents[ElemIdx];

		for (int32 k = 0; k < NumPatternItems; ++k)
		{
			UPrimitiveComponent* Component = nullptr;
			if (Element.SourceDynamicMesh != nullptr)
			{
				Component = GetPreviewDynamicMesh(Element);
			}
			else if (Element.SourceStaticMesh != nullptr)
			{
				Component = GetPreviewStaticMesh(Element);
			}
			if (Component != nullptr)
			{
				FTransform PatternTransform = (FTransform)Pattern[k];
				FTransform WorldTransform = (FTransform)ElementTransform * PatternTransform * CurrentStartFrameWorld.ToFTransform();
				Component->SetWorldTransform( WorldTransform );
				Component->SetVisibility(true);

				ElemComponents.Components.Add(Component);
			}

		}
	}

	// ResetPreviews() does not hide the preview components because changing their visibility
	// will destroy their SceneProxies, which is expensive, and in most cases the same set of
	// objects will immediately be shown, re-creating the proxy. So instead they are just all
	// left visible, and HideReturnedPreviewMeshes() fixes up the visibility on any Components
	// returned to the pools that were not immediately re-used
	HideReturnedPreviewMeshes();

	CurrentPattern = MoveTemp(Pattern);
}




UStaticMeshComponent* UPatternTool::GetPreviewStaticMesh(FPatternElement& Element)
{
	FComponentSet* FoundPool = StaticMeshPools.Find(Element.TargetIndex);
	if (FoundPool == nullptr)
	{
		StaticMeshPools.Add(Element.TargetIndex, FComponentSet{});
		FoundPool = StaticMeshPools.Find(Element.TargetIndex);
		check(FoundPool != nullptr);
	}

	if (FoundPool->Components.Num() > 0)
	{
		UStaticMeshComponent* FoundComponent = Cast<UStaticMeshComponent>(FoundPool->Components.Pop(false));
		check(FoundComponent != nullptr);
		return FoundComponent;
	}

	UStaticMeshComponent* StaticMeshComp = NewObject<UStaticMeshComponent>(PreviewGeometry->GetActor());

	StaticMeshComp->SetStaticMesh(Element.SourceStaticMesh);
	for (int32 j = 0; j < Element.SourceMaterials.Num(); ++j)
	{
		StaticMeshComp->SetMaterial(j, Element.SourceMaterials[j]);
	}

	StaticMeshComp->SetupAttachment(PreviewGeometry->GetActor()->GetRootComponent());
	StaticMeshComp->RegisterComponent();

	AllComponents.Add(StaticMeshComp);

	return StaticMeshComp;
}

void UPatternTool::ReturnStaticMeshes(FPatternElement& Element, FComponentSet& ComponentSet)
{
	if (ComponentSet.Components.Num() == 0) return;

	FComponentSet* FoundPool = StaticMeshPools.Find(Element.TargetIndex);
	check(FoundPool != nullptr);		// should never happen as we would have created in GetPreviewStaticMesh()

	for (UPrimitiveComponent* Component : ComponentSet.Components)
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			FoundPool->Components.Add(StaticMeshComponent);
		}
	}

	ComponentSet.Components.Reset();
}





UDynamicMeshComponent* UPatternTool::GetPreviewDynamicMesh(FPatternElement& Element)
{
	FComponentSet* FoundPool = DynamicMeshPools.Find(Element.TargetIndex);
	if (FoundPool == nullptr)
	{
		DynamicMeshPools.Add(Element.TargetIndex, FComponentSet{});
		FoundPool = DynamicMeshPools.Find(Element.TargetIndex);
		check(FoundPool != nullptr);
	}

	if (FoundPool->Components.Num() > 0)
	{
		UDynamicMeshComponent* FoundComponent = Cast<UDynamicMeshComponent>(FoundPool->Components.Pop(false));
		check(FoundComponent != nullptr);
		return FoundComponent;
	}

	UDynamicMeshComponent* DynamicMeshComp = NewObject<UDynamicMeshComponent>(PreviewGeometry->GetActor());

	DynamicMeshComp->SetGenerateOverlapEvents(false);
	DynamicMeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DynamicMeshComp->CollisionType = ECollisionTraceFlag::CTF_UseSimpleAsComplex;

	DynamicMeshComp->ConfigureMaterialSet(Element.SourceMaterials);
	DynamicMeshComp->SetupAttachment(PreviewGeometry->GetActor()->GetRootComponent());
	DynamicMeshComp->RegisterComponent();

	FDynamicMesh3 ElementMeshCopy(Element.SourceDynamicMesh->GetMeshRef());
	DynamicMeshComp->SetMesh(MoveTemp(ElementMeshCopy));

	AllComponents.Add(DynamicMeshComp);

	return DynamicMeshComp;
}

void UPatternTool::ReturnDynamicMeshes(FPatternElement& Element, FComponentSet& ComponentSet)
{
	if (ComponentSet.Components.Num() == 0) return;

	FComponentSet* FoundPool = DynamicMeshPools.Find(Element.TargetIndex);
	check(FoundPool != nullptr);		// should never happen as we would have created in GetPreviewStaticMesh()

	for (UPrimitiveComponent* Component : ComponentSet.Components)
	{
		if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
		{
			FoundPool->Components.Add(DynamicMeshComponent);
		}
	}

	ComponentSet.Components.Reset();
}



void UPatternTool::HideReturnedPreviewMeshes()
{
	for (TPair<int32, FComponentSet> Pair : StaticMeshPools)
	{
		for (UPrimitiveComponent* Component : Pair.Value.Components)
		{
			if (Component->IsVisible())
			{
				Component->SetVisibility(false);
			}
		}
	}
	for (TPair<int32, FComponentSet> Pair : DynamicMeshPools)
	{
		for (UPrimitiveComponent* Component : Pair.Value.Components)
		{
			if (Component->IsVisible())
			{
				Component->SetVisibility(false);
			}
		}
	}
}




void UPatternTool::EmitResults()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("CreatePattern", "Create Pattern"));

	bool bSeparateActorPerItem = OutputSettings->bSeparateActors;
	bool bCreateISMForStaticMeshes = OutputSettings->bCreateISMCs;
	bool bConvertToDynamic = OutputSettings->bConvertToDynamic;
	const UModelingComponentsSettings* ModelingSettings = GetDefault<UModelingComponentsSettings>();
	
	int32 NumElements = Elements.Num();
	int32 NumPatternItems = CurrentPattern.Num();

	TArray<AActor*> NewActors;		// set of new actors created by operation

	// TODO: investigate use of CopyPropertiesForUnrelatedObjects to transfer settings from source to target Components/Actors

	for (int32 ElemIdx = 0; ElemIdx < NumElements; ++ElemIdx)
	{
		FPatternElement& Element = Elements[ElemIdx];
		if (Element.bValid == false)
		{
			continue;
		}
		FTransformSRT3d ElementTransform = Element.BaseRotateScale;

		if (Element.SourceDynamicMesh != nullptr || bConvertToDynamic )
		{
			FDynamicMesh3 ElementMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[ElemIdx], true);

			// this lambda creates a new dynamic mesh actor w/ the materials from the current Element
			auto EmitDynamicMeshActor = [this, &Element, ModelingSettings](FDynamicMesh3&& MoveMesh, FString BaseName, FTransformSRT3d Transform)
			{
				FCreateMeshObjectParams NewMeshObjectParams;
				NewMeshObjectParams.TargetWorld = GetTargetWorld();
				NewMeshObjectParams.Transform = (FTransform)Transform;
				NewMeshObjectParams.BaseName = BaseName;
				NewMeshObjectParams.Materials = Element.SourceMaterials;
				NewMeshObjectParams.SetMesh(MoveTemp(MoveMesh));
				NewMeshObjectParams.TypeHint = ECreateObjectTypeHint::DynamicMeshActor;

				NewMeshObjectParams.bEnableCollision = ModelingSettings->bEnableCollision;
				NewMeshObjectParams.CollisionMode = ModelingSettings->CollisionMode;
				NewMeshObjectParams.bEnableRaytracingSupport = ModelingSettings->bEnableRayTracing;

				FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
				return Result;
			};


			if (bSeparateActorPerItem)
			{
				for (int32 k = 0; k < NumPatternItems; ++k)
				{
					FTransformSRT3d PatternTransform = CurrentPattern[k];
					FTransform WorldTransform = (FTransform)ElementTransform * (FTransform)PatternTransform * CurrentStartFrameWorld.ToFTransform();

					FDynamicMesh3 PatternItemMesh = ElementMesh;
					// TODO: may need to bake nonuniform scale in here, if we allow scaling in transform
					FCreateMeshObjectResult Result = EmitDynamicMeshActor(MoveTemp(PatternItemMesh),
						FString::Printf(TEXT("Pattern_%d_%d"), ElemIdx, k), WorldTransform);
					if (Result.IsOK())
					{
						NewActors.Add(Result.NewActor);
					}
				}
			}
			else
			{
				FDynamicMesh3 CombinedPatternMesh;
				CombinedPatternMesh.EnableMatchingAttributes(ElementMesh, true);
				FDynamicMeshEditor Editor(&CombinedPatternMesh);
				FMeshIndexMappings Mappings;
				for (int32 k = 0; k < NumPatternItems; ++k)
				{
					FTransformSequence3d TransformSeq;
					TransformSeq.Append(ElementTransform);
					TransformSeq.Append(CurrentPattern[k]);
					Mappings.Reset();
					Editor.AppendMesh(&ElementMesh, Mappings,
						[&](int, const FVector3d& Position) { return TransformSeq.TransformPosition(Position); },
						[&](int, const FVector3d& Normal) { return TransformSeq.TransformNormal(Normal); } );
				}

				FCreateMeshObjectResult Result = EmitDynamicMeshActor(MoveTemp(CombinedPatternMesh),
					FString::Printf(TEXT("Pattern_%d"), ElemIdx), CurrentStartFrameWorld.ToFTransform());
				if (Result.IsOK())
				{
					NewActors.Add(Result.NewActor);
				}
			}
		}
		else if (Element.SourceStaticMesh != nullptr)
		{
			UStaticMesh* SetStaticMesh = Element.SourceStaticMesh;
			UStaticMeshComponent* SourceComponent = Cast<UStaticMeshComponent>(Elements[ElemIdx].SourceComponent);
			AActor* SourceActor = (SourceComponent != nullptr) ? SourceComponent->GetOwner() : nullptr;

			if (bSeparateActorPerItem)
			{
				for (int32 k = 0; k < NumPatternItems; ++k)
				{
					FTransformSRT3d PatternTransform = CurrentPattern[k];
					FTransform WorldTransform = (FTransform)ElementTransform * (FTransform)PatternTransform * CurrentStartFrameWorld.ToFTransform();

					FActorSpawnParameters SpawnInfo;
					SpawnInfo.Template = SourceActor;
					AStaticMeshActor* NewActor = GetTargetWorld()->SpawnActor<AStaticMeshActor>(SpawnInfo);
					if (NewActor != nullptr)
					{
						NewActor->SetActorTransform(WorldTransform);
						NewActors.Add(NewActor);
					}
				}
			}
			else if (bCreateISMForStaticMeshes)
			{
				FActorSpawnParameters SpawnInfo;
				AActor* NewActor = GetTargetWorld()->SpawnActor<AActor>(SpawnInfo);
				if (NewActor != nullptr)
				{
					NewActors.Add(NewActor);

					UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(NewActor);
					ISMComponent->SetFlags(RF_Transactional);
					ISMComponent->bHasPerInstanceHitProxies = true;

					ISMComponent->SetStaticMesh(Element.SourceStaticMesh);
					for (int32 j = 0; j < Element.SourceMaterials.Num(); ++j)
					{
						ISMComponent->SetMaterial(j, Element.SourceMaterials[j]);
					}

					NewActor->SetRootComponent(ISMComponent);
					ISMComponent->OnComponentCreated();
					NewActor->AddInstanceComponent(ISMComponent);

					NewActor->SetActorTransform(CurrentStartFrameWorld.ToFTransform());

					for (int32 k = 0; k < NumPatternItems; ++k)
					{
						FTransformSRT3d PatternTransform = CurrentPattern[k];
						ISMComponent->AddInstance((FTransform)ElementTransform * (FTransform)PatternTransform);
					}

					ISMComponent->RegisterComponent();
				}
			}
			else
			{
				// Emit a single StaticMeshActor with multiple StaticMeshComponents

				AStaticMeshActor* ParentActor = nullptr;
				UStaticMeshComponent* TemplateComponent = nullptr;
				for (int32 k = 0; k < NumPatternItems; ++k)
				{
					FTransformSRT3d PatternTransform = CurrentPattern[k];
					FTransform WorldTransform = (FTransform)PatternTransform * CurrentStartFrameWorld.ToFTransform();
					if (k == 0)
					{
						FActorSpawnParameters SpawnInfo;
						SpawnInfo.Template = SourceActor;
						ParentActor = GetTargetWorld()->SpawnActor<AStaticMeshActor>(SpawnInfo);
						ParentActor->SetActorTransform(CurrentStartFrameWorld.ToFTransform());
						ParentActor->GetStaticMeshComponent()->SetWorldTransform(WorldTransform);
					}
					else if (k == 1)
					{
						TemplateComponent = DuplicateObject<UStaticMeshComponent>(ParentActor->GetStaticMeshComponent(), ParentActor);
						TemplateComponent->ClearFlags(RF_DefaultSubObject);
						TemplateComponent->SetupAttachment(ParentActor->GetRootComponent());
						TemplateComponent->OnComponentCreated();
						ParentActor->AddInstanceComponent(TemplateComponent);
						TemplateComponent->RegisterComponent();
						TemplateComponent->SetWorldTransform( WorldTransform );
					}
					else
					{
						UStaticMeshComponent* NewCloneComponent = DuplicateObject<UStaticMeshComponent>(TemplateComponent, ParentActor);
						NewCloneComponent->ClearFlags(RF_DefaultSubObject);
						NewCloneComponent->SetupAttachment(ParentActor->GetRootComponent());
						NewCloneComponent->OnComponentCreated();
						ParentActor->AddInstanceComponent(NewCloneComponent);
						NewCloneComponent->RegisterComponent();
						NewCloneComponent->SetWorldTransform( WorldTransform );
					}
				}			

			}
		}
	}

	if (NewActors.Num() > 0)
	{
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewActors);
	}

	GetToolManager()->EndUndoTransaction();
}



#undef LOCTEXT_NAMESPACE
