// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureToolCutter.h"

#include "FractureTool.h" // for LogFractureTool
#include "FractureToolContext.h"
#include "InteractiveToolsContext.h"
#include "EditorModeManager.h"
#include "FractureEditorMode.h"
#include "ToolSetupUtil.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "BaseGizmos/TransformGizmoUtil.h"

#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

#include "Voronoi/Voronoi.h"
#include "PlanarCut.h"

#include "FractureToolBackgroundTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FractureToolCutter)

using namespace UE::Fracture;

#define LOCTEXT_NAMESPACE "FractureToolCutter"


UFractureTransformGizmoSettings::UFractureTransformGizmoSettings(const FObjectInitializer& ObjInit) : Super(ObjInit)
{

}

void UFractureTransformGizmoSettings::ResetGizmo(bool bResetRotation)
{
	if (!TransformGizmo || !TransformProxy || !TransformGizmo->GetGizmoActor())
	{
		// doesn't have a valid gizmo to reset
		return;
	}
	if (!bUseGizmo || !AttachedCutter)
	{
		TransformGizmo->SetVisibility(false);
		return;
	}
	FBox CombinedBounds = AttachedCutter->GetCombinedBounds(AttachedCutter->GetFractureToolContexts());
	TransformGizmo->SetVisibility((bool)CombinedBounds.IsValid);
	if (CombinedBounds.IsValid)
	{
		if (bCenterOnSelection && !GIsTransacting)
		{
			FTransform Transform = TransformProxy->GetTransform();
			Transform.SetTranslation(CombinedBounds.GetCenter());
			if (bResetRotation)
			{
				Transform.SetRotation(FQuat::Identity);
			}
			TransformGizmo->SetNewGizmoTransform(Transform);
		}
	}
}

void UFractureTransformGizmoSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UFractureTransformGizmoSettings, bUseGizmo))
	{
		if (AttachedCutter)
		{
			AttachedCutter->UpdateUseGizmo(bUseGizmo);
			ResetGizmo();
		}
	}
}

void UFractureTransformGizmoSettings::TransformChanged(UTransformProxy* Proxy, FTransform Transform)
{
	if (bUseGizmo && AttachedCutter)
	{
		AttachedCutter->FractureContextChanged();
	}
}

void UFractureTransformGizmoSettings::Setup(UFractureToolCutterBase* Cutter, ETransformGizmoSubElements GizmoElements)
{
	AttachedCutter = Cutter;
	UFractureEditorMode* Mode = Cast<UFractureEditorMode>(GLevelEditorModeTools().GetActiveScriptableMode(UFractureEditorMode::EM_FractureEditorModeId));
	UsedToolsContext = Mode->GetInteractiveToolsContext();
	if (ensure(UsedToolsContext && AttachedCutter))
	{
		UInteractiveGizmoManager* GizmoManager = UsedToolsContext->GizmoManager;
		TransformProxy = NewObject<UTransformProxy>(this);
		TransformGizmo = UE::TransformGizmoUtil::CreateCustomTransformGizmo(GizmoManager, GizmoElements, this);
		// TODO: Stop setting bUseEditorCompositing on gizmo components like this, once gizmo rendering is improved
		// This is a hack to make gizmos more visible when the translucent fracture bone selection material is on screen
		// This hack unfortunately makes the gizmos dithered when they're behind objects, but at least they're relatively visible
		for (UActorComponent* Component : TransformGizmo->GetGizmoActor()->GetComponents())
		{
			UGizmoBaseComponent* GizmoComponent = Cast<UGizmoBaseComponent>(Component);
			if (GizmoComponent)
			{
				GizmoComponent->bUseEditorCompositing = true;
			}
		}
		TransformGizmo->SetActiveTarget(TransformProxy);
		TransformProxy->OnTransformChanged.AddUObject(this, &UFractureTransformGizmoSettings::TransformChanged);
		AttachedCutter->UpdateUseGizmo(bUseGizmo);
		ResetGizmo();
	}
}

void UFractureTransformGizmoSettings::Shutdown()
{
	if (UsedToolsContext)
	{
		UsedToolsContext->GizmoManager->DestroyAllGizmosByOwner(this);
		UsedToolsContext = nullptr;
	}
	TransformGizmo = nullptr;
	TransformProxy = nullptr;
}

void UFractureCutterSettings::TransferNoiseSettings(FNoiseSettings& NoiseSettingsOut)
{
	NoiseSettingsOut.Amplitude = Amplitude;
	NoiseSettingsOut.Frequency = Frequency;
	NoiseSettingsOut.Lacunarity = Lacunarity;
	NoiseSettingsOut.Persistence = Persistence;
	NoiseSettingsOut.Octaves = OctaveNumber;
	NoiseSettingsOut.PointSpacing = PointSpacing;
}

