// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMViewBindingListView.h"

#include "Algo/Transform.h"
#include "Bindings/MVVMBindingHelper.h"
#include "BlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "Dialog/SCustomDialog.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPropertyAccessEditor.h"
#include "K2Node_CallFunction.h"
#include "Misc/MessageDialog.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMSubsystem.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "SEnumCombo.h"
#include "SPrimaryButton.h"
#include "SSimpleButton.h" 
#include "Styling/AppStyle.h"
#include "Styling/MVVMEditorStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/SMVVMConversionPath.h"
#include "Widgets/SMVVMFieldSelector.h"
#include "Widgets/SMVVMFunctionParameter.h"
#include "Widgets/SMVVMPropertyPath.h"
#include "Widgets/SMVVMSourceSelector.h"
#include "Widgets/SMVVMViewBindingPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "BindingListView"

namespace UE::MVVM
{

// a wrapper around either a widget row or a binding row
struct FBindingEntry
{
	enum class ERowType
	{
		None,
		Widget,
		Binding,
		Parameter
	};

	FMVVMBlueprintViewBinding* GetBinding(UMVVMBlueprintView* View) const
	{
		return View->GetBindingAt(BindingIndex);
	}

	const FMVVMBlueprintViewBinding* GetBinding(const UMVVMBlueprintView* View) const
	{
		return View->GetBindingAt(BindingIndex);
	}

	ERowType GetRowType() const
	{
		return RowType;
	}

	int32 GetBindingIndex() const
	{
		return BindingIndex;
	}

	void SetBindingIndex(int32 Index)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::Binding;
		BindingIndex = Index;
	}

	void SetWidgetName(FName WidgetName)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::Widget;
		Name = WidgetName;
	}

	void SetParameterName(int32 Index, FName ParameterName)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::Parameter;
		BindingIndex = Index;
		Name = ParameterName;
	}

	FName GetName() const
	{
		return Name;
	}

	TConstArrayView<TSharedPtr<FBindingEntry>> GetChildren() const
	{
		return Children;
	}

	void AddChild(TSharedRef<FBindingEntry> Child)
	{
		Children.Add(Child);
	}

	bool operator==(const FBindingEntry& Other) const
	{
		return RowType == Other.RowType &&
			Name == Other.Name &&
			BindingIndex == Other.BindingIndex;
	}

private:
	ERowType RowType = ERowType::None;
	FName Name;
	int32 BindingIndex = INDEX_NONE;
	TArray<TSharedPtr<FBindingEntry>> Children;
};

namespace Private
{
	TArray<FName>* GetBindingModeNames()
	{
		static TArray<FName> BindingModeNames;

		if (BindingModeNames.IsEmpty())
		{
			UEnum* ModeEnum = StaticEnum<EMVVMBindingMode>();

			BindingModeNames.Reserve(ModeEnum->NumEnums());

			for (int32 BindingIndex = 0; BindingIndex < ModeEnum->NumEnums() - 1; ++BindingIndex)
			{
				const bool bIsHidden = ModeEnum->HasMetaData(TEXT("Hidden"), BindingIndex);
				if (!bIsHidden)
				{
					BindingModeNames.Add(ModeEnum->GetNameByIndex(BindingIndex));
				}
			}
		}

		return &BindingModeNames;
	}

	void ExpandAll(const TSharedPtr<STreeView<TSharedPtr<FBindingEntry>>>& TreeView, const TSharedPtr<FBindingEntry>& Entry)
	{
		TreeView->SetItemExpansion(Entry, true);

		for (const TSharedPtr<FBindingEntry>& Child : Entry->GetChildren())
		{
			ExpandAll(TreeView, Child);
		}
	}
}

class SWidgetRow : public STableRow<TSharedPtr<FBindingEntry>>
{
public:
	SLATE_BEGIN_ARGS(SWidgetRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FBindingEntry>& InEntry, UWidgetBlueprint* InWidgetBlueprint)
	{
		Entry = InEntry;
		WidgetBlueprintWeak = InWidgetBlueprint;

		STableRow<TSharedPtr<FBindingEntry>>::Construct(
			STableRow<TSharedPtr<FBindingEntry>>::FArguments()
			.Padding(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2, 1)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(150)
					[
						SNew(SSourceSelector, InWidgetBlueprint)
						.ShowClear(false)
						.AutoRefresh(true)
						.ViewModels(false)
						.SelectedSource(this, &SWidgetRow::GetSelectedWidget)
						.OnSelectionChanged(this, &SWidgetRow::SetSelectedWidget)
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SSimpleButton)
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.IsEnabled_Lambda([this]() { return !Entry->GetName().IsNone(); })
					.OnClicked(this, &SWidgetRow::AddBinding)

				]
			],
			OwnerTableView
		);
	}

