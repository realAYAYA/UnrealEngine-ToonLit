// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SNiagaraHierarchy.h"
#include "Framework/Commands/GenericCommands.h"
#include "Styling/StyleColors.h"
#include "Styling/AppStyle.h"
#include "ViewModels/HierarchyEditor/NiagaraHierarchyViewModelBase.h"
#include "Widgets/SNiagaraParameterName.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "NiagaraHierarchy"

void SNiagaraHierarchyCategory::Construct(const FArguments& InArgs,	TSharedPtr<FNiagaraHierarchyCategoryViewModel> InCategory)
{
	Category = InCategory;

	InCategory->GetOnRequestRename().BindSP(this, &SNiagaraHierarchyCategory::EnterEditingMode);

	const UNiagaraHierarchyCategory* CategoryData = InCategory->GetData<UNiagaraHierarchyCategory>();
	SetToolTipText(TAttribute<FText>::CreateLambda([CategoryData]
	{
		return CategoryData->GetTooltip();
	}));
	
	ChildSlot
	[
		SAssignNew(InlineEditableTextBlock, SInlineEditableTextBlock)
		.Style(&FNiagaraEditorStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("NiagaraEditor.HierarchyEditor.Category"))
		.Text(this, &SNiagaraHierarchyCategory::GetCategoryText)
		.OnTextCommitted(this, &SNiagaraHierarchyCategory::OnRenameCategory)
		.OnVerifyTextChanged(this, &SNiagaraHierarchyCategory::OnVerifyCategoryRename)
		.IsSelected(InArgs._IsSelected)
	];
}

void SNiagaraHierarchyCategory::EnterEditingMode() const
{
	InlineEditableTextBlock->EnterEditingMode();
}

bool SNiagaraHierarchyCategory::OnVerifyCategoryRename(const FText& NewName, FText& OutTooltip) const
{
	TArray<TSharedPtr<FNiagaraHierarchyCategoryViewModel>> Categories;
	Category.Pin()->GetParent().Pin()->GetChildrenViewModelsForType<UNiagaraHierarchyCategory, FNiagaraHierarchyCategoryViewModel>(Categories);

	if(GetCategoryText().ToString() != NewName.ToString())
	{
		TSet<FString> CategoryNames;
		for(const auto& CategoryViewModel : Categories)
		{
			CategoryNames.Add(Cast<UNiagaraHierarchyCategory>(CategoryViewModel->GetDataMutable())->GetCategoryName().ToString());
		}

		if(CategoryNames.Contains(NewName.ToString()))
		{
			OutTooltip = LOCTEXT("HierarchyCategoryCantRename_DuplicateOnLayer", "Another category of the same name already exists on the same hierarchy level!");
			return false;
		}
	}

	return true;
}

FText SNiagaraHierarchyCategory::GetCategoryText() const
{
	return FText::FromName(Cast<UNiagaraHierarchyCategory>(Category.Pin()->GetData())->GetCategoryName());
}

void SNiagaraHierarchyCategory::OnRenameCategory(const FText& NewText, ETextCommit::Type) const
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_Rename_Category", "Renamed hierarchy category"));
	Category.Pin()->GetHierarchyViewModel()->GetHierarchyDataRoot()->Modify();
	
	Cast<UNiagaraHierarchyCategory>(Category.Pin()->GetDataMutable())->SetCategoryName(FName(NewText.ToString()));
}