UFractureToolCutterBase::UFractureToolCutterBase(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	CutterSettings = NewObject<UFractureCutterSettings>(GetTransientPackage(), UFractureCutterSettings::StaticClass());
	CutterSettings->OwnerTool = this;
	CollisionSettings = NewObject<UFractureCollisionSettings>(GetTransientPackage(), UFractureCollisionSettings::StaticClass());
	CollisionSettings->OwnerTool = this;
}

bool UFractureToolCutterBase::CanExecute() const
{
	if (!IsGeometryCollectionSelected())
	{
		return false;
	}
	
	return true;
}

TArray<FFractureToolContext> UFractureToolCutterBase::GetFractureToolContexts() const
{
	TArray<FFractureToolContext> Contexts;

	// A context is gathered for each selected GeometryCollection component, or for each individual bone if Group Fracture is not used.
	TSet<UGeometryCollectionComponent*> GeomCompSelection;
	GetSelectedGeometryCollectionComponents(GeomCompSelection);

	for (UGeometryCollectionComponent* GeometryCollectionComponent : GeomCompSelection)
	{
		const UGeometryCollection* RestCollection = GeometryCollectionComponent->GetRestCollection();
		if (IsValid(RestCollection))
		{

			// Generate a context for each selected node
			FFractureToolContext FullSelection(GeometryCollectionComponent);
			FullSelection.ConvertSelectionToRigidNodes();
			FullSelection.RandomReduceSelection(CutterSettings->ChanceToFracture);

			// Update global transforms and bounds		
			const TManagedArray<FTransform>& Transform = FullSelection.GetGeometryCollection()->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
			const TManagedArray<int32>& TransformToGeometryIndex = FullSelection.GetGeometryCollection()->GetAttribute<int32>("TransformToGeometryIndex", FGeometryCollection::TransformGroup);
			const TManagedArray<FBox>& BoundingBoxes = FullSelection.GetGeometryCollection()->GetAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);

			TArray<FTransform> Transforms;
			GeometryCollectionAlgo::GlobalMatrices(Transform, FullSelection.GetGeometryCollection()->Parent, Transforms);

			TMap<int32, FBox> BoundsToBone;
			int32 TransformCount = Transform.Num();
			for (int32 Index = 0; Index < TransformCount; ++Index)
			{
				if (TransformToGeometryIndex[Index] > INDEX_NONE)
				{
					BoundsToBone.Add(Index, BoundingBoxes[TransformToGeometryIndex[Index]].TransformBy(Transforms[Index]));
				}
			}

			if (CutterSettings->bGroupFracture)
			{
				FullSelection.SetSeed(CutterSettings->RandomSeed > -1 ? CutterSettings->RandomSeed : DefaultRandomSeed);

				FBox Bounds(ForceInit);
				for (int32 BoneIndex : FullSelection.GetSelection())
				{
					if (TransformToGeometryIndex[BoneIndex] > INDEX_NONE)
					{
						Bounds += BoundsToBone[BoneIndex];
					}
				}
				FullSelection.SetBounds(Bounds);

				Contexts.Add(FullSelection);
			}
			else
			{
				// Generate a context for each selected node
				for (int32 Index : FullSelection.GetSelection())
				{
					FFractureToolContext& FractureContext = Contexts.Emplace_GetRef(GeometryCollectionComponent);

					TArray<int32> Selection;
					Selection.Add(Index);
					FractureContext.SetSelection(Selection);
					FractureContext.SetSeed(CutterSettings->RandomSeed > -1 ? CutterSettings->RandomSeed + Index : DefaultRandomSeed + Index);
					FractureContext.SetBounds(BoundsToBone[Index]);
				}
			}
		}
	}

	return Contexts;
}


FBox UFractureToolCutterBase::GetCombinedBounds(const TArray<FFractureToolContext>& Contexts) const
{
	FBox CombinedBounds(EForceInit::ForceInit);
	for (const FFractureToolContext& FractureContext : Contexts)
	{
		CombinedBounds += FractureContext.GetWorldBounds();
	}
	return CombinedBounds;
}


void UFractureToolCutterBase::UpdateDefaultRandomSeed()
{
	DefaultRandomSeed = FMath::Rand();
}



UFractureToolVoronoiCutterBase::UFractureToolVoronoiCutterBase(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	for (int32 ii = 0; ii < 100; ++ii)
	{
		// This is adapted from FColor::MakeRandomColor() but desaturated a bit
		const uint8 Hue = (uint8)(FMath::FRand() * 255.f);
		FColor DesatRandomColor = FLinearColor::MakeFromHSV8(Hue, 190, 255).ToFColor(true);
		Colors.Add(DesatRandomColor);
	}
}

