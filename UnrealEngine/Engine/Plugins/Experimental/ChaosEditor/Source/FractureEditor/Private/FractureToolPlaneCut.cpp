// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureToolPlaneCut.h"
#include "FractureEditorStyle.h"
#include "PlanarCut.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "FractureEditorModeToolkit.h"
#include "FractureToolContext.h"
#include "Drawing/MeshDebugDrawing.h"
#include "FrameTypes.h"
#include "FractureToolBackgroundTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FractureToolPlaneCut)

using namespace UE::Fracture;

#define LOCTEXT_NAMESPACE "FracturePlanar"


UFractureToolPlaneCut::UFractureToolPlaneCut(const FObjectInitializer& ObjInit) 
	: Super(ObjInit) 
{
	PlaneCutSettings = NewObject<UFracturePlaneCutSettings>(GetTransientPackage(), UFracturePlaneCutSettings::StaticClass());
	PlaneCutSettings->OwnerTool = this;

	GizmoSettings = NewObject<UFractureTransformGizmoSettings>(GetTransientPackage(), UFractureTransformGizmoSettings::StaticClass());
	GizmoSettings->OwnerTool = this;
}


void UFractureToolPlaneCut::Setup()
{
	Super::Setup();
	GizmoSettings->Setup(this);
	PlaneCutSettings->bCanCutWithMultiplePlanes = !GizmoSettings->bUseGizmo;
	NotifyOfPropertyChangeByTool(PlaneCutSettings);
}


void UFractureToolPlaneCut::Shutdown()
{
	Super::Shutdown();
	GizmoSettings->Shutdown();
}


FText UFractureToolPlaneCut::GetDisplayText() const
{
	return FText(NSLOCTEXT("Fracture", "FractureToolPlaneCut", "Plane Cut Fracture")); 
}

FText UFractureToolPlaneCut::GetTooltipText() const 
{
	return FText(NSLOCTEXT("Fracture", "FractureToolPlaneCutTooltip", "Planar fracture can be used to make cuts along a plane in your Geometry Collection. You can apply noise to planar cuts for more organic results.  Click the Fracture Button to commit the fracture to the geometry collection."));
}

FSlateIcon UFractureToolPlaneCut::GetToolIcon() const 
{
	return FSlateIcon("FractureEditorStyle", "FractureEditor.Planar");
}

void UFractureToolPlaneCut::RegisterUICommand( FFractureEditorCommands* BindingContext ) 
{
	UI_COMMAND_EXT( BindingContext, UICommandInfo, "Planar", "Planar", "Fracture with planes.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	BindingContext->Planar = UICommandInfo;
}

void UFractureToolPlaneCut::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	const UFracturePlaneCutSettings* LocalCutSettings = PlaneCutSettings;
	if (CutterSettings->bDrawDiagram)
	{
		// Draw a point centered at plane origin, and a square on the plane around it.
		auto DrawPlane = [&PDI](const FTransform& Transform, float PlaneSize, FVector Offset)
		{
			FVector Center = Transform.GetLocation() + Offset;
			FVector X = (PlaneSize * .5) * Transform.GetUnitAxis(EAxis::X);
			FVector Y = (PlaneSize * .5) * Transform.GetUnitAxis(EAxis::Y);
			FVector Corners[4]
			{
				Center - X - Y,
				Center + X - Y,
				Center + X + Y,
				Center - X + Y
			};
			PDI->DrawPoint(Center, FLinearColor::Green, 4.f, SDPG_Foreground);
			PDI->DrawLine(Corners[0], Corners[1], FLinearColor(255, 0, 0), SDPG_Foreground);
			PDI->DrawLine(Corners[1], Corners[2], FLinearColor(0, 255, 0), SDPG_Foreground);
			PDI->DrawLine(Corners[2], Corners[3], FLinearColor(255, 0, 0), SDPG_Foreground);
			PDI->DrawLine(Corners[3], Corners[0], FLinearColor(0, 255, 0), SDPG_Foreground);

			// Put some grid lines in that square
			const int32 NumGridLines = 20;
			const float GridLineSpacing = PlaneSize / (NumGridLines - 1);
			const FColor GridColor(64, 64, 64, 128);
			const float GridThickness = 1.0f;
			const UE::Geometry::FFrame3d DrawFrame(Center, Transform.GetRotation());
			MeshDebugDraw::DrawSimpleGrid(DrawFrame, NumGridLines, GridLineSpacing, GridThickness, GridColor, true, PDI, FTransform::Identity);
		};

		if (GizmoSettings->IsGizmoEnabled())
		{
			const FTransform& Transform = GizmoSettings->GetTransform();
			EnumerateVisualizationMapping(PlanesMappings, RenderCuttingPlanesTransforms.Num(), [&](int32 Idx, FVector ExplodedVector)
			{
				DrawPlane(Transform, 100.f, ExplodedVector);
			});
		}
		else // draw from computed transforms
		{
			EnumerateVisualizationMapping(PlanesMappings, RenderCuttingPlanesTransforms.Num(), [&](int32 Idx, FVector ExplodedVector)
			{
				const FTransform& Transform = RenderCuttingPlanesTransforms[Idx];
				DrawPlane(Transform, RenderCuttingPlaneSize, ExplodedVector);
			});
		}
	}
}

TArray<UObject*> UFractureToolPlaneCut::GetSettingsObjects() const
 {
	TArray<UObject*> Settings;
	Settings.Add(PlaneCutSettings);
	Settings.Add(GizmoSettings);
	Settings.Add(CutterSettings);
	Settings.Add(CollisionSettings);
	return Settings;
}


