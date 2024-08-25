// Copyright Epic Games, Inc. All Rights Reserved.

/**
* Control Rig Edit Mode Toolkit
*/
#include "EditMode/ControlRigEditModeToolkit.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "EditorModes.h"
#include "Toolkits/BaseToolkit.h"
#include "EditorModeManager.h"
#include "EditMode/SControlRigEditModeTools.h"
#include "EditMode/ControlRigEditMode.h"
#include "Modules/ModuleManager.h"
#include "EditMode/SControlRigBaseListWidget.h"
#include "EditMode/SControlRigTweenWidget.h"
#include "EditMode/SControlRigSnapper.h"
#include "Tools/SMotionTrailOptions.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "Widgets/Docking/SDockTab.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "EditMode/SControlRigDetails.h"
#include "EditMode/SControlRigOutliner.h"
#include "EditMode/SControlRigSpacePicker.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"

#define LOCTEXT_NAMESPACE "FControlRigEditModeToolkit"

namespace 
{
	static const FName AnimationName(TEXT("Animation")); 
	const TArray<FName> AnimationPaletteNames = { AnimationName };
}

bool FControlRigEditModeToolkit::bMotionTrailsTabOpen = false;
bool FControlRigEditModeToolkit::bPoseTabOpen = false;
bool FControlRigEditModeToolkit::bSnapperTabOpen = false;
bool FControlRigEditModeToolkit::bTweenOpen = false;

const FName FControlRigEditModeToolkit::PoseTabName = FName(TEXT("PoseTab"));
const FName FControlRigEditModeToolkit::MotionTrailTabName = FName(TEXT("MotionTrailTab"));
const FName FControlRigEditModeToolkit::SnapperTabName = FName(TEXT("SnapperTab"));
const FName FControlRigEditModeToolkit::TweenOverlayName = FName(TEXT("TweenOverlay"));
const FName FControlRigEditModeToolkit::OutlinerTabName = FName(TEXT("ControlRigOutlinerTab"));
const FName FControlRigEditModeToolkit::DetailsTabName = FName(TEXT("ControlRigDetailsTab"));
const FName FControlRigEditModeToolkit::SpacePickerTabName = FName(TEXT("ControlRigSpacePicker"));

TSharedPtr<SControlRigDetails> FControlRigEditModeToolkit::Details = nullptr;
TSharedPtr<SControlRigOutliner> FControlRigEditModeToolkit::Outliner = nullptr;

void FControlRigEditModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	SAssignNew(ModeTools, SControlRigEditModeTools, SharedThis(this), EditMode);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;

	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bSearchInitialKeyFocus = false;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	FModeToolkit::Init(InitToolkitHost);
}

void FControlRigEditModeToolkit::GetToolPaletteNames(TArray<FName>& InPaletteName) const
{
	InPaletteName = AnimationPaletteNames;
}

FText FControlRigEditModeToolkit::GetToolPaletteDisplayName(FName PaletteName) const
{
	if (PaletteName == AnimationName)
	{
		FText::FromName(AnimationName);
	}
	return FText();
}

void FControlRigEditModeToolkit::BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolBarBuilder)
{
	if (PaletteName == AnimationName)
	{
		ModeTools->CustomizeToolBarPalette(ToolBarBuilder);
	}
}

void FControlRigEditModeToolkit::OnToolPaletteChanged(FName PaletteName)
{

}

void FControlRigEditModeToolkit::TryInvokeToolkitUI(const FName InName)
{
	TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();

	if (InName == MotionTrailTabName)
	{
		FTabId TabID(MotionTrailTabName);
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(TabID, false /*bIsActive*/);
	}
	else if (InName == PoseTabName)
	{
		FTabId TabID(PoseTabName);
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(TabID, false /*bIsActive*/);
	}
	else if (InName == SnapperTabName)
	{
		FTabId TabID(SnapperTabName);
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(TabID, false /*bIsActive*/);
	}
	else if (InName == OutlinerTabName)
	{
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::TopRightTabID);
	}
	else if (InName == SpacePickerTabName)
	{
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::BottomLeftTabID);
	}
	else if (InName == DetailsTabName)
	{
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::BottomRightTabID);
	}
	else if (InName == TweenOverlayName)
	{
		if(TweenWidgetParent)
		{ 
			RemoveAndDestroyTweenOverlay();
		}
		else
		{
			CreateAndShowTweenOverlay();
		}
	}
}

bool FControlRigEditModeToolkit::IsToolkitUIActive(const FName InName) const
{
	if (InName == TweenOverlayName)
	{
		return TweenWidgetParent.IsValid();
	}

	TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
	return ModeUILayerPtr->GetTabManager()->FindExistingLiveTab(FTabId(InName)).IsValid();
}

