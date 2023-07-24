// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/PhysicsInspectorTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "ModelingToolTargetUtil.h"
#include "Drawing/PreviewGeometryActor.h"
#include "Util/ColorConstants.h"

#include "Physics/PhysicsDataCollection.h"
#include "Physics/CollisionGeometryVisualization.h"

// physics data
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"

#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "TargetInterfaces/PhysicsDataSource.h"
#include "ToolTargetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsInspectorTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UPhysicsInspectorTool"


const FToolTargetTypeRequirements& UPhysicsInspectorToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UPrimitiveComponentBackedTarget::StaticClass(),
		UPhysicsDataSource::StaticClass()
		});
	return TypeRequirements;
}


UMultiSelectionMeshEditingTool* UPhysicsInspectorToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UPhysicsInspectorTool>(SceneState.ToolManager);
}


void UPhysicsInspectorTool::Setup()
{
	UInteractiveTool::Setup();

	VizSettings = NewObject<UCollisionGeometryVisualizationProperties>(this);
	VizSettings->RestoreProperties(this);
	AddToolPropertySource(VizSettings);
	VizSettings->WatchProperty(VizSettings->LineThickness, [this](float NewValue) { bVisualizationDirty = true; });
	VizSettings->WatchProperty(VizSettings->Color, [this](FColor NewValue) { bVisualizationDirty = true; });
	VizSettings->WatchProperty(VizSettings->bRandomColors, [this](bool bNewValue) { bVisualizationDirty = true; });
	VizSettings->WatchProperty(VizSettings->bShowHidden, [this](bool bNewValue) { bVisualizationDirty = true; });

	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UBodySetup* BodySetup = UE::ToolTarget::GetPhysicsBodySetup(Targets[ComponentIdx]);
		if (BodySetup)
		{
			TSharedPtr<FPhysicsDataCollection> PhysicsData = MakeShared<FPhysicsDataCollection>();
			PhysicsData->InitializeFromComponent( UE::ToolTarget::GetTargetComponent(Targets[ComponentIdx]), true);

			PhysicsInfos.Add(PhysicsData);

			UPreviewGeometry* PreviewGeom = NewObject<UPreviewGeometry>(this);
			FTransform TargetTransform = (FTransform) UE::ToolTarget::GetLocalToWorldTransform(Targets[ComponentIdx]);
			PhysicsData->ExternalScale3D = TargetTransform.GetScale3D();
			TargetTransform.SetScale3D(FVector::OneVector);
			PreviewGeom->CreateInWorld(UE::ToolTarget::GetTargetActor(Targets[ComponentIdx])->GetWorld(), TargetTransform);
			PreviewElements.Add(PreviewGeom);

			InitializeGeometry(*PhysicsData, PreviewGeom);

			UPhysicsObjectToolPropertySet* ObjectProps = NewObject<UPhysicsObjectToolPropertySet>(this);
			InitializeObjectProperties(*PhysicsData, ObjectProps);
			AddToolPropertySource(ObjectProps);
		}
	}

	SetToolDisplayName(LOCTEXT("ToolName", "Physics Inspector"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Inspect Physics data for the selected Objects"),
		EToolMessageLevel::UserNotification);
}


void UPhysicsInspectorTool::OnShutdown(EToolShutdownType ShutdownType)
{
	VizSettings->SaveProperties(this);

	for (UPreviewGeometry* Preview : PreviewElements)
	{
		Preview->Disconnect();
	}
}





void UPhysicsInspectorTool::OnTick(float DeltaTime)
{
	if (bVisualizationDirty)
	{
		UpdateVisualization();
		bVisualizationDirty = false;
	}
}



void UPhysicsInspectorTool::UpdateVisualization()
{
	float UseThickness = VizSettings->LineThickness;
	FColor UseColor = VizSettings->Color;
	LineMaterial = ToolSetupUtil::GetDefaultLineComponentMaterial(GetToolManager(), !VizSettings->bShowHidden);

	int32 ColorIdx = 0;
	for (UPreviewGeometry* Preview : PreviewElements)
	{
		Preview->UpdateAllLineSets([&](ULineSetComponent* LineSet)
		{
			LineSet->SetAllLinesThickness(UseThickness);
			LineSet->SetAllLinesColor(VizSettings->bRandomColors ? LinearColors::SelectFColor(ColorIdx++) : UseColor);
		});
		Preview->SetAllLineSetsMaterial(LineMaterial);
	}
}



void UPhysicsInspectorTool::InitializeObjectProperties(const FPhysicsDataCollection& PhysicsData, UPhysicsObjectToolPropertySet* PropSet)
{
	UE::PhysicsTools::InitializePhysicsToolObjectPropertySet(&PhysicsData, PropSet);
}



void UPhysicsInspectorTool::InitializeGeometry(const FPhysicsDataCollection& PhysicsData, UPreviewGeometry* PreviewGeom)
{
	UE::PhysicsTools::InitializePreviewGeometryLines(PhysicsData, PreviewGeom,
		VizSettings->Color, VizSettings->LineThickness, 0.0f, 16, VizSettings->bRandomColors);
}



#undef LOCTEXT_NAMESPACE