private:

	FBindingSource GetSelectedWidget() const
	{
		return FBindingSource::CreateForWidget(WidgetBlueprintWeak.Get(), Entry->GetName());
	}

	void SetSelectedWidget(FBindingSource Source)
	{
		if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintWeak.Get())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			UMVVMBlueprintView* View = EditorSubsystem->GetView(WidgetBlueprint);

			for (const TSharedPtr<FBindingEntry>& ChildEntry : Entry->GetChildren())
			{
				if (FMVVMBlueprintViewBinding* Binding = ChildEntry->GetBinding(View))
				{
					FMVVMBlueprintPropertyPath CurrentPath = Binding->WidgetPath;
					CurrentPath.SetWidgetName(Source.Name);

					EditorSubsystem->SetWidgetPropertyForBinding(WidgetBlueprint, *Binding, CurrentPath);
				}
			}
		}
	}

	FReply AddBinding() const
	{
		if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintWeak.Get())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			FMVVMBlueprintViewBinding& Binding = EditorSubsystem->AddBinding(WidgetBlueprint);
			FMVVMBlueprintPropertyPath Path;
			Path.SetWidgetName(Entry->GetName());
			EditorSubsystem->SetWidgetPropertyForBinding(WidgetBlueprint, Binding, Path);
		}

		return FReply::Handled();
	}

private:
	TSharedPtr<FBindingEntry> Entry;
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprintWeak;
};

class SBindingRow : public STableRow<TSharedPtr<FBindingEntry>>
{
public:
	SLATE_BEGIN_ARGS(SBindingRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FBindingEntry>& InEntry, UWidgetBlueprint* InWidgetBlueprint)
	{
		Entry = InEntry;
		WidgetBlueprintWeak = InWidgetBlueprint;

		OnBlueprintChangedHandle = InWidgetBlueprint->OnChanged().AddSP(this, &SBindingRow::HandleBlueprintChanged);
		
		FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding();
		FBindingSource WidgetSource = FBindingSource::CreateForWidget(InWidgetBlueprint, ViewBinding->WidgetPath.GetWidgetName());

		STableRow<TSharedPtr<FBindingEntry>>::Construct(
			STableRow<TSharedPtr<FBindingEntry>>::FArguments()
			.Padding(1.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.Padding(2, 0)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left) 
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(this, &SBindingRow::IsBindingCompiled)
					.OnCheckStateChanged(this, &SBindingRow::OnIsBindingCompileChanged)
				]

				+ SHorizontalBox::Slot()
				.Padding(2, 0)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left) 
				.AutoWidth()
				[
					SNew(SSimpleButton)
					.Icon(FAppStyle::Get().GetBrush("Icons.Error"))
					.Visibility(this, &SBindingRow::GetErrorVisibility)
					.ToolTipText(this, &SBindingRow::GetErrorToolTip)
					.OnClicked(this, &SBindingRow::OnErrorButtonClicked)
				]

