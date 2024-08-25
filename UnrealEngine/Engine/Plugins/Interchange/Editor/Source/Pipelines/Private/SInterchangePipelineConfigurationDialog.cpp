// Copyright Epic Games, Inc. All Rights Reserved.
#include "SInterchangePipelineConfigurationDialog.h"

#include "InterchangeEditorPipelineDetails.h"

#include "DetailsViewArgs.h"
#include "Dialog/SCustomDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Views/TableViewMetadata.h"
#include "GameFramework/Actor.h"
#include "IDetailsView.h"
#include "IDocumentation.h"
#include "InterchangeManager.h"
#include "InterchangePipelineConfigurationBase.h"
#include "InterchangeProjectSettings.h"
#include "InterchangeTranslatorBase.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PropertyEditorModule.h"
#include "SInterchangeGraphInspectorWindow.h"
#include "SPrimaryButton.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Layout/Visibility.h"

#define LOCTEXT_NAMESPACE "InterchangePipelineConfiguration"

static bool GInterchangeDefaultBasicLayoutView = false;
static FAutoConsoleVariableRef CCvarInterchangeEnableFBXImport(
	TEXT("Interchange.FeatureFlags.Import.DefaultBasicLayoutView"),
	GInterchangeDefaultBasicLayoutView,
	TEXT("Whether the import dialog start by default in basic layout."),
	ECVF_Default);

const FName ReimportStackName = TEXT("ReimportPipeline");
const FString ReimportPipelinePrefix = TEXT("reimport_");

void SInterchangePipelineItem::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& OwnerTable,
	TSharedPtr<FInterchangePipelineItemType> InPipelineElement)
{
	LLM_SCOPE_BYNAME(TEXT("Interchange"));

	PipelineElement = InPipelineElement;
	TObjectPtr<UInterchangePipelineBase> PipelineElementPtr = PipelineElement->Pipeline;
	check(PipelineElementPtr.Get());
	FText PipelineName = LOCTEXT("InvalidPipelineName", "Invalid Pipeline");
	if (PipelineElementPtr.Get())
	{
		FString PipelineNameString = PipelineElement->DisplayName;
		if (!PipelineElement->bBasicLayout)
		{
			PipelineNameString += FString::Printf(TEXT(" (%s)"), *PipelineElementPtr->GetClass()->GetName());
		}
		PipelineName = FText::FromString(PipelineNameString);
	}
	
	static const FSlateBrush* ConflictBrush = FAppStyle::GetBrush("Icons.Error");
	const FText ConflictsComboBoxTooltip = LOCTEXT("ConflictsComboBoxTooltip", "If there is some conflict, simply select one to see more details.");
	const FText Conflict_IconTooltip = FText::Format(LOCTEXT("Conflict_IconTooltip", "There are {0} conflicts. See Conflicts section below for details."), PipelineElement->ConflictInfos.Num());

	STableRow<TSharedPtr<FInterchangePipelineItemType>>::Construct(
		STableRow<TSharedPtr<FInterchangePipelineItemType>>::FArguments()
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 6.0f, 2.0f)
			[
				SNew(SImage)
				.Image(this, &SInterchangePipelineItem::GetImageItemIcon)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 6.0f, 2.0f)
			[
				SNew(SImage)
				.ToolTipText(Conflict_IconTooltip)
				.Image(ConflictBrush)
				.Visibility_Lambda([this]()->EVisibility
					{
						return PipelineElement->ConflictInfos.Num() > 0 ? EVisibility::All : EVisibility::Collapsed;
					})
				.ColorAndOpacity(this, &SInterchangePipelineItem::GetTextColor)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(3.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(PipelineName)
				.ColorAndOpacity(this, &SInterchangePipelineItem::GetTextColor)
			]
		], OwnerTable);
}

const FSlateBrush* SInterchangePipelineItem::GetImageItemIcon() const
{
	const FSlateBrush* TypeIcon = nullptr;
	FName IconName = "PipelineConfigurationIcon.Pipeline";
	const FSlateIcon SlateIcon = FSlateIconFinder::FindIcon(IconName);
	TypeIcon = SlateIcon.GetOptionalIcon();
	if (!TypeIcon)
	{
		TypeIcon = FSlateIconFinder::FindIconBrushForClass(AActor::StaticClass());
	}
	return TypeIcon;
}

FSlateColor SInterchangePipelineItem::GetTextColor() const
{
	if (PipelineElement->ConflictInfos.Num() > 0)
	{
		return FStyleColors::Warning;
	}
	return FSlateColor::UseForeground();
}

/************************************************************************/
/* SInterchangePipelineConfigurationDialog Implementation                      */
/************************************************************************/

namespace UE::Private
{
	bool ContainStack(const TArray<FInterchangeStackInfo>& PipelineStacks, const FName StackName)
	{
		return PipelineStacks.FindByPredicate([StackName](const FInterchangeStackInfo& StackInfo)
			{
				return (StackInfo.StackName == StackName);
			}) != nullptr;
	}
}

