// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/SetCollisionGeometryTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"
#include "Selections/MeshConnectedComponents.h"
#include "DynamicSubmesh3.h"
#include "Polygroups/PolygroupUtil.h"
#include "Util/ColorConstants.h"

#include "ShapeApproximation/ShapeDetection3.h"
#include "ShapeApproximation/MeshSimpleShapeApproximation.h"

#include "Physics/CollisionGeometryVisualization.h"

// physics data
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"

#include "Async/ParallelFor.h"

#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"
#include "ModelingToolTargetUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SetCollisionGeometryTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "USetCollisionGeometryTool"


/*
 * Operators
 */

class FPhysicsCollectionOp : public TGenericDataOperator<FPhysicsDataCollection>
{
public:

	TSharedPtr<FPhysicsDataCollection, ESPMode::ThreadSafe> InitialCollision;

	TUniquePtr<FMeshSimpleShapeApproximation> UseShapeGenerator;
	// Note: UseShapeGenerator holds raw pointers to these meshes, so we keep the array of shared pointers to prevent them from getting deleted while the op runs
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>> ActiveInputMeshes;

	ECollisionGeometryType ComputeType;

	bool bUseMaxCount;
	bool bRemoveContained;
	bool bAppendToExisting;

	EProjectedHullAxis SweepAxis;
	int32 MaxCount;

	// Begin TGenericDataOperator interface
	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		check(UseShapeGenerator.IsValid());
		check(InitialCollision.IsValid());

		// calculate new collision
		TUniquePtr<FPhysicsDataCollection> NewCollision = MakeUnique<FPhysicsDataCollection>();
		NewCollision->InitializeFromExisting(*InitialCollision);
		if (bAppendToExisting || ComputeType == ECollisionGeometryType::KeepExisting)
		{
			NewCollision->CopyGeometryFromExisting(*InitialCollision);
		}

		switch (ComputeType)
		{
		case ECollisionGeometryType::KeepExisting:
		case ECollisionGeometryType::None:
			break;
		case ECollisionGeometryType::AlignedBoxes:
			UseShapeGenerator->Generate_AlignedBoxes(NewCollision->Geometry);
			break;
		case ECollisionGeometryType::OrientedBoxes:
			UseShapeGenerator->Generate_OrientedBoxes(NewCollision->Geometry);
			break;
		case ECollisionGeometryType::MinimalSpheres:
			UseShapeGenerator->Generate_MinimalSpheres(NewCollision->Geometry);
			break;
		case ECollisionGeometryType::Capsules:
			UseShapeGenerator->Generate_Capsules(NewCollision->Geometry);
			break;
		case ECollisionGeometryType::ConvexHulls:
			if (UseShapeGenerator->ConvexDecompositionMaxPieces > 1)
			{
				UseShapeGenerator->Generate_ConvexHullDecompositions(NewCollision->Geometry);
			}
			else
			{
				UseShapeGenerator->Generate_ConvexHulls(NewCollision->Geometry);
			}
			break;
		case ECollisionGeometryType::SweptHulls:
			UseShapeGenerator->Generate_ProjectedHulls(NewCollision->Geometry,
				(FMeshSimpleShapeApproximation::EProjectedHullAxisMode)(int32)SweepAxis);
			break;
		case ECollisionGeometryType::LevelSets:
			UseShapeGenerator->Generate_LevelSets(NewCollision->Geometry);
			break;
		case ECollisionGeometryType::MinVolume:
			UseShapeGenerator->Generate_MinVolume(NewCollision->Geometry);
			break;
		}

		if (!NewCollision)
		{
			ensure(false);
			return;
		}

		if (bRemoveContained)
		{
			NewCollision->Geometry.RemoveContainedGeometry();
		}

		
		if (bUseMaxCount)
		{
			NewCollision->Geometry.FilterByVolume(MaxCount);
		}

		NewCollision->CopyGeometryToAggregate();

		SetResult(MoveTemp(NewCollision));
	}
	// End TGenericDataOperator interface
};



const FToolTargetTypeRequirements& USetCollisionGeometryToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMeshDescriptionProvider::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

bool USetCollisionGeometryToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	UActorComponent* LastValidTarget = nullptr;
	SceneState.TargetManager->EnumerateSelectedAndTargetableComponents(SceneState, GetTargetRequirements(),
		[&](UActorComponent* Component) { LastValidTarget = Component; });
	return (LastValidTarget != nullptr && Cast<UStaticMeshComponent>(LastValidTarget) != nullptr);
}