				+ SHorizontalBox::Slot()
				.Padding(4, 0)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(150)
					[
						SAssignNew(WidgetFieldSelector, SFieldSelector, InWidgetBlueprint, false)
						.BindingMode(this, &SBindingRow::GetCurrentBindingMode)
						.SelectedField(this, &SBindingRow::GetSelectedWidgetProperty)
						.OnFieldSelectionChanged(this, &SBindingRow::OnWidgetPropertySelected)
						.SelectedConversionFunction(this, &SBindingRow::GetSelectedConversionFunction, false)
						.OnConversionFunctionSelectionChanged(this, &SBindingRow::OnConversionFunctionChanged, false)
						.ShowConversionFunctions(this, &SBindingRow::ShouldShowConversionFunctions, false)
						.ShowSource(false)
						.Source(WidgetSource)
						.AssignableTo(this, &SBindingRow::GetAssignableToProperty, false)
					]
				]

				+ SHorizontalBox::Slot()
				.Padding(2, 0)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left) 
				.AutoWidth()
				[
					SNew(SComboBox<FName>)
					.OptionsSource(Private::GetBindingModeNames())
					.InitiallySelectedItem(StaticEnum<EMVVMBindingMode>()->GetNameByValue((int64) ViewBinding->BindingType))
					.OnSelectionChanged(this, &SBindingRow::OnBindingModeSelectionChanged)
					.OnGenerateWidget(this, &SBindingRow::GenerateBindingModeWidget)
					.ToolTipText(this, &SBindingRow::GetCurrentBindingModeLabel)
					.Content()
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.WidthOverride(16)
						.HeightOverride(16)
						[
							SNew(SImage)
							.Image(this, &SBindingRow::GetCurrentBindingModeBrush)
						]
					]
				]
				
				+ SHorizontalBox::Slot()
				.Padding(4, 0, 2, 0)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(150)
					[
						SAssignNew(ViewModelFieldSelector, SFieldSelector, InWidgetBlueprint, true)
						.BindingMode(this, &SBindingRow::GetCurrentBindingMode)
						.SelectedField(this, &SBindingRow::GetSelectedViewModelProperty)
						.OnFieldSelectionChanged(this, &SBindingRow::OnViewModelPropertySelected)
						.SelectedConversionFunction(this, &SBindingRow::GetSelectedConversionFunction, true)
						.OnConversionFunctionSelectionChanged(this, &SBindingRow::OnConversionFunctionChanged, true)
						.ShowConversionFunctions(this, &SBindingRow::ShouldShowConversionFunctions, true)
						.AssignableTo(this, &SBindingRow::GetAssignableToProperty, true)
					]
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.Padding(2, 1)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SEnumComboBox, StaticEnum<EMVVMViewBindingUpdateMode>())
					.ContentPadding(FMargin(4, 0))
					.OnEnumSelectionChanged(this, &SBindingRow::OnUpdateModeSelectionChanged)
					.CurrentValue(this, &SBindingRow::GetUpdateModeValue)
				]
			],
			OwnerTableView
		);
	}

	~SBindingRow()
	{
		if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintWeak.Get())
		{
			WidgetBlueprint->OnChanged().Remove(OnBlueprintChangedHandle);
		}
	}

	FMVVMBlueprintViewBinding* GetThisViewBinding() const
	{
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (UMVVMBlueprintView* BlueprintViewPtr = EditorSubsystem->GetView(WidgetBlueprintWeak.Get()))
		{
			FMVVMBlueprintViewBinding* ViewBinding = Entry->GetBinding(BlueprintViewPtr);
			return ViewBinding;
		}
		return nullptr;
	}

	TArray<FMVVMBlueprintViewBinding*> GetThisViewBindingAsArray() const
	{
		TArray<FMVVMBlueprintViewBinding*> Result;
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			Result.Add(ViewBinding);
		}
		return Result;
	}