SInterchangePipelineConfigurationDialog::SInterchangePipelineConfigurationDialog()
{
	PipelineConfigurationDetailsView = nullptr;
	OwnerWindow = nullptr;
}

SInterchangePipelineConfigurationDialog::~SInterchangePipelineConfigurationDialog()
{
	if (TSharedPtr<SWindow> OwnerWindowPinned = OwnerWindow.Pin())
	{
		OwnerWindowPinned->GetOnWindowClosedEvent().RemoveAll(this);
	}
}

// Pipelines are renamed with the reimport prefix to avoid conflicts with the duplicates of the original pipelines that end up in the same package.
 // As this is the name displayed in the Dialog, conflicts won't matter.
FString SInterchangePipelineConfigurationDialog::GetPipelineDisplayName(const UInterchangePipelineBase* Pipeline)
{
	static int32 RightChopIndex = ReimportPipelinePrefix.Len();

	FString PipelineDisplayName = Pipeline->ScriptedGetPipelineDisplayName();
	if(PipelineDisplayName.IsEmpty())
	{
		PipelineDisplayName = Pipeline->GetName();
	}
	if (PipelineDisplayName.StartsWith(ReimportPipelinePrefix))
	{
		PipelineDisplayName = PipelineDisplayName.RightChop(RightChopIndex);
	}

	FString StackName;
	FString DisplayName;
	if (PipelineDisplayName.Split("_", &StackName, &DisplayName))
	{
		return DisplayName;
	}

	return PipelineDisplayName;
}

void SInterchangePipelineConfigurationDialog::SetEditPipeline(FInterchangePipelineItemType* PipelineItemToEdit)
{
	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(!PipelineItemToEdit ? nullptr : PipelineItemToEdit->Pipeline);

	if (PipelineItemToEdit)
	{
		PipelineItemToEdit->ConflictInfos.Reset();
		if (PipelineItemToEdit->ReimportObject)
		{
			PipelineItemToEdit->ConflictInfos = PipelineItemToEdit->Pipeline->GetConflictInfos(PipelineItemToEdit->ReimportObject, PipelineItemToEdit->Container, PipelineItemToEdit->SourceData);
		}
		FInterchangePipelineBaseDetailsCustomization::SetConflictsInfo(PipelineItemToEdit->ConflictInfos);
	}
	PipelineConfigurationDetailsView->SetObjects(ObjectsToEdit);
}

FReply SInterchangePipelineConfigurationDialog::OnEditTranslatorSettings()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bShowSectionSelector = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> TranslatorSettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	TranslatorSettingsDetailsView->OnFinishedChangingProperties().AddRaw(this, &SInterchangePipelineConfigurationDialog::OnFinishedChangingProperties);
	TranslatorSettingsDetailsView->SetObject(TranslatorSettings);

	TSharedRef<SCustomDialog> OptionsDialog =
		SNew(SCustomDialog)
		.Title(LOCTEXT("OptionsDialogTitle", "Translator Project Settings Editor"))
		.WindowArguments(SWindow::FArguments()
			.IsTopmostWindow(true)
			.MinHeight(300.0f)
			.MinWidth(500)
			.SizingRule(ESizingRule::UserSized))
		.HAlignContent(HAlign_Fill)
		.VAlignContent(VAlign_Fill)
		.HAlignIcon(HAlign_Left)
		.VAlignIcon(VAlign_Top)
		.UseScrollBox(true)
		.Content()
		[
			SNew(SBorder)
			.Padding(FMargin(10.0f, 3.0f))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillHeight(1.0)
				[
					TranslatorSettingsDetailsView
				]
			]
			
		]
		.Buttons(
		{
			SCustomDialog::FButton(LOCTEXT("DialogButtonOk", "Ok"))
		});
	OptionsDialog->ShowModal();

	return FReply::Handled();
}

void SInterchangePipelineConfigurationDialog::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!Translator.IsValid() || !TranslatorSettings)
	{
		return;
	}
	if (UClass* TranslatorSettingsClass = TranslatorSettings->GetClass())
	{
		//Save the config locally before the translation.
		TranslatorSettings->SaveSettings();

		//Need to Translate the source data
		FScopedSlowTask Progress(2.f, NSLOCTEXT("SInterchangePipelineConfigurationDialog", "TranslatingSourceFile...", "Translating source file..."));
		Progress.MakeDialog();
		Progress.EnterProgressFrame(1.f);
		//Reset the container
		BaseNodeContainer->Reset();

		Translator->Translate(*BaseNodeContainer.Get());

		//Refresh the dialog
		RefreshStack(false);

		Progress.EnterProgressFrame(1.f);
	}
}

