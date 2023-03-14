// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingToolsEditorModeToolkit.h"
#include "IGeometryProcessingInterfacesModule.h"
#include "GeometryProcessingInterfaces/UVEditorAssetEditor.h"
#include "ModelingToolsEditorMode.h"
#include "ModelingToolsManagerActions.h"
#include "ModelingToolsEditorModeSettings.h"
#include "ModelingToolsEditorModeStyle.h"
#include "Selection/GeometrySelectionManager.h"
#include "Engine/Selection.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "IDetailsView.h"
#include "ISettingsModule.h"
#include "EditorModeManager.h"
#include "Toolkits/AssetEditorModeUILayer.h"

#include "Widgets/Input/SButton.h"
#include "SSimpleButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Framework/MultiBox/SToolBarButtonBlock.h"
#include "Internationalization/Text.h"
#include "ModelingWidgets/ModelingCustomizationUtil.h"

// for Tool Extensions
#include "Features/IModularFeatures.h"
#include "ModelingModeToolExtensions.h"

// for Object Type properties
#include "PropertySets/CreateMeshObjectTypeProperties.h"

// for quick settings
#include "EditorInteractiveToolsFrameworkModule.h"
#include "Tools/EditorComponentSourceFactory.h"
#include "ToolTargetManager.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"
#include "SPrimaryButton.h"
#include "Dialogs/DlgPickPath.h"
#include "ModelingModeAssetUtils.h"

#include "LevelEditorViewport.h"


#define LOCTEXT_NAMESPACE "FModelingToolsEditorModeToolkit"


// if set to 1, then on mode initialization we include buttons for prototype modeling tools
static TAutoConsoleVariable<int32> CVarEnablePrototypeModelingTools(
	TEXT("modeling.EnablePrototypes"),
	0,
	TEXT("Enable unsupported Experimental prototype Modeling Tools"));
static TAutoConsoleVariable<int32> CVarEnablePolyModeling(
	TEXT("modeling.EnablePolyModel"),
	0,
	TEXT("Enable prototype PolyEdit tab"));



FModelingToolsEditorModeToolkit::FModelingToolsEditorModeToolkit()
{
}


FModelingToolsEditorModeToolkit::~FModelingToolsEditorModeToolkit()
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	Settings->OnModified.Remove(AssetSettingsModifiedHandle);
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolNotificationMessage.RemoveAll(this);
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolWarningMessage.RemoveAll(this);
}


void FModelingToolsEditorModeToolkit::CustomizeModeDetailsViewArgs(FDetailsViewArgs& ArgsInOut)
{
	//ArgsInOut.ColumnWidth = 0.3f;
}


TSharedPtr<SWidget> FModelingToolsEditorModeToolkit::GetInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.VAlign(VAlign_Fill)
		[
			ToolkitWidget.ToSharedRef()
		];
}


void FModelingToolsEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	// Have to create the ToolkitWidget here because FModeToolkit::Init() is going to ask for it and add
	// it to the Mode panel, and not ask again afterwards. However we have to call Init() to get the 
	// ModeDetailsView created, that we need to add to the ToolkitWidget. So, we will create the Widget
	// here but only add the rows to it after we call Init()
	TSharedPtr<SVerticalBox> ToolkitWidgetVBox = SNew(SVerticalBox);
	SAssignNew(ToolkitWidget, SBorder)
		.HAlign(HAlign_Fill)
		.Padding(4)
		[
			ToolkitWidgetVBox->AsShared()
		];

	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	GetToolkitHost()->OnActiveViewportChanged().AddSP(this, &FModelingToolsEditorModeToolkit::OnActiveViewportChanged);

	ModeWarningArea = SNew(STextBlock)
		.AutoWrapText(true)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.15f, 0.15f)));
	ModeWarningArea->SetText(FText::GetEmpty());
	ModeWarningArea->SetVisibility(EVisibility::Collapsed);

	ModeHeaderArea = SNew(STextBlock)
		.AutoWrapText(true)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	ModeHeaderArea->SetText(LOCTEXT("SelectToolLabel", "Select a Tool from the Toolbar"));
	ModeHeaderArea->SetJustification(ETextJustify::Center);


	ToolWarningArea = SNew(STextBlock)
		.AutoWrapText(true)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.15f, 0.15f)));
	ToolWarningArea->SetText(FText::GetEmpty());

	// add the various sections to the mode toolbox
	ToolkitWidgetVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(5)
		[
			ModeWarningArea->AsShared()
		];
	ToolkitWidgetVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(5)
		[
			ModeHeaderArea->AsShared()
		];
	ToolkitWidgetVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(5)
		[
			ToolWarningArea->AsShared()
		];
	ToolkitWidgetVBox->AddSlot().HAlign(HAlign_Fill).FillHeight(1.f)
		[
			ModeDetailsView->AsShared()
		];
	ToolkitWidgetVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(5)
		[
			MakeAssetConfigPanel()->AsShared()
		];

	ClearNotification();
	ClearWarning();

	ActiveToolName = FText::GetEmpty();
	ActiveToolMessage = FText::GetEmpty();

	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolNotificationMessage.AddSP(this, &FModelingToolsEditorModeToolkit::PostNotification);
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolWarningMessage.AddSP(this, &FModelingToolsEditorModeToolkit::PostWarning);

	UpdateObjectCreationOptionsFromSettings();

	SAssignNew(ViewportOverlayWidget, SHorizontalBox)

	+SHorizontalBox::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 0.f, 15.f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
		.Padding(8.f)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SImage)
				.Image_Lambda([this] () { return ActiveToolIcon; })
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock)
				.Text(this, &FModelingToolsEditorModeToolkit::GetActiveToolDisplayName)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0, 0.f, 2.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("OverlayAccept", "Accept"))
				.ToolTipText(LOCTEXT("OverlayAcceptTooltip", "Accept/Commit the results of the active Tool [Enter]"))
				.OnClicked_Lambda([this]() { GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(EToolShutdownType::Accept); return FReply::Handled(); })
				.IsEnabled_Lambda([this]() { return GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanAcceptActiveTool(); })
				.Visibility_Lambda([this]() { return GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->ActiveToolHasAccept() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.Text(LOCTEXT("OverlayCancel", "Cancel"))
				.ToolTipText(LOCTEXT("OverlayCancelTooltip", "Cancel the active Tool [Esc]"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]() { GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(EToolShutdownType::Cancel); return FReply::Handled(); })
				.IsEnabled_Lambda([this]() { return GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanCancelActiveTool(); })
				.Visibility_Lambda([this]() { return GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->ActiveToolHasAccept() ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("OverlayComplete", "Complete"))
				.ToolTipText(LOCTEXT("OverlayCompleteTooltip", "Exit the active Tool [Enter]"))
				.OnClicked_Lambda([this]() { GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(EToolShutdownType::Completed); return FReply::Handled(); })
				.IsEnabled_Lambda([this]() { return GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanCompleteActiveTool(); })
				.Visibility_Lambda([this]() { return GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanCompleteActiveTool() ? EVisibility::Visible : EVisibility::Collapsed; })
			]
		]	
	];

}