private:

	ECheckBoxState IsBindingEnabled() const
	{
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			return ViewModelBinding->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	ECheckBoxState IsBindingCompiled() const
	{
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			return ViewModelBinding->bCompile ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	EVisibility GetErrorVisibility() const
	{
		return GetThisViewBinding()->Errors.IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
	}

	FText GetErrorToolTip() const
	{
		static const FText NewLineText = FText::FromString(TEXT("\n"));
		FText HintText = LOCTEXT("ErrorButtonText", "Errors: (Click to show in a separate window)");
		FText ErrorsText = FText::Join(NewLineText, GetThisViewBinding()->Errors);
		return FText::Join(NewLineText, HintText, ErrorsText);
	}

	FReply OnErrorButtonClicked()
	{
		ErrorDialog.Reset();
		ErrorItems.Reset();

		const UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintWeak.Get();
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (const UMVVMBlueprintView* View = EditorSubsystem->GetView(WidgetBlueprint))
		{
			if (const FMVVMBlueprintViewBinding* ViewBinding = Entry->GetBinding(View))
			{
				for (const FText& ErrorText : ViewBinding->Errors)
				{
					ErrorItems.Add(MakeShared<FText>(ErrorText));
				}

				const FText BindingDisplayName = FText::FromString(ViewBinding->GetDisplayNameString(WidgetBlueprint));
				ErrorDialog = SNew(SCustomDialog)
					.Title(FText::Format(LOCTEXT("Compilation Errors", "Compilation Errors for {0}"), BindingDisplayName))
					.Buttons({
						SCustomDialog::FButton(LOCTEXT("OK", "OK"))
					})
					.Content()
					[
						SNew(SListView<TSharedPtr<FText>>)
						.ListItemsSource(&ErrorItems)
						.OnGenerateRow(this, &SBindingRow::OnGenerateErrorRow)
					];

				ErrorDialog->Show();
			}
		}

		return FReply::Handled();
	}

	EMVVMBindingMode GetCurrentBindingMode() const
	{
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			return ViewModelBinding->BindingType;
		}
		return EMVVMBindingMode::OneWayToDestination;
	}

	bool ShouldShowConversionFunctions(bool bViewModel) const
	{
		EMVVMBindingMode Mode = GetCurrentBindingMode();
		if (IsForwardBinding(Mode))
		{
			return bViewModel;
		}
		else if (IsBackwardBinding(Mode))
		{
			return !bViewModel;
		}
		
		return false;
	}

	const FProperty* GetAssignableToProperty(bool bViewModel) const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			TArray<FMVVMConstFieldVariant> Fields;
			if (bViewModel)
			{
				Fields = ViewBinding->WidgetPath.GetFields();
			}
			else
			{
				Fields = ViewBinding->ViewModelPath.GetFields();
			}

			if (Fields.Num() > 0)
			{
				FMVVMConstFieldVariant LastField = Fields.Last();
				if (LastField.IsProperty())
				{
					return LastField.GetProperty();
				}
				else if (LastField.IsFunction())
				{
					const UFunction* Function = LastField.GetFunction();
					if (Function != nullptr)
					{
						return BindingHelper::GetFirstArgumentProperty(Function);
					}
				}
			}
		}

		return nullptr;
	}

	TSharedRef<ITableRow> OnGenerateErrorRow(TSharedPtr<FText> Text, const TSharedRef<STableViewBase>& TableView) const
	{
		return SNew(STableRow<TSharedPtr<FText>>, TableView)
			.Content()
			[
				SNew(SEditableTextBox)
				.BackgroundColor(FStyleColors::Background)
				.IsReadOnly(true)
				.Text(*Text.Get())
			];
	}

	void OnConversionFunctionChanged(const UFunction* Function, bool bSourceToDest)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();

			if (bSourceToDest)
			{
				EditorSubsystem->SetSourceToDestinationConversionFunction(WidgetBlueprintWeak.Get(), *ViewBinding, Function);
			} 
			else
			{
				EditorSubsystem->SetDestinationToSourceConversionFunction(WidgetBlueprintWeak.Get(), *ViewBinding, Function);
			}
		}
	}

	TArray<FBindingSource> GetAvailableViewModels() const
	{
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		return EditorSubsystem->GetAllViewModels(WidgetBlueprintWeak.Get());
	}

	FMVVMBlueprintPropertyPath GetSelectedViewModelProperty() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return ViewBinding->ViewModelPath;
		}
		return FMVVMBlueprintPropertyPath();
	}

	FMVVMBlueprintPropertyPath GetSelectedWidgetProperty() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return ViewBinding->WidgetPath;
		}
		return FMVVMBlueprintPropertyPath();
	}

	const UFunction* GetSelectedConversionFunction(bool bSourceToDest) const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			return EditorSubsystem->GetConversionFunction(WidgetBlueprintWeak.Get(), *ViewBinding, bSourceToDest);
		}

		return nullptr;
	}

	void OnViewModelPropertySelected(FMVVMBlueprintPropertyPath SelectedField)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			if (ViewBinding->ViewModelPath != SelectedField)
			{
				UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
				Subsystem->SetViewModelPropertyForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, SelectedField);

				if (WidgetFieldSelector.IsValid())
				{
					WidgetFieldSelector->Refresh();
				}
			}
		}
	}

	void OnWidgetPropertySelected(FMVVMBlueprintPropertyPath SelectedField)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			if (ViewBinding->WidgetPath != SelectedField)
			{
				UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
				Subsystem->SetWidgetPropertyForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, SelectedField);

				if (ViewModelFieldSelector.IsValid())
				{
					ViewModelFieldSelector->Refresh();
				}

				if (SelectedWidget.IsValid())
				{
					SelectedWidget->SetField(SelectedField);
				}
			}
		}
	}

	void OnUpdateModeSelectionChanged(int32 Value, ESelectInfo::Type)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetUpdateModeForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, (EMVVMViewBindingUpdateMode) Value);
		}			
	}

	int32 GetUpdateModeValue() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return (int32) ViewBinding->UpdateMode;
		}
		return (int32) EMVVMViewBindingUpdateMode::Immediate;
	}

	void OnIsBindingEnableChanged(ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Undetermined)
		{
			return;
		}

		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetEnabledForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, NewState == ECheckBoxState::Checked);
		}
	}

	void OnIsBindingCompileChanged(ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Undetermined)
		{
			return;
		}

		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetCompileForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, NewState == ECheckBoxState::Checked);
		}
	}

	const FSlateBrush* GetBindingModeBrush(EMVVMBindingMode BindingMode) const
	{
		switch (BindingMode)
		{
		case EMVVMBindingMode::OneTimeToDestination:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneTime");
		case EMVVMBindingMode::OneWayToDestination:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneWayToSource");
		case EMVVMBindingMode::OneWayToSource:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneWay");
		case EMVVMBindingMode::OneTimeToSource:
			return nullptr;
		case EMVVMBindingMode::TwoWay:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.TwoWay");
		default:
			return nullptr;
		}
	}

	const FSlateBrush* GetCurrentBindingModeBrush() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return GetBindingModeBrush(ViewBinding->BindingType);
		}
		return nullptr;
	}

	FText GetCurrentBindingModeLabel() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return GetBindingModeLabel(ViewBinding->BindingType);
		}
		return FText::GetEmpty();
	}

	FText GetBindingModeLabel(EMVVMBindingMode BindingMode) const
	{
		static FText OneTimeToDestinationLabel = LOCTEXT("OneTimeToDestinationLabel", "One Time To Widget");
		static FText OneWayToDestinationLabel = LOCTEXT("OneWayToDestinationLabel", "One Way To Widget");
		static FText OneWayToSourceLabel = LOCTEXT("OneWayToSourceLabel", "One Way To View Model");
		static FText OneTimeToSourceLabel = LOCTEXT("OneTimeToSourceLabel", "One Time To View Model");
		static FText TwoWayLabel = LOCTEXT("TwoWayLabel", "Two Way");

		switch (BindingMode)
		{
		case EMVVMBindingMode::OneTimeToDestination:
			return OneTimeToDestinationLabel;
		case EMVVMBindingMode::OneWayToDestination:
			return OneWayToDestinationLabel;
		case EMVVMBindingMode::OneWayToSource:
			return OneWayToSourceLabel;
		case EMVVMBindingMode::OneTimeToSource:
			return OneTimeToSourceLabel;
		case EMVVMBindingMode::TwoWay:
			return TwoWayLabel;
		default:
			return FText::GetEmpty();
		}
	}

	TSharedRef<SWidget> GenerateBindingModeWidget(FName ValueName) const
	{
		const UEnum* ModeEnum = StaticEnum<EMVVMBindingMode>();
		int32 Index = ModeEnum->GetIndexByName(ValueName);
		EMVVMBindingMode MVVMBindingMode = EMVVMBindingMode(Index);
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					SNew(SImage)
					.Image(GetBindingModeBrush(MVVMBindingMode))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(GetBindingModeLabel(MVVMBindingMode))
				.ToolTipText(ModeEnum->GetToolTipTextByIndex(Index))
			];
	}

	void OnBindingModeSelectionChanged(FName ValueName, ESelectInfo::Type)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			const UEnum* ModeEnum = StaticEnum<EMVVMBindingMode>();
			EMVVMBindingMode NewMode = (EMVVMBindingMode) ModeEnum->GetValueByName(ValueName);

			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetBindingTypeForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, NewMode);

			if (ViewModelFieldSelector.IsValid())
			{
				ViewModelFieldSelector->Refresh();
			}

			if (WidgetFieldSelector.IsValid())
			{
				WidgetFieldSelector->Refresh();
			}
		}
	}

	void HandleBlueprintChanged(UBlueprint* Blueprint)
	{
		if (ViewModelFieldSelector.IsValid())
		{
			ViewModelFieldSelector->Refresh();
		}

		if (WidgetFieldSelector.IsValid())
		{
			WidgetFieldSelector->Refresh();
		}
	}

