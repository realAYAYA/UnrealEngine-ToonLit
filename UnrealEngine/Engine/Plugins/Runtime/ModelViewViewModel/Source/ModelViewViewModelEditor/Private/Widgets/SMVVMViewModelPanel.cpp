// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMViewModelPanel.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "IStructureDetailsView.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "PropertyEditorModule.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"

#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/MVVMEditorStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/PropertyViewer/SFieldIcon.h"
#include "Widgets/PropertyViewer/SFieldName.h"
#include "Widgets/PropertyViewer/SPropertyViewer.h"
#include "Widgets/SMVVMSelectViewModel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "SPositiveActionButton.h"

#define LOCTEXT_NAMESPACE "ViewModelPanel"

namespace UE::MVVM
{

void SMVVMViewModelPanel::Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor)
{
	UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor->GetWidgetBlueprintObj();
	check(WidgetBlueprint);
	UMVVMBlueprintView* CurrentBlueprintView = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetView(WidgetBlueprint);

	WeakBlueprintEditor = WidgetBlueprintEditor;
	WeakBlueprintView = CurrentBlueprintView;
	FieldIterator = MakeUnique<FFieldIterator_Bindable>(WidgetBlueprint, EFieldVisibility::Notify);

	if (CurrentBlueprintView)
	{
		// Listen to when the viewmodel are modified
		ViewModelsUpdatedHandle = CurrentBlueprintView->OnViewModelsUpdated.AddSP(this, &SMVVMViewModelPanel::HandleViewModelsUpdated);
	}

	CreateCommandList();

	ViewModelTreeView = SNew(UE::PropertyViewer::SPropertyViewer)
		.PropertyVisibility(UE::PropertyViewer::SPropertyViewer::EPropertyVisibility::Visible)
		.bShowSearchBox(true)
		.bShowFieldIcon(true)
		.bSanitizeName(true)
		.FieldIterator(FieldIterator.Get())
		.OnContextMenuOpening(this, &SMVVMViewModelPanel::HandleContextMenuOpening)
		.OnSelectionChanged(this, &SMVVMViewModelPanel::HandleSelectionChanged)
		.OnGenerateContainer(this, &SMVVMViewModelPanel::HandleGenerateContainer)
		.SearchBoxPreSlot()
		[
			SAssignNew(AddMenuButton, SPositiveActionButton)
			.OnGetMenuContent(this, &SMVVMViewModelPanel::MakeAddMenu)
			.Text(LOCTEXT("Viewmodel", "Viewmodel"))
			.IsEnabled(this, &SMVVMViewModelPanel::HandleCanEditViewmodelList)
		];

	FillViewModel();


	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsViewArgs.bAllowSearch = false;
	FStructureDetailsViewArgs StructDetailViewArgs;
	StructDetailViewArgs.bShowObjects = true;
	StructDetailViewArgs.bShowInterfaces = true;

	PropertyView = PropertyEditor.CreateStructureDetailView(DetailsViewArgs, StructDetailViewArgs, TSharedPtr<class FStructOnScope>());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(0)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Vertical)
			+ SSplitter::Slot()
			[
				ViewModelTreeView.ToSharedRef()
			]
			+ SSplitter::Slot()
			.SizeRule(SSplitter::SizeToContent)
			[
				PropertyView->GetWidget().ToSharedRef()
			]
		]
	];
}


SMVVMViewModelPanel::~SMVVMViewModelPanel()
{
	if (TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = WeakBlueprintEditor.Pin())
	{
		if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor->GetWidgetBlueprintObj())
		{
			WidgetBlueprint->OnExtensionAdded.RemoveAll(this);
			WidgetBlueprint->OnExtensionRemoved.RemoveAll(this);
		}
	}

	if (UMVVMBlueprintView* CurrentBlueprintView = WeakBlueprintView.Get())
	{
		// bind to check if the view is enabled
		CurrentBlueprintView->OnViewModelsUpdated.Remove(ViewModelsUpdatedHandle);
	}
}


void SMVVMViewModelPanel::FillViewModel()
{
	PropertyViewerHandles.Reset();
	EditableTextBlocks.Reset();
	SelectedViewModelGuid = FGuid();

	if (ViewModelTreeView)
	{
		ViewModelTreeView->RemoveAll();

		if (UMVVMBlueprintView* View = WeakBlueprintView.Get())
		{
			for (const FMVVMBlueprintViewModelContext& ViewModelContext : View->GetViewModels())
			{
				const UE::PropertyViewer::SPropertyViewer::FHandle Handle = ViewModelTreeView->AddContainer(ViewModelContext.GetViewModelClass());
				PropertyViewerHandles.Add(Handle, ViewModelContext.GetViewModelId());
			}
		}
	}
}