TSharedPtr<SWidget> FModelingToolsEditorModeToolkit::MakeAssetConfigPanel()
{
	//
	// New Asset Location drop-down
	//

	AssetLocationModes.Add(MakeShared<FString>(TEXT("AutoGen Folder (World-Relative)")));
	AssetLocationModes.Add(MakeShared<FString>(TEXT("AutoGen Folder (Global)")));
	AssetLocationModes.Add(MakeShared<FString>(TEXT("Current Folder")));
	AssetLocationMode = SNew(STextComboBox)
		.OptionsSource(&AssetLocationModes)
		.OnSelectionChanged_Lambda([&](TSharedPtr<FString> String, ESelectInfo::Type) { UpdateAssetLocationMode(String); });
	AssetSaveModes.Add(MakeShared<FString>(TEXT("AutoSave New Assets")));
	AssetSaveModes.Add(MakeShared<FString>(TEXT("Manual Save")));
	AssetSaveModes.Add(MakeShared<FString>(TEXT("Interactive")));
	AssetSaveMode = SNew(STextComboBox)
		.OptionsSource(&AssetSaveModes)
		.OnSelectionChanged_Lambda([&](TSharedPtr<FString> String, ESelectInfo::Type) { UpdateAssetSaveMode(String); });
	
	// initialize combos
	UpdateAssetPanelFromSettings();
	
	// register callback
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	AssetSettingsModifiedHandle = Settings->OnModified.AddLambda([this](UObject*, FProperty*) { OnAssetSettingsModified(); });


	//
	// LOD selection dropdown
	//

	AssetLODModes.Add(MakeShared<FString>(TEXT("Max Available")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("HiRes")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD0")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD1")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD2")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD3")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD4")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD5")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD6")));
	AssetLODModes.Add(MakeShared<FString>(TEXT("LOD7")));
	AssetLODMode = SNew(STextComboBox)
		.OptionsSource(&AssetLODModes)
		.OnSelectionChanged_Lambda([&](TSharedPtr<FString> String, ESelectInfo::Type)
	{
		EMeshLODIdentifier NewSelectedLOD = EMeshLODIdentifier::LOD0;
		if (*String == *AssetLODModes[0])
		{
			NewSelectedLOD = EMeshLODIdentifier::MaxQuality;
		}
		else if (*String == *AssetLODModes[1])
		{
			NewSelectedLOD = EMeshLODIdentifier::HiResSource;
		}
		else
		{
			for (int32 k = 2; k < AssetLODModes.Num(); ++k)
			{
				if (*String == *AssetLODModes[k])
				{
					NewSelectedLOD = (EMeshLODIdentifier)(k - 2);
					break;
				}
			}
		}

		if (FEditorInteractiveToolsFrameworkGlobals::RegisteredStaticMeshTargetFactoryKey >= 0)
		{
			FComponentTargetFactory* Factory = FindComponentTargetFactoryByKey(FEditorInteractiveToolsFrameworkGlobals::RegisteredStaticMeshTargetFactoryKey);
			if (Factory != nullptr)
			{
				FStaticMeshComponentTargetFactory* StaticMeshFactory = static_cast<FStaticMeshComponentTargetFactory*>(Factory);
				StaticMeshFactory->CurrentEditingLOD = NewSelectedLOD;
			}
		}

		TObjectPtr<UToolTargetManager> TargetManager = GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->TargetManager;
		UStaticMeshComponentToolTargetFactory* StaticMeshTargetFactory = TargetManager->FindFirstFactoryByType<UStaticMeshComponentToolTargetFactory>();
		if (StaticMeshTargetFactory)
		{
			StaticMeshTargetFactory->SetActiveEditingLOD(NewSelectedLOD);
		}

	});

	AssetLODModeLabel = SNew(STextBlock)
	.Text(LOCTEXT("ActiveLODLabel", "Editing LOD"))
	.ToolTipText(LOCTEXT("ActiveLODLabelToolTip", "Select the LOD to be used when editing an existing mesh."));

	const TSharedPtr<SVerticalBox> Content = SNew(SVerticalBox)
	+ SVerticalBox::Slot().HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(0, 2, 2, 2).HAlign(HAlign_Right).VAlign(VAlign_Center).AutoWidth()
				[ AssetLODModeLabel->AsShared() ]
			+ SHorizontalBox::Slot().Padding(0).FillWidth(4.0f)
				[ AssetLODMode->AsShared() ]
		];

	if (Settings->InRestrictiveMode())
	{
		NewAssetPath = SNew(SEditableTextBox)
		.Text(this, &FModelingToolsEditorModeToolkit::GetRestrictiveModeAutoGeneratedAssetPathText)
		.OnTextCommitted(this, &FModelingToolsEditorModeToolkit::OnRestrictiveModeAutoGeneratedAssetPathTextCommitted);

		Content->AddSlot()[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(0, 2, 2, 2).HAlign(HAlign_Right).VAlign(VAlign_Center).AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewAssetPathLabel", "New Asset Path"))
				.ToolTipText(LOCTEXT("NewAssetPathToolTip", "Path used for storing newly generated assets within the project folder."))
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Fill).Padding(0).FillWidth(1.0f)
			[
				NewAssetPath->AsShared()
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Right).Padding(0, 0, 0, 0).AutoWidth()
			[
				SNew(SSimpleButton)
				.OnClicked_Lambda( [this]() { SelectNewAssetPath(); return FReply::Handled(); } )
				.Icon(FAppStyle::Get().GetBrush("Icons.FolderOpen"))
			]
		];
	}
	else
	{
		Content->AddSlot()[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(0, 2, 2, 2).HAlign(HAlign_Right).VAlign(VAlign_Center).AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetLocationLabel", "New Asset Location"))
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Fill).Padding(0).FillWidth(1.0f)
			[
				AssetLocationMode->AsShared()
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Right).Padding(0, 0, 0, 0).AutoWidth()
			[
				SNew(SSimpleButton)
				.OnClicked_Lambda( [this]() { OnShowAssetSettings(); return FReply::Handled(); } )
				.Icon(FAppStyle::Get().GetBrush("Icons.Settings"))
			]
		];
	}

	TSharedPtr<SExpandableArea> AssetConfigPanel = SNew(SExpandableArea)
		.HeaderPadding(FMargin(2.0f))
		.Padding(FMargin(2.f))
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.BorderBackgroundColor(FLinearColor(.6, .6, .6, 1.0f))
		.BodyBorderBackgroundColor(FLinearColor::Transparent)
		.AreaTitleFont(FAppStyle::Get().GetFontStyle("EditorModesPanel.CategoryFontStyle"))
		.BodyContent()
		[
			Content->AsShared()
		]
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ModelingSettingsPanelHeader", "Modeling Mode Quick Settings"))
			.Justification(ETextJustify::Center)
			.Font(FAppStyle::Get().GetFontStyle("EditorModesPanel.CategoryFontStyle"))
		];

	return AssetConfigPanel;

}