private:
	TSharedPtr<FBindingEntry> Entry;
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprintWeak;
	TSharedPtr<SFieldSelector> WidgetFieldSelector;
	TSharedPtr<SFieldSelector> ViewModelFieldSelector;
	TSharedPtr<SWidget> ContextMenuOptionHelper;
	TSharedPtr<SCustomDialog> ErrorDialog;
	TArray<TSharedPtr<FText>> ErrorItems;
	FDelegateHandle OnBlueprintChangedHandle;
	TSharedPtr<UE::MVVM::FFieldIterator_Bindable> WidgetFieldIterator;
	TSharedPtr<SMenuAnchor> WidgetMenuAnchor;
	TSharedPtr<SSourceBindingList> WidgetBindingList;
	TSharedPtr<SFieldEntry> SelectedWidget;
};

class SFunctionParameterRow : public STableRow<TSharedPtr<FBindingEntry>>
{
	SLATE_BEGIN_ARGS(SFunctionParameterRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FBindingEntry>& InEntry, UWidgetBlueprint* InWidgetBlueprint)
	{
		Entry = InEntry;
		check(Entry->GetRowType() == FBindingEntry::ERowType::Parameter);

		WidgetBlueprint = InWidgetBlueprint;

		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		UMVVMBlueprintView* View = EditorSubsystem->GetView(InWidgetBlueprint);
		FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View);
		const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);

		FSlateColor PrimaryColor, SecondaryColor;
		const FSlateBrush* PrimaryBrush = nullptr;
		const FSlateBrush* SecondaryBrush = nullptr;
		FText DisplayName, ToolTip;

		bool bSimpleConversionFunction = false;
		
		if (UEdGraphPin* Pin = EditorSubsystem->GetConversionFunctionArgumentPin(InWidgetBlueprint, *Binding, Entry->GetName(), bSourceToDestination))
		{
			PrimaryBrush = FBlueprintEditor::GetVarIconAndColorFromPinType(Pin->PinType, PrimaryColor, SecondaryBrush, SecondaryColor);
			DisplayName = Pin->GetDisplayName();
			ToolTip = FText::FromString(Pin->PinToolTip);
		}
		else if (const UFunction* Function = EditorSubsystem->GetConversionFunction(InWidgetBlueprint, *Binding, bSourceToDestination))
		{
			// no wrapper graph, this is a simple conversion function of the form: int32 Convert(float x)
			if (const FProperty* Argument = BindingHelper::GetFirstArgumentProperty(Function))
			{
				bSimpleConversionFunction = true;

				PrimaryBrush = FBlueprintEditor::GetVarIconAndColorFromProperty(Argument, PrimaryColor, SecondaryBrush, SecondaryColor);
				DisplayName = Argument->GetDisplayNameText();
				ToolTip = Argument->GetToolTipText();
			}
		}

		STableRow<TSharedPtr<FBindingEntry>>::Construct(
			STableRow<TSharedPtr<FBindingEntry>>::FArguments()
		[
			SNew(SBox)
			.HeightOverride(30)
			.ToolTipText(ToolTip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(100)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(0, 0, 8, 0)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(PrimaryBrush)
							.ColorAndOpacity(PrimaryColor)
							.DesiredSizeOverride(FVector2D(16, 16))
						]
						+ SHorizontalBox::Slot()
						[
							SNew(STextBlock)
							.Text(DisplayName)
						]
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(200)
					[
						SNew(SFunctionParameter)
						.OnGetBindingMode(this, &SFunctionParameterRow::OnGetBindingMode)
						.WidgetBlueprint(InWidgetBlueprint)
						.Binding(Binding)
						.ParameterName(Entry->GetName())
						.SourceToDestination(bSourceToDestination)
						.AllowDefault(!bSimpleConversionFunction)
					]
				]
			]
		], OwnerTableView);
	}

