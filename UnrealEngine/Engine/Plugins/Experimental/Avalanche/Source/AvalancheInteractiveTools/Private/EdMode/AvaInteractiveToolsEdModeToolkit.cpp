// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaInteractiveToolsEdModeToolkit.h"
#include "AvaInteractiveToolsEdMode.h"
#include "AvalancheInteractiveToolsModule.h"
#include "IAvalancheInteractiveToolsModule.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "Tools/UEdMode.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "AvaInteractiveToolsEdModeToolkit"

FAvaInteractiveToolsEdModeToolkit::FAvaInteractiveToolsEdModeToolkit()
{
	bUsesToolkitBuilder = true;
}

FAvaInteractiveToolsEdModeToolkit::~FAvaInteractiveToolsEdModeToolkit()
{
}

FName FAvaInteractiveToolsEdModeToolkit::GetToolkitFName() const
{
	static const FName ToolkitName("ModelingToolsEditorMode");
	return ToolkitName;
}

FText FAvaInteractiveToolsEdModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "AvaInteractiveToolsEdMode Tool");
}

TSharedPtr<SWidget> FAvaInteractiveToolsEdModeToolkit::GetInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.VAlign(VAlign_Fill)
		[
			ToolkitWidget.ToSharedRef()
		];
}

void FAvaInteractiveToolsEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	RegisterPalettes();

	SAssignNew(ToolkitWidget, SBorder)
		.HAlign(HAlign_Fill)
		.Padding(0)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			ToolkitBuilder->GenerateWidget()->AsShared()
		];
}

void FAvaInteractiveToolsEdModeToolkit::GetToolPaletteNames(TArray<FName>& InPaletteName) const
{
	const TMap<FName, TSharedPtr<FUICommandInfo>>& Categories = IAvalancheInteractiveToolsModule::Get().GetCategories();
	Categories.GetKeys(InPaletteName);
}

FText FAvaInteractiveToolsEdModeToolkit::GetToolPaletteDisplayName(FName InPaletteName) const
{
	const TMap<FName, TSharedPtr<FUICommandInfo>>& Categories = IAvalancheInteractiveToolsModule::Get().GetCategories();

	if (const TSharedPtr<FUICommandInfo>* CommandInfo = Categories.Find(InPaletteName))
	{
		return CommandInfo->Get()->GetLabel();
	}

	return FText::FromName(InPaletteName);
}

void FAvaInteractiveToolsEdModeToolkit::OnToolPaletteChanged(FName InPaletteName)
{
	if (UAvaInteractiveToolsEdMode* AvaInteractiveToolsEdMode = Cast<UAvaInteractiveToolsEdMode>(GetScriptableEditorMode().Get()))
	{
		AvaInteractiveToolsEdMode->OnToolPaletteChanged(InPaletteName);
	}
}

void FAvaInteractiveToolsEdModeToolkit::OnToolStarted(UInteractiveToolManager* InManager, UInteractiveTool* InTool)
{
	// Nothing
}

void FAvaInteractiveToolsEdModeToolkit::OnToolEnded(UInteractiveToolManager* InManager, UInteractiveTool* InTool)
{
	// Nothing
}

void FAvaInteractiveToolsEdModeToolkit::InvokeUI()
{
	FModeToolkit::InvokeUI();

	InlineContentHolder->SetContent(GetInlineContent().ToSharedRef());
}