void UFractureToolPlaneCut::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UFractureTransformGizmoSettings, bUseGizmo))
	{
		PlaneCutSettings->bCanCutWithMultiplePlanes = !GizmoSettings->bUseGizmo;
		NotifyOfPropertyChangeByTool(PlaneCutSettings);
	}
	
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}


void UFractureToolPlaneCut::FractureContextChanged()
{
	UpdateDefaultRandomSeed();
	TArray<FFractureToolContext> FractureContexts = GetFractureToolContexts();

	ClearVisualizations();

	RenderCuttingPlaneSize = FLT_MAX;
	for (FFractureToolContext& FractureContext : FractureContexts)
	{
		FBox Bounds = FractureContext.GetWorldBounds();
		if (!Bounds.IsValid)
		{
			continue;
		}
		int32 CollectionIdx = VisualizedCollections.Add(FractureContext.GetGeometryCollectionComponent());
		int32 BoneIdx = FractureContext.GetSelection().Num() == 1 ? FractureContext.GetSelection()[0] : INDEX_NONE;
		PlanesMappings.AddMapping(CollectionIdx, BoneIdx, RenderCuttingPlanesTransforms.Num());

		GenerateSliceTransforms(FractureContext, RenderCuttingPlanesTransforms);

		if (Bounds.GetExtent().GetMax() < RenderCuttingPlaneSize)
		{
			RenderCuttingPlaneSize = Bounds.GetExtent().GetMax();
		}
	}
}


class FPlaneFractureOp : public FGeometryCollectionFractureOperator
{
public:
	FPlaneFractureOp(const FGeometryCollection& SourceCollection) : FGeometryCollectionFractureOperator(SourceCollection)
	{}

	virtual ~FPlaneFractureOp() = default;

	TArray<int> Selection;
	TArray<FPlane> CuttingPlanes;
	FInternalSurfaceMaterials InternalSurfaceMaterials;
	float PointSpacing;
	float Grout;
	int Seed;
	FTransform Transform;
	UE::Geometry::FDynamicMesh3 CuttingMesh;

	// TGenericDataOperator interface:
	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		ResultGeometryIndex = CutMultipleWithMultiplePlanes(CuttingPlanes, InternalSurfaceMaterials, *CollectionCopy, Selection, Grout, PointSpacing, Seed, Transform, true, Progress);

		if (Progress && Progress->Cancelled())
		{
			return;
		}
		SetResult(MoveTemp(CollectionCopy));
	}
};


int32 UFractureToolPlaneCut::ExecuteFracture(const FFractureToolContext& FractureContext)
{
	if (FractureContext.IsValid())
	{
		TUniquePtr<FPlaneFractureOp> PlaneCutOp = MakeUnique<FPlaneFractureOp>(*(FractureContext.GetGeometryCollection()));
		PlaneCutOp->Selection = FractureContext.GetSelection();
		PlaneCutOp->Grout = CutterSettings->Grout;
		PlaneCutOp->PointSpacing = CollisionSettings->GetPointSpacing();
		PlaneCutOp->Seed = FractureContext.GetSeed();
		PlaneCutOp->Transform = FractureContext.GetTransform();

		if (GizmoSettings->IsGizmoEnabled())
		{
			FTransform Transform = GizmoSettings->GetTransform();
			PlaneCutOp->CuttingPlanes.Add(FPlane(Transform.GetLocation(), Transform.GetUnitAxis(EAxis::Z)));
		}
		else
		{
			TArray<FTransform> CuttingPlaneTransforms;
			GenerateSliceTransforms(FractureContext, CuttingPlaneTransforms);
			for (const FTransform& Transform : CuttingPlaneTransforms)
			{
				PlaneCutOp->CuttingPlanes.Add(FPlane(Transform.GetLocation(), Transform.GetUnitAxis(EAxis::Z)));
			}
		}
		
		FNoiseSettings NoiseSettings;
		if (CutterSettings->Amplitude > 0.0f)
		{
			CutterSettings->TransferNoiseSettings(NoiseSettings);
			PlaneCutOp->InternalSurfaceMaterials.NoiseSettings = NoiseSettings;
		}

		int Result = RunCancellableGeometryCollectionOp<FPlaneFractureOp>(*(FractureContext.GetGeometryCollection()),
			MoveTemp(PlaneCutOp), LOCTEXT("ComputingPlaneFractureMessage", "Computing Plane Fracture"));
		return Result;
	}

	return INDEX_NONE;
}

void UFractureToolPlaneCut::GenerateSliceTransforms(const FFractureToolContext& Context, TArray<FTransform>& CuttingPlaneTransforms)
{
	FRandomStream RandStream(Context.GetSeed());

	FBox Bounds = Context.GetWorldBounds();
	const FVector Extent(Bounds.Max - Bounds.Min);

	CuttingPlaneTransforms.Reserve(CuttingPlaneTransforms.Num() + PlaneCutSettings->NumberPlanarCuts);
	for (int32 ii = 0; ii < PlaneCutSettings->NumberPlanarCuts; ++ii)
	{
		FVector Position(Bounds.Min + FVector(RandStream.FRand(), RandStream.FRand(), RandStream.FRand()) * Extent);
		CuttingPlaneTransforms.Emplace(FTransform(FRotator(RandStream.FRand() * 360.0f, RandStream.FRand() * 360.0f, 0.0f), Position));
	}
}

void UFractureToolPlaneCut::SelectedBonesChanged()
{
	GizmoSettings->ResetGizmo();
	Super::SelectedBonesChanged();
}

#undef LOCTEXT_NAMESPACE