void FModelingToolsEditorModeToolkit::InitializeAfterModeSetup()
{
	if (bFirstInitializeAfterModeSetup)
	{
		// force update of the active asset LOD mode, this is necessary because the update modifies
		// ToolTarget Factories that are only available once ModelingToolsEditorMode has been initialized
		AssetLODMode->SetSelectedItem(AssetLODModes[0]);

		bFirstInitializeAfterModeSetup = false;
	}

}



void FModelingToolsEditorModeToolkit::UpdateActiveToolProperties()
{
	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	if (CurTool == nullptr)
	{
		return;
	}

	// Before actually changing the detail panel, we need to see where the current keyboard focus is, because
	// if it's inside the detail panel, we'll need to reset it to the detail panel as a whole, else we might
	// lose it entirely when that detail panel element gets destroyed (which would make us unable to receive any
	// hotkey presses until the user clicks somewhere).
	TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (FocusedWidget != ModeDetailsView) 
	{
		// Search upward from the currently focused widget
		TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
		while (CurrentWidget.IsValid())
		{
			if (CurrentWidget == ModeDetailsView)
			{
				// Reset focus to the detail panel as a whole to avoid losing it when the inner elements change.
				FSlateApplication::Get().SetKeyboardFocus(ModeDetailsView);
				break;
			}

			CurrentWidget = CurrentWidget->GetParentWidget();
		}
	}
		
	ModeDetailsView->SetObjects(CurTool->GetToolProperties(true));
}

void FModelingToolsEditorModeToolkit::InvalidateCachedDetailPanelState(UObject* ChangedObject)
{
	ModeDetailsView->InvalidateCachedState();
}


void FModelingToolsEditorModeToolkit::PostNotification(const FText& Message)
{
	ClearNotification();

	ActiveToolMessage = Message;

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		ActiveToolMessageHandle = GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PushStatusBarMessage(ModeUILayerPtr->GetStatusBarName(), ActiveToolMessage);
	}
}

void FModelingToolsEditorModeToolkit::ClearNotification()
{
	ActiveToolMessage = FText::GetEmpty();

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PopStatusBarMessage(ModeUILayerPtr->GetStatusBarName(), ActiveToolMessageHandle);
	}
	ActiveToolMessageHandle.Reset();
}


void FModelingToolsEditorModeToolkit::PostWarning(const FText& Message)
{
	ToolWarningArea->SetText(Message);
	ToolWarningArea->SetVisibility(EVisibility::Visible);
}

void FModelingToolsEditorModeToolkit::ClearWarning()
{
	ToolWarningArea->SetText(FText());
	ToolWarningArea->SetVisibility(EVisibility::Collapsed);
}



FName FModelingToolsEditorModeToolkit::GetToolkitFName() const
{
	return FName("ModelingToolsEditorMode");
}

FText FModelingToolsEditorModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("ModelingToolsEditorModeToolkit", "DisplayName", "ModelingToolsEditorMode Tool");
}

static const FName PrimitiveTabName(TEXT("Shapes"));
static const FName CreateTabName(TEXT("Create"));
static const FName AttributesTabName(TEXT("Attributes"));
static const FName TriModelingTabName(TEXT("TriModel"));
static const FName PolyModelingTabName(TEXT("PolyModel"));
static const FName MeshProcessingTabName(TEXT("MeshOps"));
static const FName UVTabName(TEXT("UVs"));
static const FName TransformTabName(TEXT("Transform"));
static const FName DeformTabName(TEXT("Deform"));
static const FName VolumesTabName(TEXT("Volumes"));
static const FName PrototypesTabName(TEXT("Prototypes"));
static const FName PolyEditTabName(TEXT("PolyEdit"));
static const FName VoxToolsTabName(TEXT("VoxOps"));
static const FName LODToolsTabName(TEXT("LODs"));
static const FName BakingToolsTabName(TEXT("Baking"));
static const FName ModelingFavoritesTabName(TEXT("Favorites"));

static const FName SelectionModesTabName(TEXT("SelectionModes"));
static const FName SelectionActionsTabName(TEXT("SelectionActions"));


