// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMViewModelContextListWidget.h"

#include "Algo/Transform.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPropertyAccessEditor.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "SPrimaryButton.h"
#include "SSimpleButton.h"
#include "Styling/MVVMEditorStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Types/MVVMFieldVariant.h"
#include "Widgets/SMVVMFieldEntry.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Views/SListView.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "ViewModelContextListWidget"

namespace UE::MVVM::Private
{
	bool IsPropertyTypeChildOf(const FProperty* Property, const UClass* ParentClass)
	{
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (ObjectProperty->PropertyClass->IsChildOf(ParentClass))
			{
				return true;
			}
		}

		return false;
	}

	bool HasBindableViewModelFieldRecursive(UStruct* InStruct, const UClass* InParentClass, TSet<UStruct*>& VisitedSet, uint32 RecursionDepth)
	{
		if (RecursionDepth > 10)
		{
			return false;
		}

		if (VisitedSet.Contains(InStruct))
		{
			return false;
		}

		VisitedSet.Add(InStruct);

		for (TFieldIterator<FProperty> PropIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;

			if (IsPropertyTypeChildOf(Property, InParentClass) && UE::MVVM::BindingHelper::IsValidForSourceBinding(Property))
			{
				return true;
			}

			bool bFoundValidField = false;

			if (const FObjectPropertyBase* ObjectPropertyBase = CastField<FObjectPropertyBase>(Property))
			{
				bFoundValidField = HasBindableViewModelFieldRecursive(ObjectPropertyBase->PropertyClass, InParentClass, VisitedSet, RecursionDepth + 1);
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				bFoundValidField = HasBindableViewModelFieldRecursive(StructProperty->Struct, InParentClass, VisitedSet, RecursionDepth + 1);
			}

			if (bFoundValidField)
			{
				return true;
			}
		}

		for (TFieldIterator<UFunction> FuncIt(InStruct, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;

			const FProperty* ReturnProperty = UE::MVVM::BindingHelper::GetReturnProperty(Function);

			if (IsPropertyTypeChildOf(ReturnProperty, InParentClass) && UE::MVVM::BindingHelper::IsValidForSourceBinding(ReturnProperty))
			{
				return true;
			}

			bool bFoundValidField = false;

			if (const FObjectPropertyBase* ObjectPropertyBase = CastField<FObjectPropertyBase>(ReturnProperty))
			{
				bFoundValidField = HasBindableViewModelFieldRecursive(ObjectPropertyBase->PropertyClass, InParentClass, VisitedSet, RecursionDepth + 1);
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ReturnProperty))
			{
				bFoundValidField = HasBindableViewModelFieldRecursive(StructProperty->Struct, InParentClass, VisitedSet, RecursionDepth + 1);
			}

			if (bFoundValidField)
			{
				return true;
			}
		}

		return false;
	}

	DECLARE_DELEGATE_OneParam(FOnViewModelContextRemovedDelegate, FGuid)
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnViewModelContextRenamedDelegate, FGuid, FText);

	class SMVVMManageViewModelsListEntryRow : public SMultiColumnTableRow<TSharedPtr<FMVVMBlueprintViewModelContext>>
	{
	public:
		static FName RemoveButtonColumnName;
		static FName ClassColumnName;
		static FName NameColumnName;
		static FName CreationTypeColumnName;
		static FName CreationGetterColumnName;

	public:
		SLATE_BEGIN_ARGS(SMVVMManageViewModelsListEntryRow) {}
			SLATE_ARGUMENT(UWidgetBlueprint* , WidgetBlueprint)
			SLATE_EVENT(FOnViewModelContextRenamedDelegate, OnViewModelContextRenamed)
			SLATE_EVENT(FOnViewModelContextRemovedDelegate, OnViewModelContextRemoved)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FMVVMBlueprintViewModelContext>& InEntry)
		{
			Entry = InEntry;
			OnViewModelContextRenamed = InArgs._OnViewModelContextRenamed;
			OnViewModelContextRemoved = InArgs._OnViewModelContextRemoved;
			WidgetBlueprint = InArgs._WidgetBlueprint;

			NameValidator = MakeUnique<FKismetNameValidator>(InArgs._WidgetBlueprint, Entry->GetViewModelName(), WidgetBlueprint->SkeletonGeneratedClass);

			SMultiColumnTableRow<TSharedPtr<FMVVMBlueprintViewModelContext>>::Construct(
				FSuperRowType::FArguments()
				.Padding(1.0f),
				OwnerTableView
			);

			UpdateGetterContainer();
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			SMVVMManageViewModelsListEntryRow* Self = this;
			if (InColumnName == RemoveButtonColumnName)
			{
				return SNew(SSimpleButton)
					.ToolTipText(LOCTEXT("RemoveViewModelContextButtonToolTip", "Remove ViewModelContext"))
					.Icon(FSlateIcon(FMVVMEditorStyle::Get().GetStyleSetName(), "ViewModelSelection.RemoveIcon").GetIcon())
					.OnClicked_Lambda(
						[Self]() {
							Self->OnViewModelContextRemoved.ExecuteIfBound(Self->Entry->GetViewModelId());
							return FReply::Handled();
						}
					);
			}
			else if (InColumnName == ClassColumnName)
			{
				return SNew(SBox)
					.Padding(FMargin(2.0f, 0.0f))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Entry->GetViewModelClass()->GetDisplayNameText())
					];
			}
			else if (InColumnName == NameColumnName)
			{
				TSharedPtr<FMVVMBlueprintViewModelContext> TempEntry = Entry;

				return SNew(SBox)
					.Padding(FMargin(2.0f, 0.0f))
					[
						SNew(SEditableTextBox)
						.Text(Entry->GetDisplayName())
						.OnTextCommitted_Lambda(
							[TempEntry](const FText& NewText, ETextCommit::Type CommitType)
							{
								TempEntry->ViewModelName = *NewText.ToString();
							})
						.OnVerifyTextChanged_Lambda(
							[Self](const FText& InText, FText& OutErrorMessage) -> bool
							{
								if (InText.IsEmptyOrWhitespace())
								{
									OutErrorMessage = LOCTEXT("ViewModelNameEmptyOrWhitespaceError", "The View Model Name cannot be empty.");
									return false;
								}
								for (TCHAR Char : InText.ToString())
								{
									if (FText::IsWhitespace(Char))
									{
										OutErrorMessage = LOCTEXT("ViewModelNameContainsWhitespaceError", "The View Model Name cannot contain whitespaces.");
										return false;
									}
								}

								EValidatorResult ValidatorResult = Self->NameValidator->IsValid(InText.ToString());
								if (ValidatorResult == EValidatorResult::Ok)
								{
									if (Self->OnViewModelContextRenamed.IsBound())
									{
										if (!Self->OnViewModelContextRenamed.Execute(Self->Entry->GetViewModelId(), InText))
										{
											ValidatorResult = EValidatorResult::AlreadyInUse;
										}
									}
								}

								if (ValidatorResult != EValidatorResult::Ok)
								{
									OutErrorMessage = Self->NameValidator->GetErrorText(InText.ToString(), ValidatorResult);
									return false;
								}
								return true;
							})
					];
			}
			else if (InColumnName == CreationTypeColumnName)
			{
				FProperty* Property = FMVVMBlueprintViewModelContext::StaticStruct()->FindPropertyByName(TEXT("CreationType"));

				TSharedRef<SComboButton> CreationTypeWidget = 
					SNew(SComboButton)
					.OnGetMenuContent(this, &SMVVMManageViewModelsListEntryRow::HandleGetMenuContent_CreationType)
					.ButtonContent()
					[
						SNew(SBox)
						.Padding(FMargin(2.0f, 0.0f))
						[
							SAssignNew(CreationTypeTextBlock, STextBlock)
							.Text(UEnum::GetDisplayValueAsText(Entry->CreationType))
						]
					];
				return CreationTypeWidget;
			}
			else if (InColumnName == CreationGetterColumnName)
			{
				return SAssignNew(GetterWidgetContainer, SHorizontalBox);
			}

			return SNullWidget::NullWidget;
		}

	private:
		void HandleAddBinding(TArray<TSharedPtr<FBindingChainElement>> NewBindingChain)
		{
			FString PropertyPath;

			for (TSharedPtr<FBindingChainElement>& ChainElement : NewBindingChain)
			{
				if (!PropertyPath.IsEmpty())
				{
					PropertyPath += TEXT(".");
				}
				PropertyPath += ChainElement->Field.GetName();
			}

			Entry->ViewModelPropertyPath = PropertyPath;
			PropertyPathTextBlock->SetText(FText::FromString(PropertyPath));
			PropertyPathTextBlock->SetToolTipText(FText::FromString(PropertyPath));
		}

		void UpdateGetterContainer()
		{
			TSharedPtr<FMVVMBlueprintViewModelContext> TempEntry = Entry;
			GetterWidgetContainer->ClearChildren();

			if (Entry->CreationType == EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection)
			{
				GetterWidgetContainer->AddSlot()
				[
					SNew(SBox)
					.Padding(FMargin(2.0f, 0.0f))
					[
						SNew(SEditableTextBox)
						.Text(FText::FromName(Entry->GlobalViewModelIdentifier))
						.OnTextCommitted_Lambda(
							[TempEntry](const FText& NewText, ETextCommit::Type CommitType)
							{
								TempEntry->GlobalViewModelIdentifier = FName(NewText.ToString());
							})
					]
				];
			}
			else if (Entry->CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath)
			{
				TArray<TSharedPtr<FBindingChainElement>> BindingChain;
				FMenuBuilder MenuBuilder(true, NULL);
				GeneratePropertyPathMenuContent(MenuBuilder, WidgetBlueprint->SkeletonGeneratedClass, BindingChain);

				GetterWidgetContainer->AddSlot()
				[
					SNew(SBox)
					.Padding(FMargin(2.0f, 0.0f))
					[
						SNew(SComboButton)
						.ButtonContent()
						[
							SAssignNew(PropertyPathTextBlock, STextBlock)
							.Text(FText::FromString(Entry->ViewModelPropertyPath))
							.ToolTipText(FText::FromString(Entry->ViewModelPropertyPath))
						]
						.MenuContent()
						[
							MenuBuilder.MakeWidget()
						]
					]
				];
			}

		}

		TSharedRef<SWidget> HandleGetMenuContent_CreationType()
		{
			FMenuBuilder MenuBuilder(true, NULL);
			SMVVMManageViewModelsListEntryRow* Self = this;

			MenuBuilder.AddMenuEntry(
				FUIAction(
					FExecuteAction::CreateLambda([Self]() 
						{ 
							Self->Entry->CreationType = EMVVMBlueprintViewModelContextCreationType::Manual;
							Self->CreationTypeTextBlock->SetText(UEnum::GetDisplayValueAsText(Self->Entry->CreationType));
							Self->UpdateGetterContainer();
						})
				),
				SNew(STextBlock).Text(LOCTEXT("Manual", "Manual"))
							);

			MenuBuilder.AddMenuEntry(
				FUIAction(
					FExecuteAction::CreateLambda([Self]() 
						{ 
							Self->Entry->CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;
							Self->CreationTypeTextBlock->SetText(UEnum::GetDisplayValueAsText(Self->Entry->CreationType));
							Self->UpdateGetterContainer();
						})
				),
				SNew(STextBlock).Text(LOCTEXT("CreateInstance", "Create New Instance"))
			);

			MenuBuilder.AddMenuEntry(
				FUIAction(
					FExecuteAction::CreateLambda([Self]() 
						{ 
							Self->Entry->CreationType = EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection; 
							Self->CreationTypeTextBlock->SetText(UEnum::GetDisplayValueAsText(Self->Entry->CreationType));
							Self->UpdateGetterContainer();
						})
				),
				SNew(STextBlock).Text(LOCTEXT("GlobalViewModelCollection", "Get From Global collection"))
			);

			MenuBuilder.AddMenuEntry(
				FUIAction(
					FExecuteAction::CreateLambda([Self]() 
						{ 
							Self->Entry->CreationType = EMVVMBlueprintViewModelContextCreationType::PropertyPath; 
							Self->CreationTypeTextBlock->SetText(UEnum::GetDisplayValueAsText(Self->Entry->CreationType));
							Self->UpdateGetterContainer();
						})
				),
				SNew(STextBlock).Text(LOCTEXT("PropertyPath", "Get From Property Path"))
			);

			return MenuBuilder.MakeWidget();
		}

		void GeneratePropertyPathMenuContent(FMenuBuilder& MenuBuilder, UStruct* InOwnerStruct, TArray<TSharedPtr<FBindingChainElement>> InBindingChain)
		{
			using UE::MVVM::FMVVMConstFieldVariant;
			using namespace UE::MVVM::Private;

			auto MakeFieldWidget = [](FMVVMConstFieldVariant Field)
			{
				FMVVMBlueprintPropertyPath Path;
				Path.SetBasePropertyPath(Field);

				return SNew(SFieldEntry)
					.Field(Path);
			};

			UClass* InBindableClass = Cast<UClass>(InOwnerStruct);
			const UClass* ViewModelClass = Entry->GetViewModelClass();

			check(ViewModelClass);

			bool bHasFunctionEntry = false;
			MenuBuilder.BeginSection("Functions", LOCTEXT("Functions", "Functions"));
			for (TFieldIterator<UFunction> FuncIt(InBindableClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
			{
				UFunction* Function = *FuncIt;

				TArray<TSharedPtr<FBindingChainElement>> NewBindingChain(InBindingChain);
				NewBindingChain.Emplace(MakeShared<FBindingChainElement>(Function));

				const FProperty* ReturnProperty = UE::MVVM::BindingHelper::GetReturnProperty(Function);

				if (IsPropertyTypeChildOf(ReturnProperty, ViewModelClass))
				{
					MenuBuilder.AddMenuEntry(
						FUIAction(FExecuteAction::CreateSP(this, &SMVVMManageViewModelsListEntryRow::HandleAddBinding, NewBindingChain)),
						MakeFieldWidget(FMVVMConstFieldVariant(Function))
					);
					bHasFunctionEntry = true;
				}
				else
				{
					TSet<UStruct*> VisitedSet;
					bool bFoundValidField = false;
					if (const FObjectPropertyBase* ObjectPropertyBase = CastField<FObjectPropertyBase>(ReturnProperty))
					{
						if (HasBindableViewModelFieldRecursive(ObjectPropertyBase->PropertyClass, ViewModelClass, VisitedSet, 1))
						{
							MenuBuilder.AddSubMenu(
								MakeFieldWidget(FMVVMConstFieldVariant(Function)),
								FNewMenuDelegate::CreateSP(this, &SMVVMManageViewModelsListEntryRow::GeneratePropertyPathMenuContent, static_cast<UStruct*>(ObjectPropertyBase->PropertyClass), NewBindingChain)
							);
							bHasFunctionEntry = true;
						}
					}
					else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ReturnProperty))
					{
						if (HasBindableViewModelFieldRecursive(StructProperty->Struct, ViewModelClass, VisitedSet, 1))
						{
							MenuBuilder.AddSubMenu(
								MakeFieldWidget(FMVVMConstFieldVariant(Function)),
								FNewMenuDelegate::CreateSP(this, &SMVVMManageViewModelsListEntryRow::GeneratePropertyPathMenuContent, static_cast<UStruct*>(StructProperty->Struct), NewBindingChain)
							);
							bHasFunctionEntry = true;
						}
					}
				}
			}
			MenuBuilder.EndSection(); //Functions

			bool bHasPropertyEntry = false;
			MenuBuilder.BeginSection("Properties", LOCTEXT("Properties", "Properties"));
			for (TFieldIterator<FProperty> PropIt(InBindableClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;

				TArray<TSharedPtr<FBindingChainElement>> NewBindingChain(InBindingChain);
				NewBindingChain.Emplace(MakeShared<FBindingChainElement>(Property));

				if (IsPropertyTypeChildOf(Property, ViewModelClass) &&
					Property->GetFName() != Entry->GetViewModelName()) // don't include the entry itself)
				{
					MenuBuilder.AddMenuEntry(
						FUIAction(FExecuteAction::CreateSP(this, &SMVVMManageViewModelsListEntryRow::HandleAddBinding, NewBindingChain)),
						MakeFieldWidget(FMVVMConstFieldVariant(Property))
					);
					bHasPropertyEntry = true;
				}
				else
				{
					TSet<UStruct*> VisitedSet;
					bool bFoundValidField = false;
					if (const FObjectPropertyBase* ObjectPropertyBase = CastField<FObjectPropertyBase>(Property))
					{
						if (HasBindableViewModelFieldRecursive(ObjectPropertyBase->PropertyClass, ViewModelClass, VisitedSet, 1))
						{
							MenuBuilder.AddSubMenu(
								MakeFieldWidget(FMVVMConstFieldVariant(Property)),
								FNewMenuDelegate::CreateSP(this, &SMVVMManageViewModelsListEntryRow::GeneratePropertyPathMenuContent, static_cast<UStruct*>(ObjectPropertyBase->PropertyClass), NewBindingChain)
							);
							bHasPropertyEntry = true;
						}
					}
					else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
					{
						if (HasBindableViewModelFieldRecursive(StructProperty->Struct, ViewModelClass, VisitedSet, 1))
						{
							MenuBuilder.AddSubMenu(
								MakeFieldWidget(FMVVMConstFieldVariant(Property)),
								FNewMenuDelegate::CreateSP(this, &SMVVMManageViewModelsListEntryRow::GeneratePropertyPathMenuContent, static_cast<UStruct*>(StructProperty->Struct), NewBindingChain)
							);
							bHasPropertyEntry = true;
						}
					}
				}
			}
			MenuBuilder.EndSection(); //Properties

			if (!bHasFunctionEntry && !bHasPropertyEntry)
			{
				FMenuEntryParams MenuEntry;
				MenuEntry.DirectActions.ExecuteAction = FExecuteAction::CreateLambda([](){});
				MenuEntry.DirectActions.CanExecuteAction = FCanExecuteAction::CreateLambda([](){ return false; });
				MenuEntry.LabelOverride = LOCTEXT("NoPropertyPathEntry", "No Available Path");
				MenuEntry.ToolTipOverride = FText::Format(LOCTEXT("NoPropertyPathEntryToolTip", "No available path that returns a viewmodel of the '{0}' type."), ViewModelClass->GetDisplayNameText());
				MenuBuilder.AddMenuEntry(MenuEntry);
			}
		}

		TSharedPtr<FMVVMBlueprintViewModelContext> Entry;
		UWidgetBlueprint* WidgetBlueprint;
		TSharedPtr<SHorizontalBox> GetterWidgetContainer;
		TSharedPtr<STextBlock> PropertyPathTextBlock;
		TSharedPtr<STextBlock> CreationTypeTextBlock;
		TUniquePtr<FKismetNameValidator> NameValidator;

		FOnViewModelContextRenamedDelegate OnViewModelContextRenamed;
		FOnViewModelContextRemovedDelegate OnViewModelContextRemoved;
	};

	FName SMVVMManageViewModelsListEntryRow::RemoveButtonColumnName = "RemoveButton";
	FName SMVVMManageViewModelsListEntryRow::ClassColumnName = "ViewModel";
	FName SMVVMManageViewModelsListEntryRow::NameColumnName = "Name";
	FName SMVVMManageViewModelsListEntryRow::CreationTypeColumnName = "CreationType";
	FName SMVVMManageViewModelsListEntryRow::CreationGetterColumnName = "Getter";

	class SMVVMViewModelDeleteConfirmationDialog : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMVVMViewModelDeleteConfirmationDialog) {}
			SLATE_ARGUMENT(TArray<TSharedPtr<SHorizontalBox>>, ConflictList)
			SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs)
		{
			ConflictListSource = InArgs._ConflictList;
			WeakParentWindow = InArgs._ParentWindow;

			SAssignNew(ConflictListViewWidget, SListView<TSharedPtr<SHorizontalBox>>)
				.ListItemsSource(&ConflictListSource)
				.SelectionMode(ESelectionMode::None)
				.ItemHeight(30.0f)
				.OnGenerateRow(this, &SMVVMViewModelDeleteConfirmationDialog::HandleGenerateRow);
		
			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("AssetDeleteDialog.Background"))
				.Padding(10)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(5.0f)
						[
							SNew(STextBlock)
							.Font( FAppStyle::GetFontStyle( "BoldFont" ) )
							.Text(LOCTEXT("References", "This ViewModel is still referenced by the following bindings"))
							.ShadowOffset( FVector2D( 1.0f, 1.0f ) )
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(5.0f)
						[
							ConflictListViewWidget.ToSharedRef()
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4,4)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text( LOCTEXT("MVVMViewModelForceDelete", "Force Delete ViewModel"))
						.ToolTipText(LOCTEXT("MVVMViewModelForceDeleteTooltipText", "These bindings will be in an invalid state and you must remove or fix them manually."))
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Danger")
						.TextStyle(FAppStyle::Get(), "FlatButton.DefaultTextStyle")
						.OnClicked(this, &SMVVMViewModelDeleteConfirmationDialog::HandleForceDelete)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4,4)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text( LOCTEXT("Cancel", "Cancel"))
						.ToolTipText(LOCTEXT("CancelDeleteTooltipText", "Cancel the delete"))
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
						.TextStyle(FAppStyle::Get(), "FlatButton.DefaultTextStyle")
						.OnClicked(this, &SMVVMViewModelDeleteConfirmationDialog::HandleCancel)
					]
				]
			];
		}

		bool ShouldDeleteViewModel()
		{
			return bShouldDelete;
		}
	private:
		TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<SHorizontalBox> Item, const TSharedRef<STableViewBase>& OwnerTable)
		{
			typedef STableRow<TSharedPtr<SHorizontalBox>> RowType;

			TSharedRef<RowType> NewRow = SNew(RowType, OwnerTable);

			NewRow->SetContent(Item.ToSharedRef());

			return NewRow;
		}

		FReply HandleCancel()
		{
			bShouldDelete = false;
			if (TSharedPtr<SWindow> ParentWindow = WeakParentWindow.Pin())
			{
				ParentWindow->RequestDestroyWindow();
			}
			return FReply::Handled();
		}

		FReply HandleForceDelete()
		{
			bShouldDelete = true;
			if (TSharedPtr<SWindow> ParentWindow = WeakParentWindow.Pin())
			{
				ParentWindow->RequestDestroyWindow();
			}
			return FReply::Handled();
		}

	private:
		TSharedPtr<SListView<TSharedPtr<SHorizontalBox>>> ConflictListViewWidget;
		TArray<TSharedPtr<SHorizontalBox>> ConflictListSource;
		bool bShouldDelete = false;
		TWeakPtr<SWindow> WeakParentWindow;
	};

	const FSlateBrush* ManageViewModelsGetModeBrush(EMVVMBindingMode BindingMode)
	{
		switch (BindingMode)
		{
		case EMVVMBindingMode::OneTimeToDestination:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneTime");
		case EMVVMBindingMode::OneWayToDestination:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneWay");
		case EMVVMBindingMode::OneWayToSource:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneWayToSource");
		case EMVVMBindingMode::OneTimeToSource:
			return nullptr;
		case EMVVMBindingMode::TwoWay:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.TwoWay");
		default:
			return nullptr;
		}
	}
} // namespace UE::MVVM::Private