FText FControlRigEditModeToolkit::GetActiveToolDisplayName() const
{
	return ModeTools->GetActiveToolName();
}

FText FControlRigEditModeToolkit::GetActiveToolMessage() const
{

	return ModeTools->GetActiveToolMessage();
}

TSharedRef<SDockTab> SpawnPoseTab(const FSpawnTabArgs& Args, TWeakPtr<FControlRigEditModeToolkit> SharedToolkit)
{
	return SNew(SDockTab)
		[
			SNew(SControlRigBaseListWidget)
			.InSharedToolkit(SharedToolkit)
		];
}

TSharedRef<SDockTab> SpawnSnapperTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SNew(SControlRigSnapper)
		];
}

TSharedRef<SDockTab> SpawnMotionTrailTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SNew(SMotionTrailOptions)
		];
}

TSharedRef<SDockTab> SpawnOutlinerTab(const FSpawnTabArgs& Args, FControlRigEditMode* InEditorMode)
{
	return SNew(SDockTab)
		[
			SAssignNew(FControlRigEditModeToolkit::Outliner, SControlRigOutliner, *InEditorMode)
		];
}

TSharedRef<SDockTab> SpawnSpacePickerTab(const FSpawnTabArgs& Args, FControlRigEditMode* InEditorMode)
{
	return SNew(SDockTab)
		[
			SNew(SControlRigSpacePicker, *InEditorMode)
		];
}

TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args, FControlRigEditMode* InEditorMode)
{
	return SNew(SDockTab)
		[
			SAssignNew(FControlRigEditModeToolkit::Details,SControlRigDetails, *InEditorMode)
		];
}