private:

	EMVVMBindingMode OnGetBindingMode() const
	{
		if (UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			UMVVMBlueprintView* View = EditorSubsystem->GetView(WidgetBlueprintPtr);

			if (FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View))
			{
				return Binding->BindingType;
			}
		}
		return EMVVMBindingMode::OneWayToDestination;
	}

private:
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprint;
	TSharedPtr<FBindingEntry> Entry;
};

void SBindingsList::Construct(const FArguments& InArgs, TSharedPtr<SBindingsPanel> Owner, UMVVMWidgetBlueprintExtension_View* InMVVMExtension)
{
	BindingPanel = Owner;
	MVVMExtension = InMVVMExtension;
	check(InMVVMExtension);
	check(InMVVMExtension->GetBlueprintView());

	MVVMExtension->OnBlueprintViewChangedDelegate().AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnBindingsUpdated.AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnViewModelsUpdated.AddSP(this, &SBindingsList::Refresh);

	ChildSlot
	[
		SAssignNew(TreeView, STreeView<TSharedPtr<FBindingEntry>>)
		.TreeItemsSource(&RootWidgets)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SBindingsList::GenerateEntryRow)
		.OnGetChildren(this, &SBindingsList::GetChildrenOfEntry)
		.OnContextMenuOpening(this, &SBindingsList::OnSourceConstructContextMenu)
		.OnSelectionChanged(this, &SBindingsList::OnSourceListSelectionChanged)
		.ItemHeight(32)
	];

	Refresh();
}

SBindingsList::~SBindingsList()
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		MVVMExtensionPtr->OnBlueprintViewChangedDelegate().RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnBindingsUpdated.RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnViewModelsUpdated.RemoveAll(this);
	}
}

void SBindingsList::GetChildrenOfEntry(TSharedPtr<FBindingEntry> Entry, TArray<TSharedPtr<FBindingEntry>>& OutChildren) const
{
	TConstArrayView<TSharedPtr<FBindingEntry>> Children = Entry->GetChildren();
	OutChildren.Append(Children.GetData(), Children.Num());
}