UMultiSelectionMeshEditingTool* USetCollisionGeometryToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<USetCollisionGeometryTool>(SceneState.ToolManager);
}


void USetCollisionGeometryTool::Setup()
{
	UInteractiveTool::Setup();

	// if we have one selection, use it as the source, otherwise use all but the last selected mesh
	bSourcesHidden = (Targets.Num() > 1);
	if (Targets.Num() == 1)
	{
		SourceObjectIndices.Add(0);
	}
	else
	{
		for (int32 k = 0; k < Targets.Num() -1; ++k)
		{
			SourceObjectIndices.Add(k);
			UE::ToolTarget::HideSourceObject(Targets[k]);
		}
	}

	// collect input meshes
	InitialSourceMeshes.SetNum(SourceObjectIndices.Num());
	ParallelFor(SourceObjectIndices.Num(), [&](int32 k)
	{
		InitialSourceMeshes[k] = UE::ToolTarget::GetDynamicMeshCopy(Targets[k]);
	});

	UToolTarget* CollisionTarget = Targets[Targets.Num() - 1];
	PreviewGeom = NewObject<UPreviewGeometry>(this);
	FTransform PreviewTransform = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(CollisionTarget);
	OrigTargetTransform = PreviewTransform;
	TargetScale3D = PreviewTransform.GetScale3D();
	PreviewTransform.SetScale3D(FVector::OneVector);
	PreviewGeom->CreateInWorld(UE::ToolTarget::GetTargetActor(CollisionTarget)->GetWorld(), PreviewTransform);

	// initialize initial collision object
	InitialCollision = MakeShared<FPhysicsDataCollection, ESPMode::ThreadSafe>();
	InitialCollision->InitializeFromComponent(UE::ToolTarget::GetTargetComponent(CollisionTarget), true);
	InitialCollision->ExternalScale3D = TargetScale3D;

	// create tool options
	Settings = NewObject<USetCollisionGeometryToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);
	Settings->bUseWorldSpace = (SourceObjectIndices.Num() > 1);
	Settings->WatchProperty(Settings->InputMode, [this](ESetCollisionGeometryInputMode) { OnInputModeChanged(); });
	Settings->WatchProperty(Settings->GeometryType, [this](ECollisionGeometryType) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bUseWorldSpace, [this](bool) { bInputMeshesValid = false; });
	Settings->WatchProperty(Settings->bAppendToExisting, [this](bool) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bRemoveContained, [this](bool) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bEnableMaxCount, [this](bool) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->MaxCount, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->MinThickness, [this](float) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bDetectBoxes, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bDetectSpheres, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bDetectCapsules, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bSimplifyHulls, [this](bool) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->HullTargetFaceCount, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->MaxHullsPerMesh, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->ConvexDecompositionSearchFactor, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->AddHullsErrorTolerance, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->MinPartThickness, [this](int32) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->bSimplifyPolygons, [this](bool) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->HullTolerance, [this](float) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->SweepAxis, [this](EProjectedHullAxis) { InvalidateCompute(); });
	Settings->WatchProperty(Settings->LevelSetResolution, [this](int32) { InvalidateCompute(); });

	if (InitialSourceMeshes.Num() == 1)
	{
		PolygroupLayerProperties = NewObject<UPolygroupLayersProperties>(this);
		PolygroupLayerProperties->RestoreProperties(this, TEXT("SetCollisionGeometryTool"));
		PolygroupLayerProperties->InitializeGroupLayers(&InitialSourceMeshes[0]);
		PolygroupLayerProperties->WatchProperty(PolygroupLayerProperties->ActiveGroupLayer, [&](FName) { OnSelectedGroupLayerChanged(); });
		AddToolPropertySource(PolygroupLayerProperties);
	}

	VizSettings = NewObject<UCollisionGeometryVisualizationProperties>(this);
	VizSettings->RestoreProperties(this);
	AddToolPropertySource(VizSettings);
	VizSettings->WatchProperty(VizSettings->LineThickness, [this](float NewValue) { bVisualizationDirty = true; });
	VizSettings->WatchProperty(VizSettings->Color, [this](FColor NewValue) { bVisualizationDirty = true; });
	VizSettings->WatchProperty(VizSettings->bRandomColors, [this](bool bNewValue) { bVisualizationDirty = true; });
	VizSettings->WatchProperty(VizSettings->bShowHidden, [this](bool bNewValue) { bVisualizationDirty = true; });

	// add option for collision properties
	CollisionProps = NewObject<UPhysicsObjectToolPropertySet>(this);
	AddToolPropertySource(CollisionProps);

	SetToolDisplayName(LOCTEXT("ToolName", "Mesh To Collision"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Initialize Simple Collision geometry for a Mesh from one or more input Meshes (including itself)."),
		EToolMessageLevel::UserNotification);

	// Make sure we are set to precompute input meshes on first tick
	bInputMeshesValid = false;
}