FReply SMVVMViewModelPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


void SMVVMViewModelPanel::HandleViewUpdated(UBlueprintExtension*)
{
	bool bViewUpdated = false;

	if (!ViewModelsUpdatedHandle.IsValid())
	{
		if (TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = WeakBlueprintEditor.Pin())
		{
			if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor->GetWidgetBlueprintObj())
			{
				UMVVMBlueprintView* CurrentBlueprintView = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetView(WidgetBlueprint);
				WeakBlueprintView = CurrentBlueprintView;

				if (CurrentBlueprintView)
				{
					ViewModelsUpdatedHandle = CurrentBlueprintView->OnViewModelsUpdated.AddSP(this, &SMVVMViewModelPanel::HandleViewModelsUpdated);
					bViewUpdated = true;
				}
			}
		}
	}

	if (bViewUpdated)
	{
		FillViewModel();
	}
}


void SMVVMViewModelPanel::HandleViewModelsUpdated()
{
	FillViewModel();
}


void SMVVMViewModelPanel::OpenAddViewModelMenu()
{
	AddMenuButton->SetIsMenuOpen(true, true);
}


TSharedRef<SWidget> SMVVMViewModelPanel::MakeAddMenu()
{
	const UWidgetBlueprint* WidgetBlueprint = nullptr;
	if (TSharedPtr<FWidgetBlueprintEditor> EditorPin = WeakBlueprintEditor.Pin())
	{
		WidgetBlueprint = EditorPin->GetWidgetBlueprintObj();
	}

	return SNew(SBox)
		.WidthOverride(600)
		.HeightOverride(500)
		[
			SNew(SMVVMSelectViewModel, WidgetBlueprint)
			.OnCancel(this, &SMVVMViewModelPanel::HandleCancelAddMenu)
			.OnViewModelCommitted(this, &SMVVMViewModelPanel::HandleAddMenuViewModel)
		];
}


void SMVVMViewModelPanel::HandleCancelAddMenu()
{
	if (AddMenuButton)
	{
		AddMenuButton->SetIsMenuOpen(false, false);
	}
}

void SMVVMViewModelPanel::HandleAddMenuViewModel(const UClass* SelectedClass)
{
	if (AddMenuButton)
	{
		AddMenuButton->SetIsMenuOpen(false, false);
		if (SelectedClass)
		{
			if (TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = WeakBlueprintEditor.Pin())
			{
				if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor->GetWidgetBlueprintObj())
				{
					UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
					check(EditorSubsystem);

					UMVVMBlueprintView* CurrentBlueprintView = WeakBlueprintView.Get();
					if (!CurrentBlueprintView)
					{
						CurrentBlueprintView = EditorSubsystem->RequestView(WidgetBlueprint);
						WeakBlueprintView = CurrentBlueprintView;
						ViewModelsUpdatedHandle = CurrentBlueprintView->OnViewModelsUpdated.AddSP(this, &SMVVMViewModelPanel::HandleViewModelsUpdated);
					}

					EditorSubsystem->AddViewModel(WidgetBlueprint, SelectedClass);
				}
			}
		}
	}
}


bool SMVVMViewModelPanel::HandleCanEditViewmodelList() const
{
	if (TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = WeakBlueprintEditor.Pin())
	{
		return WidgetBlueprintEditor->InEditingMode();
	}
	return false;
}