void FAvaInteractiveToolsEdModeToolkit::RequestModeUITabs()
{
	if (const TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin())
	{
		PrimaryTabInfo.OnSpawnTab = FOnSpawnTab::CreateSP(SharedThis(this), &FAvaInteractiveToolsEdModeToolkit::CreatePrimaryModePanel);
		PrimaryTabInfo.TabLabel = LOCTEXT("MotionDesignToolboxTab", "Motion Design");
		PrimaryTabInfo.TabTooltip = LOCTEXT("MotionDesignToolboxTabTooltipText", "Opens the Motion Design tab.");
		ModeUILayerPtr->SetModePanelInfo(UAssetEditorUISubsystem::TopLeftTabID, PrimaryTabInfo);

		if (!HasIntegratedToolPalettes() && !HasToolkitBuilder())
		{
			ToolbarInfo.OnSpawnTab = FOnSpawnTab::CreateSP(SharedThis(this), &FAvaInteractiveToolsEdModeToolkit::MakeModeToolbarTab);
			ToolbarInfo.TabLabel = LOCTEXT("MotionDesignToolbarTab", "Motion Design Toolbar");
			ToolbarInfo.TabTooltip = LOCTEXT("MotionDesignToolbarTabTooltipText", "Opens the toolbar for the Motion Design toolbox.");
			ModeUILayerPtr->SetModePanelInfo(UAssetEditorUISubsystem::VerticalToolbarID, ToolbarInfo);
		}
	}
}

void FAvaInteractiveToolsEdModeToolkit::RegisterPalettes()
{
	UAvaInteractiveToolsEdMode* AvaInteractiveToolsMode = Cast<UAvaInteractiveToolsEdMode>(GetScriptableEditorMode().Get());

	ModeDetailsView->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateLambda(
			[](const FPropertyAndParent& InPropertyAndParent) -> bool
			{
				static const FString Material = FString(TEXT("Material"));
				static const FName Category = FName(TEXT("Category"));
				const FString CategoryMeta = InPropertyAndParent.Property.GetMetaData(Category);
				return CategoryMeta != Material;
			}
	));

	ToolkitSections = MakeShared<FToolkitSections>();
	//ToolkitSections->DetailsView = ModeDetailsView;

	ToolkitSections->ToolWarningArea = SNew(STextBlock)
		.Text(this, &FAvaInteractiveToolsEdModeToolkit::GetToolWarningText)
		.AutoWrapText(true);
		
	ToolkitBuilder = MakeShared<FToolkitBuilder>(
		AvaInteractiveToolsMode->GetModeInfo().ToolbarCustomizationName,
		GetToolkitCommands(),
		ToolkitSections);

	const TMap<FName, TSharedPtr<FUICommandInfo>>& Categories = IAvalancheInteractiveToolsModule::Get().GetCategories();
	TSharedPtr<FUICommandInfo> FirstCategoryCommand = nullptr;

	for (const TPair<FName, TSharedPtr<FUICommandInfo>>& Category : Categories)
	{
		if (Category.Value.IsValid())
		{
			TArray<TSharedPtr<FUICommandInfo>> CategoryCommands;
			const TArray<FAvaInteractiveToolsToolParameters>* ToolList = IAvalancheInteractiveToolsModule::Get().GetTools(Category.Key);

			if (ToolList)
			{
				for (const FAvaInteractiveToolsToolParameters& Tool : *ToolList)
				{
					if (Tool.UICommand.IsValid())
					{
						CategoryCommands.Add(Tool.UICommand);
					}
				}
			}

			if (CategoryCommands.IsEmpty() == false)
			{
				ToolkitBuilder->AddPalette(MakeShared<FToolPalette>(Category.Value.ToSharedRef(), CategoryCommands));

				if (FirstCategoryCommand.IsValid() == false)
				{
					FirstCategoryCommand = Category.Value;
				}
			}
		}
	}

	if (FirstCategoryCommand.IsValid())
	{
		ToolkitBuilder->SetActivePaletteOnLoad(FirstCategoryCommand.Get());
	}

	ToolkitBuilder->UpdateWidget();
}

FText FAvaInteractiveToolsEdModeToolkit::GetToolWarningText() const
{
	static const FText ToolActive = LOCTEXT("ActiveToolWarning", "Tool Active.\n\nSelect the tool again to perform the default action (if supported). This will spawn a predefined actor in the middle of the viewport.\n\nRight click or press escape to cancel.");
	static const FText ToolInactive = LOCTEXT("InactiveToolWarning", "Select a tool to start drawing.\n\nShift click will show tool presets, if available.");

	return FAvalancheInteractiveToolsModule::Get().HasActiveTool()
		? ToolActive
		: ToolInactive;
}

#undef LOCTEXT_NAMESPACE