TSharedRef<SBox> SInterchangePipelineConfigurationDialog::SpawnPipelineConfiguration()
{
	AvailableStacks.Reset();
	TSharedPtr<FString> SelectedStack;
	if (bReimport)
	{
		CurrentStackName = ReimportStackName;
	}
	else
	{
		CurrentStackName = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bSceneImport, *SourceData);
	}

	//In case we do not have a valid stack name use the first stack
	FName FirstStackName = PipelineStacks.Num() > 0 ? PipelineStacks[0].StackName : CurrentStackName;
	if (!UE::Private::ContainStack(PipelineStacks, CurrentStackName))
	{
		CurrentStackName = FirstStackName;
	}

	for(FInterchangeStackInfo& Stack : PipelineStacks)
	{
		TSharedPtr<FString> StackNamePtr = MakeShared<FString>(Stack.StackName.ToString());
		if (CurrentStackName == Stack.StackName)
		{
			for (const TObjectPtr<UInterchangePipelineBase>& DefaultPipeline : Stack.Pipelines)
			{
				check(DefaultPipeline);
				if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstanceInSourceAssetPackage(DefaultPipeline))
				{
					GeneratedPipeline->TransferAdjustSettings(DefaultPipeline);
					if (Stack.StackName == ReimportStackName)
					{
						
						//We save the pipeline settings to allow Reset to Default to work
						GeneratedPipeline->SaveSettings(Stack.StackName);
					}
					else
					{
						//Load the settings for this pipeline
						GeneratedPipeline->LoadSettings(Stack.StackName);
						GeneratedPipeline->PreDialogCleanup(Stack.StackName);
					}
					GeneratedPipeline->SetBasicLayoutMode(bBasicLayout);
					if (bFilterOptions && BaseNodeContainer.IsValid())
					{
						GeneratedPipeline->FilterPropertiesFromTranslatedData(BaseNodeContainer.Get());
					}
					PipelineListViewItems.Add(MakeShareable(new FInterchangePipelineItemType{ GetPipelineDisplayName(DefaultPipeline), GeneratedPipeline, ReimportObject.Get(), BaseNodeContainer.Get(), SourceData.Get(), bBasicLayout }));
				}
			}
			SelectedStack = StackNamePtr;
		}
		AvailableStacks.Add(StackNamePtr);
	}

	FText PipelineListTooltip = LOCTEXT("PipelineListTooltip", "Select a pipeline you want to edit properties for. The pipeline properties will be recorded and changes will be available in subsequent use of that pipeline");
	PipelinesListView = SNew(SPipelineListViewType)
		.SelectionMode(ESelectionMode::Single)
		.ListItemsSource(&PipelineListViewItems)
		.OnGenerateRow(this, &SInterchangePipelineConfigurationDialog::MakePipelineListRowWidget)
		.OnSelectionChanged(this, &SInterchangePipelineConfigurationDialog::OnPipelineSelectionChanged)
		.ToolTipText(PipelineListTooltip);

	TSharedPtr<STextComboBox> TextComboBoxPtr;
	//Only use a combo box if there is more then one stack
	if (AvailableStacks.Num() > 1)
	{
		FText StackComboBoxTooltip = LOCTEXT("StackComboBoxTooltip", "Selected pipeline stack will be used for the current import. To change the pipeline stack used when automating or without dialog please change the default pipeline stacks in the project settings");
		TextComboBoxPtr = SNew(STextComboBox)
			.OptionsSource(&AvailableStacks)
			.OnSelectionChanged(this, &SInterchangePipelineConfigurationDialog::OnStackSelectionChanged)
			.ToolTipText(StackComboBoxTooltip);
		if (SelectedStack.IsValid())
		{
			TextComboBoxPtr->SetSelectedItem(SelectedStack);
		}
	}

	FText CurrentStackText = LOCTEXT("CurrentStackText", "Choose Pipeline Stack: ");

	TSharedPtr<SWidget> StackTextComboBox;
	if (!TextComboBoxPtr.IsValid())
	{
		CurrentStackText = FText::Format(LOCTEXT("CurrentStackTextNoComboBox", "Pipeline Stack: {0}"), FText::FromName(CurrentStackName));
		StackTextComboBox = SNew(SBox)
		[
			SNew(STextBlock)
			.Text(CurrentStackText)
		];
	}
	else
	{

		StackTextComboBox = SNew(SHorizontalBox)
		.Visibility_Lambda([this]()
			{
				return bBasicLayout ? EVisibility::Collapsed : EVisibility::All;
			})
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			[
				SNew(STextBlock)
				.Text(CurrentStackText)
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			TextComboBoxPtr.ToSharedRef()
		];
	}

	TSharedPtr<SBox> InspectorBox;
	TSharedRef<SBox> PipelineConfigurationPanelBox = SNew(SBox)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoHeight()
			[
				StackTextComboBox.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f)
			.AutoHeight()
			[
				SNew(SBox)
				.MinDesiredHeight(50)
				.MaxDesiredHeight(140)
				[
					PipelinesListView.ToSharedRef()
				]
			]
		]
		+ SVerticalBox::Slot()
		.Padding(0.0f, 5.0f, 0.0f, 2.0f)
		.FillHeight(1.0f)
		[
			SAssignNew(InspectorBox, SBox)
		]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bShowSectionSelector = true;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	PipelineConfigurationDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBox->SetContent(PipelineConfigurationDetailsView->AsShared());
	SetEditPipeline(nullptr);
	PipelineConfigurationDetailsView->GetIsPropertyVisibleDelegate().BindLambda([this](const FPropertyAndParent& PropertyAndParent)
		{
			return IsPropertyVisible(PropertyAndParent);
		});
	PipelineConfigurationDetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (CurrentSelectedPipeline && CurrentSelectedPipeline->IsPropertyChangeNeedRefresh(PropertyChangedEvent))
		{
			//Refresh the pipeline
			constexpr bool bStackSelectionChange = false;
			RefreshStack(bStackSelectionChange);
		}
	});
	return PipelineConfigurationPanelBox;
}

