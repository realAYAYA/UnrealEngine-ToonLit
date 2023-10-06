// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptableToolsEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "ScriptableToolsEditorMode.h"
#include "ScriptableToolsEditorModeManagerCommands.h"
#include "ScriptableToolsEditorModeStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "Interfaces/IPluginManager.h"

#include "InteractiveToolManager.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "SPrimaryButton.h"

#include "ScriptableInteractiveTool.h"
#include "ScriptableToolSet.h"

#define LOCTEXT_NAMESPACE "FScriptableToolsEditorModeToolkit"



FScriptableToolsEditorModeToolkit::FScriptableToolsEditorModeToolkit()
{
}


FScriptableToolsEditorModeToolkit::~FScriptableToolsEditorModeToolkit()
{
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolNotificationMessage.RemoveAll(this);
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolWarningMessage.RemoveAll(this);
}


void FScriptableToolsEditorModeToolkit::CustomizeModeDetailsViewArgs(FDetailsViewArgs& ArgsInOut)
{
	//ArgsInOut.ColumnWidth = 0.3f;
}

void FScriptableToolsEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
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

	GetToolkitHost()->OnActiveViewportChanged().AddSP(this, &FScriptableToolsEditorModeToolkit::OnActiveViewportChanged);

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

	ClearNotification();
	ClearWarning();

	ActiveToolName = FText::GetEmpty();
	ActiveToolMessage = FText::GetEmpty();

	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolNotificationMessage.AddSP(this, &FScriptableToolsEditorModeToolkit::PostNotification);
	GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->OnToolWarningMessage.AddSP(this, &FScriptableToolsEditorModeToolkit::PostWarning);

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
				.Text(this, &FScriptableToolsEditorModeToolkit::GetActiveToolDisplayName)
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
				.Visibility_Lambda([this]() { return GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->ActiveToolHasAccept() ? EVisibility::Collapsed : EVisibility::Visible; })
			]
		]	
	];

}





void FScriptableToolsEditorModeToolkit::InitializeAfterModeSetup()
{
	if (bFirstInitializeAfterModeSetup)
	{
		bFirstInitializeAfterModeSetup = false;
	}

	UpdateActiveToolCategories();
}



void FScriptableToolsEditorModeToolkit::UpdateActiveToolCategories()
{
	// build tool category list
	ActiveToolCategories.Reset();
	UScriptableToolsEditorMode* EditorMode = Cast<UScriptableToolsEditorMode>(GetScriptableEditorMode());
	UScriptableToolSet* ScriptableTools = EditorMode->GetActiveScriptableTools();
	ScriptableTools->ForEachScriptableTool( [&](UClass* ToolClass, UBaseScriptableToolBuilder* ToolBuilder) 
	{
		UScriptableInteractiveTool* ToolCDO = Cast<UScriptableInteractiveTool>(ToolClass->GetDefaultObject());
		if (ToolCDO->ToolCategory.IsEmpty() == false)
		{
			FName CategoryName(ToolCDO->ToolCategory.ToString());
			if (ActiveToolCategories.Contains(CategoryName) == false)
			{
				ActiveToolCategories.Add(CategoryName, ToolCDO->ToolCategory);
			}
		}
	});
}



void FScriptableToolsEditorModeToolkit::UpdateActiveToolProperties()
{
	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	if (CurTool != nullptr)
	{
		ModeDetailsView->SetObjects(CurTool->GetToolProperties(true));
	}
}

void FScriptableToolsEditorModeToolkit::InvalidateCachedDetailPanelState(UObject* ChangedObject)
{
	ModeDetailsView->InvalidateCachedState();
}


void FScriptableToolsEditorModeToolkit::PostNotification(const FText& Message)
{
	ClearNotification();

	ActiveToolMessage = Message;

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		ActiveToolMessageHandle = GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PushStatusBarMessage(ModeUILayerPtr->GetStatusBarName(), ActiveToolMessage);
	}
}

void FScriptableToolsEditorModeToolkit::ClearNotification()
{
	ActiveToolMessage = FText::GetEmpty();

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->PopStatusBarMessage(ModeUILayerPtr->GetStatusBarName(), ActiveToolMessageHandle);
	}
	ActiveToolMessageHandle.Reset();
}


void FScriptableToolsEditorModeToolkit::PostWarning(const FText& Message)
{
	ToolWarningArea->SetText(Message);
	ToolWarningArea->SetVisibility(EVisibility::Visible);
}

void FScriptableToolsEditorModeToolkit::ClearWarning()
{
	ToolWarningArea->SetText(FText());
	ToolWarningArea->SetVisibility(EVisibility::Collapsed);
}



FName FScriptableToolsEditorModeToolkit::GetToolkitFName() const
{
	return FName("ScriptableToolsEditorMode");
}

FText FScriptableToolsEditorModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("ScriptableToolsEditorModeToolkit", "DisplayName", "ScriptableToolsEditorMode Tool");
}

static const FName CustomToolsTabName(TEXT("CustomTools"));

void FScriptableToolsEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Reset();
	bool bFoundUncategorized = false;

	UScriptableToolsEditorMode* EditorMode = Cast<UScriptableToolsEditorMode>(GetScriptableEditorMode());
	UInteractiveToolManager* ToolManager = EditorMode->GetToolManager();
	UScriptableToolSet* ScriptableTools = EditorMode->GetActiveScriptableTools();
	ScriptableTools->ForEachScriptableTool( [&](UClass* ToolClass, UBaseScriptableToolBuilder* ToolBuilder) 
	{
		UScriptableInteractiveTool* ToolCDO = Cast<UScriptableInteractiveTool>(ToolClass->GetDefaultObject());
		if (ToolCDO->bShowToolInEditor == false)
		{
			return;
		}

		if (ToolCDO->ToolCategory.IsEmpty() == false)
		{
			FName CategoryName(ToolCDO->ToolCategory.ToString());
			if (ActiveToolCategories.Contains(CategoryName))
			{
				PaletteNames.AddUnique(CategoryName);
			}
			else
			{
				bFoundUncategorized = true;
			}
		}
		else
		{
			bFoundUncategorized = true;
		}
	});

	if ( bFoundUncategorized )
	{
		PaletteNames.Add(CustomToolsTabName);
	}
}


FText FScriptableToolsEditorModeToolkit::GetToolPaletteDisplayName(FName Palette) const
{ 
	if (ActiveToolCategories.Contains(Palette))
	{
		return ActiveToolCategories[Palette];
	}
	return FText::FromName(Palette);
}

