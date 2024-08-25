// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SClothEditorRestSpaceViewport.h"
#include "ChaosClothAsset/ClothEditorRestSpaceViewportClient.h"
#include "SViewportToolBar.h"
#include "ChaosClothAsset/SClothEditorRestSpaceViewportToolBar.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "ChaosClothAsset/ClothEditorMode.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "SChaosClothAssetEditorRestSpaceViewport"

void SChaosClothAssetEditorRestSpaceViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	RestSpaceViewportClient = InArgs._RestSpaceViewportClient;

	SAssetEditorViewport::FArguments ParentArgs;
	ParentArgs._EditorViewportClient = InArgs._RestSpaceViewportClient;
	if (InArgs._ViewportSize.IsSet())
	{
		ParentArgs._ViewportSize = InArgs._ViewportSize;
	}
	SAssetEditorViewport::Construct(ParentArgs, InViewportConstructionArgs);
	Client->VisibilityDelegate.BindSP(this, &SChaosClothAssetEditorRestSpaceViewport::IsVisible);
}

UChaosClothAssetEditorMode* SChaosClothAssetEditorRestSpaceViewport::GetEdMode() const
{
	if (const FEditorModeTools* const EditorModeTools = Client->GetModeTools())
	{
		if (UChaosClothAssetEditorMode* const ClothEdMode = Cast<UChaosClothAssetEditorMode>(EditorModeTools->GetActiveScriptableMode(UChaosClothAssetEditorMode::EM_ChaosClothAssetEditorModeId)))
		{
			return ClothEdMode;
		}
	}
	return nullptr;
}

