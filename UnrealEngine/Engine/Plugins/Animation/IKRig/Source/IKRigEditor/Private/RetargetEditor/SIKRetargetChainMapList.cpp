// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/SIKRetargetChainMapList.h"

#include "ScopedTransaction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPersonaToolkit.h"
#include "SKismetInspector.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SPositiveActionButton.h"
#include "Retargeter/IKRetargeter.h"
#include "RigEditor/IKRigEditorStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "SSearchableComboBox.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "Widgets/Input/SSearchBox.h"
#include "Retargeter/IKRetargetSettings.h"

#define LOCTEXT_NAMESPACE "SIKRigRetargetChains"

static const FName ColumnId_TargetChainLabel( "Target Bone Chain" );
static const FName ColumnId_SourceChainLabel( "Source Bone Chain" );
static const FName ColumnId_IKGoalNameLabel( "Target IK Goal" );
static const FName ColumnId_ResetLabel( "Reset" );

TSharedRef<ITableRow> FRetargetChainMapElement::MakeListRowWidget(
	const TSharedRef<STableViewBase>& InOwnerTable,
	TSharedRef<FRetargetChainMapElement> InChainElement,
	TSharedPtr<SIKRetargetChainMapList> InChainList)
{
	return SNew(SIKRetargetChainMapRow, InOwnerTable, InChainElement, InChainList);
}

void SIKRetargetChainMapRow::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& InOwnerTableView,
	TSharedRef<FRetargetChainMapElement> InChainElement,
	TSharedPtr<SIKRetargetChainMapList> InChainList)
{
	ChainMapElement = InChainElement;
	ChainMapList = InChainList;

	// generate list of source chains
	// NOTE: cannot just use FName because "None" is considered a null entry and removed from ComboBox.
	SourceChainOptions.Reset();
	SourceChainOptions.Add(MakeShareable(new FString(TEXT("None"))));
	const UIKRigDefinition* SourceIKRig = ChainMapList.Pin()->EditorController.Pin()->AssetController->GetIKRig(ERetargetSourceOrTarget::Source);
	if (SourceIKRig)
	{
		const TArray<FBoneChain>& Chains = SourceIKRig->GetRetargetChains();
		for (const FBoneChain& BoneChain : Chains)
		{
			SourceChainOptions.Add(MakeShareable(new FString(BoneChain.ChainName.ToString())));
		}
	}

	SMultiColumnTableRow< FRetargetChainMapElementPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SIKRetargetChainMapRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == ColumnId_TargetChainLabel)
	{
		TSharedRef<SWidget> NewWidget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(3.0f, 1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(ChainMapElement.Pin()->ChainMap->TargetChain))
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
			];
		return NewWidget;
	}

	if (ColumnName == ColumnId_SourceChainLabel)
	{
		TSharedRef<SWidget> NewWidget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(3.0f, 1.0f)
		[
			SNew(SSearchableComboBox)
			.OptionsSource(&SourceChainOptions)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
			{
				return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
			})
			.OnSelectionChanged(this, &SIKRetargetChainMapRow::OnSourceChainComboSelectionChanged)
			[
				SNew(STextBlock)
				.Text(this, &SIKRetargetChainMapRow::GetSourceChainName)
			]
		];
		return NewWidget;
	}

	if (ColumnName == ColumnId_IKGoalNameLabel)
	{
		TSharedRef<SWidget> NewWidget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(3.0f, 1.0f)
		[
			SNew(STextBlock)
			.Text(GetTargetIKGoalName())
			.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
		];
		return NewWidget;
	}

	if (ColumnName == ColumnId_ResetLabel)
	{
		TSharedRef<SWidget> NewWidget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(3.0f, 1.0f)
		[
			SNew(SButton)
			.OnClicked(this, &SIKRetargetChainMapRow::OnResetToDefaultClicked) 
			.Visibility(this, &SIKRetargetChainMapRow::GetResetToDefaultVisibility) 
			.ToolTipText(LOCTEXT("ResetChainToDefaultToolTip", "Reset Chain Settings to Default"))
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		];
		return NewWidget;
	}
	
	checkNoEntry();
	return SNullWidget::NullWidget;
}