void SInterchangePipelineConfigurationDialog::Construct(const FArguments& InArgs)
{
	LLM_SCOPE_BYNAME(TEXT("Interchange"));
	//Make sure there is a valid default value

	OwnerWindow = InArgs._OwnerWindow;
	SourceData = InArgs._SourceData;
	bSceneImport = InArgs._bSceneImport;
	bReimport = InArgs._bReimport;
	PipelineStacks = InArgs._PipelineStacks;
	OutPipelines = InArgs._OutPipelines;
	BaseNodeContainer = InArgs._BaseNodeContainer;
	ReimportObject = InArgs._ReimportObject;
	SourceData = InArgs._SourceData;
	Translator = InArgs._Translator;
	if (Translator.IsValid())
	{
		TranslatorSettings = Translator->GetSettings();
	}

	if (ReimportObject.IsValid())
	{
		ensure(bReimport);
	}
	

	check(OutPipelines);

	check(OwnerWindow.IsValid());
	if (TSharedPtr<SWindow> OwnerWindowPinned = OwnerWindow.Pin())
	{
		OwnerWindowPinned->GetOnWindowClosedEvent().AddRaw(this, &SInterchangePipelineConfigurationDialog::OnWindowClosed);
	}

	//Get the default layout when the user open the import dialog for the first time.
	bBasicLayout = GInterchangeDefaultBasicLayoutView;

	if (bReimport)
	{
		bFilterOptions = false;
	}
	else if(GConfig->DoesSectionExist(TEXT("InterchangeImportDialogOptions"), GEditorPerProjectIni))
	{
		GConfig->GetBool(TEXT("InterchangeImportDialogOptions"), TEXT("FilterOptions"), bFilterOptions, GEditorPerProjectIni);
		GConfig->GetBool(TEXT("InterchangeImportDialogOptions"), TEXT("BasicLayout"), bBasicLayout, GEditorPerProjectIni);
	}

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(10.0f, 3.0f))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 2.0f)
				[
					SNew(SButton)
					.Visibility_Lambda([this]()
						{
							return !TranslatorSettings ? EVisibility::Collapsed : EVisibility::All;
						})
					.ToolTipText(LOCTEXT("SInterchangePipelineConfigurationDialog_TranslatorSettings_Tooltip", "Edit translator project settings."))
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnEditTranslatorSettings)
					[
						SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.Settings"))
							.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(this, &SInterchangePipelineConfigurationDialog::GetSourceDescription)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(10.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("SInterchangePipelineConfigurationDialog_BasicLayoutOptions_tooltip", "Basic Layout display only the basic pipelines properties."))
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SInterchangePipelineConfigurationDialog_BasicLayoutOptions", "Basic Layout"))
					]
					+ SHorizontalBox::Slot()
					.Padding(4.f, 0.f)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SInterchangePipelineConfigurationDialog::IsBasicLayoutEnabled)
						.OnCheckStateChanged(this, &SInterchangePipelineConfigurationDialog::OnBasicLayoutChanged)
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(10.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("SInterchangePipelineConfigurationDialog_FilterPipelineOptions_tooltip", "Filter the pipeline options using the source content data."))
					.Visibility_Lambda([this]()
						{
							return bReimport ? EVisibility::Collapsed : EVisibility::All;
						})
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SInterchangePipelineConfigurationDialog_FilterPipelineOptions", "Filter on Contents"))
					]
					+ SHorizontalBox::Slot()
					.Padding(4.f, 0.f)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SInterchangePipelineConfigurationDialog::IsFilteringOptions)
						.OnCheckStateChanged(this, &SInterchangePipelineConfigurationDialog::OnFilterOptionsChanged)
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(2.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([this]()
						{
							return PipelinesListView->GetNumItemsSelected() == 1;
						})
					.Text(LOCTEXT("SInterchangePipelineConfigurationDialog_ResetToPipelineAsset", "Reset Selected Pipeline"))
					.ToolTipText_Lambda([bReimportClosure = bReimport]()
						{
							if (bReimportClosure)
							{
								return LOCTEXT("SInterchangePipelineConfigurationDialog_ResetToPipelineAsset_TooltipReimport", "Reset the selected pipeline to the values used the last time this asset was imported.");
							}
							else
							{
								return LOCTEXT("SInterchangePipelineConfigurationDialog_ResetToPipelineAsset_Tooltip", "Reset the properties of the selected pipeline.");
							}
						})
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnResetToDefault)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SpawnPipelineConfiguration()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0.f)
				.AutoWidth()
				[
					IDocumentation::Get()->CreateAnchor(FString("interchange-framework-in-unreal-engine"))
				]
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0.f)
				.AutoWidth()
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("InspectorGraphWindow_ReuseSettingsToolTip", "When importing multiple files, keep the same import settings for every file or open the settings dialog for each file."))
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InspectorGraphWindow_ReuseSettings", "Use the same settings for subsequent files"))
					]
					+ SHorizontalBox::Slot()
					.Padding(4.f, 0.f)
					[
						SAssignNew(UseSameSettingsForAllCheckBox, SCheckBox)
						.IsChecked(true)
						.IsEnabled(this, &SInterchangePipelineConfigurationDialog::IsImportButtonEnabled)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f) 
				[
					SNew(SPrimaryButton)
					.Icon(this, &SInterchangePipelineConfigurationDialog::GetImportButtonIcon)
					.Text(LOCTEXT("InspectorGraphWindow_Import", "Import"))
					.ToolTipText(this, &SInterchangePipelineConfigurationDialog::GetImportButtonTooltip)
					.IsEnabled(this, &SInterchangePipelineConfigurationDialog::IsImportButtonEnabled)
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnCloseDialog, ECloseEventType::Import)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("InspectorGraphWindow_Preview", "Preview..."))
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnPreviewImport)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f) 
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("InspectorGraphWindow_Cancel", "Cancel"))
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnCloseDialog, ECloseEventType::Cancel)
				]
			]
		]
	];

	//Select the first pipeline
	if (PipelineListViewItems.Num() > 0)
	{
		bool bSelectFirst = true;
		FString LastPipelineName;
		FString KeyName = CurrentStackName.ToString() + TEXT("_LastSelectedPipeline");
		if (GConfig->GetString(TEXT("InterchangeSelectPipeline"), *KeyName, LastPipelineName, GEditorPerProjectIni))
		{
			for (TSharedPtr<FInterchangePipelineItemType> PipelineItem : PipelineListViewItems)
			{
				FString PipelineItemName = PipelineItem->Pipeline->GetClass()->GetName();
				if (PipelineItemName.Equals(LastPipelineName))
				{
					PipelinesListView->SetSelection(PipelineItem, ESelectInfo::Direct);
					bSelectFirst = false;
					break;
				}
			}
		}
		if (bSelectFirst)
		{
			PipelinesListView->SetSelection(PipelineListViewItems[0], ESelectInfo::Direct);
		}
	}
}