void SMVVMViewModelContextListWidget::Construct(const FArguments& InArgs)
{
	using UE::MVVM::Private::SMVVMManageViewModelsListEntryRow;

	WidgetBlueprint = InArgs._WidgetBlueprint;
	Bindings = InArgs._Bindings;
	OnViewModelContextsUpdated = InArgs._OnViewModelContextsUpdated;
	WeakParentWindow = InArgs._ParentWindow;

	TSharedRef<SWidget> ButtonsPanelContent = InArgs._ButtonsPanel.Widget;
	if (InArgs._ButtonsPanel.Widget == SNullWidget::NullWidget && WeakParentWindow.IsValid())
	{
		SAssignNew(ButtonsPanelContent, SUniformGridPanel)
			.SlotPadding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("ViewModelFinishButtonText", "Finish"))
				.OnClicked(this, &SMVVMViewModelContextListWidget::HandleClicked_Finish)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ViewModelCancelButtonText", "Cancel"))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SMVVMViewModelContextListWidget::HandleClicked_Cancel)
			];
	}

	for (const FMVVMBlueprintViewModelContext& Context : InArgs._ExistingViewModelContexts)
	{
		ContextListSource.Add(MakeShared<FMVVMBlueprintViewModelContext>(Context));
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(6)
		[
			SAssignNew(ContextListWidget, SListView<TSharedPtr<FMVVMBlueprintViewModelContext>>)
			.SelectionMode(ESelectionMode::Single)
			.ListItemsSource(&ContextListSource)
			.OnGenerateRow(this, &SMVVMViewModelContextListWidget::HandleGenerateRowForListView)
			.ItemHeight(20.0f)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(SMVVMManageViewModelsListEntryRow::RemoveButtonColumnName)
				.FixedWidth(30.0f)
				.DefaultLabel(FText::GetEmpty()) 
				+ SHeaderRow::Column(SMVVMManageViewModelsListEntryRow::ClassColumnName)
				.DefaultLabel(LOCTEXT("Class", "Class"))
				+ SHeaderRow::Column(SMVVMManageViewModelsListEntryRow::NameColumnName)
				.DefaultLabel(LOCTEXT("Name", "Name"))
				+ SHeaderRow::Column(SMVVMManageViewModelsListEntryRow::CreationTypeColumnName)
				.DefaultLabel(LOCTEXT("CreationType", "Creation Type"))
				+ SHeaderRow::Column(SMVVMManageViewModelsListEntryRow::CreationGetterColumnName)
				.DefaultLabel(LOCTEXT("Getter", "Getter"))
			)
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.AutoHeight()
		.Padding(8)
		[
			ButtonsPanelContent
		]
	];
}