FReply SIKRetargetChainMapRow::OnResetToDefaultClicked()
{
	ChainMapList.Pin()->ResetChainSettings(ChainMapElement.Pin()->ChainMap.Get());
	return FReply::Handled();
}

EVisibility SIKRetargetChainMapRow::GetResetToDefaultVisibility() const
{
	const FTargetChainSettings& DefaultSettings = FTargetChainSettings();
	const FTargetChainSettings& Settings = ChainMapElement.Pin()->ChainMap->Settings;
	return (Settings == DefaultSettings) ? EVisibility::Hidden : EVisibility::Visible;
}

void SIKRetargetChainMapRow::OnSourceChainComboSelectionChanged(TSharedPtr<FString> InName, ESelectInfo::Type SelectInfo)
{
	UIKRetargeterController* RetargeterController = ChainMapList.Pin()->GetRetargetController();
	if (!RetargeterController)
	{
		return; 
	}

	const FName SourceChainName = FName(*InName.Get());
	RetargeterController->SetSourceChainForTargetChain(ChainMapElement.Pin()->ChainMap.Get(), SourceChainName);
}

FText SIKRetargetChainMapRow::GetSourceChainName() const
{
	UIKRetargeterController* RetargeterController = ChainMapList.Pin()->GetRetargetController();
	if (!RetargeterController)
	{
		return FText::FromName(NAME_None); 
	}

	return FText::FromName(ChainMapElement.Pin()->ChainMap->SourceChain);
}

FText SIKRetargetChainMapRow::GetTargetIKGoalName() const
{
	UIKRetargeterController* RetargeterController = ChainMapList.Pin()->GetRetargetController();
	if (!RetargeterController)
	{
		return FText(); 
	}

	const UIKRigDefinition* IKRig = RetargeterController->GetIKRig(ERetargetSourceOrTarget::Target);
	if (!IKRig)
	{
		return FText(); 
	}

	const FBoneChain* Chain = IKRig->GetRetargetChainByName(ChainMapElement.Pin()->ChainMap->TargetChain);
	if (!Chain)
	{
		return FText(); 
	}

	if (Chain->IKGoalName == NAME_None)
	{
		return FText::FromString("");
	}
	
	return FText::FromName(Chain->IKGoalName);
}