void UFractureToolVoronoiCutterBase::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (CutterSettings->bDrawSites)
	{
		EnumerateVisualizationMapping(SitesMappings, VoronoiSites.Num(), [&](int32 Idx, FVector ExplodedVector)
		{
			PDI->DrawPoint(VoronoiSites[Idx] + ExplodedVector, FLinearColor(.62, .94, .84), 4.f, SDPG_Foreground);
		});
	}

	UpdateLineSetExplodedVectors();
}

void UFractureToolVoronoiCutterBase::UpdateLineSetExplodedVectors()
{
	EnumerateVisualizationMapping(EdgesMappings, VoronoiLineSets.Num(), [&](int32 Idx, FVector ExplodedVector)
	{
		if (!VoronoiLineSets[Idx])
		{
			return;
		}
		// TODO: If we add diagrams even when visibility is toggled off, we also need to update visibility here, e.g.: VoronoiLineSets[Idx]->SetVisibility(CutterSettings->bDrawDiagram);
		AActor* Actor = VoronoiLineSets[Idx]->GetOwner();
		VoronoiLineSets[Idx]->SetRelativeLocation(Actor->GetTransform().InverseTransformVector(ExplodedVector));
	});
}

void UFractureToolVoronoiCutterBase::UpdateVisualizations(TArray<FFractureToolContext>& FractureContexts)
{
	ClearVisualizations();

	int32 MaxSitesToShowEdges = 100000; // computing all the voronoi diagrams can make the program non-responsive above this
	int32 MaxSitesToShowSites = 1000000; // PDI struggles to render the site positions above this
	bool bEstAboveMaxSites = false;

	for (FFractureToolContext& FractureContext : FractureContexts)
	{
		if (!FractureContext.GetBounds().IsValid) // skip contexts w/ invalid bounds
		{
			continue;
		}
		int32 CollectionIdx = VisualizedCollections.Emplace(FractureContext.GetGeometryCollectionComponent());
		int32 BoneIdx = FractureContext.GetSelection().Num() == 1 ? FractureContext.GetSelection()[0] : INDEX_NONE;
		SitesMappings.AddMapping(CollectionIdx, BoneIdx, VoronoiSites.Num());
		EdgesMappings.AddMapping(CollectionIdx, BoneIdx, VoronoiLineSets.Num());

		// Generate voronoi diagrams and cache visualization info
		TArray<FVector> LocalVoronoiSites;
		GenerateVoronoiSites(FractureContext, LocalVoronoiSites);
		// if diagram(s) become too large, skip the visualization
		if (LocalVoronoiSites.Num() * FractureContexts.Num() > MaxSitesToShowSites || VoronoiSites.Num() + LocalVoronoiSites.Num() > MaxSitesToShowSites)
		{
			UE_LOG(LogFractureTool, Warning, TEXT("Voronoi diagram(s) number of sites too large; will not display Voronoi diagram sites"));
			ClearVisualizations();
			break;
		}
		VoronoiSites.Append(LocalVoronoiSites);
		if (bEstAboveMaxSites || LocalVoronoiSites.Num() * FractureContexts.Num() > MaxSitesToShowEdges || VoronoiSites.Num() > MaxSitesToShowEdges)
		{
			UE_LOG(LogFractureTool, Warning, TEXT("Voronoi diagram(s) number of sites too large; will not display Voronoi diagram edges"));
			ClearEdges();
			bEstAboveMaxSites = true;
		}
		else if (CutterSettings->bDrawDiagram)
		{
			FBox VoronoiBounds = GetVoronoiBounds(FractureContext, LocalVoronoiSites);
			ULineSetComponent* VoronoiLineSet = NewObject<ULineSetComponent>(FractureContext.GetGeometryCollectionComponent());
			VoronoiLineSet->SetupAttachment(FractureContext.GetGeometryCollectionComponent());
			VoronoiLineSet->SetLineMaterial(ToolSetupUtil::GetDefaultLineComponentMaterial(nullptr, false));
			// TODO: If we switch to adding diagrams even when visibility is toggled off, we also need to update visibility here, e.g.: VoronoiLineSets[Idx]->SetVisibility(CutterSettings->bDrawDiagram);
			VoronoiLineSet->RegisterComponent();
			TArray<TTuple<FVector, FVector>> VoronoiEdges;
			GetVoronoiEdges(LocalVoronoiSites, VoronoiBounds, VoronoiEdges, CellMember);
			FTransform ToWorld = FractureContext.GetTransform();
			VoronoiLineSet->ReserveLines(VoronoiEdges.Num());
			for (int32 Idx = 0; Idx < VoronoiEdges.Num(); ++Idx)
			{
				const TTuple<FVector, FVector>& Line = VoronoiEdges[Idx];
				VoronoiLineSet->AddLine(ToWorld.InverseTransformPosition(VoronoiEdges[Idx].Key), ToWorld.InverseTransformPosition(VoronoiEdges[Idx].Value), Colors[CellMember[Idx] % 100], 1.3, .1);
			}
			VoronoiLineSets.Add(VoronoiLineSet);
		}
	}

	if (VoronoiLineSets.Num() > 0)
	{
		OverrideEditorViewFlagsForLineRendering();
		UpdateLineSetExplodedVectors();
	}
	else
	{
		RestoreEditorViewFlags();
	}
}