void SNiagaraHierarchySection::Construct(const FArguments& InArgs, TSharedPtr<FNiagaraHierarchySectionViewModel> InSection, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel)
{
	SectionViewModel = InSection;
	HierarchyViewModel = InHierarchyViewModel;

	OnSectionActivatedDelegate = InArgs._OnSectionActivated;
	
	if(SectionViewModel != nullptr)
	{
		SectionViewModel->GetOnRequestRename().BindSP(this, &SNiagaraHierarchySection::EnterEditingMode);

		SDropTarget::FArguments LeftDropTargetArgs;
		SDropTarget::FArguments OntoDropTargetArgs;
		SDropTarget::FArguments RightDropTargetArgs;

		LeftDropTargetArgs
			.OnAllowDrop(this, &SNiagaraHierarchySection::OnCanAcceptDrop, EItemDropZone::AboveItem)
			.OnDropped(this, &SNiagaraHierarchySection::OnDroppedOn, EItemDropZone::AboveItem)
			.VerticalImage(FAppStyle::GetNoBrush())
			.HorizontalImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.DropTarget.BorderHorizontal"));

		OntoDropTargetArgs
			.OnAllowDrop(this, &SNiagaraHierarchySection::OnCanAcceptDrop, EItemDropZone::OntoItem)
			.OnDropped(this, &SNiagaraHierarchySection::OnDroppedOn, EItemDropZone::OntoItem);
		
		RightDropTargetArgs
			.OnAllowDrop(this, &SNiagaraHierarchySection::OnCanAcceptDrop, EItemDropZone::BelowItem)
			.OnDropped(this, &SNiagaraHierarchySection::OnDroppedOn, EItemDropZone::BelowItem)
			.VerticalImage(FAppStyle::GetNoBrush())
			.HorizontalImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.DropTarget.BorderHorizontal"));

		SetToolTipText(TAttribute<FText>::CreateSP(this, &SNiagaraHierarchySection::GetTooltipText));

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SNiagaraSectionDragDropTarget)
				.DropTargetArgs(LeftDropTargetArgs)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SNiagaraSectionDragDropTarget)
				.DropTargetArgs(OntoDropTargetArgs
				.Content()
				[
					SAssignNew(MenuAnchor, SMenuAnchor)
					.OnGetMenuContent(this, &SNiagaraHierarchySection::OnGetMenuContent)
					[
						SNew(SCheckBox)
						.Visibility(EVisibility::HitTestInvisible)
						.Style(FAppStyle::Get(), "DetailsView.SectionButton")
						.OnCheckStateChanged(this, &SNiagaraHierarchySection::OnSectionCheckChanged)
						.IsChecked(this, &SNiagaraHierarchySection::GetSectionCheckState)
						.Padding(FMargin(8.f, 4.f))
						[
							SAssignNew(InlineEditableTextBlock, SInlineEditableTextBlock)
							.IsSelected(InArgs._IsSelected)
							.Text(this, &SNiagaraHierarchySection::GetText)
							.OnTextCommitted(this, &SNiagaraHierarchySection::OnRenameSection)
							.OnVerifyTextChanged(this, &SNiagaraHierarchySection::OnVerifySectionRename)
						]
					]
				])
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SNiagaraSectionDragDropTarget)
				.DropTargetArgs(RightDropTargetArgs)				
			]
		];		
	}
	// if this section doesn't represent data, it's the "All" widget
	else
	{
		ChildSlot
		[
			SNew(SDropTarget)
			.OnAllowDrop(this, &SNiagaraHierarchySection::OnCanAcceptDrop, EItemDropZone::OntoItem)
			.OnDropped(this, &SNiagaraHierarchySection::OnDroppedOn, EItemDropZone::OntoItem)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.OnCheckStateChanged(this, &SNiagaraHierarchySection::OnSectionCheckChanged)
				.IsChecked(this, &SNiagaraHierarchySection::GetSectionCheckState)
				.Padding(FMargin(8.f, 4.f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AllSection", "All"))
				]
			]			
		];
	}
}

SNiagaraHierarchySection::~SNiagaraHierarchySection()
{
	SectionViewModel.Reset();
}

void SNiagaraHierarchySection::EnterEditingMode() const
{
	InlineEditableTextBlock->EnterEditingMode();
}