const TArray<FName> FModelingToolsEditorModeToolkit::PaletteNames_Standard = { PrimitiveTabName, CreateTabName, PolyModelingTabName, TriModelingTabName, DeformTabName, TransformTabName, MeshProcessingTabName, VoxToolsTabName, AttributesTabName, UVTabName, BakingToolsTabName, VolumesTabName, LODToolsTabName };


void FModelingToolsEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames = PaletteNames_Standard;

	TArray<FName> ExistingNames;
	for ( FName Name : PaletteNames )
	{
		ExistingNames.Add(Name);
	}

	const UModelingToolsEditorModeSettings* ModelingModeSettings = GetDefault<UModelingToolsEditorModeSettings>();
	bool bEnableSelectionUI = ModelingModeSettings && ModelingModeSettings->bEnablePersistentSelections;
	if (bEnableSelectionUI)
	{
		if (bShowActiveSelectionActions)
		{
			PaletteNames.Insert(SelectionActionsTabName, 0);
			ExistingNames.Add(SelectionActionsTabName);
		}

		PaletteNames.Insert(SelectionModesTabName, 0);
	}

	bool bEnablePrototypes = (CVarEnablePrototypeModelingTools.GetValueOnGameThread() > 0);
	if (bEnablePrototypes)
	{
		PaletteNames.Add(PrototypesTabName);
		ExistingNames.Add(PrototypesTabName);
	}

	bool bEnablePolyModel = (CVarEnablePolyModeling.GetValueOnGameThread() > 0);
	if (bEnablePolyModel)
	{
		PaletteNames.Add(PolyEditTabName);
		ExistingNames.Add(PolyEditTabName);
	}

	if (IModularFeatures::Get().IsModularFeatureAvailable(IModelingModeToolExtension::GetModularFeatureName()))
	{
		TArray<IModelingModeToolExtension*> Extensions = IModularFeatures::Get().GetModularFeatureImplementations<IModelingModeToolExtension>(
			IModelingModeToolExtension::GetModularFeatureName());
		for (int32 k = 0; k < Extensions.Num(); ++k)
		{
			FText ExtensionName = Extensions[k]->GetExtensionName();
			FText SectionName = Extensions[k]->GetToolSectionName();
			FName SectionIndex(SectionName.ToString());
			if (ExistingNames.Contains(SectionIndex))
			{
				UE_LOG(LogTemp, Warning, TEXT("Modeling Mode Extension [%s] uses existing Section Name [%s] - buttons may not be visible"), *ExtensionName.ToString(), *SectionName.ToString());
			}
			else
			{
				PaletteNames.Add(SectionIndex);
				ExistingNames.Add(SectionIndex);
			}
		}
	}

	UModelingToolsModeCustomizationSettings* UISettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>();

	// if user has provided custom ordering of tool palettes in the Editor Settings, try to apply them
	if (UISettings->ToolSectionOrder.Num() > 0)
	{
		TArray<FName> NewPaletteNames;
		for (FString SectionName : UISettings->ToolSectionOrder)
		{
			for (int32 k = 0; k < PaletteNames.Num(); ++k)
			{
				if (SectionName.Equals(PaletteNames[k].ToString(), ESearchCase::IgnoreCase)
				 || SectionName.Equals(GetToolPaletteDisplayName(PaletteNames[k]).ToString(), ESearchCase::IgnoreCase))
				{
					NewPaletteNames.Add(PaletteNames[k]);
					PaletteNames.RemoveAt(k);
					break;
				}
			}
		}
		NewPaletteNames.Append(PaletteNames);
		PaletteNames = MoveTemp(NewPaletteNames);
	}

	// if user has provided a list of favorite tools, add that palette to the list
	if (UISettings->ToolFavorites.Num() > 0)
	{
		PaletteNames.Insert(ModelingFavoritesTabName, 0);
	}

}


FText FModelingToolsEditorModeToolkit::GetToolPaletteDisplayName(FName Palette) const
{ 
	return FText::FromName(Palette);
}

void FModelingToolsEditorModeToolkit::BuildToolPalette(FName PaletteIndex, class FToolBarBuilder& ToolbarBuilder) 
{
	const FModelingToolsManagerCommands& Commands = FModelingToolsManagerCommands::Get();
	UModelingToolsModeCustomizationSettings* UISettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>();

	if (PaletteIndex == ModelingFavoritesTabName)
	{
		// build Favorites tool palette
		for (FString ToolName : UISettings->ToolFavorites)
		{
			bool bFound = false;
			TSharedPtr<FUICommandInfo> FoundToolCommand = Commands.FindToolByName(ToolName, bFound);
			if ( bFound )
			{ 
				ToolbarBuilder.AddToolBarButton(FoundToolCommand);
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("ModelingMode: could not find Favorited Tool %s"), *ToolName);
			}
		}
	}
	else if (PaletteIndex == SelectionModesTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_ToObjectType);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_ToTriangleType);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_ToPolygroupType);

		ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_ToVertexType);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_ToEdgeType);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_ToFaceType);
	}
	else if (PaletteIndex == SelectionActionsTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_Delete);
		//ToolbarBuilder.AddToolBarButton(Commands.BeginSelectionAction_Extrude);
	}
	else if (PaletteIndex == PrimitiveTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddBoxPrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddSpherePrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddCylinderPrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddConePrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddTorusPrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddArrowPrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddRectanglePrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddDiscPrimitiveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddStairsPrimitiveTool);
	}
	else if (PaletteIndex == CreateTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawPolygonTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawPolyPathTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDrawAndRevolveTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginRevolveBoundaryTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCombineMeshesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDuplicateMeshesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPatternTool);
	}
	else if (PaletteIndex == TransformTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginTransformMeshesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAlignObjectsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginEditPivotTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddPivotActorTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginBakeTransformTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginTransferMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginConvertMeshesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSplitMeshesTool);
	}
	else if (PaletteIndex == DeformTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginSculptMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginRemeshSculptMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSmoothMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginOffsetMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshSpaceDeformerTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginLatticeDeformerTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDisplaceMeshTool);
	}
	else if (PaletteIndex == MeshProcessingTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginSimplifyMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginRemeshMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginWeldEdgesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginRemoveOccludedTrianglesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSelfUnionTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginProjectToTargetTool);
	}
	else if (PaletteIndex == LODToolsTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginLODManagerTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGenerateStaticMeshLODAssetTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginISMEditorTool);
	}
	else if (PaletteIndex == VoxToolsTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginVoxelSolidifyTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginVoxelBlendTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginVoxelMorphologyTool);
