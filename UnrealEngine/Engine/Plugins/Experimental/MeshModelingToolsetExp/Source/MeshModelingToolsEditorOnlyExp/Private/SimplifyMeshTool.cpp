// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimplifyMeshTool.h"
#include "InteractiveToolManager.h"
#include "Properties/RemeshProperties.h"
#include "Properties/MeshStatisticsProperties.h"
#include "Drawing/MeshElementsVisualizer.h"
#include "ToolBuilderUtil.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "ModelingToolTargetUtil.h"
#include "ToolSetupUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Util/ColorConstants.h"


//#include "ProfilingDebugging/ScopedTimers.h" // enable this to use the timer.
#include "Modules/ModuleManager.h"
#include "IMeshReductionManagerModule.h"
#include "IMeshReductionInterfaces.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimplifyMeshTool)


#if WITH_EDITOR
#include "Misc/ScopedSlowTask.h"
#endif

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "USimplifyMeshTool"

DEFINE_LOG_CATEGORY_STATIC(LogMeshSimplification, Log, All);

/*
 * ToolBuilder
 */
USingleSelectionMeshEditingTool* USimplifyMeshToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<USimplifyMeshTool>(SceneState.ToolManager);
}

/*
 * Tool
 */
USimplifyMeshToolProperties::USimplifyMeshToolProperties()
{
	SimplifierType = ESimplifyType::QEM;
	TargetMode = ESimplifyTargetType::Percentage;
	TargetPercentage = 50;
	TargetTriangleCount = 1000;
	TargetVertexCount = 1000;
	MinimalAngleThreshold = 0.01;
	TargetEdgeLength = 5.0;
	bReproject = false;
	bPreventNormalFlips = true;
	bDiscardAttributes = false;
	bGeometricConstraint = false;
	bShowGroupColors = false;
	GroupBoundaryConstraint = EGroupBoundaryConstraint::Ignore;
	MaterialBoundaryConstraint = EMaterialBoundaryConstraint::Ignore;
}

void USimplifyMeshTool::Setup()
{
	UInteractiveTool::Setup();

	// hide component and create + show preview
	UE::ToolTarget::HideSourceObject(Target);

	Preview = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	Preview->Setup(GetTargetWorld(), this);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(Preview->PreviewMesh, nullptr);
	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);
	Preview->ConfigureMaterials( MaterialSet.Materials,
		ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager())
	);

	// some of this could be done async...
	{
		// if in editor, create progress indicator dialog because building mesh copies can be slow (for very large meshes)
		// this is especially needed because of the copy we make of the meshdescription; for Reasons, copying meshdescription is pretty slow
#if WITH_EDITOR
		static const FText SlowTaskText = LOCTEXT("SimplifyMeshInit", "Building mesh simplification data...");

		FScopedSlowTask SlowTask(3.0f, SlowTaskText);
		SlowTask.MakeDialog();

		// Declare progress shortcut lambdas
		auto EnterProgressFrame = [&SlowTask](int Progress)
	{
			SlowTask.EnterProgressFrame((float)Progress);
		};
#else
		auto EnterProgressFrame = [](int Progress) {};
#endif
		FGetMeshParameters GetMeshParams;
		GetMeshParams.bWantMeshTangents = true;
		OriginalMeshDescription = MakeShared<FMeshDescription, ESPMode::ThreadSafe>(UE::ToolTarget::GetMeshDescriptionCopy(Target, GetMeshParams));

		EnterProgressFrame(1);
		// UE::ToolTarget::GetDynamicMeshCopy() would recompute the tangents a second time here
		OriginalMesh = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>();
		FMeshDescriptionToDynamicMesh Converter; 
		Converter.Convert(OriginalMeshDescription.Get(), *OriginalMesh, true);   // convert with tangent overlay

		EnterProgressFrame(2);
		OriginalMeshSpatial = MakeShared<FDynamicMeshAABBTree3, ESPMode::ThreadSafe>(OriginalMesh.Get(), true);
	}

	Preview->PreviewMesh->SetTransform((FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target));
	Preview->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	Preview->PreviewMesh->UpdatePreview(OriginalMesh.Get());

	// initialize our properties
	SimplifyProperties = NewObject<USimplifyMeshToolProperties>(this);
	SimplifyProperties->RestoreProperties(this);
	AddToolPropertySource(SimplifyProperties);

	SimplifyProperties->WatchProperty(SimplifyProperties->bShowGroupColors,
									  [this](bool bNewValue) { UpdateVisualization(); });

	MeshStatisticsProperties = NewObject<UMeshStatisticsProperties>(this);
	AddToolPropertySource(MeshStatisticsProperties);

	MeshElementsDisplay = NewObject<UMeshElementsVisualizer>(this);
	MeshElementsDisplay->CreateInWorld(Preview->PreviewMesh->GetWorld(), Preview->PreviewMesh->GetTransform());
	if (ensure(MeshElementsDisplay->Settings))
	{
		MeshElementsDisplay->Settings->bShowWireframe = true;
		MeshElementsDisplay->Settings->RestoreProperties(this, TEXT("Simplify"));
		AddToolPropertySource(MeshElementsDisplay->Settings);
	}
	MeshElementsDisplay->SetMeshAccessFunction([this](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc) {
		Preview->ProcessCurrentMesh(ProcessFunc);
	});


	Preview->OnMeshUpdated.AddLambda([this](UMeshOpPreviewWithBackgroundCompute* Compute)
	{
		Compute->ProcessCurrentMesh([&](const FDynamicMesh3& ReadMesh)
		{
			MeshStatisticsProperties->Update(ReadMesh);
			MeshElementsDisplay->NotifyMeshChanged();
		});
	});

	UpdateVisualization();
	Preview->InvalidateResult();

	SetToolDisplayName(LOCTEXT("ToolName", "Simplify"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Reduce the number of triangles in the selected Mesh using various strategies."),
		EToolMessageLevel::UserNotification);
}