bool SNiagaraHierarchySection::OnCanAcceptDrop(TSharedPtr<FDragDropOperation> DragDropOperation, EItemDropZone ItemDropZone) const
{
	if(SectionViewModel.IsValid())
	{
		return SectionViewModel->OnCanAcceptDropInternal(DragDropOperation, ItemDropZone).IsSet();
	}
	// for the All section which has no valid view model, we simply do a check if the sections of the dragged categories are different
	else
	{
		if(DragDropOperation->IsOfType<FNiagaraHierarchyDragDropOp>())
		{
			TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = StaticCastSharedPtr<FNiagaraHierarchyDragDropOp>(DragDropOperation);
			TWeakPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem = HierarchyDragDropOp->GetDraggedItem();

			if(const UNiagaraHierarchyCategory* Category = Cast<UNiagaraHierarchyCategory>(DraggedItem.Pin()->GetData()))
			{
				return TryGetSectionData() != Category->GetSection();
			}
		}
	}

	return false;
}

FReply SNiagaraHierarchySection::OnDroppedOn(const FGeometry&, const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const
{
	if(SectionViewModel.IsValid())
	{
		return SectionViewModel->OnDroppedOn(DragDropEvent, DropZone, SectionViewModel);
	}
	else
	{
		if(TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FNiagaraHierarchyDragDropOp>())
		{
			if(UNiagaraHierarchyCategory* Category = Cast<UNiagaraHierarchyCategory>(HierarchyDragDropOp->GetDraggedItem().Pin()->GetDataMutable()))
			{
				Category->SetSection(nullptr);
				TArray<UNiagaraHierarchyCategory*> ChildrenCategories;
				Category->GetChildrenOfType<>(ChildrenCategories, true);

				for(UNiagaraHierarchyCategory* ChildCategory : ChildrenCategories)
				{
					ChildCategory->SetSection(nullptr);
				}

				// we only need to reparent if the parent isn't already the root. This stops unnecessary reordering
				if(HierarchyDragDropOp->GetDraggedItem().Pin()->GetParent() != HierarchyViewModel->GetHierarchyViewModelRoot())
				{
					HierarchyViewModel->GetHierarchyViewModelRoot()->ReparentToThis(HierarchyDragDropOp->GetDraggedItem().Pin());
				}
			
				HierarchyViewModel->RefreshHierarchyView();

				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

FReply SNiagaraHierarchySection::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(SectionViewModel.IsValid())
	{
		// we handle the event here so we can react on mouse button up
		if(MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
		{
			return FReply::Handled();
		}
		else if(MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			OnSectionActivatedDelegate.ExecuteIfBound(SectionViewModel);
			return FReply::Handled().DetectDrag(AsShared(), EKeys::LeftMouseButton).SetUserFocus(AsShared());	
		}
	}

	return FReply::Unhandled();
}

FReply SNiagaraHierarchySection::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(SectionViewModel.IsValid())
	{
		if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			MenuAnchor->SetIsOpen(true);
			OnSectionActivatedDelegate.ExecuteIfBound(SectionViewModel);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SNiagaraHierarchySection::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && SectionViewModel.IsValid())
	{
		TSharedRef<FNiagaraSectionDragDropOp> SectionDragDropOp = MakeShared<FNiagaraSectionDragDropOp>(SectionViewModel);
		SectionDragDropOp->Construct();
		return FReply::Handled().BeginDragDrop(SectionDragDropOp);
	}
	
	return FReply::Unhandled();
}

void SNiagaraHierarchySection::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	if(TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FNiagaraHierarchyDragDropOp>())
	{
		HierarchyDragDropOp->SetDescription(FText::GetEmpty());
	}
}

TSharedRef<SWidget> SNiagaraHierarchySection::OnGetMenuContent() const
{
	FMenuBuilder MenuBuilder(true, HierarchyViewModel->GetCommands());
	
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);

	return MenuBuilder.MakeWidget();
}

UNiagaraHierarchySection* SNiagaraHierarchySection::TryGetSectionData() const
{
	return SectionViewModel.IsValid() ? Cast<UNiagaraHierarchySection>(SectionViewModel->GetDataMutable()) : nullptr;
}

FText SNiagaraHierarchySection::GetText() const
{
	return SectionViewModel->GetSectionNameAsText();
}

FText SNiagaraHierarchySection::GetTooltipText() const
{
	return SectionViewModel->GetSectionTooltip();
}

void SNiagaraHierarchySection::OnRenameSection(const FText& Text, ETextCommit::Type CommitType) const
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_Rename_Section", "Renamed hierarchy section"));
	HierarchyViewModel->GetHierarchyDataRoot()->Modify();
	
	SectionViewModel->SetSectionNameAsText(Text);
}

bool SNiagaraHierarchySection::OnVerifySectionRename(const FText& NewName, FText& OutTooltip) const
{
	// this function shouldn't be used in case the section isn't valid but we'll make sure regardless
	if(!SectionViewModel.IsValid())
	{
		return false;
	}

	if(SectionViewModel->GetSectionName().ToString() != NewName.ToString())
	{
		TArray<FString> SectionNames;

		SectionNames.Add("All");
		for(auto& Section : HierarchyViewModel->GetHierarchyViewModelRoot()->GetSectionViewModels())
		{
			SectionNames.Add(Section->GetSectionName().ToString());
		}

		if(SectionNames.Contains(NewName.ToString()))
		{
			OutTooltip = LOCTEXT("HierarchySectionCantRename_Duplicate", "A section with that name already exists!");
			return false;
		}
	}

	return true;
}

ECheckBoxState SNiagaraHierarchySection::GetSectionCheckState() const
{
	return HierarchyViewModel->GetActiveSection() == SectionViewModel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNiagaraHierarchySection::OnSectionCheckChanged(ECheckBoxState NewState)
{
	OnSectionActivatedDelegate.ExecuteIfBound(SectionViewModel);
}

void SNiagaraHierarchy::Construct(const FArguments& InArgs, TObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel)
{
	HierarchyViewModel = InHierarchyViewModel;
	
	HierarchyViewModel->OnRefreshSourceView().BindSP(this, &SNiagaraHierarchy::RefreshSourceView);
	HierarchyViewModel->OnRefreshHierarchyView().BindSP(this, &SNiagaraHierarchy::RefreshHierarchyView);
	HierarchyViewModel->OnRefreshSections().BindSP(this, &SNiagaraHierarchy::RefreshSectionsWidget);

	HierarchyViewModel->OnSectionActivated().AddSP(this, &SNiagaraHierarchy::OnSectionActivated);
	HierarchyViewModel->OnItemAdded().AddSP(this, &SNiagaraHierarchy::OnItemAdded);
	
	OnGenerateRowContentWidget = InArgs._OnGenerateRowContentWidget;
	OnGenerateCustomDetailsPanelNameWidget = InArgs._OnGenerateCustomDetailsPanelNameWidget;
	
	SHorizontalBox::FSlot* DetailsPanelSlot = nullptr;

	LightBackgroundBrush = FSlateColorBrush(FStyleColors::Panel);
	RecessedBackgroundBrush = FSlateColorBrush(FStyleColors::Recessed);

	TSharedRef<SWidget> AddSectionButton = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.OnPressed_UObject(HierarchyViewModel.Get(), &UNiagaraHierarchyViewModelBase::AddSection)
			.ButtonStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.HierarchyEditor.ButtonStyle")
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[			
					SNew(STextBlock)
					.Text(LOCTEXT("AddSectionLabel","Add Section"))				
				]
			]
		];

	TSharedRef<SWidget> AddCategoryButton = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.OnPressed_UObject(HierarchyViewModel.Get(), &UNiagaraHierarchyViewModelBase::AddCategory)
			.ButtonStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.HierarchyEditor.ButtonStyle")
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[			
					SNew(STextBlock)
					.Text(LOCTEXT("AddCategoryLabel","Add Category"))				
				]
			]
		];
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&RecessedBackgroundBrush)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SAssignNew(SourceTreeView, STreeView<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>)
				.TreeItemsSource(&InHierarchyViewModel->GetSourceItems())
				.OnSelectionChanged(this, &SNiagaraHierarchy::OnSelectionChanged, false)
				.OnGenerateRow(this, &SNiagaraHierarchy::GenerateSourceItemRow)
				.OnGetChildren_UObject(InHierarchyViewModel.Get(), &UNiagaraHierarchyViewModelBase::OnGetChildren)
				.OnItemToString_Debug_UObject(InHierarchyViewModel.Get(), &UNiagaraHierarchyViewModelBase::OnItemToStringDebug)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSeparator).Orientation(EOrientation::Orient_Vertical)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.Padding(0.f)
						.BorderImage(&LightBackgroundBrush)
						[
							AddSectionButton
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(1.f)
					[
						SAssignNew(SectionBox, SWrapBox)
						.UseAllottedWidth(true)
					]				
				]
				+ SVerticalBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(&LightBackgroundBrush)
						.Padding(0.f)
						[
							AddCategoryButton
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(HierarchyTreeView, STreeView<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>)
						.TreeItemsSource(&InHierarchyViewModel->GetHierarchyItems())
						.OnSelectionChanged(this, &SNiagaraHierarchy::OnSelectionChanged, true)
						.OnGenerateRow(this, &SNiagaraHierarchy::GenerateHierarchyItemRow)
						.OnGetChildren_UObject(InHierarchyViewModel.Get(), &UNiagaraHierarchyViewModelBase::OnGetChildren)
						.OnItemToString_Debug_UObject(InHierarchyViewModel.Get(), &UNiagaraHierarchyViewModelBase::OnItemToStringDebug)
					]
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					[
						SNew(SDropTarget)
						.OnDropped(this, &SNiagaraHierarchy::HandleHierarchyRootDrop)
						.OnAllowDrop(this, &SNiagaraHierarchy::CanDropOnRoot)
					]
				]
			]
			+ SHorizontalBox::Slot().Expose(DetailsPanelSlot)		
		]
	];

	if(InHierarchyViewModel->SupportsDetailsPanel())
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::ObjectsUseNameArea;
		DetailsViewArgs.bShowObjectLabel = false;

		DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		
		if(OnGenerateCustomDetailsPanelNameWidget.IsBound())
		{
			TSharedRef<SWidget> CustomDetailsPanelNameWidget = OnGenerateCustomDetailsPanelNameWidget.Execute(nullptr);
			DetailsPanel->SetNameAreaCustomContent(CustomDetailsPanelNameWidget);
		}		
		
		for(auto& Customizations : HierarchyViewModel->GetInstanceCustomizations())
		{
			DetailsPanel->RegisterInstancedCustomPropertyLayout(Customizations.Key, Customizations.Value);
		}
		
		DetailsPanelSlot->AttachWidget(DetailsPanel.ToSharedRef());
	}
	
	RefreshSectionsWidget();
	HierarchyViewModel->GetHierarchyViewModelRoot()->GetOnSynced().AddSP(this, &SNiagaraHierarchy::RefreshSectionsWidget);
	
	HierarchyViewModel->GetCommands()->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SNiagaraHierarchy::RequestRenameSelectedItem),
		FCanExecuteAction::CreateSP(this, &SNiagaraHierarchy::CanRequestRenameSelectedItem));

	HierarchyViewModel->GetCommands()->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SNiagaraHierarchy::DeleteSelectedItems),
		FCanExecuteAction::CreateSP(this, &SNiagaraHierarchy::CanDeleteSelectedItems));

	HierarchyViewModel->ForceFullRefresh();
}