void SIKRetargetChainMapList::Construct(
	const FArguments& InArgs,
	TSharedRef<FIKRetargetEditorController> InEditorController)
{
	EditorController = InEditorController;
	EditorController.Pin()->SetChainsView(SharedThis(this));

	TextFilter = MakeShareable(new FTextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::BasicString));
	
	ChildSlot
    [
	    SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SPositiveActionButton)
				.Icon(FAppStyle::Get().GetBrush("Icons.Settings"))
				.Text(LOCTEXT("EditGlobalButtonLabel", "Global Settings"))
				.ToolTipText(LOCTEXT("EditGlobalButtonToolTip", "Edit the global retarget settings."))
				.OnClicked(this, &SIKRetargetChainMapList::OnGlobalSettingsButtonClicked)
			]
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SPositiveActionButton)
				.Icon(FAppStyle::Get().GetBrush("Icons.Settings"))
				.Text(LOCTEXT("EditRootButtonLabel", "Root Settings"))
				.ToolTipText(LOCTEXT("EditRootButtonToolTip", "Edit the root retarget settings."))
				.OnClicked(this, &SIKRetargetChainMapList::OnRootSettingsButtonClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TargetRootLabel", "Target: "))
				.TextStyle(FAppStyle::Get(), "NormalText")
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SIKRetargetChainMapList::GetTargetRootBone)
				.IsEnabled(false)
			]
				
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SourceRootLabel", "Source: "))
				.TextStyle(FAppStyle::Get(), "NormalText")
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SIKRetargetChainMapList::GetSourceRootBone)
				.IsEnabled(false)
			]
		]

		+SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.f, 0.0))
			[
				SNew(SPositiveActionButton)
				.Visibility(this, &SIKRetargetChainMapList::IsAutoMapButtonVisible)
				.Icon(FAppStyle::Get().GetBrush("Icons.Refresh"))
				.Text(LOCTEXT("AutoMapButtonLabel", "Auto-Map Chains"))
				.ToolTipText(LOCTEXT("AutoMapButtonToolTip", "Automatically assign source chains based on fuzzy string match"))
				.OnClicked(this, &SIKRetargetChainMapList::OnAutoMapButtonClicked)
			]
			
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSearchBox)
				.SelectAllTextWhenFocused(true)
				.OnTextChanged( this, &SIKRetargetChainMapList::OnFilterTextChanged )
				.HintText( LOCTEXT( "SearchBoxHint", "Filter Chain List...") )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.0))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
				.ForegroundColor(FSlateColor::UseStyle())
				.ContentPadding(2.0f)
				.OnGetMenuContent( this, &SIKRetargetChainMapList::CreateFilterMenuWidget )
				.ToolTipText( LOCTEXT("ChainMapFilterToolTip", "Filter list of chain mappings."))
				.HasDownArrow(true)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]
	    
        +SVerticalBox::Slot()
        [
			SAssignNew(ListView, SRetargetChainMapListViewType )
			.SelectionMode(ESelectionMode::Multi)
			.IsEnabled(this, &SIKRetargetChainMapList::IsChainMapEnabled)
			.ListItemsSource( &ListViewItems )
			.OnGenerateRow( this, &SIKRetargetChainMapList::MakeListRowWidget )
			.OnMouseButtonClick_Lambda([this](TSharedPtr<FRetargetChainMapElement> Item)
			{
				OnItemClicked(Item);
			})

			.ItemHeight( 22.0f )
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column(ColumnId_TargetChainLabel)
				.DefaultLabel(LOCTEXT("TargetColumnLabel", "Target Chain"))
				.DefaultTooltip(LOCTEXT("TargetChainToolTip", "The chain on the target skeleton to copy animation TO."))
				
				+ SHeaderRow::Column(ColumnId_IKGoalNameLabel)
				.DefaultLabel(LOCTEXT("IKColumnLabel", "Target IK Goal"))
				.DefaultTooltip(LOCTEXT("IKGoalToolTip", "The IK Goal assigned to the target chain (if any). Note, this goal should be on the LAST bone in the chain."))

				+ SHeaderRow::Column(ColumnId_SourceChainLabel)
				.DefaultLabel(LOCTEXT("SourceColumnLabel", "Source Chain"))
				.DefaultTooltip(LOCTEXT("SourceChainToolTip", "The chain on the source skeleton to copy animation FROM."))
				
				+ SHeaderRow::Column(ColumnId_ResetLabel)
                .DefaultLabel(LOCTEXT("ResetColumnLabel", "Reset"))
                .DefaultTooltip(LOCTEXT("ResetChainToolTip", "Reset the FK and IK settings for this chain."))
			)
        ]
    ];

	RefreshView();
}

void SIKRetargetChainMapList::ClearSelection() const
{
	ListView->ClearSelection();
}

void SIKRetargetChainMapList::ResetChainSettings(URetargetChainSettings* ChainMap) const
{
	FScopedTransaction Transaction(LOCTEXT("ResetChainSettings", "Reset Retarget Chain Settings"));
	ChainMap->Modify();
	ChainMap->Settings = FTargetChainSettings();
	EditorController.Pin()->RefreshDetailsView();
}

UIKRetargeterController* SIKRetargetChainMapList::GetRetargetController() const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return nullptr;
	}

	return Controller->AssetController;
}

FText SIKRetargetChainMapList::GetSourceRootBone() const
{
	UIKRetargeterController* RetargeterController = GetRetargetController();
	if (!RetargeterController)
	{
		return FText::FromName(NAME_None); 
	}
	
	return FText::FromName(RetargeterController->GetRetargetRootBone(ERetargetSourceOrTarget::Source));
}

FText SIKRetargetChainMapList::GetTargetRootBone() const
{
	UIKRetargeterController* RetargeterController = GetRetargetController();
	if (!RetargeterController)
	{
		return FText::FromName(NAME_None); 
	}
	
	return FText::FromName(RetargeterController->GetRetargetRootBone(ERetargetSourceOrTarget::Target));
}