void SMVVMViewModelContextListWidget::AddViewModelContext(TSubclassOf<UMVVMViewModelBase> ViewModelClass)
{
	TSet<FString> ExistingContextNames;
	for (const TSharedPtr<FMVVMBlueprintViewModelContext>& Context : ContextListSource)
	{
		ExistingContextNames.Add(Context->GetDisplayName().ToString());
	}

	TSharedPtr<FMVVMBlueprintViewModelContext> NewContext = MakeShared<FMVVMBlueprintViewModelContext>(ViewModelClass, ViewModelClass->GetFName());
	FString ClassName = ViewModelClass->ClassGeneratedBy != nullptr ? ViewModelClass->ClassGeneratedBy->GetName() : ViewModelClass->GetAuthoredName();
	FString TempNewName = ClassName;
	FKismetNameValidator NameValidator(WidgetBlueprint);

	int32 Index = 1;
	while ((NameValidator.IsValid(TempNewName) != EValidatorResult::Ok) || (ExistingContextNames.Contains(TempNewName)))
	{
		TempNewName = ClassName + "_";
		TempNewName.AppendInt(Index);

		++Index;
	}

	NewContext->ViewModelName = *TempNewName;
	ContextListSource.Add(NewContext);
	ContextListWidget->RequestListRefresh();
}