#if WITH_PROXYLOD
		ToolbarBuilder.AddToolBarButton(Commands.BeginVoxelBooleanTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginVoxelMergeTool);
#endif	// WITH_PROXYLOD
	}
	else if (PaletteIndex == TriModelingTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshSelectionTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginTriEditTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginHoleFillTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMirrorTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPlaneCutTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolygonCutTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshTrimTool);
	}
	else if (PaletteIndex == PolyModelingTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyEditTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyDeformTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCubeGridTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshBooleanTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCutMeshWithMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSubdividePolyTool);
	}
	else if (PaletteIndex == AttributesTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshInspectorTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginEditNormalsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginEditTangentsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginAttributeEditorTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyGroupsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshGroupPaintTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshAttributePaintTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginEditMeshMaterialsTool);
	} 
	else if (PaletteIndex == BakingToolsTabName )
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginBakeMeshAttributeMapsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginBakeMultiMeshAttributeMapsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginBakeMeshAttributeVertexTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginBakeRenderCaptureTool);
	}
	else if (PaletteIndex == UVTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginGlobalUVGenerateTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGroupUVGenerateTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginUVProjectionTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginUVSeamEditTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginTransformUVIslandsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginUVLayoutTool);
		// Handle the inclusion of the optional UVEditor button if the UVEditor plugin has been found
		IGeometryProcessingInterfacesModule& GeomProcInterfaces = FModuleManager::Get().LoadModuleChecked<IGeometryProcessingInterfacesModule>("GeometryProcessingInterfaces");
		IGeometryProcessing_UVEditorAssetEditor* UVEditorAPI = GeomProcInterfaces.GetUVEditorAssetEditorImplementation();
		if (UVEditorAPI)
		{
			ToolbarBuilder.AddToolBarButton(Commands.LaunchUVEditor);
		}
	}
	else if (PaletteIndex == VolumesTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginVolumeToMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshToVolumeTool);
		ToolbarBuilder.AddSeparator();
		ToolbarBuilder.AddToolBarButton(Commands.BeginBspConversionTool);
		ToolbarBuilder.AddSeparator();
		ToolbarBuilder.AddToolBarButton(Commands.BeginPhysicsInspectorTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSetCollisionGeometryTool);
		//ToolbarBuilder.AddToolBarButton(Commands.BeginEditCollisionGeometryTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginExtractCollisionGeometryTool);
	}
	else if (PaletteIndex == PrototypesTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginAddPatchTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginShapeSprayTool);
	}
	else if (PaletteIndex == PolyEditTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_FaceSelect);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_EdgeSelect);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_VertexSelect);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_AllSelect);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_LoopSelect);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_RingSelect);
		ToolbarBuilder.AddSeparator();
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_Extrude);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_Inset);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_Outset);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyModelTool_CutFaces);
		ToolbarBuilder.AddSeparator();
		ToolbarBuilder.AddToolBarButton(Commands.BeginSubdividePolyTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyEditTool);
	}
	else
	{
		TArray<IModelingModeToolExtension*> Extensions = IModularFeatures::Get().GetModularFeatureImplementations<IModelingModeToolExtension>(
			IModelingModeToolExtension::GetModularFeatureName());
		for (int32 k = 0; k < Extensions.Num(); ++k)
		{
			FText SectionName = Extensions[k]->GetToolSectionName();
			FName SectionIndex(SectionName.ToString());
			if (PaletteIndex == SectionIndex)
			{
				FExtensionToolQueryInfo ExtensionQueryInfo;
				ExtensionQueryInfo.bIsInfoQueryOnly = true;
				TArray<FExtensionToolDescription> ToolSet;
				Extensions[k]->GetExtensionTools(ExtensionQueryInfo, ToolSet);
				for (const FExtensionToolDescription& ToolInfo : ToolSet)
				{
					ToolbarBuilder.AddToolBarButton(ToolInfo.ToolCommand);
				}
			}
		}
	}
}