SNiagaraHierarchy::~SNiagaraHierarchy()
{
	HierarchyViewModel->GetHierarchyViewModelRoot()->GetOnSynced().RemoveAll(this);

	HierarchyViewModel->OnRefreshSourceView().Unbind();
	HierarchyViewModel->OnRefreshHierarchyView().Unbind();
	HierarchyViewModel->OnRefreshSections().Unbind();

	HierarchyViewModel->OnSectionActivated().RemoveAll(this);
	HierarchyViewModel->OnItemAdded().RemoveAll(this);
	
	HierarchyViewModel->GetCommands()->UnmapAction(FGenericCommands::Get().Delete);
	HierarchyViewModel->GetCommands()->UnmapAction(FGenericCommands::Get().Rename);
}

FReply SNiagaraHierarchy::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return HierarchyViewModel->GetCommands()->ProcessCommandBindings(InKeyEvent) ? FReply::Handled() : FReply::Unhandled();
}

FReply SNiagaraHierarchy::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// we catch any mouse button down event so that we can continue using our commands
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse, true);
}

FReply SNiagaraHierarchy::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse, true);
}

void SNiagaraHierarchy::RefreshSourceView(bool bFullRefresh) const
{
	if(bFullRefresh)
	{
		SourceTreeView->RebuildList();
	}
	else
	{
		SourceTreeView->RequestTreeRefresh();
	}
}