void FScriptableToolsEditorModeToolkit::BuildToolPalette(FName PaletteIndex, class FToolBarBuilder& ToolbarBuilder) 
{
	const FScriptableToolsEditorModeManagerCommands& Commands = FScriptableToolsEditorModeManagerCommands::Get();

	static TArray<TSharedPtr<FUIAction>> ActionsHack;

	bool bIsUncategorizedPalette = (PaletteIndex == CustomToolsTabName);

	ActionsHack.Reset();
	
	// this is kind of dumb and we probably should maintain a map <FName, TArray<ToolClass>> when we build the ActiveToolCategories...
	UScriptableToolsEditorMode* EditorMode = Cast<UScriptableToolsEditorMode>(GetScriptableEditorMode());
	UInteractiveToolManager* ToolManager = EditorMode->GetToolManager();
	UScriptableToolSet* ScriptableTools = EditorMode->GetActiveScriptableTools();
	ScriptableTools->ForEachScriptableTool([&](UClass* ToolClass, UBaseScriptableToolBuilder* ToolBuilder)
	{
		UScriptableInteractiveTool* ToolCDO = Cast<UScriptableInteractiveTool>(ToolClass->GetDefaultObject());
		if (ToolCDO->bShowToolInEditor == false)
		{
			return;
		}

		FName UseCategoryName = ToolCDO->ToolCategory.IsEmpty() ? CustomToolsTabName : FName(ToolCDO->ToolCategory.ToString());
		if (ActiveToolCategories.Contains(UseCategoryName) == false)
		{
			UseCategoryName = CustomToolsTabName;
		}
		if (UseCategoryName != PaletteIndex)
		{
			return;
		}

		FString ToolIdentifier = ToolClass->GetName();

		TSharedPtr<FUIAction> NewAction = MakeShared<FUIAction>(
		FExecuteAction::CreateLambda([this, ToolClass, ToolIdentifier, ToolManager]()
		{
			UScriptableInteractiveTool* ToolCDO = Cast<UScriptableInteractiveTool>(ToolClass->GetDefaultObject());
			//UE_LOG(LogTemp, Warning, TEXT("STARTING TOOL [%s] (Class/Identifier %s)"), *ToolCDO->ToolName.ToString(), *ToolIdentifier);
			if (ToolManager->SelectActiveToolType(EToolSide::Mouse, ToolIdentifier))
			{
				if (ToolManager->CanActivateTool(EToolSide::Mouse, ToolIdentifier)) 
				{
					bool bLaunched = ToolManager->ActivateTool(EToolSide::Mouse);
					//ensure(bLaunched);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("FAILED TO SET ACTIVE TOOL TYPE!"));
			}

		}));

		ActionsHack.Add(NewAction);

		FName InExtensionHook = NAME_None;
		TAttribute<FText> Label = ToolCDO->ToolName.IsEmpty() ? LOCTEXT("EmptyToolName", "Tool") : ToolCDO->ToolName;
		TAttribute<FText> Tooltip = ToolCDO->ToolTooltip.IsEmpty() ? FText() : ToolCDO->ToolTooltip;
		
		// default icon comes with the mode
		TAttribute<FSlateIcon> Icon = FSlateIcon(FScriptableToolsEditorModeStyle::Get()->GetStyleSetName(), "ScriptableToolsEditorModeToolCommands.DefaultToolIcon");

		// if a custom icon is defined, try to find it, this can fail in many ways, in that case
		// the default icon is kept
		if (ToolCDO->CustomIconPath.IsEmpty() == false)
		{
			FName ToolIconToken( FString("ScriptableToolsEditorModeToolCommands.") + ToolIdentifier );

			// Custom Tool Icons are assumed to reside in the same Content folder as the Plugin/Project that
			// the Tool Class is defined in, and that the CustomIconPath is a relative path inside that Content folder.
			// use the class Package path to determine if this it is in a Plugin or directly in the Project, so that
			// we can get the right ContentDir below.
			// (Note that a relative ../../../ style path can always be used to redirect to any other file...)
			FString FullPathName = ToolCDO->GetClass()->GetPathName();
			FString PathPart, FilenamePart, ExtensionPart;
			FPaths::Split(FullPathName, PathPart, FilenamePart, ExtensionPart);

			FString FullIconPath = ToolCDO->CustomIconPath;
			if (PathPart.StartsWith("/Game"))
			{
				FullIconPath = FPaths::ProjectContentDir() / ToolCDO->CustomIconPath;
			}
			else
			{
				TArray<FString> PathParts;
				PathPart.ParseIntoArray(PathParts, TEXT("/"));
				if (PathParts.Num() > 0)
				{
					FString PluginContentDir = IPluginManager::Get().FindPlugin(PathParts[0])->GetContentDir();
					FullIconPath = PluginContentDir / ToolCDO->CustomIconPath;
				}
				else  // something is wrong, fall back to project content dir
				{
					FullIconPath = FPaths::ProjectContentDir() / ToolCDO->CustomIconPath;
				}
			}

			if (FScriptableToolsEditorModeStyle::TryRegisterCustomIcon(ToolIconToken, FullIconPath, ToolCDO->CustomIconPath))
			{
				Icon = FSlateIcon(FScriptableToolsEditorModeStyle::Get()->GetStyleSetName(), ToolIconToken);
			}
		}
		
		ToolbarBuilder.AddToolBarButton(*NewAction, InExtensionHook, Label, Tooltip, Icon);

	});
}

void FScriptableToolsEditorModeToolkit::InvokeUI()
{
	FModeToolkit::InvokeUI();
}



void FScriptableToolsEditorModeToolkit::ForceToolPaletteRebuild()
{
	this->UpdateActiveToolCategories();
	this->RebuildModeToolPalette();
}


void FScriptableToolsEditorModeToolkit::OnToolPaletteChanged(FName PaletteName) 
{
}



void FScriptableToolsEditorModeToolkit::EnableShowRealtimeWarning(bool bEnable)
{
	if (bShowRealtimeWarning != bEnable)
	{
		bShowRealtimeWarning = bEnable;
		UpdateShowWarnings();
	}
}

void FScriptableToolsEditorModeToolkit::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	UpdateActiveToolProperties();

	UInteractiveTool* CurTool = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveTool(EToolSide::Left);
	CurTool->OnPropertySetsModified.AddSP(this, &FScriptableToolsEditorModeToolkit::UpdateActiveToolProperties);
	CurTool->OnPropertyModifiedDirectlyByTool.AddSP(this, &FScriptableToolsEditorModeToolkit::InvalidateCachedDetailPanelState);

	ModeHeaderArea->SetVisibility(EVisibility::Collapsed);

	ActiveToolName = CurTool->GetToolInfo().ToolDisplayName;
	if (UScriptableInteractiveTool* ScriptableTool = Cast<UScriptableInteractiveTool>(CurTool))
	{
		if ( ScriptableTool->ToolLongName.IsEmpty() == false )
		{
			ActiveToolName = ScriptableTool->ToolLongName;
		}
		else if ( ScriptableTool->ToolName.IsEmpty() == false )
		{
			ActiveToolName = ScriptableTool->ToolName;
		}
	}

	// try to update icon
	FString ActiveToolIdentifier = GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveToolName(EToolSide::Left);
	ActiveToolIdentifier.InsertAt(0, ".");
	FName ActiveToolIconName = ISlateStyle::Join(FScriptableToolsEditorModeManagerCommands::Get().GetContextName(), TCHAR_TO_ANSI(*ActiveToolIdentifier));
	ActiveToolIcon = FScriptableToolsEditorModeStyle::Get()->GetOptionalBrush(ActiveToolIconName);

	GetToolkitHost()->AddViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef());
}

void FScriptableToolsEditorModeToolkit::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
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
}

void FScriptableToolsEditorModeToolkit::OnActiveViewportChanged(TSharedPtr<IAssetViewport> OldViewport, TSharedPtr<IAssetViewport> NewViewport)
{
	// Only worry about handling this notification if we have an active tool
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

void FScriptableToolsEditorModeToolkit::UpdateShowWarnings()
{
	if (bShowRealtimeWarning )
	{
		if (ModeWarningArea->GetVisibility() == EVisibility::Collapsed)
		{
			ModeWarningArea->SetText(LOCTEXT("ScriptableToolsModeToolkitRealtimeWarning", "Realtime Mode is required for Scriptable Tools to work correctly. Please enable Realtime Mode in the Viewport Options or with the Ctrl+r hotkey."));
			ModeWarningArea->SetVisibility(EVisibility::Visible);
		}
	}
	else
	{
		ModeWarningArea->SetText(FText());
		ModeWarningArea->SetVisibility(EVisibility::Collapsed);
	}

}



#undef LOCTEXT_NAMESPACE