void FModelingToolsEditorModeToolkit::InvokeUI()
{
	FModeToolkit::InvokeUI();

	// FModeToolkit::UpdatePrimaryModePanel() wrapped our GetInlineContent() output in a SScrollBar widget,
	// however this doesn't make sense as we want to dock panels to the "top" and "bottom" of our mode panel area,
	// and the details panel in the middle has it's own scrollbar already. The SScrollBar is hardcoded as the content
	// of FModeToolkit::InlineContentHolder so we can just replace it here
	InlineContentHolder->SetContent(GetInlineContent().ToSharedRef());

	//
	// Apply custom section header colors.
	// See comments below, this is done via directly manipulating Slate widgets generated deep inside BaseToolkit.cpp,
	// and will stop working if the Slate widget structure changes
	//

	UModelingToolsModeCustomizationSettings* UISettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>();

	// look up default radii for palette toolbar expandable area headers
	FVector4 HeaderRadii(4, 4, 0, 0);
	const FSlateBrush* BaseBrush = FAppStyle::Get().GetBrush("PaletteToolbar.ExpandableAreaHeader");
	if (BaseBrush != nullptr)
	{
		HeaderRadii = BaseBrush->OutlineSettings.CornerRadii;
	}

	// Generate a map for tool specific colors
	TMap<FString, FLinearColor> SectionIconColorMap;
	TMap<FString, TMap<FString, FLinearColor>> SectionToolIconColorMap;
	for (const FModelingModeCustomToolColor& ToolColor : UISettings->ToolColors)
	{
		FString SectionName, ToolName;
		ToolColor.ToolName.Split(".", &SectionName, &ToolName);
		SectionName.ToLowerInline();
		if (ToolName.Len() > 0)
		{
			if (!SectionToolIconColorMap.Contains(SectionName))
			{
				SectionToolIconColorMap.Emplace(SectionName, TMap<FString, FLinearColor>());
			}
			SectionToolIconColorMap[SectionName].Add(ToolName, ToolColor.Color);
		}
		else
		{
			SectionIconColorMap.Emplace(ToolColor.ToolName.ToLower(), ToolColor.Color);
		}
	}

	for (FEdModeToolbarRow& ToolbarRow : ActiveToolBarRows)
	{
		// Update section header colors
		for (FModelingModeCustomSectionColor ToolColor : UISettings->SectionColors)
		{
			if (ToolColor.SectionName.Equals(ToolbarRow.PaletteName.ToString(), ESearchCase::IgnoreCase)
			 || ToolColor.SectionName.Equals(ToolbarRow.DisplayName.ToString(), ESearchCase::IgnoreCase))
			{
				// code below is highly dependent on the structure of the ToolbarRow.ToolbarWidget. Currently this is 
				// a SMultiBoxWidget, a few levels below a SExpandableArea. The SExpandableArea contains a SVerticalBox
				// with the header as a SBorder in Slot 0. The code will fail gracefully if this structure changes.

				TSharedPtr<SWidget> ExpanderVBoxWidget = (ToolbarRow.ToolbarWidget.IsValid() && ToolbarRow.ToolbarWidget->GetParentWidget().IsValid()) ?
					ToolbarRow.ToolbarWidget->GetParentWidget()->GetParentWidget() : TSharedPtr<SWidget>();
				if (ExpanderVBoxWidget.IsValid() && ExpanderVBoxWidget->GetTypeAsString().Compare(TEXT("SVerticalBox")) == 0)
				{
					TSharedPtr<SVerticalBox> ExpanderVBox = StaticCastSharedPtr<SVerticalBox>(ExpanderVBoxWidget);
					if (ExpanderVBox.IsValid() && ExpanderVBox->NumSlots() > 0)
					{
						const TSharedRef<SWidget>& SlotWidgetRef = ExpanderVBox->GetSlot(0).GetWidget();
						TSharedPtr<SWidget> SlotWidgetPtr(SlotWidgetRef);
						if (SlotWidgetPtr.IsValid() && SlotWidgetPtr->GetTypeAsString().Compare(TEXT("SBorder")) == 0)
						{
							TSharedPtr<SBorder> TopBorder = StaticCastSharedPtr<SBorder>(SlotWidgetPtr);
							if (TopBorder.IsValid())
							{
								TopBorder->SetBorderImage(new FSlateRoundedBoxBrush(FSlateColor(ToolColor.Color), HeaderRadii));
							}
						}
					}
				}
				break;
			}
		}

		// Update tool colors
		FLinearColor* SectionIconColor = SectionIconColorMap.Find(ToolbarRow.PaletteName.ToString().ToLower());
		if (!SectionIconColor)
		{
			SectionIconColor = SectionIconColorMap.Find(ToolbarRow.DisplayName.ToString().ToLower());
		}
		TMap<FString, FLinearColor>* SectionToolIconColors = SectionToolIconColorMap.Find(ToolbarRow.PaletteName.ToString().ToLower());
		if (!SectionToolIconColors)
		{
			SectionToolIconColors = SectionToolIconColorMap.Find(ToolbarRow.DisplayName.ToString().ToLower());
		}
		if (SectionIconColor || SectionToolIconColors)
		{
			// code below is highly dependent on the structure of the ToolbarRow.ToolbarWidget. Currently this is 
			// a SMultiBoxWidget. The code will fail gracefully if this structure changes.
			
			if (ToolbarRow.ToolbarWidget.IsValid() && ToolbarRow.ToolbarWidget->GetTypeAsString().Compare(TEXT("SMultiBoxWidget")) == 0)
			{
				auto FindFirstChildWidget = [](const TSharedPtr<SWidget>& Widget, const FString& WidgetType)
				{
					TSharedPtr<SWidget> Result;
					UE::ModelingUI::ProcessChildWidgetsByType(Widget->AsShared(), WidgetType,[&Result](TSharedRef<SWidget> Widget)
					{
						Result = TSharedPtr<SWidget>(Widget);
						// Stop processing after first occurrence
						return false;
					});
					return Result;
				};

				TSharedPtr<SWidget> PanelWidget = FindFirstChildWidget(ToolbarRow.ToolbarWidget, TEXT("SUniformWrapPanel"));
				if (PanelWidget.IsValid())
				{
					// This contains each of the FToolBarButtonBlock items for this row.
					FChildren* PanelChildren = PanelWidget->GetChildren();
					const int32 NumChild = PanelChildren ? PanelChildren->NumSlot() : 0;
					for (int32 ChildIdx = 0; ChildIdx < NumChild; ++ChildIdx)
					{
						const TSharedRef<SWidget> ChildWidgetRef = PanelChildren->GetChildAt(ChildIdx);
						TSharedPtr<SWidget> ChildWidgetPtr(ChildWidgetRef);
						if (ChildWidgetPtr.IsValid() && ChildWidgetPtr->GetTypeAsString().Compare(TEXT("SToolBarButtonBlock")) == 0)
						{
							TSharedPtr<SToolBarButtonBlock> ToolBarButton = StaticCastSharedPtr<SToolBarButtonBlock>(ChildWidgetPtr);
							if (ToolBarButton.IsValid())
							{
								TSharedPtr<SWidget> LayeredImageWidget = FindFirstChildWidget(ToolBarButton, TEXT("SLayeredImage"));
								TSharedPtr<SWidget> TextBlockWidget = FindFirstChildWidget(ToolBarButton, TEXT("STextBlock"));
								if (LayeredImageWidget.IsValid() && TextBlockWidget.IsValid())
								{
									TSharedPtr<SImage> ImageWidget = StaticCastSharedPtr<SImage>(LayeredImageWidget);
									TSharedPtr<STextBlock> TextWidget = StaticCastSharedPtr<STextBlock>(TextBlockWidget);
									// Check if this Section.Tool has an explicit color entry. If not, fallback
									// to any Section-wide color entry, otherwise leave the tint alone.
									FLinearColor* TintColor = SectionToolIconColors ? SectionToolIconColors->Find(TextWidget->GetText().ToString()) : nullptr;
									if (!TintColor)
									{
										const FString* SourceText = FTextInspector::GetSourceString(TextWidget->GetText());
										TintColor = SectionToolIconColors && SourceText ? SectionToolIconColors->Find(*SourceText) : nullptr;
										if (!TintColor)
										{
											TintColor = SectionIconColor;
										}
									}
									if (TintColor)
									{
										ImageWidget->SetColorAndOpacity(FSlateColor(*TintColor));
									}
								}
							}
						}
					}
				}
			}
		}
	}
}