void SBindingsList::Refresh()
{
	UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get();
	UMVVMBlueprintView* BlueprintView = MVVMExtensionPtr ? MVVMExtensionPtr->GetBlueprintView() : nullptr;

	// store the current binding index
	TArray<TSharedPtr<FBindingEntry>> PreviousSelectedEntries;
	if (TreeView.IsValid() && BlueprintView)
	{
		PreviousSelectedEntries = TreeView->GetSelectedItems();
	}

	RootWidgets.Reset();

	TArray<TSharedPtr<FBindingEntry>> NewSelectedEntries;

	// generate our entries
	// for each widget with bindings, create an entry at the root level
	// then add all bindings that reference that widget as its children
	if (BlueprintView)
	{
		auto ReselectIfRequired = [&PreviousSelectedEntries, &NewSelectedEntries](const TSharedPtr<FBindingEntry>& ToFind)
		{
			if (PreviousSelectedEntries.ContainsByPredicate([ToFind](const TSharedPtr<FBindingEntry>& Entry) -> bool
				{
					return Entry.Get() == ToFind.Get();
				}))
			{
				NewSelectedEntries.Add(ToFind);
			}
		};

		TArrayView<FMVVMBlueprintViewBinding> Bindings = BlueprintView->GetBindings();
		for (int32 BindingIndex = 0; BindingIndex < Bindings.Num(); ++BindingIndex)
		{
			const FMVVMBlueprintViewBinding& Binding = Bindings[BindingIndex];
			
			FName WidgetName = Binding.WidgetPath.GetWidgetName();

			TSharedPtr<FBindingEntry> ExistingWidget;
			for (TSharedPtr<FBindingEntry> Widget : RootWidgets)
			{
				if (WidgetName == Widget->GetName())
				{
					ExistingWidget = Widget;
					break;
				}
			}

			if (!ExistingWidget.IsValid())
			{
				TSharedRef<FBindingEntry> NewWidget = MakeShared<FBindingEntry>();
				NewWidget->SetWidgetName(WidgetName);
				ExistingWidget = NewWidget;

				RootWidgets.Add(NewWidget);

				ReselectIfRequired(NewWidget);
			}

			TSharedRef<FBindingEntry> NewBindingEntry = MakeShared<FBindingEntry>();
			NewBindingEntry->SetBindingIndex(BindingIndex);
			ExistingWidget->AddChild(NewBindingEntry);
			ReselectIfRequired(NewBindingEntry);

			// create entries for conversion function parameters
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			const UFunction* Function = EditorSubsystem->GetConversionFunction(MVVMExtensionPtr->GetWidgetBlueprint(), Binding, UE::MVVM::IsForwardBinding(Binding.BindingType));
			if (Function != nullptr)
			{
				TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = BindingHelper::TryGetArgumentsForConversionFunction(Function);
				if (ArgumentsResult.HasValue())
				{
					for (const FProperty* Argument : ArgumentsResult.GetValue())
					{
						TSharedRef<FBindingEntry> NewArgumentEntry = MakeShared<FBindingEntry>();
						NewArgumentEntry->SetParameterName(BindingIndex, Argument->GetFName());
						NewBindingEntry->AddChild(NewArgumentEntry);
						ReselectIfRequired(NewArgumentEntry);
					}
				}
			}
		}
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();

		TreeView->SetItemSelection(NewSelectedEntries, true);

		for (const TSharedPtr<FBindingEntry>& Entry : RootWidgets)
		{
			Private::ExpandAll(TreeView, Entry);
		}
	}
}

TSharedRef<ITableRow> SBindingsList::GenerateEntryRow(TSharedPtr<FBindingEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedPtr<ITableRow> Row;

	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		switch (Entry->GetRowType())
		{
			case FBindingEntry::ERowType::Widget:
			{
				Row = SNew(SWidgetRow, OwnerTable, Entry, MVVMExtensionPtr->GetWidgetBlueprint());
				break;
			}
			case FBindingEntry::ERowType::Binding:
			{
				Row = SNew(SBindingRow, OwnerTable, Entry, MVVMExtensionPtr->GetWidgetBlueprint());
				break;
			}
			case FBindingEntry::ERowType::Parameter:
			{
				Row = SNew(SFunctionParameterRow, OwnerTable, Entry, MVVMExtensionPtr->GetWidgetBlueprint());
				break;
			}
		}

		return Row.ToSharedRef();
	}

	ensureMsgf(false, TEXT("Failed to create binding or widget row."));
	return SNew(STableRow<TSharedPtr<FBindingEntry>>, OwnerTable);
}