void SNiagaraHierarchy::RefreshHierarchyView(bool bFullRefresh) const
{
	// the top layer objects might have changed due to filtering. We need to refresh these too.
	HierarchyTreeView->SetTreeItemsSource(&HierarchyViewModel->GetHierarchyItems());
	if(bFullRefresh)
	{
		HierarchyTreeView->RebuildList();
	}
	else
	{
		HierarchyTreeView->RequestTreeRefresh();
	}
}

void SNiagaraHierarchy::RefreshSectionsWidget()
{
	SectionBox->ClearChildren();
	SectionsWidgetMap.Empty();
	
	for (TSharedPtr<FNiagaraHierarchySectionViewModel>& Section : HierarchyViewModel->GetHierarchyViewModelRoot()->GetSectionViewModels())
	{
		TSharedPtr<SNiagaraHierarchySection> SectionWidget = SNew(SNiagaraHierarchySection, Section, HierarchyViewModel)
		.OnSectionActivated_Lambda([this](TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel)
		{
			HierarchyViewModel->SetActiveSection(SectionViewModel);
		});
		SectionsWidgetMap.Add(Section, SectionWidget);
		
		SectionBox->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SectionWidget.ToSharedRef()		
		];
	}
	
	TSharedPtr<SNiagaraHierarchySection> DefaultSectionWidget = SNew(SNiagaraHierarchySection, nullptr, HierarchyViewModel)
	.OnSectionActivated_Lambda([this](TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel)
	{
		HierarchyViewModel->SetActiveSection(SectionViewModel);
	});
	
	SectionsWidgetMap.Add(nullptr, DefaultSectionWidget);

	SectionBox->AddSlot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		DefaultSectionWidget.ToSharedRef()
	];
}

