// Copyright Epic Games, Inc. All Rights Reserved.


#include "MeshSelect.h"
#include "InputState.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"
#include "MeshPaintHelpers.h"
#include "Components/MeshComponent.h"
#include "IMeshPaintComponentAdapter.h"
#include "MeshPaintAdapterFactory.h"
#include "EngineUtils.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshSelect)


#define LOCTEXT_NAMESPACE "MeshSelection"
bool UVertexAdapterClickToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UVertexAdapterClickToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UVertexAdapterClickTool* NewTool = NewObject<UVertexAdapterClickTool>(SceneState.ToolManager);
	return NewTool;
}

bool UTextureAdapterClickToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UTextureAdapterClickToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UTextureAdapterClickTool* NewTool = NewObject<UTextureAdapterClickTool>(SceneState.ToolManager);
	return NewTool;
}


UMeshClickTool::UMeshClickTool()
{
}

void UMeshClickTool::Setup()
{
	UInteractiveTool::Setup();

	// add default button input behaviors for devices
	USingleClickInputBehavior* MouseBehavior = NewObject<USingleClickInputBehavior>();
	MouseBehavior->Initialize(this);
	MouseBehavior->Modifiers.RegisterModifier(AdditiveSelectionModifier, FInputDeviceState::IsShiftKeyDown);
	AddInputBehavior(MouseBehavior);

	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartMeshSelectTool", "Select a mesh. Switch tools to paint vertex colors, blend between textures, or paint directly onto a texture file."),
		EToolMessageLevel::UserNotification);

	// Set up selection mechanic to select valid meshes
	SelectionMechanic = NewObject<UMeshPaintSelectionMechanic>(this);
	SelectionMechanic->Setup(this);
}

void UMeshClickTool::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	if (ModifierID == AdditiveSelectionModifier)
	{
		SelectionMechanic->SetAddToSelectionSet(bIsOn);
	}
}

FInputRayHit UMeshClickTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	return SelectionMechanic->IsHitByClick(ClickPos);
}

void UMeshClickTool::OnClicked(const FInputDeviceRay& ClickPos)
{
	SelectionMechanic->OnClicked(ClickPos);
}


UVertexAdapterClickTool::UVertexAdapterClickTool()
	: UMeshClickTool()
{

}

bool UVertexAdapterClickTool::IsMeshAdapterSupported(TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter) const
{
	return MeshAdapter.IsValid() ? MeshAdapter->SupportsVertexPaint() : false;
}

UTextureAdapterClickTool::UTextureAdapterClickTool()
	: UMeshClickTool()
{

}

bool UTextureAdapterClickTool::IsMeshAdapterSupported(TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter) const
{
	return MeshAdapter.IsValid() ? MeshAdapter->SupportsTexturePaint() : false;
}

#undef LOCTEXT_NAMESPACE