TUniquePtr<UE::Geometry::TGenericDataOperator<FPhysicsDataCollection>> USetCollisionGeometryTool::MakeNewOperator()
{
	TUniquePtr<FPhysicsCollectionOp> Op = MakeUnique<FPhysicsCollectionOp>();

	Op->InitialCollision = InitialCollision;

	// Pick the approximator and input meshes that will be used by the op
	TSharedPtr<UE::Geometry::FMeshSimpleShapeApproximation, ESPMode::ThreadSafe>* Approximator = nullptr;
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>>* Inputs = nullptr;
	if (Settings->InputMode == ESetCollisionGeometryInputMode::CombineAll)
	{
		Approximator = &CombinedInputMeshesApproximator;
		Inputs = &CombinedInputMeshes;
	}
	else if (Settings->InputMode == ESetCollisionGeometryInputMode::PerMeshComponent)
	{
		Approximator = &SeparatedMeshesApproximator;
		Inputs = &SeparatedInputMeshes;
	}
	else if (Settings->InputMode == ESetCollisionGeometryInputMode::PerMeshGroup)
	{
		Approximator = &PerGroupMeshesApproximator;
		Inputs = &PerGroupInputMeshes;
	}
	else
	{
		Approximator = &InputMeshesApproximator;
		Inputs = &InputMeshes;
	}
	Op->UseShapeGenerator = MakeUnique<FMeshSimpleShapeApproximation>(**Approximator);
	Op->ActiveInputMeshes = *Inputs;

	Op->UseShapeGenerator->bDetectSpheres = Settings->bDetectSpheres;
	Op->UseShapeGenerator->bDetectBoxes = Settings->bDetectBoxes;
	Op->UseShapeGenerator->bDetectCapsules = Settings->bDetectCapsules;
	Op->UseShapeGenerator->MinDimension = Settings->MinThickness;
	Op->UseShapeGenerator->bSimplifyHulls = Settings->bSimplifyHulls;
	Op->UseShapeGenerator->HullTargetFaceCount = Settings->HullTargetFaceCount;
	Op->UseShapeGenerator->ConvexDecompositionMaxPieces = Settings->MaxHullsPerMesh;
	Op->UseShapeGenerator->ConvexDecompositionSearchFactor = Settings->ConvexDecompositionSearchFactor;
	Op->UseShapeGenerator->ConvexDecompositionErrorTolerance = Settings->AddHullsErrorTolerance;
	Op->UseShapeGenerator->ConvexDecompositionMinPartThickness = Settings->MinPartThickness;
	Op->UseShapeGenerator->HullSimplifyTolerance = Settings->HullTolerance;
	Op->UseShapeGenerator->LevelSetGridResolution = Settings->LevelSetResolution;

	Op->ComputeType = Settings->GeometryType;
	Op->bAppendToExisting = Settings->bAppendToExisting;
	Op->bUseMaxCount = Settings->bEnableMaxCount;
	Op->MaxCount = Settings->MaxCount;
	Op->bRemoveContained = Settings->bRemoveContained;
	Op->SweepAxis = Settings->SweepAxis;

	return Op;
}


