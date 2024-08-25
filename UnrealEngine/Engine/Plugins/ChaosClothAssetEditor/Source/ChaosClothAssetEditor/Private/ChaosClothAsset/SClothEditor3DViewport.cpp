// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SClothEditor3DViewport.h"
#include "ChaosClothAsset/ClothEditor3DViewportClient.h"
#include "ChaosClothAsset/SClothEditor3DViewportToolBar.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "ChaosClothAsset/ClothEditorMode.h"
#include "ChaosClothAsset/ClothEditorPreviewScene.h"
#include "ChaosClothAsset/SClothAnimationScrubPanel.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "EditorModeTools.h"
#include "Widgets/Layout/SBorder.h"
#include "Components/SkeletalMeshComponent.h"

#define LOCTEXT_NAMESPACE "SChaosClothAssetEditor3DViewport"

void SChaosClothAssetEditor3DViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	SAssetEditorViewport::FArguments ParentArgs;
	ParentArgs._EditorViewportClient = InArgs._EditorViewportClient;
	if (InArgs._ViewportSize.IsSet())
	{
		ParentArgs._ViewportSize = InArgs._ViewportSize;
	}
	ToolkitCommandList = InArgs._ToolkitCommandList;
	SAssetEditorViewport::Construct(ParentArgs, InViewportConstructionArgs);
	Client->VisibilityDelegate.BindSP(this, &SChaosClothAssetEditor3DViewport::IsVisible);

	ViewportOverlay->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.FillWidth(1)
		.Padding(10.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
			.Visibility_Raw(this, &SChaosClothAssetEditor3DViewport::GetAnimControlVisibility)
			.Padding(10.0f, 2.0f)
			[
				SNew(SClothAnimationScrubPanel, GetPreviewScene())
				.ViewInputMin(this, &SChaosClothAssetEditor3DViewport::GetViewMinInput)
				.ViewInputMax(this, &SChaosClothAssetEditor3DViewport::GetViewMaxInput)
			]
		]
	];
}

TWeakPtr<UE::Chaos::ClothAsset::FChaosClothPreviewScene> SChaosClothAssetEditor3DViewport::GetPreviewScene()
{
	const TSharedPtr<UE::Chaos::ClothAsset::FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<UE::Chaos::ClothAsset::FChaosClothAssetEditor3DViewportClient>(Client);
	return ClothViewportClient->GetClothPreviewScene();
}

TWeakPtr<const UE::Chaos::ClothAsset::FChaosClothPreviewScene> SChaosClothAssetEditor3DViewport::GetPreviewScene() const
{
	const TSharedPtr<const UE::Chaos::ClothAsset::FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<UE::Chaos::ClothAsset::FChaosClothAssetEditor3DViewportClient>(Client);
	return ClothViewportClient->GetClothPreviewScene();
}


