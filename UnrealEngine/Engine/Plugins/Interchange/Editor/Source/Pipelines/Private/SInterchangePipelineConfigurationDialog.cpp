// Copyright Epic Games, Inc. All Rights Reserved.
#include "SInterchangePipelineConfigurationDialog.h"

#include "DetailsViewArgs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "GameFramework/Actor.h"
#include "Framework/Views/TableViewMetadata.h"
#include "IDetailsView.h"
#include "IDocumentation.h"
#include "InterchangeManager.h"
#include "InterchangePipelineConfigurationBase.h"
#include "InterchangeProjectSettings.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "PropertyEditorModule.h"
#include "SPrimaryButton.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "InterchangePipelineConfiguration"

const FName ReimportStackName = TEXT("ReimportPipeline");
const FString ReimportPipelinePrefix = TEXT("reimport_");

 // Pipelines are renamed with the reimport prefix to avoid conflicts with the duplicates of the original pipelines that end up in the same package.
 // As this is the name displayed in the Dialog, conflicts won't matter.
FString SInterchangePipelineConfigurationDialog::GetPipelineDisplayName(const UInterchangePipelineBase* Pipeline)
{
	static int32 RightChopIndex = ReimportPipelinePrefix.Len();

	FString PipelineDisplayName = Pipeline->GetName();
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

void SInterchangePipelineItem::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& OwnerTable,
	TSharedPtr<FInterchangePipelineItemType> InPipelineElement)
{
	PipelineElement = InPipelineElement;
	TObjectPtr<UInterchangePipelineBase> PipelineElementPtr = PipelineElement->Pipeline;
	check(PipelineElementPtr.Get());
	FText PipelineName = LOCTEXT("InvalidPipelineName", "Invalid Pipeline");
	if (PipelineElementPtr.Get())
	{
		FString PipelineNameString = FString::Printf(TEXT("%s (%s)"), *PipelineElement->DisplayName, *PipelineElementPtr->GetClass()->GetName());
		PipelineName = FText::FromString(PipelineNameString);
	}
		
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
			.FillWidth(1.0f)
			.Padding(3.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(PipelineName)
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

					PipelineListViewItems.Add(MakeShareable(new FInterchangePipelineItemType{ GetPipelineDisplayName(DefaultPipeline), GeneratedPipeline}));
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
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	PipelineConfigurationDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBox->SetContent(PipelineConfigurationDetailsView->AsShared());
	PipelineConfigurationDetailsView->SetObject(nullptr);
	PipelineConfigurationDetailsView->GetIsPropertyVisibleDelegate().BindLambda([this](const FPropertyAndParent& PropertyAndParent)
		{
			return IsPropertyVisible(PropertyAndParent);
		});
	return PipelineConfigurationPanelBox;
}

void SInterchangePipelineConfigurationDialog::Construct(const FArguments& InArgs)
{
	//Make sure there is a valid default value

	OwnerWindow = InArgs._OwnerWindow;
	SourceData = InArgs._SourceData;
	bSceneImport = InArgs._bSceneImport;
	bReimport = InArgs._bReimport;
	PipelineStacks = InArgs._PipelineStacks;
	OutPipelines = InArgs._OutPipelines;

	check(OutPipelines);

	check(OwnerWindow.IsValid());
	if (TSharedPtr<SWindow> OwnerWindowPinned = OwnerWindow.Pin())
	{
		OwnerWindowPinned->GetOnWindowClosedEvent().AddRaw(this, &SInterchangePipelineConfigurationDialog::OnWindowClosed);
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
				.FillWidth(1.0f)
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
								return LOCTEXT("SInterchangePipelineConfigurationDialog_ResetToPipelineAsset_TooltipReimport", "Reset the selected pipeline properties to the asset import data pipeline properties.");
							}
							else
							{
								return LOCTEXT("SInterchangePipelineConfigurationDialog_ResetToPipelineAsset_Tooltip", "Reset the selected pipeline properties to the stack pipeline properties.");
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
					IDocumentation::Get()->CreateAnchor(FString("Engine/Content/Interchange/PipelineConfiguration"))
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
	//Multi selection is not allowed
	ensure(SelectedPipelines.Num() <= 1);
	for(TWeakObjectPtr<UObject> WeakObject : SelectedPipelines)
	{
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
									//Switch the pipeline the element point on
									PipelineListViewItems[PipelineIndex]->Pipeline = GeneratedPipeline;
									PipelineConfigurationDetailsView->SetObject(GeneratedPipeline, true);
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
	TOptional<FText> InvalidReason;
	if (!ValidateAllPipelineSettings(InvalidReason) && InvalidReason.IsSet())
	{
		return InvalidReason.GetValue();
	}
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
		if (!FApp::IsUnattended())
		{
			FString Message = FText(LOCTEXT("InterchangePipelineCancelEscKey", "Are you sure you want to cancel the import?")).ToString();
			if (FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *Message, TEXT("Cancel Import")) == EAppReturnType::Type::Yes)
			{
				return OnCloseDialog(ECloseEventType::Cancel);
			}
		}
	}
	return FReply::Unhandled();
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

	//Save current stack settings, we want the same settings when we will go back to the same stack
	//When doing a reimport we do not want to save the setting because the context have special default
	//value for some options like: (Import Materials, Import Textures...).
	//So when doing a reimport switching stack is like doing a reset to default on all pipelines
	if (!bReimport)
	{
		SaveAllPipelineSettings();
	}

	//Use the stack select by interchange manager
	CurrentStackName = NewStackName;
	
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
				if (Stack.StackName != ReimportStackName)
				{
					//Load the settings for this pipeline
					GeneratedPipeline->LoadSettings(Stack.StackName);
					GeneratedPipeline->PreDialogCleanup(Stack.StackName);
				}
				PipelineListViewItems.Add(MakeShareable(new FInterchangePipelineItemType{ GetPipelineDisplayName(DefaultPipeline), GeneratedPipeline}));
			}
		}
	}

	//Select the first pipeline
	if (PipelineListViewItems.Num() > 0)
	{
		PipelinesListView->SetSelection(PipelineListViewItems[0], ESelectInfo::Direct);
	}
	PipelinesListView->RequestListRefresh();
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
	PipelineConfigurationDetailsView->SetObject(CurrentSelectedPipeline.Get());
	
	if (CurrentSelectedPipeline)
	{
		FString CurrentPipelineName = CurrentSelectedPipeline->GetClass()->GetName();
		FString KeyName = CurrentStackName.ToString() + TEXT("_LastSelectedPipeline");
		GConfig->SetString(TEXT("InterchangeSelectPipeline"), *KeyName, *CurrentPipelineName, GEditorPerProjectIni);
	}
}

#undef LOCTEXT_NAMESPACE