void USetCollisionGeometryTool::OnShutdown(EToolShutdownType ShutdownType)
{
	VizSettings->SaveProperties(this);
	Settings->SaveProperties(this);
	if (PolygroupLayerProperties)
	{
		PolygroupLayerProperties->SaveProperties(this, TEXT("SetCollisionGeometryTool"));
	}

	PreviewGeom->Disconnect();

	// show hidden sources
	if (bSourcesHidden)
	{
		for (int32 k : SourceObjectIndices)
		{
			UE::ToolTarget::ShowSourceObject(Targets[k]);
		}
	}

	if (Compute)
	{
		Compute->Shutdown();
	}

	if (ShutdownType == EToolShutdownType::Accept)
	{
		// Make sure rendering is done so that we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		GetToolManager()->BeginUndoTransaction(LOCTEXT("UpdateCollision", "Update Collision"));

		// code below derived from FStaticMeshEditor::DuplicateSelectedPrims(), FStaticMeshEditor::OnCollisionSphere(), and GeomFitUtils.cpp::GenerateSphylAsSimpleCollision()

		UPrimitiveComponent* Component = UE::ToolTarget::GetTargetComponent(Targets[Targets.Num() - 1]);
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
		TObjectPtr<UStaticMesh> StaticMesh = (StaticMeshComponent) ? StaticMeshComponent->GetStaticMesh() : nullptr;
		UBodySetup* BodySetup = (StaticMesh) ? StaticMesh->GetBodySetup() : nullptr;
		if (BodySetup != nullptr)
		{
			// mark the BodySetup for modification. Do we need to modify the UStaticMesh??
			BodySetup->Modify();

			// clear existing simple collision. This will call BodySetup->InvalidatePhysicsData()
			BodySetup->RemoveSimpleCollision();

			// set new collision geometry
			BodySetup->AggGeom = GeneratedCollision->AggGeom;

			// update collision type
			BodySetup->CollisionTraceFlag = (ECollisionTraceFlag)(int32)Settings->SetCollisionType;

			// rebuild physics meshes
			BodySetup->CreatePhysicsMeshes();

			// rebuild nav collision (? StaticMeshEditor does this)
			StaticMesh->CreateNavCollision(/*bIsUpdate=*/true);

			// update physics state on all components using this StaticMesh
			for (FThreadSafeObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
			{
				UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(*Iter);
				if (SMComponent->GetStaticMesh() == StaticMesh)
				{
					if (SMComponent->IsPhysicsStateCreated())
					{
						SMComponent->RecreatePhysicsState();
					}
				}
			}

			// do we need to do a post edit change here??

			// mark static mesh as dirty so it gets resaved?
			StaticMesh->MarkPackageDirty();

#if WITH_EDITORONLY_DATA
			// mark the static mesh as having customized collision so it is not regenerated on reimport
			StaticMesh->bCustomizedCollision = true;
#endif // WITH_EDITORONLY_DATA
		}

		// post the undo transaction
		GetToolManager()->EndUndoTransaction();
	}

}




void USetCollisionGeometryTool::OnTick(float DeltaTime)
{
	if (bInputMeshesValid == false)
	{
		PrecomputeInputMeshes();
		bInputMeshesValid = true;
		InvalidateCompute();
	}

	if (Compute)
	{
		Compute->Tick(DeltaTime);

		if (Compute->HaveValidResult())
		{
			TUniquePtr<FPhysicsDataCollection> Result = Compute->Shutdown();
			if (Result.IsValid())
			{
				GeneratedCollision = MakeShareable<FPhysicsDataCollection>(Result.Release());

				bVisualizationDirty = true;

				// update visualization
				PreviewGeom->RemoveAllLineSets();
				UE::PhysicsTools::InitializePreviewGeometryLines(*GeneratedCollision, PreviewGeom,
					VizSettings->Color, VizSettings->LineThickness, 0.0f, 16, VizSettings->bRandomColors);

				// update property set
				CollisionProps->Reset();
				UE::PhysicsTools::InitializePhysicsToolObjectPropertySet(GeneratedCollision.Get(), CollisionProps);
			}
		}
	}

	if (bVisualizationDirty)
	{
		UpdateVisualization();
		bVisualizationDirty = false;
	}
}


void USetCollisionGeometryTool::InvalidateCompute()
{
	if (PreviewGeom)
	{
		PreviewGeom->RemoveAllLineSets();
	}

	if (!bInputMeshesValid)
	{
		// InvalidateCompute() will be called again when the input meshes are valid
		return;
	}

	if (!Compute)
	{
		// Initialize background compute
		Compute = MakeUnique<TGenericDataBackgroundCompute<FPhysicsDataCollection>>();
		Compute->Setup(this);
	}
	Compute->InvalidateResult();
}


void USetCollisionGeometryTool::OnInputModeChanged()
{
	if (PolygroupLayerProperties != nullptr)
	{
		SetToolPropertySourceEnabled(PolygroupLayerProperties, Settings->InputMode == ESetCollisionGeometryInputMode::PerMeshGroup);
	}
	InvalidateCompute();
}

void USetCollisionGeometryTool::OnSelectedGroupLayerChanged()
{
	bInputMeshesValid = false;
	InvalidateCompute();
}


void USetCollisionGeometryTool::UpdateActiveGroupLayer()
{
	if (InitialSourceMeshes.Num() != 1)
	{
		ensure(false);		// should not get here
		return;
	}
	FDynamicMesh3* GroupLayersMesh = &InitialSourceMeshes[0];

	if (PolygroupLayerProperties->HasSelectedPolygroup() == false)
	{
		ActiveGroupSet = MakeUnique<UE::Geometry::FPolygroupSet>(GroupLayersMesh);
	}
	else
	{
		FName SelectedName = PolygroupLayerProperties->ActiveGroupLayer;
		FDynamicMeshPolygroupAttribute* FoundAttrib = UE::Geometry::FindPolygroupLayerByName(*GroupLayersMesh, SelectedName);
		ensureMsgf(FoundAttrib, TEXT("Selected Attribute Not Found! Falling back to Default group layer."));
		ActiveGroupSet = MakeUnique<UE::Geometry::FPolygroupSet>(GroupLayersMesh, FoundAttrib);
	}
}




void USetCollisionGeometryTool::UpdateVisualization()
{
	float UseThickness = VizSettings->LineThickness;
	FColor UseColor = VizSettings->Color;
	int32 ColorIdx = 0;
	PreviewGeom->UpdateAllLineSets([&](ULineSetComponent* LineSet)
	{
		LineSet->SetAllLinesThickness(UseThickness);
		LineSet->SetAllLinesColor(VizSettings->bRandomColors ? LinearColors::SelectFColor(ColorIdx++) : UseColor);
	});

	LineMaterial = ToolSetupUtil::GetDefaultLineComponentMaterial(GetToolManager(), !VizSettings->bShowHidden);
	PreviewGeom->SetAllLineSetsMaterial(LineMaterial);
}







void USetCollisionGeometryTool::InitializeDerivedMeshSet(
	const TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>>& FromInputMeshes,
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>>& ToMeshes,
	TFunctionRef<bool(const FDynamicMesh3* Mesh, int32 Tri0, int32 Tri1)> TrisConnectedPredicate)
{
	// find connected-components on input meshes, under given connectivity predicate
	TArray<TUniquePtr<FMeshConnectedComponents>> ComponentSets;
	ComponentSets.SetNum(FromInputMeshes.Num());
	ParallelFor(FromInputMeshes.Num(), [&](int32 k)
	{
		const FDynamicMesh3* Mesh = FromInputMeshes[k].Get();
		ComponentSets[k] = MakeUnique<FMeshConnectedComponents>(Mesh);
		ComponentSets[k]->FindConnectedTriangles(
			[Mesh, &TrisConnectedPredicate](int32 Tri0, int32 Tri1) 
			{ 
				return TrisConnectedPredicate(Mesh, Tri0, Tri1); 
			}
		);
	});

	// Assemble a list of all the submeshes we want to compute, so we can do them all in parallel
	struct FSubmeshSource
	{
		const FDynamicMesh3* SourceMesh;
		FIndex2i ComponentIdx;
	};
	TArray<FSubmeshSource> AllSubmeshes;
	for (int32 k = 0; k < FromInputMeshes.Num(); ++k)
	{
		const FDynamicMesh3* Mesh = FromInputMeshes[k].Get();
		int32 NumComponents = ComponentSets[k]->Num();
		for ( int32 j = 0; j < NumComponents; ++j )
		{
			const FMeshConnectedComponents::FComponent& Component = ComponentSets[k]->GetComponent(j);
			if (Component.Indices.Num() > 1)		// ignore single triangles
			{
				AllSubmeshes.Add(FSubmeshSource{ Mesh, FIndex2i(k,j) });
			}
		}
	}


	// compute all the submeshes
	ToMeshes.Reset();
	ToMeshes.SetNum(AllSubmeshes.Num());
	ParallelFor(AllSubmeshes.Num(), [&](int32 k)
	{
		const FSubmeshSource& Source = AllSubmeshes[k];
		const FMeshConnectedComponents::FComponent& Component = ComponentSets[Source.ComponentIdx.A]->GetComponent(Source.ComponentIdx.B);
		FDynamicSubmesh3 Submesh(Source.SourceMesh, Component.Indices, (int32)EMeshComponents::None, false);
		ToMeshes[k] = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>( MoveTemp(Submesh.GetSubmesh()) );
	});
}


template<typename T>
TArray<const T*> MakeRawPointerList(const TArray<TSharedPtr<T, ESPMode::ThreadSafe>>& InputList)
{
	TArray<const T*> Result;
	Result.Reserve(InputList.Num());
	for (const TSharedPtr<T, ESPMode::ThreadSafe>& Ptr : InputList)
	{
		Result.Add(Ptr.Get());
	}
	return MoveTemp(Result);
}


void USetCollisionGeometryTool::PrecomputeInputMeshes()
{
	if (InitialSourceMeshes.Num() == 1)
	{
		UpdateActiveGroupLayer();
	}

	UToolTarget* CollisionTarget = Targets[Targets.Num() - 1];
	FTransformSRT3d TargetTransform(UE::ToolTarget::GetLocalToWorldTransform(CollisionTarget));

	InputMeshes.Reset();
	InputMeshes.SetNum(SourceObjectIndices.Num());
	ParallelFor(SourceObjectIndices.Num(), [&](int32 k)
	{
		FDynamicMesh3 SourceMesh = InitialSourceMeshes[k];
		if (Settings->bUseWorldSpace)
		{
			FTransformSRT3d ToWorld(UE::ToolTarget::GetLocalToWorldTransform(Targets[k]));
			MeshTransforms::ApplyTransform(SourceMesh, ToWorld, true);
			MeshTransforms::ApplyTransformInverse(SourceMesh, TargetTransform, true);
		}
		SourceMesh.DiscardAttributes();
		InputMeshes[k] = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>(MoveTemp(SourceMesh));
	});
	InputMeshesApproximator = MakeShared<FMeshSimpleShapeApproximation, ESPMode::ThreadSafe>();
	InputMeshesApproximator->InitializeSourceMeshes(MakeRawPointerList<FDynamicMesh3>(InputMeshes));


	// build combined input
	CombinedInputMeshes.Reset();
	FDynamicMesh3 CombinedMesh;
	CombinedMesh.EnableTriangleGroups();
	FDynamicMeshEditor Appender(&CombinedMesh);
	FMeshIndexMappings TmpMappings;
	for (const TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>& InputMesh : InputMeshes)
	{
		TmpMappings.Reset();
		Appender.AppendMesh(InputMesh.Get(), TmpMappings);
	}
	CombinedInputMeshes.Add( MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>(MoveTemp(CombinedMesh)) );
	CombinedInputMeshesApproximator = MakeShared<FMeshSimpleShapeApproximation, ESPMode::ThreadSafe>();
	CombinedInputMeshesApproximator->InitializeSourceMeshes(MakeRawPointerList<FDynamicMesh3>(CombinedInputMeshes));

	// build separated input meshes
	SeparatedInputMeshes.Reset();
	InitializeDerivedMeshSet(InputMeshes, SeparatedInputMeshes, 
		[&](const FDynamicMesh3* Mesh, int32 Tri0, int32 Tri1) { return true; });
	SeparatedMeshesApproximator = MakeShared<FMeshSimpleShapeApproximation, ESPMode::ThreadSafe>();
	SeparatedMeshesApproximator->InitializeSourceMeshes(MakeRawPointerList<FDynamicMesh3>(SeparatedInputMeshes));

	// build per-group input meshes
	PerGroupInputMeshes.Reset();
	if (ActiveGroupSet.IsValid())
	{
		check(InputMeshes.Num() == 1);
		InitializeDerivedMeshSet(InputMeshes, PerGroupInputMeshes,
			[&](const FDynamicMesh3* Mesh, int32 Tri0, int32 Tri1) { return ActiveGroupSet->GetTriangleGroup(Tri0) == ActiveGroupSet->GetTriangleGroup(Tri1); });
	}
	else
	{
		InitializeDerivedMeshSet(InputMeshes, PerGroupInputMeshes,
			[&](const FDynamicMesh3* Mesh, int32 Tri0, int32 Tri1) { return Mesh->GetTriangleGroup(Tri0) == Mesh->GetTriangleGroup(Tri1); });
	}
	PerGroupMeshesApproximator = MakeShared<FMeshSimpleShapeApproximation, ESPMode::ThreadSafe>();
	PerGroupMeshesApproximator->InitializeSourceMeshes(MakeRawPointerList<FDynamicMesh3>(PerGroupInputMeshes));

}


#undef LOCTEXT_NAMESPACE