namespace Private
{
	void GatherChildBindings(UMVVMBlueprintView* BlueprintView, const TConstArrayView<TSharedPtr<FBindingEntry>>& Entries, TArray<const FMVVMBlueprintViewBinding*>& OutBindings)
	{
		for (const TSharedPtr<FBindingEntry>& Entry : Entries)
		{
			const FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(BlueprintView);
			if (Binding != nullptr)
			{
				OutBindings.AddUnique(Binding);
			}

			GatherChildBindings(BlueprintView, Entry->GetChildren(), OutBindings);
		}
	}
}

void SBindingsList::OnDeleteSelected()
{
	TArray<TSharedPtr<FBindingEntry>> Selection = TreeView->GetSelectedItems();
	if (Selection.Num() == 0)
	{
		return;
	}

	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		if (UMVVMBlueprintView* BlueprintView = MVVMExtensionPtr->GetBlueprintView())
		{
			const UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr->GetWidgetBlueprint();

			TArray<const FMVVMBlueprintViewBinding*> BindingsToRemove;
			Private::GatherChildBindings(BlueprintView, Selection, BindingsToRemove);

			if (BindingsToRemove.Num() == 0)
			{
				return;
			}

			TArray<FText> BindingDisplayNames;
			for (const FMVVMBlueprintViewBinding* Binding : BindingsToRemove)
			{
				BindingDisplayNames.Add(FText::FromString(Binding->GetDisplayNameString(WidgetBlueprint)));
			}

			const FText Message = FText::Format(BindingDisplayNames.Num() == 1 ?
				LOCTEXT("ConfirmDeleteSingle", "Are you sure that you want to delete this binding?\n\n{1}") :
				LOCTEXT("ConfirmDeleteMultiple", "Are you sure that you want to delete these {0} bindings?\n\n{1}"), 
			BindingDisplayNames.Num(), 
			FText::Join(FText::FromString("\n"), BindingDisplayNames));

			const FText Title = LOCTEXT("DeleteBindings", "Delete Bindings?");

			if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::Yes, Message, &Title) == EAppReturnType::Yes)
			{
				if (TSharedPtr<SBindingsPanel> BindingPanelPtr = BindingPanel.Pin())
				{
					BindingPanelPtr->OnBindingListSelectionChanged(TConstArrayView<FMVVMBlueprintViewBinding*>());
				}

				for (const FMVVMBlueprintViewBinding* Binding : BindingsToRemove)
				{
					BlueprintView->RemoveBinding(Binding);
				}
			}
		}
	}
}

TSharedPtr<SWidget> SBindingsList::OnSourceConstructContextMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	TArray<TSharedPtr<FBindingEntry>> Selection = TreeView->GetSelectedItems();
	if (Selection.Num() > 0)
	{
		FUIAction RemoveAction;
		RemoveAction.ExecuteAction = FExecuteAction::CreateSP(this, &SBindingsList::OnDeleteSelected);
		MenuBuilder.AddMenuEntry(LOCTEXT("RemoveBinding", "Remove Binding"), 
			LOCTEXT("RemoveBindingTooltip", "Remove this binding."), 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"), 
			RemoveAction);
	}

	return MenuBuilder.MakeWidget();
}

FReply SBindingsList::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		OnDeleteSelected();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SBindingsList::OnSourceListSelectionChanged(TSharedPtr<FBindingEntry> Entry, ESelectInfo::Type SelectionType) const
{
	if (bSelectionChangedGuard)
	{
		return;
	}
	TGuardValue<bool> ReentrantGuard(bSelectionChangedGuard, true);

	if (TSharedPtr<SBindingsPanel> BindingPanelPtr = BindingPanel.Pin())
	{
		if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
		{
			if (UMVVMBlueprintView* View = MVVMExtensionPtr->GetBlueprintView())
			{
				TArray<TSharedPtr<FBindingEntry>> SelectedEntries = TreeView->GetSelectedItems();
				TArray<FMVVMBlueprintViewBinding*> SelectedBindings;

				for (const TSharedPtr<FBindingEntry>& SelectedEntry : SelectedEntries)
				{
					if (FMVVMBlueprintViewBinding* SelectedBinding = Entry->GetBinding(View))
					{
						SelectedBindings.Add(SelectedBinding);
					}
				}

				BindingPanelPtr->OnBindingListSelectionChanged(SelectedBindings);
			}
		}
	}
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