bool SIKRetargetChainMapList::IsChainMapEnabled() const
{
	UIKRetargeterController* RetargeterController = GetRetargetController();
	if (!RetargeterController)
	{
		return false; 
	}
	
	if (RetargeterController->GetAsset()->GetTargetIKRig())
	{
		const TArray<FBoneChain>& Chains = RetargeterController->GetAsset()->GetTargetIKRig()->GetRetargetChains();
		return Chains.Num() > 0;
	}

	return false;
}

void SIKRetargetChainMapList::RefreshView()
{
	UIKRetargeterController* AssetController = GetRetargetController();
	if (!AssetController)
	{
		return; 
	}

	auto FilterString = [this](const FString& StringToTest) ->bool
	{
		return TextFilter->TestTextFilter(FBasicStringFilterExpressionContext(StringToTest));
	};

	auto DoesChainHaveIK = [AssetController](const FName TargetChainName) ->bool
	{
		const UIKRigDefinition* IKRig = AssetController->GetIKRig(ERetargetSourceOrTarget::Target);
		if (!IKRig)
		{
			return false;
		}

		const FBoneChain* Chain = IKRig->GetRetargetChainByName(TargetChainName);
		if (!Chain)
		{
			return false;
		}
		
		return Chain->IKGoalName != NAME_None;
	};

	const TArray<FName>& SelectedChains = EditorController.Pin()->GetSelectedChains();
	const FName LiteralNone = FName("None");

	// refresh items
	ListViewItems.Reset();
	const TArray<TObjectPtr<URetargetChainSettings>>& ChainMappings = AssetController->GetChainMappings();
	for (const TObjectPtr<URetargetChainSettings> ChainMap : ChainMappings)
	{
		// apply text filter to items
		if (!(TextFilter->GetFilterText().IsEmpty() ||
			FilterString(ChainMap->SourceChain.ToString()) ||
			FilterString(ChainMap->TargetChain.ToString())))
		{
			continue;
		}
		
		// apply "only IK" filter
		if (ChainFilterOptions.bHideChainsWithoutIK && !DoesChainHaveIK(ChainMap->TargetChain))
		{
			continue;
		}

		// apply "hide mapped chains" filter
		if (ChainFilterOptions.bHideMappedChains && ChainMap->SourceChain != LiteralNone)
		{
			continue;
		}
		
		// apply "hide un-mapped chains" filter
		if (ChainFilterOptions.bHideUnmappedChains && ChainMap->SourceChain == LiteralNone)
		{
			continue;
		}
		
		// create an item for this chain
		TSharedPtr<FRetargetChainMapElement> ChainItem = FRetargetChainMapElement::Make(ChainMap);
		ListViewItems.Add(ChainItem);

		// select/deselect it
		const bool bIsSelected = SelectedChains.Contains(ChainMap->TargetChain);
		ListView->SetItemSelection(ChainItem, bIsSelected, ESelectInfo::Direct);
	}
	
	ListView->RequestListRefresh();
}