void FModelingToolsEditorModeToolkit::ForceToolPaletteRebuild()
{

	UGeometrySelectionManager* SelectionManager = Cast<UModelingToolsEditorMode>(GetScriptableEditorMode())->GetSelectionManager();
	if (SelectionManager)
	{
		bool bHasActiveSelection = SelectionManager->HasSelection();

		if (bShowActiveSelectionActions != bHasActiveSelection)
		{
			bShowActiveSelectionActions = bHasActiveSelection;
			this->RebuildModeToolPalette();
		}
	}
	else
	{
		bShowActiveSelectionActions = false;
	}
}


void FModelingToolsEditorModeToolkit::OnToolPaletteChanged(FName PaletteName) 
{
}



void FModelingToolsEditorModeToolkit::ShowRealtimeAndModeWarnings(bool bShowRealtimeWarning)
{
	FText WarningText{};
	if (GEditor->bIsSimulatingInEditor)
	{
		WarningText = LOCTEXT("ModelingModeToolkitSimulatingWarning", "Cannot use Modeling Tools while simulating.");
	}
	else if (GEditor->PlayWorld != NULL)
	{
		WarningText = LOCTEXT("ModelingModeToolkitPIEWarning", "Cannot use Modeling Tools in PIE.");
	}
	else if (bShowRealtimeWarning)
	{
		WarningText = LOCTEXT("ModelingModeToolkitRealtimeWarning", "Realtime Mode is required for Modeling Tools to work correctly. Please enable Realtime Mode in the Viewport Options or with the Ctrl+r hotkey.");
	}
	if (!WarningText.IdenticalTo(ActiveWarning))
	{
		ActiveWarning = WarningText;
		ModeWarningArea->SetVisibility(ActiveWarning.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
		ModeWarningArea->SetText(ActiveWarning);
	}
}

void FModelingToolsEditorModeToolkit::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	UpdateActiveToolProperties();

	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	CurTool->OnPropertySetsModified.AddSP(this, &FModelingToolsEditorModeToolkit::UpdateActiveToolProperties);
	CurTool->OnPropertyModifiedDirectlyByTool.AddSP(this, &FModelingToolsEditorModeToolkit::InvalidateCachedDetailPanelState);

	ModeHeaderArea->SetVisibility(EVisibility::Collapsed);
	ActiveToolName = CurTool->GetToolInfo().ToolDisplayName;

	// try to update icon
	FString ActiveToolIdentifier = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveToolName(EToolSide::Left);
	ActiveToolIdentifier.InsertAt(0, ".");
	FName ActiveToolIconName = ISlateStyle::Join(FModelingToolsManagerCommands::Get().GetContextName(), TCHAR_TO_ANSI(*ActiveToolIdentifier));
	ActiveToolIcon = FModelingToolsEditorModeStyle::Get()->GetOptionalBrush(ActiveToolIconName);


	GetToolkitHost()->AddViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef());

	// disable LOD level picker once Tool is active
	AssetLODMode->SetEnabled(false);
	AssetLODModeLabel->SetEnabled(false);

	// Invalidate all the level viewports so that e.g. hitproxy buffers are cleared
	// (fixes the editor gizmo still being clickable despite not being visible)
	if (GIsEditor)
	{
		for (FLevelEditorViewportClient* Viewport : GEditor->GetLevelViewportClients())
		{
			Viewport->Invalidate();
		}
	}
}

void FModelingToolsEditorModeToolkit::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	if (IsHosted())
	{
		GetToolkitHost()->RemoveViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef());
	}

	ModeDetailsView->SetObject(nullptr);
	ActiveToolName = FText::GetEmpty();
	ModeHeaderArea->SetVisibility(EVisibility::Visible);
	ModeHeaderArea->SetText(LOCTEXT("SelectToolLabel", "Select a Tool from the Toolbar"));
	ClearNotification();
	ClearWarning();
	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	if ( CurTool )
	{
		CurTool->OnPropertySetsModified.RemoveAll(this);
		CurTool->OnPropertyModifiedDirectlyByTool.RemoveAll(this);
	}

	// re-enable LOD level picker
	AssetLODMode->SetEnabled(true);
	AssetLODModeLabel->SetEnabled(true);
}

void FModelingToolsEditorModeToolkit::OnActiveViewportChanged(TSharedPtr<IAssetViewport> OldViewport, TSharedPtr<IAssetViewport> NewViewport)
{
	// Only worry about handling this notification if Modeling has an active tool
	if (!ActiveToolName.IsEmpty())
	{
		// Check first to see if this changed because the old viewport was deleted and if not, remove our hud
		if (OldViewport)	
		{
			GetToolkitHost()->RemoveViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef(), OldViewport);
		}

		// Add the hud to the new viewport
		GetToolkitHost()->AddViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef(), NewViewport);
	}
}