void SChaosClothAssetEditorRestSpaceViewport::BindCommands()
{
	using namespace UE::Chaos::ClothAsset;

	SAssetEditorViewport::BindCommands();

	const FChaosClothAssetEditorCommands& CommandInfos = FChaosClothAssetEditorCommands::Get();

	CommandList->MapAction(
		CommandInfos.SetConstructionMode2D,
		FExecuteAction::CreateLambda([this]()
		{
			if (UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				ClothEdMode->SetConstructionViewMode(EClothPatternVertexType::Sim2D);
			}
		}),
		FCanExecuteAction::CreateLambda([this]() 
		{ 
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->CanChangeConstructionViewModeTo(EClothPatternVertexType::Sim2D);
			}
			return false; 
		}),
		FIsActionChecked::CreateLambda([this]() 
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->GetConstructionViewMode() == EClothPatternVertexType::Sim2D;
			}
			return false;
		}));


	CommandList->MapAction(
		CommandInfos.SetConstructionMode3D,
		FExecuteAction::CreateLambda([this]()
		{
			if (UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode()) 
			{
				ClothEdMode->SetConstructionViewMode(EClothPatternVertexType::Sim3D);
			}
		}),
		FCanExecuteAction::CreateLambda([this]() 
		{ 
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->CanChangeConstructionViewModeTo(EClothPatternVertexType::Sim3D);
			}
			return false; 
		}),
		FIsActionChecked::CreateLambda([this]() 
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->GetConstructionViewMode() == EClothPatternVertexType::Sim3D;
			}
			return false;
		}));



	CommandList->MapAction(
		CommandInfos.SetConstructionModeRender,
		FExecuteAction::CreateLambda([this]()
		{
			if (UChaosClothAssetEditorMode* ClothEdMode = GetEdMode()) 
			{
				ClothEdMode->SetConstructionViewMode(EClothPatternVertexType::Render);
			}
		}),
		FCanExecuteAction::CreateLambda([this]() 
		{ 
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->CanChangeConstructionViewModeTo(EClothPatternVertexType::Render);
			}
			return false; 
		}),
		FIsActionChecked::CreateLambda([this]() 
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->GetConstructionViewMode() == EClothPatternVertexType::Render;
			}
			return false;
		}));

	CommandList->MapAction(
		CommandInfos.ToggleConstructionViewWireframe,
		FExecuteAction::CreateLambda([this]()
		{
			const FEditorModeTools* const EditorModeTools = Client->GetModeTools();
			UChaosClothAssetEditorMode* const ClothEdMode = Cast<UChaosClothAssetEditorMode>(EditorModeTools->GetActiveScriptableMode(UChaosClothAssetEditorMode::EM_ChaosClothAssetEditorModeId));

			if (ClothEdMode)
			{
				ClothEdMode->ToggleConstructionViewWireframe();
			}
		}),
		FCanExecuteAction::CreateLambda([this]() 
		{ 
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->CanSetConstructionViewWireframeActive();
			}
			return false;
		}),
		FIsActionChecked::CreateLambda([this]() 
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->IsConstructionViewWireframeActive();
			}
			return false;
		}));

	CommandList->MapAction(
		CommandInfos.ToggleConstructionViewSeams,
		FExecuteAction::CreateLambda([this]()
		{
			if (UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				ClothEdMode->ToggleConstructionViewSeams();
			}
		}),
		FCanExecuteAction::CreateLambda([this]()
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->CanSetConstructionViewSeamsActive();
			}
			return false;
		}),
		FIsActionChecked::CreateLambda([this]()
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->IsConstructionViewSeamsActive();
			}
			return false;
		}));

	CommandList->MapAction(
		CommandInfos.ToggleConstructionViewSeamsCollapse,
		FExecuteAction::CreateLambda([this]()
		{
			if (UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				ClothEdMode->ToggleConstructionViewSeamsCollapse();
			}
		}),
		FCanExecuteAction::CreateLambda([this]()
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->CanSetConstructionViewSeamsCollapse();
			}
			return false;
		}),
		FIsActionChecked::CreateLambda([this]()
		{
			if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
			{
				return ClothEdMode->IsConstructionViewSeamsCollapseActive();
			}
			return false;
		}));

	CommandList->MapAction(
		CommandInfos.TogglePatternColor,
		FExecuteAction::CreateLambda([this]()
			{
				if (UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
				{
					ClothEdMode->TogglePatternColor();
				}
			}),
		FCanExecuteAction::CreateLambda([this]()
			{
				if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
				{
					return ClothEdMode->CanSetPatternColor();
				}
				return false;
			}),
		FIsActionChecked::CreateLambda([this]()
			{
				if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
				{
					return ClothEdMode->IsPatternColorActive();
				}
				return false;
			}));

	CommandList->MapAction(
		CommandInfos.ToggleMeshStats,
		FExecuteAction::CreateLambda([this]()
			{
				if (UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
				{
					ClothEdMode->ToggleMeshStats();
				}
			}),
		FCanExecuteAction::CreateLambda([this]()
			{
				if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
				{
					return ClothEdMode->CanSetMeshStats();
				}
				return false;
			}),
		FIsActionChecked::CreateLambda([this]()
			{
				if (const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode())
				{
					return ClothEdMode->IsMeshStatsActive();
				}
				return false;
			}));

}

TSharedPtr<SWidget> SChaosClothAssetEditorRestSpaceViewport::MakeViewportToolbar()
{
	return SNew(SChaosClothAssetEditorRestSpaceViewportToolBar, SharedThis(this))
		.CommandList(CommandList)
		.RestSpaceViewportClient(RestSpaceViewportClient);
}


void SChaosClothAssetEditorRestSpaceViewport::OnFocusViewportToSelection()
{
	const UChaosClothAssetEditorMode* const ClothEdMode = GetEdMode();

	if (ClothEdMode)
	{
		const FBox BoundingBox = ClothEdMode->SelectionBoundingBox();
		if (BoundingBox.IsValid && !(BoundingBox.Min == FVector::Zero() && BoundingBox.Max == FVector::Zero()))
		{
			Client->FocusViewportOnBox(BoundingBox);

			// Reset any changes to the clip planes by the scroll zoom behavior
			Client->OverrideNearClipPlane(UE_KINDA_SMALL_NUMBER);
			Client->OverrideFarClipPlane(0);
		}
	}
}

bool SChaosClothAssetEditorRestSpaceViewport::IsVisible() const
{
	// Intentionally not calling SEditorViewport::IsVisible because it will return false if our simulation is more than 250ms.
	return ViewportWidget.IsValid();
}

TSharedRef<class SEditorViewport> SChaosClothAssetEditorRestSpaceViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SChaosClothAssetEditorRestSpaceViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SChaosClothAssetEditorRestSpaceViewport::OnFloatingButtonClicked()
{
}

#undef LOCTEXT_NAMESPACE