TSharedRef<SWidget> SIKRetargetChainMapList::CreateFilterMenuWidget()
{
	const FUIAction FilterHideMappedAction = FUIAction(
		FExecuteAction::CreateLambda([this]
		{
			ChainFilterOptions.bHideMappedChains = !ChainFilterOptions.bHideMappedChains;
			RefreshView();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return ChainFilterOptions.bHideMappedChains;
		}));

	const FUIAction FilterOnlyUnMappedAction = FUIAction(
		FExecuteAction::CreateLambda([this]
		{
			ChainFilterOptions.bHideUnmappedChains = !ChainFilterOptions.bHideUnmappedChains;
			RefreshView();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return ChainFilterOptions.bHideUnmappedChains;
		}));

	const FUIAction FilterIKChainAction = FUIAction(
		FExecuteAction::CreateLambda([this]
		{
			ChainFilterOptions.bHideChainsWithoutIK = !ChainFilterOptions.bHideChainsWithoutIK;
			RefreshView();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return ChainFilterOptions.bHideChainsWithoutIK;
		}));
	
	static constexpr bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder(CloseAfterSelection, CommandList);

	MenuBuilder.BeginSection("Chain Map Filters", LOCTEXT("ChainMapFiltersSection", "Filter Chain Mappings"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("HideMappedLabel", "Hide Mapped Chains"),
		LOCTEXT("HideMappedTooltip", "Hide chains mapped to a source chain."),
		FSlateIcon(),
		FilterHideMappedAction,
		NAME_None,
		EUserInterfaceActionType::Check);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("HideUnMappedLabel", "Hide Unmapped Chains"),
		LOCTEXT("HideUnMappedTooltip", "Hide chains not mapped to a source chain."),
		FSlateIcon(),
		FilterOnlyUnMappedAction,
		NAME_None,
		EUserInterfaceActionType::Check);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("HideNonIKLabel", "Hide Chains Without IK"),
		LOCTEXT("HideNonIKTooltip", "Hide chains not using IK."),
		FSlateIcon(),
		FilterIKChainAction,
		NAME_None,
		EUserInterfaceActionType::Check);
	
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Clear", LOCTEXT("ClearMapFiltersSection", "Clear"));
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClearMapFilterLabel", "Clear Filters"),
		LOCTEXT("ClearMapFilterTooltip", "Clear all filters to show all chain mappings."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]
		{
			ChainFilterOptions = FChainMapFilterOptions();
			RefreshView();
		})));

	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

void SIKRetargetChainMapList::OnFilterTextChanged(const FText& SearchText)
{
	TextFilter->SetFilterText(SearchText);
	RefreshView();
}

TSharedRef<ITableRow> SIKRetargetChainMapList::MakeListRowWidget(
	TSharedPtr<FRetargetChainMapElement> InElement,
    const TSharedRef<STableViewBase>& OwnerTable)
{
	return InElement->MakeListRowWidget(
		OwnerTable,
		InElement.ToSharedRef(),
		SharedThis(this));
}

void SIKRetargetChainMapList::OnItemClicked(TSharedPtr<FRetargetChainMapElement> InItem) const 
{
	// get list of selected chains
	TArray<FName> SelectedChains;
	TArray<TSharedPtr<FRetargetChainMapElement>> SelectedItems = ListView.Get()->GetSelectedItems();
	for (const TSharedPtr<FRetargetChainMapElement>& Item : SelectedItems)
	{
		SelectedChains.Add(Item.Get()->ChainMap->TargetChain);
	}
	
	// replace the chain selection
	constexpr bool bEditFromChainsView = true;
	EditorController.Pin()->EditChainSelection(SelectedChains, ESelectionEdit::Replace, bEditFromChainsView);
}

EVisibility SIKRetargetChainMapList::IsAutoMapButtonVisible() const
{
	return IsChainMapEnabled() ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SIKRetargetChainMapList::OnAutoMapButtonClicked() const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return FReply::Unhandled();
	}
	
	UIKRetargeterController* RetargeterController = GetRetargetController();
	if (!RetargeterController)
	{
		return FReply::Unhandled();
	}

	Controller->ClearOutputLog();
	RetargeterController->CleanChainMapping();
	RetargeterController->AutoMapChains();
	return FReply::Handled();
}

FReply SIKRetargetChainMapList::OnGlobalSettingsButtonClicked() const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return FReply::Unhandled();
	}

	UIKRetargeterController* RetargeterController = GetRetargetController();
	if (!RetargeterController)
	{
		return FReply::Unhandled();
	}

	UIKRetargetGlobalSettings* GlobalSettings = Controller->AssetController->GetAsset()->GetGlobalSettingsUObject();
	if (GlobalSettings->EditorController != Controller)
	{
		GlobalSettings->EditorController = Controller;
	}
	
	Controller->SetDetailsObject(GlobalSettings);
	
	return FReply::Handled();
}

FReply SIKRetargetChainMapList::OnRootSettingsButtonClicked() const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return FReply::Unhandled();
	}

	UIKRetargeterController* RetargeterController = GetRetargetController();
	if (!RetargeterController)
	{
		return FReply::Unhandled();
	}
	
	Controller->SetRootSelected(true);
	return FReply::Handled();
}



#undef LOCTEXT_NAMESPACE