void FModelingToolsEditorModeToolkit::UpdateAssetLocationMode(TSharedPtr<FString> NewString)
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	EModelingModeAssetGenerationLocation AssetGenerationLocation = EModelingModeAssetGenerationLocation::AutoGeneratedWorldRelativeAssetPath;
	if (NewString == AssetLocationModes[0])
	{
		AssetGenerationLocation = EModelingModeAssetGenerationLocation::AutoGeneratedWorldRelativeAssetPath;
	}
	else if (NewString == AssetLocationModes[1])
	{
		AssetGenerationLocation = EModelingModeAssetGenerationLocation::AutoGeneratedGlobalAssetPath;
	}
	else if (NewString == AssetLocationModes[2])
	{
		AssetGenerationLocation = EModelingModeAssetGenerationLocation::CurrentAssetBrowserPathIfAvailable;
	}

	Settings->SetAssetGenerationLocation(AssetGenerationLocation);
	
	Settings->SaveConfig();
}

void FModelingToolsEditorModeToolkit::UpdateAssetSaveMode(TSharedPtr<FString> NewString)
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	EModelingModeAssetGenerationBehavior AssetGenerationBehavior = EModelingModeAssetGenerationBehavior::AutoGenerateButDoNotAutosave;
	if (NewString == AssetSaveModes[0])
	{
		AssetGenerationBehavior = EModelingModeAssetGenerationBehavior::AutoGenerateAndAutosave;
	}
	else if (NewString == AssetSaveModes[1])
	{
		AssetGenerationBehavior = EModelingModeAssetGenerationBehavior::AutoGenerateButDoNotAutosave;
	}
	else if (NewString == AssetSaveModes[2])
	{
		AssetGenerationBehavior = EModelingModeAssetGenerationBehavior::InteractivePromptToSave;
	}

	Settings->SetAssetGenerationMode(AssetGenerationBehavior);

	Settings->SaveConfig();
}

void FModelingToolsEditorModeToolkit::UpdateAssetPanelFromSettings()
{
	const UModelingToolsEditorModeSettings* Settings = GetDefault<UModelingToolsEditorModeSettings>();

	switch (Settings->GetAssetGenerationLocation())
	{
	case EModelingModeAssetGenerationLocation::CurrentAssetBrowserPathIfAvailable:
		AssetLocationMode->SetSelectedItem(AssetLocationModes[2]);
		break;
	case EModelingModeAssetGenerationLocation::AutoGeneratedGlobalAssetPath:
		AssetLocationMode->SetSelectedItem(AssetLocationModes[1]);
		break;
	case EModelingModeAssetGenerationLocation::AutoGeneratedWorldRelativeAssetPath:
	default:
		AssetLocationMode->SetSelectedItem(AssetLocationModes[0]);
		break;
	}

	AssetLocationMode->SetEnabled(!Settings->InRestrictiveMode());

	switch (Settings->GetAssetGenerationMode())
	{
	case EModelingModeAssetGenerationBehavior::AutoGenerateButDoNotAutosave:
		AssetSaveMode->SetSelectedItem(AssetSaveModes[1]);
		break;
	case EModelingModeAssetGenerationBehavior::InteractivePromptToSave:
		AssetSaveMode->SetSelectedItem(AssetSaveModes[2]);
		break;
	default:
		AssetSaveMode->SetSelectedItem(AssetSaveModes[0]);
		break;
	}
}


void FModelingToolsEditorModeToolkit::UpdateObjectCreationOptionsFromSettings()
{
	// update DynamicMeshActor Settings
	const UModelingToolsEditorModeSettings* Settings = GetDefault<UModelingToolsEditorModeSettings>();

	// enable/disable dynamic mesh actors
	UCreateMeshObjectTypeProperties::bEnableDynamicMeshActorSupport = true;

	// set configured default type
	if (Settings->DefaultMeshObjectType == EModelingModeDefaultMeshObjectType::DynamicMeshActor)
	{
		UCreateMeshObjectTypeProperties::DefaultObjectTypeIdentifier = UCreateMeshObjectTypeProperties::DynamicMeshActorIdentifier;
	}
	else if (Settings->DefaultMeshObjectType == EModelingModeDefaultMeshObjectType::VolumeActor)
	{
		UCreateMeshObjectTypeProperties::DefaultObjectTypeIdentifier = UCreateMeshObjectTypeProperties::VolumeIdentifier;
	}
	else
	{
		UCreateMeshObjectTypeProperties::DefaultObjectTypeIdentifier = UCreateMeshObjectTypeProperties::StaticMeshIdentifier;
	}
}

void FModelingToolsEditorModeToolkit::OnAssetSettingsModified()
{
	UpdateObjectCreationOptionsFromSettings();
	UpdateAssetPanelFromSettings();
}

void FModelingToolsEditorModeToolkit::OnShowAssetSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", "ModelingMode");
	}
}

void FModelingToolsEditorModeToolkit::SelectNewAssetPath() const
{	
	const FString PackageFolderPath = UE::Modeling::GetGlobalAssetRootPath();
	
	const TSharedPtr<SDlgPickPath> PickPathWidget =
		SNew(SDlgPickPath)
		.Title(FText::FromString("Choose New Asset Path"))
		.DefaultPath(FText::FromString(PackageFolderPath + GetRestrictiveModeAutoGeneratedAssetPathText().ToString()))
		.AllowReadOnlyFolders(false);

	if (PickPathWidget->ShowModal() == EAppReturnType::Ok)
	{
		FString Path = PickPathWidget->GetPath().ToString();
		Path.RemoveFromStart(PackageFolderPath);	// Remove project folder prefix
		
		UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
		Settings->SetRestrictiveModeAutoGeneratedAssetPath(Path);
	}
}


FText FModelingToolsEditorModeToolkit::GetRestrictiveModeAutoGeneratedAssetPathText() const
{
	const UModelingToolsEditorModeSettings* Settings = GetDefault<UModelingToolsEditorModeSettings>();
	return FText::FromString(Settings->GetRestrictiveModeAutoGeneratedAssetPath());
}

void FModelingToolsEditorModeToolkit::OnRestrictiveModeAutoGeneratedAssetPathTextCommitted(const FText& InNewText, ETextCommit::Type InTextCommit) const
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	Settings->SetRestrictiveModeAutoGeneratedAssetPath(InNewText.ToString());
}


#undef LOCTEXT_NAMESPACE