bool SNiagaraHierarchy::IsItemSelected(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) const
{
	return HierarchyTreeView->IsItemSelected(Item);
}

void SNiagaraHierarchy::SelectObjectInDetailsPanel(UObject* Object) const
{
	if(DetailsPanel.IsValid())
	{
		DetailsPanel->SetObject(Object);
	}
}

TSharedRef<ITableRow> SNiagaraHierarchy::GenerateSourceItemRow(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase)
{
	return SNew(STableRow<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>, TableViewBase)
	.OnDragDetected(HierarchyItem.ToSharedRef(), &FNiagaraHierarchyItemViewModelBase::OnDragDetected, true)
	[
		OnGenerateRowContentWidget.Execute(HierarchyItem.ToSharedRef())
	];
}

TSharedRef<ITableRow> SNiagaraHierarchy::GenerateHierarchyItemRow(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase)
{
	return SNew(STableRow<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>, TableViewBase)
	.OnAcceptDrop(HierarchyItem.ToSharedRef(), &FNiagaraHierarchyItemViewModelBase::OnDroppedOn)
	.OnCanAcceptDrop(HierarchyItem.ToSharedRef(), &FNiagaraHierarchyItemViewModelBase::OnCanAcceptDrop)
	.OnDragDetected(HierarchyItem.ToSharedRef(), &FNiagaraHierarchyItemViewModelBase::OnDragDetected, false)
	[
		OnGenerateRowContentWidget.Execute(HierarchyItem.ToSharedRef())
	];
}