TSharedRef<ITableRow> SMVVMViewModelContextListWidget::HandleGenerateRowForListView(TSharedPtr<FMVVMBlueprintViewModelContext> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	using UE::MVVM::Private::SMVVMManageViewModelsListEntryRow;
	return SNew(SMVVMManageViewModelsListEntryRow, OwnerTable, Item)
		.WidgetBlueprint(WidgetBlueprint)
		.OnViewModelContextRenamed(this, &SMVVMViewModelContextListWidget::IsContextNameAvailable)
		.OnViewModelContextRemoved(this, &SMVVMViewModelContextListWidget::RemoveViewModelContext);
}

bool SMVVMViewModelContextListWidget::IsContextNameAvailable(FGuid Guid, FText ContextName)
{
	for (TSharedPtr<FMVVMBlueprintViewModelContext>& Context : ContextListSource)
	{
		if (Context->GetViewModelId() == Guid)
		{
			continue;
		}

		if (Context->ViewModelName == *ContextName.ToString())
		{
			return false;
		}
	}

	return true;
}

void SMVVMViewModelContextListWidget::RemoveViewModelContext(FGuid Guid)
{
	TSharedPtr<FMVVMBlueprintViewModelContext> ContextToRemove;
	for (TSharedPtr<FMVVMBlueprintViewModelContext>& Context : ContextListSource)
	{
		if (Context->GetViewModelId() == Guid)
		{
			ContextToRemove = Context;
			break;
		}
	}

	if (ValidateRemoveViewModelContext(ContextToRemove))
	{
		ContextListSource.Remove(ContextToRemove);
		ContextListWidget->RequestListRefresh();
	}
}