void FControlRigEditModeToolkit::CreateAndShowTweenOverlay()
{
	FVector2D NewTweenWidgetLocation = GetDefault<UControlRigEditModeSettings>()->LastInViewportTweenWidgetLocation;

	if (NewTweenWidgetLocation.IsZero())
	{
		const FVector2D ActiveViewportSize = GetToolkitHost()->GetActiveViewportSize();
		NewTweenWidgetLocation.X = ActiveViewportSize.X / 2.0f;
		NewTweenWidgetLocation.Y = FMath::Max(ActiveViewportSize.Y - 100.0f, 0);
	}
	
	UpdateTweenWidgetLocation(NewTweenWidgetLocation);

	SAssignNew(TweenWidgetParent, SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.Padding(TAttribute<FMargin>(this, &FControlRigEditModeToolkit::GetTweenWidgetPadding))
		[
			SAssignNew(TweenWidget, SControlRigTweenWidget)
			.InOwningToolkit(SharedThis(this))
			.InOwningEditMode(SharedThis(&EditMode))
		];

	TryShowTweenOverlay();
}

void FControlRigEditModeToolkit::GetToNextActiveSlider()
{
	if (TweenWidgetParent && TweenWidget)
	{
		TweenWidget->GetToNextActiveSlider();
	}
}
bool FControlRigEditModeToolkit::CanChangeAnimSliderTool() const
{
	return (TweenWidgetParent && TweenWidget);
}

void FControlRigEditModeToolkit::DragAnimSliderTool(double Val)
{
	if (TweenWidgetParent && TweenWidget)
	{
		TweenWidget->DragAnimSliderTool(Val);
	}
}
void FControlRigEditModeToolkit::ResetAnimSlider()
{
	if (TweenWidgetParent && TweenWidget)
	{
		TweenWidget->ResetAnimSlider();
	}
}

void FControlRigEditModeToolkit::StartAnimSliderTool()
{
	if (TweenWidgetParent && TweenWidget)
	{
		TweenWidget->StartAnimSliderTool();
	}
}
void FControlRigEditModeToolkit::TryShowTweenOverlay()
{
	if (TweenWidgetParent)
	{
		GetToolkitHost()->AddViewportOverlayWidget(TweenWidgetParent.ToSharedRef());
	}
}

void FControlRigEditModeToolkit::RemoveAndDestroyTweenOverlay()
{
	TryRemoveTweenOverlay();
	if (TweenWidgetParent)
	{
		TweenWidgetParent.Reset();
		TweenWidget.Reset();
	}
}

void FControlRigEditModeToolkit::TryRemoveTweenOverlay()
{
	if (IsHosted() && TweenWidgetParent)
	{
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
		{
			if (TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule->GetFirstLevelEditor())
			{
				for (TSharedPtr<SLevelViewport> LevelViewport : LevelEditor->GetViewports())
				{
					if (LevelViewport.IsValid())
					{
						if (TweenWidgetParent)
						{
							LevelViewport->RemoveOverlayWidget(TweenWidgetParent.ToSharedRef());
						}
					}
				}
			}
		}
	}
}

void FControlRigEditModeToolkit::UpdateTweenWidgetLocation(const FVector2D InLocation)
{
	const FVector2D ActiveViewportSize = GetToolkitHost()->GetActiveViewportSize();
	FVector2D ScreenPos = InLocation;

	const float EdgeFactor = 0.97f;
	const float MinX = ActiveViewportSize.X * (1 - EdgeFactor);
	const float MinY = ActiveViewportSize.Y * (1 - EdgeFactor);
	const float MaxX = ActiveViewportSize.X * EdgeFactor;
	const float MaxY = ActiveViewportSize.Y * EdgeFactor;
	const bool bOutside = ScreenPos.X < MinX || ScreenPos.X > MaxX || ScreenPos.Y < MinY || ScreenPos.Y > MaxY;
	if (bOutside)
	{
		// reset the location if it was placed out of bounds
		ScreenPos.X = ActiveViewportSize.X / 2.0f;
		ScreenPos.Y = FMath::Max(ActiveViewportSize.Y - 100.0f, 0);
	}
	InViewportTweenWidgetLocation = ScreenPos;
	UControlRigEditModeSettings* ControlRigEditModeSettings = GetMutableDefault<UControlRigEditModeSettings>();
	ControlRigEditModeSettings->LastInViewportTweenWidgetLocation = ScreenPos;
	ControlRigEditModeSettings->SaveConfig();
}

FMargin FControlRigEditModeToolkit::GetTweenWidgetPadding() const
{
	return FMargin(InViewportTweenWidgetLocation.X, InViewportTweenWidgetLocation.Y, 0, 0);
}

void FControlRigEditModeToolkit::RequestModeUITabs()
{
	FModeToolkit::RequestModeUITabs();
	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		TSharedRef<FWorkspaceItem> MenuGroup = ModeUILayerPtr->GetModeMenuCategory().ToSharedRef();

		FMinorTabConfig DetailTabInfo;
		DetailTabInfo.OnSpawnTab = FOnSpawnTab::CreateStatic(&SpawnDetailsTab, &EditMode);
		DetailTabInfo.TabLabel = LOCTEXT("ControlRigDetailTab", "Anim Details");
		DetailTabInfo.TabTooltip = LOCTEXT("ControlRigDetailTabTooltip", "Show Details For Selected Controls.");
		ModeUILayerPtr->SetModePanelInfo(UAssetEditorUISubsystem::BottomRightTabID, DetailTabInfo);

		FMinorTabConfig OutlinerTabInfo;
		OutlinerTabInfo.OnSpawnTab = FOnSpawnTab::CreateStatic(&SpawnOutlinerTab, &EditMode);
		OutlinerTabInfo.TabLabel = LOCTEXT("AnimationOutlinerTab", "Anim Outliner");
		OutlinerTabInfo.TabTooltip = LOCTEXT("AnimationOutlinerTabTooltip", "Control Rig Controls");
		ModeUILayerPtr->SetModePanelInfo(UAssetEditorUISubsystem::TopRightTabID, OutlinerTabInfo);
		/* doesn't work as expected
		FMinorTabConfig SpawnSpacePickerTabInfo;
		SpawnSpacePickerTabInfo.OnSpawnTab = FOnSpawnTab::CreateStatic(&SpawnSpacePickerTab, &EditMode);
		SpawnSpacePickerTabInfo.TabLabel = LOCTEXT("ControlRigSpacePickerTab", "Control Rig Space Picker");
		SpawnSpacePickerTabInfo.TabTooltip = LOCTEXT("ControlRigSpacePickerTabTooltip", "Control Rig Space Picker");
		ModeUILayerPtr->SetModePanelInfo(UAssetEditorUISubsystem::TopLeftTabID, SpawnSpacePickerTabInfo);
		*/

		ModeUILayerPtr->GetTabManager()->UnregisterTabSpawner(SnapperTabName);
		ModeUILayerPtr->GetTabManager()->RegisterTabSpawner(SnapperTabName, FOnSpawnTab::CreateStatic(&SpawnSnapperTab))
			.SetDisplayName(LOCTEXT("ControlRigSnapperTab", "Control Rig Snapper"))
			.SetTooltipText(LOCTEXT("ControlRigSnapperTabTooltip", "Snap child objects to a parent object over a set of frames."))
			.SetGroup(MenuGroup)
			.SetIcon(FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.SnapperTool")));
		ModeUILayerPtr->GetTabManager()->RegisterDefaultTabWindowSize(SnapperTabName, FVector2D(300, 325));

		ModeUILayerPtr->GetTabManager()->UnregisterTabSpawner(PoseTabName);

		TWeakPtr<FControlRigEditModeToolkit> WeakToolkit = SharedThis(this);
		ModeUILayerPtr->GetTabManager()->RegisterTabSpawner(PoseTabName, FOnSpawnTab::CreateLambda([WeakToolkit](const FSpawnTabArgs& Args)
		{
			return SpawnPoseTab(Args, WeakToolkit);
		}))
			.SetDisplayName(LOCTEXT("ControlRigPoseTab", "Control Rig Pose"))
			.SetTooltipText(LOCTEXT("ControlRigPoseTabTooltip", "Show Poses."))
			.SetGroup(MenuGroup)
			.SetIcon(FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.PoseTool")));
		ModeUILayerPtr->GetTabManager()->RegisterDefaultTabWindowSize(PoseTabName, FVector2D(675, 625));

		ModeUILayerPtr->GetTabManager()->UnregisterTabSpawner(MotionTrailTabName);
		ModeUILayerPtr->GetTabManager()->RegisterTabSpawner(MotionTrailTabName, FOnSpawnTab::CreateStatic(&SpawnMotionTrailTab))
			.SetDisplayName(LOCTEXT("MotionTrailTab", "Motion Trail"))
			.SetTooltipText(LOCTEXT("MotionTrailTabTooltip", "Display motion trails for animated objects."))
			.SetGroup(MenuGroup)
			.SetIcon(FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.EditableMotionTrails")));
		ModeUILayerPtr->GetTabManager()->RegisterDefaultTabWindowSize(MotionTrailTabName, FVector2D(425, 575));

		ModeUILayer.Pin()->ToolkitHostShutdownUI().BindSP(this, &FControlRigEditModeToolkit::UnregisterAndRemoveFloatingTabs);


	}
};

void FControlRigEditModeToolkit::InvokeUI()
{
	FModeToolkit::InvokeUI();

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::TopRightTabID);
		// doesn't work as expected todo ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::TopLeftTabID);
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::BottomRightTabID);
		if (bTweenOpen)
		{
			CreateAndShowTweenOverlay();
		}
		if (bMotionTrailsTabOpen)
		{
			TryInvokeToolkitUI(MotionTrailTabName);
		}
		if (bSnapperTabOpen)
		{
			TryInvokeToolkitUI(SnapperTabName);
		}
		if (bPoseTabOpen)
		{
			TryInvokeToolkitUI(PoseTabName);
		}
	}	
}