void SNiagaraHierarchy::RequestRenameSelectedItem()
{
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> SelectedItems = HierarchyTreeView->GetSelectedItems();

	if(SelectedItems.Num() == 0)
	{
		TSharedPtr<FNiagaraHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveSection();
		if(ActiveSection)
		{
			SelectedItems = { ActiveSection };
		}
	}
	
	if(SelectedItems.Num() == 1)
	{
		SelectedItems[0]->RequestRename();
	}
}

bool SNiagaraHierarchy::CanRequestRenameSelectedItem() const
{
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> SelectedItems = HierarchyTreeView->GetSelectedItems();

	if(SelectedItems.Num() == 0)
	{
		TSharedPtr<FNiagaraHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveSection();
		if(ActiveSection)
		{
			SelectedItems = { ActiveSection };
		}
	}
	
	if(SelectedItems.Num() == 1)
	{
		return SelectedItems[0]->CanRename();
	}

	return false;
}

void SNiagaraHierarchy::DeleteSelectedItems()
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_DeleteSelectedItems", "Deleted selected hierarchy items"));
	HierarchyViewModel->GetHierarchyDataRoot()->Modify();
	
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> SelectedItems = HierarchyTreeView->GetSelectedItems();

	if(SelectedItems.Num() == 0)
	{
		TSharedPtr<FNiagaraHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveSection();
		if(ActiveSection)
		{
			SelectedItems = { ActiveSection };
		}
	}
	
	for(TSharedPtr<FNiagaraHierarchyItemViewModelBase>& SelectedItem : SelectedItems)
	{
		SelectedItem->Finalize();
		if(SelectedItem->GetParent().IsValid())
		{
			SelectedItem->GetParent().Pin()->SyncToData();
		}
	}

	RefreshSourceView();
	RefreshHierarchyView();
	HierarchyViewModel->OnHierarchyChanged().Broadcast();
}

bool SNiagaraHierarchy::CanDeleteSelectedItems() const
{
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> SelectedItems = HierarchyTreeView->GetSelectedItems();
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> SelectedItemsInSource = SourceTreeView->GetSelectedItems();

	// we only want to allow inferring the intent to delete a section if _nothing_ else is selected
	if(SelectedItems.Num() == 0 && SelectedItemsInSource.Num() == 0)
	{
		TSharedPtr<FNiagaraHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveSection();
		if(ActiveSection)
		{
			SelectedItems = { ActiveSection };
		}
	}

	if(SelectedItems.Num() > 0)
	{
		bool bCanDelete = true;
		for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> SelectedItem : SelectedItems)
		{
			bCanDelete &= SelectedItems[0]->CanDelete();
		}

		return bCanDelete;
	}

	return false;
}

void SNiagaraHierarchy::OnItemAdded(TSharedPtr<FNiagaraHierarchyItemViewModelBase> AddedItem)
{
	// when a new item is created (opposed to dragged & dropped from source view, i.e. only categories so far)
	// we make sure to request a tree refresh, select the row, and request a pending rename since the widget will created a frame later
	if(AddedItem->GetData()->IsA<UNiagaraHierarchyItem>() || AddedItem->GetData()->IsA<UNiagaraHierarchyCategory>())
	{
		HierarchyTreeView->RequestTreeRefresh();
		HierarchyTreeView->SetSelection(AddedItem);
	}
	else if(AddedItem->GetData()->IsA<UNiagaraHierarchySection>())
	{
		RefreshSectionsWidget();
	}

	AddedItem->RequestRenamePending();
}

void SNiagaraHierarchy::OnSectionActivated(TSharedPtr<FNiagaraHierarchySectionViewModel> Section) const
{
	OnSelectionChanged(Section, ESelectInfo::Direct, true);
}