TSharedRef<SWidget> SMVVMViewModelPanel::HandleGenerateContainer(UE::PropertyViewer::SPropertyViewer::FHandle ContainerHandle, TOptional<FText> DisplayName)
{
	if (const FGuid* VMGuidPtr = PropertyViewerHandles.Find(ContainerHandle))
	{
		const FGuid VMGuid = *VMGuidPtr;
		if (UMVVMBlueprintView* View = WeakBlueprintView.Get())
		{
			if (const FMVVMBlueprintViewModelContext* ViewModelContext = View->FindViewModel(VMGuid))
			{
				if (ViewModelContext->GetViewModelClass())
				{
					TSharedRef<SInlineEditableTextBlock> EditableTextBlock = SNew(SInlineEditableTextBlock)
						.Text(ViewModelContext->GetDisplayName())
						.OnVerifyTextChanged(this, &SMVVMViewModelPanel::HandleVerifyNameTextChanged, VMGuid)
						.OnTextCommitted(this, &SMVVMViewModelPanel::HandleNameTextCommited, VMGuid);
					EditableTextBlocks.Add(VMGuid, EditableTextBlock);

					return SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(UE::PropertyViewer::SFieldIcon, ViewModelContext->GetViewModelClass())
						]
						+ SHorizontalBox::Slot()
						.Padding(4.0f)
						[
							EditableTextBlock
						];
				}
				else
				{
					return SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.ErrorWithColor"))
						]
						+ SHorizontalBox::Slot()
						.Padding(4.0f)
						[
							SNew(STextBlock)
							.Text(ViewModelContext->GetDisplayName())
						];
				}
			}
		}
	}

	return SNew(STextBlock)
		.Text(LOCTEXT("ViewmodelError", "Error. Invalid viewmodel."));
}


bool SMVVMViewModelPanel::HandleVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage, FGuid ViewModelGuid)
{
	return RenameViewModelProperty(ViewModelGuid, InText, false, OutErrorMessage);
}


void SMVVMViewModelPanel::HandleNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo, FGuid ViewModelGuid)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		FText OutErrorMessage;
		RenameViewModelProperty(ViewModelGuid, InText, true, OutErrorMessage);
	}
}


void SMVVMViewModelPanel::CreateCommandList()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SMVVMViewModelPanel::HandleDeleteViewModel),
		FCanExecuteAction::CreateSP(this, &SMVVMViewModelPanel::HandleCanDeleteViewModel)
	);
	CommandList->MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SMVVMViewModelPanel::HandleRenameViewModel),
		FCanExecuteAction::CreateSP(this, &SMVVMViewModelPanel::HandleCanRenameViewModel)
	);
}


void SMVVMViewModelPanel::HandleDeleteViewModel()
{
	TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = WeakBlueprintEditor.Pin();
	if (WidgetBlueprintEditor == nullptr)
	{
		return;
	}

	UWidgetBlueprint* WidgetBP = WidgetBlueprintEditor->GetWidgetBlueprintObj();
	if (WidgetBP == nullptr)
	{
		return;
	}

	UMVVMBlueprintView* BlueprintView = WeakBlueprintView.Get();
	if (BlueprintView == nullptr)
	{
		return;
	}

	TArray<UE::PropertyViewer::SPropertyViewer::FSelectedItem> Items = ViewModelTreeView->GetSelectedItems();
	for (UE::PropertyViewer::SPropertyViewer::FSelectedItem& SelectedItem : Items)
	{
		if (SelectedItem.bIsContainerSelected)
		{
			if (FGuid* VMGuidPtr = PropertyViewerHandles.Find(SelectedItem.Handle))
			{
				if (const FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(*VMGuidPtr))
				{
					GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->RemoveViewModel(WidgetBP, ViewModelContext->GetViewModelName());
				}
			}
		}
	}
}


bool SMVVMViewModelPanel::HandleCanDeleteViewModel() const
{
	if (TSharedPtr<FWidgetBlueprintEditor> BlueprintEditor = WeakBlueprintEditor.Pin())
	{
		return BlueprintEditor->InEditingMode();
	}
	return false;
}


void SMVVMViewModelPanel::HandleRenameViewModel()
{
	TArray<UE::PropertyViewer::SPropertyViewer::FSelectedItem> Items = ViewModelTreeView->GetSelectedItems();
	if (Items.Num() == 1 && Items[0].bIsContainerSelected)
	{
		if (const FGuid* VMGuidPtr = PropertyViewerHandles.Find(Items[0].Handle))
		{
			if (TSharedPtr<SInlineEditableTextBlock>* TextBlockPtr = EditableTextBlocks.Find(*VMGuidPtr))
			{
				(*TextBlockPtr)->EnterEditingMode();
			}
		}
	}
}


bool SMVVMViewModelPanel::HandleCanRenameViewModel() const
{
	if (TSharedPtr<FWidgetBlueprintEditor> BlueprintEditor = WeakBlueprintEditor.Pin())
	{
		return BlueprintEditor->InEditingMode();
	}
	return false;
}


