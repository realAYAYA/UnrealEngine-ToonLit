// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchListRowModel.h"

#include "DMXControlConsoleFaderGroup.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Styling/SlateTypes.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchListRowModel"

namespace UE::DMX::Private
{
	FDMXControlConsoleFixturePatchListRowModel::FDMXControlConsoleFixturePatchListRowModel(const TWeakObjectPtr<UDMXEntityFixturePatch> InWeakFixturePatch, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		: WeakFixturePatch(InWeakFixturePatch)
		, WeakEditorModel(InWeakEditorModel)
	{}

	bool FDMXControlConsoleFixturePatchListRowModel::IsRowEnabled() const
	{
		const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			return true;
		}

		const UDMXControlConsoleEditorModel* EditorModel = WeakEditorModel.Get();
		if (!EditorModel)
		{
			return true;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return true;
		}

		// Do only if active layout is not default Layout
		if (ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return true;
		}

		const UDMXControlConsoleFaderGroupController* FaderGroupController = ActiveLayout->FindFaderGroupControllerByFixturePatch(FixturePatch);
		return !IsValid(FaderGroupController);
	}

	ECheckBoxState FDMXControlConsoleFixturePatchListRowModel::GetFaderGroupEnabledState() const
	{
		const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			// Fixture groups shown in the list always have a fixture patch
			return ECheckBoxState::Undetermined;
		}

		const UDMXControlConsoleEditorModel* EditorModel = WeakEditorModel.Get();
		if (!EditorModel)
		{
			return ECheckBoxState::Undetermined;
		}

		const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		if (ControlConsoleData)
		{
			const UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
			return FaderGroup && FaderGroup->IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		return ECheckBoxState::Undetermined;
	}

	void FDMXControlConsoleFixturePatchListRowModel::SetFaderGroupEnabled(bool bEnable)
	{
		const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			return;
		}

		const UDMXControlConsoleEditorModel* EditorModel = WeakEditorModel.Get();
		if (!EditorModel)
		{
			return;
		}

		const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		if (!ControlConsoleData)
		{
			return;
		}

		if (UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch))
		{
			const FText TransactionText = bEnable ?
				LOCTEXT("EnableFaderGroupTransaction", "Enable Fader Group") :
				LOCTEXT("DisableFaderGroupTransaction", "Disable Fader Group");
			const FScopedTransaction SetFaderGroupEnabledTransaction(TransactionText);

			FaderGroup->Modify();
			FaderGroup->SetEnabled(bEnable);
		}
	}
}

#undef LOCTEXT_NAMESPACE 