bool SInterchangePipelineConfigurationDialog::IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	if (bReimport)
	{
		const FName ReimportRestrictKey(TEXT("ReimportRestrict"));
		return !(PropertyAndParent.Property.GetBoolMetaData(ReimportRestrictKey));
	}
	return true;
}

const FSlateBrush* SInterchangePipelineConfigurationDialog::GetImportButtonIcon() const
{
	const FSlateBrush* TypeIcon = nullptr;
	for (TSharedPtr<FInterchangePipelineItemType> PipelineItem : PipelineListViewItems)
	{
		if (PipelineItem.IsValid() && PipelineItem->Pipeline)
		{
			if (PipelineItem->ConflictInfos.Num() > 0)
			{
				const FSlateIcon SlateIcon = FSlateIconFinder::FindIcon("Icons.Warning");
				return SlateIcon.GetOptionalIcon();
			}
		}
	}
	return TypeIcon;
}


FText SInterchangePipelineConfigurationDialog::GetSourceDescription() const
{
	FText ActionDescription;
	if (bReimport)
	{
		ActionDescription = LOCTEXT("GetSourceDescription_Reimport", "Reimport");
	}
	else
	{
		ActionDescription = LOCTEXT("GetSourceDescription_Import", "Import");
	}
	if (SourceData.IsValid())
	{
		ActionDescription = FText::Format(LOCTEXT("GetSourceDescription", "{0} source {1}"), ActionDescription, FText::FromString(SourceData->GetFilename()));
	}
	return ActionDescription;
}