bool USimplifyMeshTool::CanAccept() const
{
	return Super::CanAccept() && Preview->HaveValidResult();
}


void USimplifyMeshTool::OnShutdown(EToolShutdownType ShutdownType)
{
	SimplifyProperties->SaveProperties(this);
	
	if (ensure(MeshElementsDisplay->Settings))
	{
		MeshElementsDisplay->Settings->SaveProperties(this, TEXT("Simplify"));
	}
	MeshElementsDisplay->Disconnect();

	UE::ToolTarget::ShowSourceObject(Target);
	FDynamicMeshOpResult Result = Preview->Shutdown();
	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SimplifyMeshToolTransactionName", "Simplify Mesh"));
		UE::ToolTarget::CommitDynamicMeshUpdate(Target, *Result.Mesh, true);
		GetToolManager()->EndUndoTransaction();
	}
}


void USimplifyMeshTool::OnTick(float DeltaTime)
{
	Preview->Tick(DeltaTime);
	MeshElementsDisplay->OnTick(DeltaTime);
}

TUniquePtr<FDynamicMeshOperator> USimplifyMeshTool::MakeNewOperator()
{
	TUniquePtr<FSimplifyMeshOp> Op = MakeUnique<FSimplifyMeshOp>();

	Op->bDiscardAttributes = SimplifyProperties->bDiscardAttributes;
	// We always want attributes enabled on result even if we discard them initially
	Op->bResultMustHaveAttributesEnabled = true;
	Op->bPreventNormalFlips = SimplifyProperties->bPreventNormalFlips;
	Op->bPreserveSharpEdges = SimplifyProperties->bPreserveSharpEdges;
	Op->bAllowSeamCollapse = !SimplifyProperties->bPreserveSharpEdges;
	Op->bReproject = SimplifyProperties->bReproject;
	Op->SimplifierType = SimplifyProperties->SimplifierType;
	Op->TargetCount = ( SimplifyProperties->TargetMode == ESimplifyTargetType::VertexCount) ?  SimplifyProperties->TargetVertexCount : SimplifyProperties->TargetTriangleCount;
	Op->MinimalPlanarAngleThresh = SimplifyProperties->MinimalAngleThreshold;
	Op->TargetEdgeLength = SimplifyProperties->TargetEdgeLength;
	Op->TargetMode = SimplifyProperties->TargetMode;
	Op->TargetPercentage = SimplifyProperties->TargetPercentage;
	Op->MeshBoundaryConstraint = (EEdgeRefineFlags)SimplifyProperties->MeshBoundaryConstraint;
	Op->GroupBoundaryConstraint = (EEdgeRefineFlags)SimplifyProperties->GroupBoundaryConstraint;
	Op->MaterialBoundaryConstraint = (EEdgeRefineFlags)SimplifyProperties->MaterialBoundaryConstraint;
	Op->bGeometricDeviationConstraint = SimplifyProperties->bGeometricConstraint;
	Op->GeometricTolerance = SimplifyProperties->GeometricTolerance;
	Op->PolyEdgeAngleTolerance = SimplifyProperties->PolyEdgeAngleTolerance;
	FTransform LocalToWorld = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target);
	Op->SetTransform(LocalToWorld);

	Op->OriginalMeshDescription = OriginalMeshDescription;
	Op->OriginalMesh = OriginalMesh;
	Op->OriginalMeshSpatial = OriginalMeshSpatial;

	IMeshReductionManagerModule& MeshReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	Op->MeshReduction = MeshReductionModule.GetStaticMeshReductionInterface();

	return Op;
}


void USimplifyMeshTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if ( Property )
	{
		if ( Property->GetFName() == GET_MEMBER_NAME_CHECKED(USimplifyMeshToolProperties, bShowGroupColors) )
		{
			UpdateVisualization();
		}
		else
		{
			Preview->InvalidateResult();
		}
	}
}

void USimplifyMeshTool::UpdateVisualization()
{
	if (SimplifyProperties->bShowGroupColors)
	{
		Preview->OverrideMaterial = ToolSetupUtil::GetSelectionMaterial(GetToolManager());
		Preview->PreviewMesh->SetTriangleColorFunction([this](const FDynamicMesh3* Mesh, int TriangleID)
		{
			return LinearColors::SelectFColor(Mesh->GetTriangleGroup(TriangleID));
		},
		UPreviewMesh::ERenderUpdateMode::FastUpdate);
	}
	else
	{
		Preview->OverrideMaterial = nullptr;
		Preview->PreviewMesh->ClearTriangleColorFunction(UPreviewMesh::ERenderUpdateMode::FastUpdate);
	}
}





#undef LOCTEXT_NAMESPACE