void UFractureToolVoronoiCutterBase::FractureContextChanged()
{
	UpdateDefaultRandomSeed();
	TArray<FFractureToolContext> FractureContexts = GetFractureToolContexts();
	UpdateVisualizations(FractureContexts);
}


class FVoronoiFractureOp : public FGeometryCollectionFractureOperator
{
public:
	FVoronoiFractureOp(const FGeometryCollection& SourceCollection) : FGeometryCollectionFractureOperator(SourceCollection)
	{}

	virtual ~FVoronoiFractureOp() = default;

	TArray<int> Selection;
	FBox Bounds;
	TArray<FVector> Sites;
	TOptional<FNoiseSettings> NoiseSettings;
	float PointSpacing;
	float Grout;
	int Seed;
	FTransform Transform;

	// TGenericDataOperator interface:
	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		// Assume Voronoi diagram only takes ~1% of the total time to compute
		FProgressCancel::FProgressScope VoronoiDiagramProgress =
			FProgressCancel::CreateScopeTo(Progress, .01, LOCTEXT("ComputingVoronoiDiagramMessage", "Computing Voronoi Diagram"));

		FVector Origin = Transform.GetTranslation();
		for (FVector& Site : Sites)
		{
			Site -= Origin;
		}
		Bounds.Min -= Origin;
		Bounds.Max -= Origin;
		FVoronoiDiagram Voronoi(Sites, Bounds, .1f);

		FPlanarCells VoronoiPlanarCells = FPlanarCells(Sites, Voronoi);
		VoronoiPlanarCells.InternalSurfaceMaterials.NoiseSettings = NoiseSettings;

		if (Progress && Progress->Cancelled())
		{
			return;
		}

		VoronoiDiagramProgress.Done();

		// All remaining work is assigned to the mesh fracture
		FProgressCancel::FProgressScope FractureMeshProgress =
			FProgressCancel::CreateScopeTo(Progress, 1, LOCTEXT("FractureMeshMessage", "Fracturing Mesh"));

		ResultGeometryIndex = CutMultipleWithPlanarCells(VoronoiPlanarCells, *CollectionCopy, Selection, Grout, PointSpacing, Seed, Transform, true, true, Progress, Origin);
		
		SetResult(MoveTemp(CollectionCopy));
	}
};


int32 UFractureToolVoronoiCutterBase::ExecuteFracture(const FFractureToolContext& FractureContext)
{
	if (FractureContext.IsValid())
	{
		TArray<FVector> Sites;
		GenerateVoronoiSites(FractureContext, Sites);
		FBox VoronoiBounds = GetVoronoiBounds(FractureContext, Sites);

		TUniquePtr<FVoronoiFractureOp> VoronoiOp = MakeUnique<FVoronoiFractureOp>(*(FractureContext.GetGeometryCollection()));
		VoronoiOp->Bounds = VoronoiBounds;
		VoronoiOp->Selection = FractureContext.GetSelection();
		VoronoiOp->Grout = CutterSettings->Grout;
		VoronoiOp->PointSpacing = CollisionSettings->GetPointSpacing();
		VoronoiOp->Sites = Sites;
		if (CutterSettings->Amplitude > 0.0f)
		{
			FNoiseSettings Settings;
			CutterSettings->TransferNoiseSettings(Settings);
			VoronoiOp->NoiseSettings = Settings;
		}
		VoronoiOp->Seed = FractureContext.GetSeed();
		VoronoiOp->Transform = FractureContext.GetTransform();

		int Result = RunCancellableGeometryCollectionOp<FVoronoiFractureOp>(*(FractureContext.GetGeometryCollection()),
			MoveTemp(VoronoiOp), LOCTEXT("ComputingVoronoiFractureMessage", "Computing Voronoi Fracture"));
		return Result;
	}

	return INDEX_NONE;
}

FBox UFractureToolVoronoiCutterBase::GetVoronoiBounds(const FFractureToolContext& FractureContext, const TArray<FVector>& Sites) const
{
	FBox VoronoiBounds = FractureContext.GetWorldBounds(); 
	if (Sites.Num() > 0)
	{
		VoronoiBounds += FBox(Sites);
	}
	
	return VoronoiBounds.ExpandBy(CutterSettings->GetMaxVertexMovement() + KINDA_SMALL_NUMBER);
}

#undef LOCTEXT_NAMESPACE