FReply SInterchangePipelineConfigurationDialog::OnResetToDefault()
{
	FReply Result = FReply::Handled();
	TArray<TWeakObjectPtr<UObject>> SelectedPipelines = PipelineConfigurationDetailsView->GetSelectedObjects();
	if (CurrentStackName == NAME_None)
	{
		return Result;
	}

	FInterchangePipelineItemType* PipelineToEdit = nullptr;

	//Multi selection is not allowed
	for(TWeakObjectPtr<UObject> WeakObject : SelectedPipelines)
	{
		//We test the cast because we can have null or other type selected (i.e. translator settings class default object).
		if (UInterchangePipelineBase* Pipeline = Cast<UInterchangePipelineBase>(WeakObject.Get()))
		{
			const UClass* PipelineClass = Pipeline->GetClass();

			for (FInterchangeStackInfo& Stack : PipelineStacks)
			{
				if (Stack.StackName != CurrentStackName)
				{
					continue;
				}
				for (const TObjectPtr<UInterchangePipelineBase>& DefaultPipeline : Stack.Pipelines)
				{
					//We assume the pipelines inside one stack are all different classes, we use the class to know which default asset we need to duplicate
					if (DefaultPipeline->GetClass() == PipelineClass)
					{
						for(int32 PipelineIndex = 0; PipelineIndex < PipelineListViewItems.Num(); ++PipelineIndex)
						{
							TObjectPtr<UInterchangePipelineBase> PipelineElement = PipelineListViewItems[PipelineIndex]->Pipeline;
							if (PipelineElement.Get() == Pipeline)
							{
								if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstanceInSourceAssetPackage(DefaultPipeline))
								{
									GeneratedPipeline->TransferAdjustSettings(DefaultPipeline);
									GeneratedPipeline->SetBasicLayoutMode(bBasicLayout);
									if(bFilterOptions && BaseNodeContainer.IsValid())
									{
										GeneratedPipeline->FilterPropertiesFromTranslatedData(BaseNodeContainer.Get());
									}
									//Switch the pipeline the element point on
									PipelineListViewItems[PipelineIndex]->Pipeline = GeneratedPipeline;
									PipelineToEdit = PipelineListViewItems[PipelineIndex].Get();
									PipelinesListView->SetSelection(PipelineListViewItems[PipelineIndex], ESelectInfo::Direct);
									PipelinesListView->RequestListRefresh();
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	SetEditPipeline(PipelineToEdit);
	return Result;
}

bool SInterchangePipelineConfigurationDialog::ValidateAllPipelineSettings(TOptional<FText>& OutInvalidReason) const
{
	for (TSharedPtr<FInterchangePipelineItemType> PipelineElement : PipelineListViewItems)
	{
		check(PipelineElement->Pipeline);
		if (!PipelineElement->Pipeline->IsSettingsAreValid(OutInvalidReason))
		{
			return false;
		}
	}
	return true;
}

bool SInterchangePipelineConfigurationDialog::IsImportButtonEnabled() const
{
	TOptional<FText> InvalidReason;
	return ValidateAllPipelineSettings(InvalidReason);
}

FText SInterchangePipelineConfigurationDialog::GetImportButtonTooltip() const
{
	//Pipeline validation
	TOptional<FText> InvalidReason;
	if (!ValidateAllPipelineSettings(InvalidReason) && InvalidReason.IsSet())
	{
		return InvalidReason.GetValue();
	}

	//Pipeline conflicts
	for (TSharedPtr<FInterchangePipelineItemType> PipelineItem : PipelineListViewItems)
	{
		if (PipelineItem.IsValid() && PipelineItem->Pipeline)
		{
			if (PipelineItem->ConflictInfos.Num() > 0)
			{
				return LOCTEXT("ImportButtonConflictTooltip", "There is one or more pipeline conflicts, look at any conflict in the pipeline list to have more detail.");
			}
		}
	}

	//Default tooltip
	return LOCTEXT("ImportButtonDefaultTooltip", "Selected pipeline stack will be used for the current import");
}

void SInterchangePipelineConfigurationDialog::SaveAllPipelineSettings() const
{
	for (TSharedPtr<FInterchangePipelineItemType> PipelineElement : PipelineListViewItems)
	{
		if (PipelineElement->Pipeline)
		{
			PipelineElement->Pipeline->SaveSettings(CurrentStackName);
		}
	}
}

void SInterchangePipelineConfigurationDialog::ClosePipelineConfiguration(const ECloseEventType CloseEventType)
{
	if (CloseEventType == ECloseEventType::Cancel || CloseEventType == ECloseEventType::WindowClosing)
	{
		bCanceled = true;
		bImportAll = false;
	}
	else //ECloseEventType::Import
	{
		bCanceled = false;
		bImportAll = UseSameSettingsForAllCheckBox->IsChecked();
		
		//Fill the OutPipelines array
		for (TSharedPtr<FInterchangePipelineItemType> PipelineElement : PipelineListViewItems)
		{
			if (!bReimport)
			{
				// Create a name that would not cause conflict when this asset maybe reimported.
				FString NewPipelineName = ReimportPipelinePrefix + PipelineElement->DisplayName;
				PipelineElement->Pipeline->Rename(*NewPipelineName);
			}
			OutPipelines->Add(PipelineElement->Pipeline);
		}
	}

	//Save the settings only if its not a re-import
	if (!bReimport)
	{
		SaveAllPipelineSettings();
	}

	PipelineConfigurationDetailsView = nullptr;

	if (CloseEventType != ECloseEventType::WindowClosing)
	{
		if (TSharedPtr<SWindow> OwnerWindowPin = OwnerWindow.Pin())
		{
			OwnerWindowPin->GetOnWindowClosedEvent().RemoveAll(this);
			OwnerWindowPin->RequestDestroyWindow();
		}
	}
	OwnerWindow = nullptr;
}

FReply SInterchangePipelineConfigurationDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCloseDialog(ECloseEventType::Cancel);
	}
	return FReply::Unhandled();
}

void SInterchangePipelineConfigurationDialog::RefreshStack(bool bStackSelectionChange)
{
	LLM_SCOPE_BYNAME(TEXT("Interchange"));

	//Save current stack settings, we want the same settings when we will go back to the same stack
	//When doing a reimport we do not want to save the setting because the context have special default
	//value for some options like: (Import Materials, Import Textures...).
	//So when doing a reimport switching stack is like doing a reset to default on all pipelines
	if (!bReimport || !bStackSelectionChange)
	{
		SaveAllPipelineSettings();
	}

	int32 CurrentPipelineIndex = 0;
	if (!bStackSelectionChange)
	{
		//store the selected pipeline
		for (int32 PipelineIndex = 0; PipelineIndex < PipelineListViewItems.Num(); ++PipelineIndex)
		{
			const TSharedPtr<FInterchangePipelineItemType> PipelineItem = PipelineListViewItems[PipelineIndex];
			if (PipelineItem->Pipeline == CurrentSelectedPipeline)
			{
				CurrentPipelineIndex = PipelineIndex;
				break;
			}
		}
	}

	//Rebuild the Pipeline list item
	PipelineListViewItems.Reset();

	for (FInterchangeStackInfo& Stack : PipelineStacks)
	{
		TSharedPtr<FString> StackNamePtr = MakeShared<FString>(Stack.StackName.ToString());
		if (CurrentStackName != Stack.StackName)
		{
			continue;
		}
		for (const TObjectPtr<UInterchangePipelineBase>& DefaultPipeline : Stack.Pipelines)
		{
			check(DefaultPipeline);
			if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstanceInSourceAssetPackage(DefaultPipeline))
			{
				GeneratedPipeline->TransferAdjustSettings(DefaultPipeline);
				if (Stack.StackName != ReimportStackName || !bStackSelectionChange)
				{
					//Load the settings for this pipeline
					GeneratedPipeline->LoadSettings(Stack.StackName);
					if (bStackSelectionChange)
					{
						//Do not reset pipeline value if we are just refreshing the filtering
						GeneratedPipeline->PreDialogCleanup(Stack.StackName);
					}
				}
				GeneratedPipeline->SetBasicLayoutMode(bBasicLayout);
				if (bFilterOptions && BaseNodeContainer.IsValid())
				{
					GeneratedPipeline->FilterPropertiesFromTranslatedData(BaseNodeContainer.Get());
				}
				PipelineListViewItems.Add(MakeShareable(new FInterchangePipelineItemType{ GetPipelineDisplayName(DefaultPipeline), GeneratedPipeline, ReimportObject.Get(), BaseNodeContainer.Get(), SourceData.Get(), bBasicLayout }));
			}
		}
	}

	//Select the first pipeline
	if (PipelineListViewItems.Num() > 0)
	{
		CurrentPipelineIndex = PipelineListViewItems.IsValidIndex(CurrentPipelineIndex) ? CurrentPipelineIndex : 0;
		PipelinesListView->SetSelection(PipelineListViewItems[CurrentPipelineIndex], ESelectInfo::Direct);
	}
	PipelinesListView->RequestListRefresh();
}

void SInterchangePipelineConfigurationDialog::OnStackSelectionChanged(TSharedPtr<FString> String, ESelectInfo::Type)
{
	if (!String.IsValid())
	{
		return;
	}

	FName NewStackName = FName(*String.Get());
	if (!UE::Private::ContainStack(PipelineStacks, NewStackName))
	{
		return;
	}

	//Nothing change the selection is the same
	if (CurrentStackName == NewStackName)
	{
		return;
	}

	//Use the stack select by interchange manager
	CurrentStackName = NewStackName;

	constexpr bool bStackSelectionChange = true;
	RefreshStack(bStackSelectionChange);
}

TSharedRef<ITableRow> SInterchangePipelineConfigurationDialog::MakePipelineListRowWidget(
	TSharedPtr<FInterchangePipelineItemType> InElement,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	check(InElement->Pipeline);
	return SNew(SInterchangePipelineItem, OwnerTable, InElement);
}

void SInterchangePipelineConfigurationDialog::OnPipelineSelectionChanged(TSharedPtr<FInterchangePipelineItemType> InItem, ESelectInfo::Type SelectInfo)
{
	CurrentSelectedPipeline = nullptr;
	if (InItem)
	{
		CurrentSelectedPipeline = InItem->Pipeline;
	}
	SetEditPipeline(InItem.Get());
	
	if (CurrentSelectedPipeline)
	{
		FString CurrentPipelineName = CurrentSelectedPipeline->GetClass()->GetName();
		FString KeyName = CurrentStackName.ToString() + TEXT("_LastSelectedPipeline");
		GConfig->SetString(TEXT("InterchangeSelectPipeline"), *KeyName, *CurrentPipelineName, GEditorPerProjectIni);
	}
}

void SInterchangePipelineConfigurationDialog::OnFilterOptionsChanged(ECheckBoxState CheckState)
{
	bool bNewCheckValue = CheckState == ECheckBoxState::Checked ? true : false;
	if (bNewCheckValue == bFilterOptions)
	{
		//Check state did not change
		return;
	}
	bFilterOptions = bNewCheckValue;
	//Refresh the pipeline
	constexpr bool bStackSelectionChange = false;
	RefreshStack(bStackSelectionChange);

	GConfig->SetBool(TEXT("InterchangeImportDialogOptions"), TEXT("FilterOptions"), bFilterOptions, GEditorPerProjectIni);
}

void SInterchangePipelineConfigurationDialog::OnBasicLayoutChanged(ECheckBoxState CheckState)
{
	bool bNewCheckValue = CheckState == ECheckBoxState::Checked ? true : false;
	if (bNewCheckValue == bBasicLayout)
	{
		//Check state did not change
		return;
	}
	bBasicLayout = bNewCheckValue;
	//Refresh the pipeline
	constexpr bool bStackSelectionChange = false;
	RefreshStack(bStackSelectionChange);

	GConfig->SetBool(TEXT("InterchangeImportDialogOptions"), TEXT("BasicLayout"), bBasicLayout, GEditorPerProjectIni);
}

FReply SInterchangePipelineConfigurationDialog::OnPreviewImport() const
{
	auto ClearObjectFlags = [](UObject* Obj)
		{
			Obj->ClearFlags(RF_Standalone | RF_Public);
			Obj->ClearInternalFlags(EInternalObjectFlags::Async);
		};
	UInterchangeBaseNodeContainer* DuplicateBaseNodeContainer = DuplicateObject<UInterchangeBaseNodeContainer>(BaseNodeContainer.Get(), GetTransientPackage());

	TArray<UInterchangeSourceData*> SourceDatas;
	SourceDatas.Add(SourceData.Get());

	//Execute all pipelines on the duplicated container
	UInterchangeResultsContainer* Results = NewObject<UInterchangeResultsContainer>(GetTransientPackage());
	for (int32 PipelineIndex = 0; PipelineIndex < PipelineListViewItems.Num(); ++PipelineIndex)
	{
		const TSharedPtr<FInterchangePipelineItemType> PipelineItem = PipelineListViewItems[PipelineIndex];
		
		//Duplicate the pipeline because ScriptedExecutePipeline is not const
		if (UInterchangePipelineBase* DuplicatedPipeline = DuplicateObject<UInterchangePipelineBase>(PipelineItem->Pipeline, GetTransientPackage()))
		{
			DuplicatedPipeline->SetResultsContainer(Results);
			DuplicatedPipeline->ScriptedExecutePipeline(DuplicateBaseNodeContainer, SourceDatas, FString());
			ClearObjectFlags(DuplicatedPipeline);
		}
	}

	
	DuplicateBaseNodeContainer->IterateNodesOfType<UInterchangeFactoryBaseNode>([ClosureReimportObject = ReimportObject](const FString& NodeUid, UInterchangeFactoryBaseNode* Node)
		{

			//Set all node in preview mode so hide the internal data attributes
			Node->UserInterfaceContext = EInterchangeNodeUserInterfaceContext::Preview;

			//If we reimport a specific object we want to disabled all factory nodes that are not supporting the reimport object class
			if (ClosureReimportObject.IsValid())
			{
				Node->SetEnabled(ClosureReimportObject.Get()->IsA(Node->GetObjectClass()));
			}
		});

	//Create and show the graph inspector UI dialog
	TSharedRef<SWindow> Window = SNew(SWindow)
		.ClientSize(FVector2D(800.f, 650.f))
		.Title(NSLOCTEXT("SInterchangePipelineConfigurationDialog", "InterchangePreviewTitle", "Interchange Preview"));
	TSharedPtr<SInterchangeGraphInspectorWindow> InterchangeGraphInspectorWindow;

	Window->SetContent
	(
		SAssignNew(InterchangeGraphInspectorWindow, SInterchangeGraphInspectorWindow)
		.InterchangeBaseNodeContainer(DuplicateBaseNodeContainer)
		.bPreview(true)
		.OwnerWindow(Window)
	);

	FSlateApplication::Get().AddModalWindow(Window, OwnerWindow.Pin(), false);

	//Make sure all temporary object are not flags to persist
	//We cannot run a gc now since the pipeline we will return are not yet hold by the AsyncHelper, so they will be garbage collect
	ClearObjectFlags(DuplicateBaseNodeContainer);
	ClearObjectFlags(Results);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