void FControlRigEditModeToolkit::UnregisterAndRemoveFloatingTabs()
{
	if (FSlateApplication::IsInitialized())
	{
		if (TweenWidgetParent)
		{
			bTweenOpen = true;
		}
		else
		{
			bTweenOpen = false;
		}
		RemoveAndDestroyTweenOverlay();
		if (ModeUILayer.IsValid())
		{
			TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();

			TSharedPtr<SDockTab> MotionTrailTab = ModeUILayerPtr->GetTabManager()->FindExistingLiveTab(FTabId(MotionTrailTabName));
			if (MotionTrailTab)
			{
				bMotionTrailsTabOpen = true;
				MotionTrailTab->RequestCloseTab();
			}
			else
			{
				bMotionTrailsTabOpen = false;
			}
			ModeUILayerPtr->GetTabManager()->UnregisterTabSpawner(MotionTrailTabName);

			TSharedPtr<SDockTab> SnapperTab = ModeUILayerPtr->GetTabManager()->FindExistingLiveTab(FTabId(SnapperTabName));
			if (SnapperTab)
			{
				bSnapperTabOpen = true;
				SnapperTab->RequestCloseTab();
			}
			else
			{
				bSnapperTabOpen = false;
			}
			ModeUILayerPtr->GetTabManager()->UnregisterTabSpawner(SnapperTabName);
		
			TSharedPtr<SDockTab> PoseTab = ModeUILayerPtr->GetTabManager()->FindExistingLiveTab(FTabId(PoseTabName));
			if (PoseTab)
			{
				bPoseTabOpen = true;
				PoseTab->RequestCloseTab();
			}
			else
			{
				bPoseTabOpen = false;
			}
			ModeUILayerPtr->GetTabManager()->UnregisterTabSpawner(PoseTabName);
		}
	}
}

#undef LOCTEXT_NAMESPACE