void SChaosClothAssetEditor3DViewport::BindCommands()
{
	using namespace UE::Chaos::ClothAsset;

	SAssetEditorViewport::BindCommands();
	const FChaosClothAssetEditorCommands& CommandInfos = FChaosClothAssetEditorCommands::Get();

	ToolkitCommandList->MapAction(
		CommandInfos.TogglePreviewWireframe,
		FExecuteAction::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
			ClothViewportClient->EnableRenderMeshWireframe(!ClothViewportClient->RenderMeshWireframeEnabled());
		}),
		FCanExecuteAction::CreateLambda([this]() { return true; }),
		FIsActionChecked::CreateLambda([this]() { return StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client)->RenderMeshWireframeEnabled(); }));

	ToolkitCommandList->MapAction(
		CommandInfos.SoftResetSimulation,
		FExecuteAction::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
			ClothViewportClient->SoftResetSimulation();
		}),
		FCanExecuteAction::CreateLambda([this]() { return true; }),
		FIsActionChecked::CreateLambda([this]() { return false; }));

	ToolkitCommandList->MapAction(
		CommandInfos.HardResetSimulation,
		FExecuteAction::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
			ClothViewportClient->HardResetSimulation();
		}),
		FCanExecuteAction::CreateLambda([this]() { return true; }),
		FIsActionChecked::CreateLambda([this]() { return false; }));


	ToolkitCommandList->MapAction(
		CommandInfos.ToggleSimulationSuspended,
		FExecuteAction::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);

			const bool bIsSuspended = ClothViewportClient->IsSimulationSuspended();
			if (bIsSuspended)
			{
				ClothViewportClient->ResumeSimulation();
			}
			else
			{
				ClothViewportClient->SuspendSimulation();
			}
		}),
		FCanExecuteAction::CreateLambda([this]() { return true; }),
		FIsActionChecked::CreateLambda([this]() { return StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client)->IsSimulationSuspended(); }) );

	ToolkitCommandList->MapAction(
		CommandInfos.LODAuto,
		FExecuteAction::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
			ClothViewportClient->SetLODModel(INDEX_NONE);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
			return ClothViewportClient->IsLODModelSelected(INDEX_NONE);
		}
		));

	ToolkitCommandList->MapAction(
		CommandInfos.LOD0,
		FExecuteAction::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
			ClothViewportClient->SetLODModel(0);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
			return ClothViewportClient->IsLODModelSelected(0);
		}
		));

	// all other LODs will be added dynamically

}

TSharedPtr<SWidget> SChaosClothAssetEditor3DViewport::MakeViewportToolbar()
{
	return SNew(SChaosClothAssetEditor3DViewportToolBar, SharedThis(this))
		.CommandList(ToolkitCommandList);
}

bool SChaosClothAssetEditor3DViewport::IsVisible() const
{
	// Intentionally not calling SEditorViewport::IsVisible because it will return false if our simulation is more than 250ms.
	return ViewportWidget.IsValid();
}

void SChaosClothAssetEditor3DViewport::OnFocusViewportToSelection()
{
	using namespace UE::Chaos::ClothAsset;

	const TSharedPtr<const FChaosClothAssetEditor3DViewportClient> ViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
	const FBox PreviewBoundingBox = ViewportClient->PreviewBoundingBox();

	if (PreviewBoundingBox.IsValid && !(PreviewBoundingBox.Min == FVector::Zero() && PreviewBoundingBox.Max == FVector::Zero()))
	{
		Client->FocusViewportOnBox(PreviewBoundingBox);
	}
}

TSharedRef<class SEditorViewport> SChaosClothAssetEditor3DViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SChaosClothAssetEditor3DViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

float SChaosClothAssetEditor3DViewport::GetViewMinInput() const
{
	return 0.0f;
}

float SChaosClothAssetEditor3DViewport::GetViewMaxInput() const
{
	using namespace UE::Chaos::ClothAsset;

	// (these are non-const because UAnimSingleNodeInstance::GetLength() is non-const)
	const TSharedPtr<FChaosClothAssetEditor3DViewportClient> ClothViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(Client);
	const TSharedPtr<FChaosClothPreviewScene> Scene = ClothViewportClient->GetClothPreviewScene().Pin();
	if (Scene)
	{
		if (UAnimSingleNodeInstance* const PreviewInstance = Scene->GetPreviewAnimInstance())
		{
			return PreviewInstance->GetLength();
		}
	}

	return 0.0f;
}

EVisibility SChaosClothAssetEditor3DViewport::GetAnimControlVisibility() const
{
	using namespace UE::Chaos::ClothAsset;

	const TSharedPtr<const FChaosClothPreviewScene> Scene = GetPreviewScene().Pin();
	return (Scene && Scene->GetSkeletalMeshComponent() && Scene->GetSkeletalMeshComponent()->GetSkeletalMeshAsset() && Scene->GetPreviewAnimInstance()) ? EVisibility::Visible : EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE
