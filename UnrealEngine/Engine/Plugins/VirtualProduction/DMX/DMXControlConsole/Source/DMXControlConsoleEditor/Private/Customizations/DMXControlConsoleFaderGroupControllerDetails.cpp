// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroupControllerDetails.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "IPropertyUtilities.h"
#include "Layout/Visibility.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderGroupControllerDetails"

namespace UE::DMX::Private
{
	FDMXControlConsoleFaderGroupControllerDetails::FDMXControlConsoleFaderGroupControllerDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		: WeakEditorModel(InWeakEditorModel)
	{}

	TSharedRef<IDetailCustomization> FDMXControlConsoleFaderGroupControllerDetails::MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	{
		return MakeShared<FDMXControlConsoleFaderGroupControllerDetails>(InWeakEditorModel);
	}

	void FDMXControlConsoleFaderGroupControllerDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
	{
		PropertyUtilities = InDetailLayout.GetPropertyUtilities();

		InDetailLayout.HideCategory("DMX Controller");

		const TSharedRef<IPropertyHandle> UserNameHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderGroupController::GetUserNamePropertyName());
		InDetailLayout.HideProperty(UserNameHandle);
		const TSharedRef<IPropertyHandle> EditorColorHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderGroupController::GetEditorColorPropertyName());
		InDetailLayout.HideProperty(EditorColorHandle);

		IDetailCategoryBuilder& FaderGroupControllerCategory = InDetailLayout.EditCategory("DMX Fader Group Controller", FText::GetEmpty());
		FaderGroupControllerCategory.AddProperty(UserNameHandle);
		FaderGroupControllerCategory.AddProperty(EditorColorHandle)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &FDMXControlConsoleFaderGroupControllerDetails::GetEditorColorVisibility));
	
		// Lock CheckBox section
		FaderGroupControllerCategory.AddCustomRow(FText::GetEmpty())
			.NameContent()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("FaderGroupLoxkCheckBox", "Is Locked"))
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FDMXControlConsoleFaderGroupControllerDetails::IsLockChecked)
				.OnCheckStateChanged(this, &FDMXControlConsoleFaderGroupControllerDetails::OnLockToggleChanged)
			];

		// Clear button section
		FaderGroupControllerCategory.AddCustomRow(FText::GetEmpty())
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &FDMXControlConsoleFaderGroupControllerDetails::GetClearButtonVisibility))
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(5.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.OnClicked(this, &FDMXControlConsoleFaderGroupControllerDetails::OnClearButtonClicked)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(LOCTEXT("ClearButtonTitle", "Clear"))
					]
				]
			];
	}

	bool FDMXControlConsoleFaderGroupControllerDetails::AreAllFaderGroupControllersUnpatched() const
	{
		const TArray<UDMXControlConsoleFaderGroupController*> SelectedFaderGroupControllers = GetValidFaderGroupControllersBeingEdited();
		const bool bAreAllFaderGroupControllersUnpatched = Algo::AllOf(SelectedFaderGroupControllers,
			[](const UDMXControlConsoleFaderGroupController* FaderGroupController)
			{
				return FaderGroupController && !FaderGroupController->HasFixturePatch();
			});

		return bAreAllFaderGroupControllersUnpatched;
	}

	bool FDMXControlConsoleFaderGroupControllerDetails::IsFaderGroupControllersColorEditable() const
	{
		const TArray<UDMXControlConsoleFaderGroupController*> SelectedFaderGroupControllers = GetValidFaderGroupControllersBeingEdited();
		const bool bAreControllersUnpatchedOrMultiple = Algo::AllOf(SelectedFaderGroupControllers,
			[](const UDMXControlConsoleFaderGroupController* FaderGroupController)
			{
				return 
					FaderGroupController &&
					(!FaderGroupController->HasFixturePatch() ||
					FaderGroupController->GetFaderGroups().Num() > 1);
			});

		return bAreControllersUnpatchedOrMultiple;
	}

	ECheckBoxState FDMXControlConsoleFaderGroupControllerDetails::IsLockChecked() const
	{
		const TArray<UDMXControlConsoleFaderGroupController*> SelectedFaderGroupControllers = GetValidFaderGroupControllersBeingEdited();
		const bool bAreAllFaderGroupControllersUnlocked = Algo::AllOf(SelectedFaderGroupControllers, 
			[](const UDMXControlConsoleFaderGroupController* SelectedFaderGroupController)
			{
				return SelectedFaderGroupController && !SelectedFaderGroupController->IsLocked();
			});

		if (bAreAllFaderGroupControllersUnlocked)
		{
			return ECheckBoxState::Unchecked;
		}

		const bool bIsAnyFaderGroupControllerUnlocked = Algo::AnyOf(SelectedFaderGroupControllers, 
			[](const UDMXControlConsoleFaderGroupController* SelectedFaderGroupController)
			{
				return SelectedFaderGroupController && !SelectedFaderGroupController->IsLocked();
			});

		return bIsAnyFaderGroupControllerUnlocked ? ECheckBoxState::Undetermined : ECheckBoxState::Checked;
	}

	FReply FDMXControlConsoleFaderGroupControllerDetails::OnClearButtonClicked()
	{
		if (!WeakEditorModel.IsValid())
		{
			return FReply::Handled();
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = WeakEditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return FReply::Handled();
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout || ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return FReply::Handled();
		}

		const FScopedTransaction FaderGroupControllerFixturePatchClearTransaction(LOCTEXT("FaderGroupControllerFixturePatchClearTransaction", "Clear Fixture Patch"));

		TArray<UObject*> FaderGroupControllersToSelect;
		TArray<UObject*> FaderGroupControllersToUnselect;
		const TArray<UDMXControlConsoleFaderGroupController*> SelectedFaderGroupControllers = GetValidFaderGroupControllersBeingEdited();
		for (UDMXControlConsoleFaderGroupController* SelectedFaderGroupController : SelectedFaderGroupControllers)
		{
			if (!SelectedFaderGroupController || !SelectedFaderGroupController->HasFixturePatch())
			{
				continue;
			}

			UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = SelectedFaderGroupController->GetFaderGroups();
			if (!ControlConsoleData || FaderGroups.IsEmpty())
			{
				continue;
			}

			// Create a new unpatched Fader Group 
			UDMXControlConsoleFaderGroupRow& OwnerRow = FaderGroups[0]->GetOwnerFaderGroupRowChecked();
			OwnerRow.PreEditChange(nullptr);
			UDMXControlConsoleFaderGroup* NewFaderGroup = OwnerRow.AddFaderGroup(FaderGroups[0]->GetIndex());
			OwnerRow.PostEditChange();
			if (!NewFaderGroup)
			{
				continue;
			}

			const int32 RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(SelectedFaderGroupController);
			const int32 ColumnIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(SelectedFaderGroupController);

			// Create a new Fader Group Controller to replace the patched one
			ActiveLayout->PreEditChange(nullptr);
			ActiveLayout->RemoveFromActiveFaderGroupControllers(SelectedFaderGroupController);
			UDMXControlConsoleFaderGroupController* NewController = ActiveLayout->AddToLayout(NewFaderGroup, NewFaderGroup->GetFaderGroupName(), RowIndex, ColumnIndex);
			if (NewController)
			{
				NewController->Modify();
				NewController->SetIsActive(true);
				NewController->SetIsExpanded(SelectedFaderGroupController->IsExpanded());

				ActiveLayout->AddToActiveFaderGroupControllers(NewController);
			}
			ActiveLayout->PostEditChange();

			SelectedFaderGroupController->Modify();
			SelectedFaderGroupController->SetIsActive(false);
			SelectedFaderGroupController->Destroy();

			FaderGroupControllersToSelect.Add(NewController);
			FaderGroupControllersToUnselect.Add(SelectedFaderGroupController);
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = WeakEditorModel->GetSelectionHandler();
		constexpr bool bNotifySelectionChange = false;
		SelectionHandler->AddToSelection(FaderGroupControllersToSelect, bNotifySelectionChange);
		SelectionHandler->RemoveFromSelection(FaderGroupControllersToUnselect);

		WeakEditorModel->RequestUpdateEditorModel();

		return FReply::Handled();
	}

	void FDMXControlConsoleFaderGroupControllerDetails::OnLockToggleChanged(ECheckBoxState CheckState)
	{
		const TArray<UDMXControlConsoleFaderGroupController*> SelectedFaderGroupControllers = GetValidFaderGroupControllersBeingEdited();
		for (UDMXControlConsoleFaderGroupController* SelectedFaderGroupController : SelectedFaderGroupControllers)
		{
			if (SelectedFaderGroupController && SelectedFaderGroupController->IsMatchingFilter())
			{
				const bool bIsLocked = CheckState == ECheckBoxState::Checked;
				SelectedFaderGroupController->SetLocked(bIsLocked);
			}
		}
	}

	EVisibility FDMXControlConsoleFaderGroupControllerDetails::GetEditorColorVisibility() const
	{
		return IsFaderGroupControllersColorEditable() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility FDMXControlConsoleFaderGroupControllerDetails::GetClearButtonVisibility() const
	{
		bool bIsVisible = !AreAllFaderGroupControllersUnpatched();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleLayouts() : nullptr;
		if (ControlConsoleLayouts)
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
			bIsVisible &= ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked();
		}
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	TArray<UDMXControlConsoleFaderGroupController*> FDMXControlConsoleFaderGroupControllerDetails::GetValidFaderGroupControllersBeingEdited() const
	{
		const TArray<TWeakObjectPtr<UObject>>& EditedObjects = PropertyUtilities->GetSelectedObjects();
		TArray<UDMXControlConsoleFaderGroupController*> Result;
		Algo::TransformIf(EditedObjects, Result,
			[](TWeakObjectPtr<UObject> Object)
			{
				return IsValid(Cast<UDMXControlConsoleFaderGroupController>(Object.Get()));
			},
			[](TWeakObjectPtr<UObject> Object)
			{
				return Cast<UDMXControlConsoleFaderGroupController>(Object.Get());
			});

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