bool SMVVMViewModelContextListWidget::ValidateRemoveViewModelContext(TSharedPtr<FMVVMBlueprintViewModelContext> ContextToRemove)
{
	using UE::MVVM::Private::SMVVMViewModelDeleteConfirmationDialog;
	TArray<TSharedPtr<SHorizontalBox>> ConflictList;
	for (const FMVVMBlueprintViewBinding& Binding : Bindings)
	{
		if (Binding.ViewModelPath.GetViewModelId() == ContextToRemove->GetViewModelId())
		{
			TSharedRef<SHorizontalBox> Container = SNew(SHorizontalBox);
			Container->AddSlot()
			.Padding(FMargin(3.0f, 0.0f))
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Binding.ViewModelPath.GetBasePropertyPath()))
			];

			Container->AddSlot()
			.Padding(FMargin(3.0f, 0.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					SNew(SImage)
					.Image(UE::MVVM::Private::ManageViewModelsGetModeBrush(Binding.BindingType))
				]
			];

			Container->AddSlot()
			.Padding(FMargin(3.0f, 0.0f))
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FormatOrdered(INVTEXT("{0}.{1}"), FText::FromString(Binding.WidgetPath.GetWidgetName().ToString()), FText::FromString(Binding.WidgetPath.GetBasePropertyPath())))
			];
			ConflictList.Add(Container);
		}
	}

	if (!ConflictList.IsEmpty())
	{
		TSharedRef<SWindow> ViewModelDeleteConfirmationWindow = SNew(SWindow)
			.Title(LOCTEXT("MVVMViewModelDeleteConfirmationWindowHeader", "Delete ViewModel Confirmation"))
			.SupportsMaximize(false)
			.ClientSize(FVector2D(400.0f, 300.0f));

		TSharedRef<SMVVMViewModelDeleteConfirmationDialog> DeleteConfirmationDialog = SNew(SMVVMViewModelDeleteConfirmationDialog)
			.ConflictList(ConflictList)
			.ParentWindow(ViewModelDeleteConfirmationWindow);

		ViewModelDeleteConfirmationWindow->SetContent(DeleteConfirmationDialog);

		GEditor->EditorAddModalWindow(ViewModelDeleteConfirmationWindow);

		return DeleteConfirmationDialog->ShouldDeleteViewModel();
	}

	return true;
}

TArray<FMVVMBlueprintViewModelContext> SMVVMViewModelContextListWidget::GetViewModelContexts() const
{
	TArray<FMVVMBlueprintViewModelContext> NewContextList;
	Algo::Transform(ContextListSource, NewContextList, [](TSharedPtr<FMVVMBlueprintViewModelContext> InElement)
		{
			return *InElement.Get();
		});

	return NewContextList;
}

FReply SMVVMViewModelContextListWidget::HandleClicked_Finish()
{
	OnViewModelContextsUpdated.ExecuteIfBound(GetViewModelContexts());

	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SMVVMViewModelContextListWidget::HandleClicked_Cancel()
{
	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