TSharedPtr<SWidget> SMVVMViewModelPanel::HandleContextMenuOpening(UE::PropertyViewer::SPropertyViewer::FHandle ContainerHandle, TArrayView<const FFieldVariant> Fields) const
{
	if (Fields.Num() == 0 && ContainerHandle.IsValid() )
	{
		FMenuBuilder MenuBuilder(true, CommandList);

		MenuBuilder.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
		{
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}
	return TSharedPtr<SWidget>();
}


void SMVVMViewModelPanel::HandleSelectionChanged(UE::PropertyViewer::SPropertyViewer::FHandle ContainerHandle, TArrayView<const FFieldVariant> Fields, ESelectInfo::Type SelectionType)
{
	SelectedViewModelGuid = FGuid();

	bool bSet = false;
	if (Fields.Num() == 0)
	{
		if (const FGuid* VMGuidPtr = PropertyViewerHandles.Find(ContainerHandle))
		{
			SelectedViewModelGuid = *VMGuidPtr;
			if (UMVVMBlueprintView* BlueprintView = WeakBlueprintView.Get())
			{
				if (FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(SelectedViewModelGuid))
				{
					PropertyView->SetStructureData(MakeShared<FStructOnScope>(FMVVMBlueprintViewModelContext::StaticStruct(), reinterpret_cast<uint8*>(ViewModelContext)));
					bSet = true;
				}
			}
		}
	}
	
	if (!bSet)
	{
		PropertyView->SetStructureData(TSharedPtr<FStructOnScope>());
	}
}


bool SMVVMViewModelPanel::RenameViewModelProperty(FGuid ViewModelGuid, const FText& RenameTo, bool bCommit, FText& OutErrorMessage) const
{
	if (RenameTo.IsEmptyOrWhitespace())
	{
		OutErrorMessage = LOCTEXT("EmptyViewModelName", "Empty viewmodel name");
		return false;
	}

	const FString& NewNameString = RenameTo.ToString();
	if (NewNameString.Len() >= NAME_SIZE)
	{
		OutErrorMessage = LOCTEXT("ViewModelNameTooLong", "Viewmodel Name is Too Long");
		return false;
	}

	FString GeneratedName = SlugStringForValidName(NewNameString);
	if (GeneratedName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("EmptyViewModelName", "Empty viewmodel name");
		return false;
	}

	const FName GeneratedFName(*GeneratedName);
	check(GeneratedFName.IsValidXName(INVALID_OBJECTNAME_CHARACTERS));

	if (TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = WeakBlueprintEditor.Pin())
	{
		if (UWidgetBlueprint* WidgetBP = WidgetBlueprintEditor->GetWidgetBlueprintObj())
		{
			if (UMVVMBlueprintView* View = WeakBlueprintView.Get())
			{
				if (const FMVVMBlueprintViewModelContext* ViewModelContext = View->FindViewModel(ViewModelGuid))
				{
					if (bCommit)
					{
						return GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->RenameViewModel(WidgetBP, ViewModelContext->GetViewModelName(), *NewNameString, OutErrorMessage);
					}
					else
					{
						return GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->VerifyViewModelRename(WidgetBP, ViewModelContext->GetViewModelName(), *NewNameString, OutErrorMessage);
					}
				}
			}
		}
	}
	return false;
}


namespace Private
{
	static FName NAME_ViewModelName = TEXT("ViewModelName");
}

void SMVVMViewModelPanel::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	PreviousViewModelPropertyName = FName();
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == Private::NAME_ViewModelName && SelectedViewModelGuid.IsValid())
	{
		if (UMVVMBlueprintView* BlueprintView = WeakBlueprintView.Get())
		{
			if (const FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(SelectedViewModelGuid))
			{
				PreviousViewModelPropertyName = ViewModelContext->GetViewModelName();
			}
		}
	}
}


void SMVVMViewModelPanel::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == Private::NAME_ViewModelName && SelectedViewModelGuid.IsValid())
	{
		UMVVMBlueprintView* BlueprintView = WeakBlueprintView.Get();
		TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = WeakBlueprintEditor.Pin();
		UWidgetBlueprint* WidgetBP = WidgetBlueprintEditor ? WidgetBlueprintEditor->GetWidgetBlueprintObj() : nullptr;
		if (BlueprintView && WidgetBP)
		{
			if (FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(SelectedViewModelGuid))
			{
				FName NewName = ViewModelContext->GetViewModelName();
				ViewModelContext->ViewModelName = PreviousViewModelPropertyName;
				FText OutErrorMessage;
				GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->RenameViewModel(WidgetBP, PreviousViewModelPropertyName, NewName, OutErrorMessage);
			}
		}
	}
}

} // namespace

#undef LOCTEXT_NAMESPACE