void SNiagaraHierarchy::OnSelectionChanged(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, ESelectInfo::Type Type, bool bFromHierarchy) const
{
	if(DetailsPanel.IsValid())
	{
		if(HierarchyItem.IsValid())
		{
			// when se select a section, and the previous item selection is no longer available due to it, we would get a selection refresh next tick
			// to wipe out the current selection. We want to avoid that, so we manually clear the selected items in that case.
			if(HierarchyItem->GetData()->IsA<UNiagaraHierarchySection>())
			{
				HierarchyTreeView->ClearSelection();
			}

			// we clear the selection of the other tree view to avoid 
			if(bFromHierarchy)
			{
				SourceTreeView->ClearSelection();
			}
			else
			{
				HierarchyTreeView->ClearSelection();
			}
			
			UObject* DataForEditing = HierarchyItem->GetDataForEditing();
			DataForEditing->SetFlags(RF_Transactional);

			// we make sure the object we are editing is transactional
			DetailsPanel->SetObject(DataForEditing);	
		}
		else
		{
			DetailsPanel->SetObject(nullptr);
		}
	}
	
	if(DetailsPanel.IsValid() && OnGenerateCustomDetailsPanelNameWidget.IsBound() && HierarchyItem.IsValid())
	{
		TSharedRef<SWidget> NameWidget = OnGenerateCustomDetailsPanelNameWidget.Execute(HierarchyItem);
		DetailsPanel->SetNameAreaCustomContent(NameWidget);
	}

	HierarchyViewModel->OnSelectionChanged(HierarchyItem);
}

FReply SNiagaraHierarchy::HandleHierarchyRootDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent) const
{
	// we pass in nullptr because the dragged object is contained within the drag drop event
	return HierarchyViewModel->GetHierarchyViewModelRoot()->OnDroppedOn(DragDropEvent, EItemDropZone::OntoItem, nullptr);
}

bool SNiagaraHierarchy::CanDropOnRoot(TSharedPtr<FDragDropOperation> DragDropOp) const
{
	if(DragDropOp->IsOfType<FNiagaraHierarchyDragDropOp>())
	{
		TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = StaticCastSharedPtr<FNiagaraHierarchyDragDropOp>(DragDropOp);
		TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem = HierarchyDragDropOp->GetDraggedItem().Pin();
		TOptional<EItemDropZone> Result = HierarchyViewModel->GetHierarchyViewModelRoot()->OnCanAcceptDropInternal(DragDropOp, EItemDropZone::OntoItem);
		return Result.IsSet() && Result.GetValue() == EItemDropZone::OntoItem ? true : false;
	}

	return false;
}

void SNiagaraSectionDragDropTarget::Construct(const FArguments& InArgs)
{
	SDropTarget::Construct(InArgs._DropTargetArgs);
}

FReply SNiagaraSectionDragDropTarget::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FNiagaraHierarchyDragDropOp>();

	if(HierarchyDragDropOp.IsValid())
	{
		if(AllowDrop(DragDropEvent.GetOperation()))
		{
			if(const UNiagaraHierarchySection* Section = Cast<UNiagaraHierarchySection>(HierarchyDragDropOp->GetDraggedItem().Pin()->GetData()))
			{
				FText Text = LOCTEXT("MoveSectionHere", "The section will be moved here.");
				HierarchyDragDropOp->SetDescription(Text);
			}

			if(const UNiagaraHierarchyCategory* Category = Cast<UNiagaraHierarchyCategory>(HierarchyDragDropOp->GetDraggedItem().Pin()->GetData()))
			{
				FText Text = LOCTEXT("DropCategoryOnSection", "Move category here");
				HierarchyDragDropOp->SetDescription(Text);
			}
		}
		else
		{
			if(const UNiagaraHierarchySection* Section = Cast<UNiagaraHierarchySection>(HierarchyDragDropOp->GetDraggedItem().Pin()->GetData()))
			{
				HierarchyDragDropOp->SetDescription(LOCTEXT("SectionOnSectionNotAllowed", "Can't drop section on a section."));
			}
		}
	}

	return SDropTarget::OnDragOver(MyGeometry, DragDropEvent);
}

void SNiagaraSectionDragDropTarget::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FNiagaraHierarchyDragDropOp>();

	if(HierarchyDragDropOp.IsValid())
	{
		HierarchyDragDropOp->SetDescription(FText::GetEmpty());
	}
	
	SDropTarget::OnDragLeave(DragDropEvent);
}

#undef LOCTEXT_NAMESPACE
