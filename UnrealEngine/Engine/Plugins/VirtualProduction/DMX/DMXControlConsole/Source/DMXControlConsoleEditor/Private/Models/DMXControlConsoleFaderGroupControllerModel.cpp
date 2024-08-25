// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroupControllerModel.h"

#include "Algo/Transform.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"


namespace UE::DMX::Private
{
	FDMXControlConsoleFaderGroupControllerModel::FDMXControlConsoleFaderGroupControllerModel(const TWeakObjectPtr<UDMXControlConsoleFaderGroupController> InWeakFaderGroupController, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		: WeakFaderGroupController(InWeakFaderGroupController)
		, WeakEditorModel(InWeakEditorModel)
	{}

	UDMXControlConsoleFaderGroupController* FDMXControlConsoleFaderGroupControllerModel::GetFaderGroupController() const
	{
		return WeakFaderGroupController.Get();
	}

	UDMXControlConsoleFaderGroup* FDMXControlConsoleFaderGroupControllerModel::GetFirstAvailableFaderGroup() const
	{
		if (!WeakFaderGroupController.IsValid() || WeakFaderGroupController->GetFaderGroups().IsEmpty())
		{
			return nullptr;
		}

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = WeakFaderGroupController->GetFaderGroups();
		if (FaderGroups.IsEmpty())
		{
			return nullptr;
		}

		return FaderGroups[0].Get();
	}

	TArray<UDMXControlConsoleElementController*> FDMXControlConsoleFaderGroupControllerModel::GetMatchingFilterElementControllersOnly() const
	{
		TArray<UDMXControlConsoleElementController*> MatchingFilterElementControllers;
		if (WeakFaderGroupController.IsValid())
		{
			const TArray<UDMXControlConsoleElementController*>& ElementControllers = WeakFaderGroupController->GetElementControllers();
			Algo::TransformIf(ElementControllers, MatchingFilterElementControllers,
				[](const UDMXControlConsoleElementController* ElementController)
				{
					return ElementController && ElementController->IsMatchingFilter();
				},
				[](UDMXControlConsoleElementController* ElementController)
				{
					return ElementController;
				});
		}

		return MatchingFilterElementControllers;
	}

	FString FDMXControlConsoleFaderGroupControllerModel::GetRelativeControllerName() const
	{
		if (!WeakFaderGroupController.IsValid())
		{
			return FString();
		}

		if (!HasSingleFaderGroup())
		{
			return WeakFaderGroupController->GetUserName();
		}

		const UDMXControlConsoleFaderGroup* FirstFaderGroup = GetFirstAvailableFaderGroup();
		if (!FirstFaderGroup)
		{
			return WeakFaderGroupController->GetUserName();
		}

		return FirstFaderGroup->GetFaderGroupName();
	}

	bool FDMXControlConsoleFaderGroupControllerModel::HasSingleFaderGroup() const
	{
		return WeakFaderGroupController.IsValid() && WeakFaderGroupController->GetFaderGroups().Num() == 1;
	}

	bool FDMXControlConsoleFaderGroupControllerModel::CanAddFaderGroupController() const
	{
		if (!WeakFaderGroupController.IsValid() || !WeakEditorModel.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = WeakEditorModel->GetControlConsoleLayouts(); 
		if (!ControlConsoleData || !ControlConsoleLayouts)
		{
			return false;
		}

		// True if active layout is User Layout, no vertical layout mode and no global filter
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		return
			IsValid(ActiveLayout) &&
			ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked() &&
			ActiveLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Vertical &&
			ControlConsoleData->FilterString.IsEmpty();
	}

	bool FDMXControlConsoleFaderGroupControllerModel::CanAddFaderGroupControllerOnNewRow() const
	{
		if (!WeakFaderGroupController.IsValid() || !WeakEditorModel.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = WeakEditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleData || !ControlConsoleLayouts)
		{
			return false;
		}

		// True if active layout is user layout and there's no global filter
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return false;
		}

		bool bCanAdd =
			ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked() &&
			ActiveLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Horizontal &&
			ControlConsoleData->FilterString.IsEmpty();

		// True if grid layout mode and this is the first active fader group controller in the row
		if (ActiveLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Grid)
		{
			bCanAdd &=
				WeakFaderGroupController->IsActive() &&
				ActiveLayout->GetFaderGroupControllerColumnIndex(WeakFaderGroupController.Get()) == 0;
		}

		return bCanAdd;
	}

	bool FDMXControlConsoleFaderGroupControllerModel::CanAddElementController() const
	{
		// True if the controller has one unpatched fader group and there's no global filter
		bool bCanAdd = 
			WeakFaderGroupController.IsValid() && 
			!WeakFaderGroupController->HasFixturePatch() &&
			HasSingleFaderGroup();

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		if (ControlConsoleData)
		{
			bCanAdd &= ControlConsoleData->FilterString.IsEmpty();
		}

		return bCanAdd;
	}

	bool FDMXControlConsoleFaderGroupControllerModel::IsLocked() const
	{
		return WeakFaderGroupController.IsValid() && WeakFaderGroupController->IsLocked();
	}
}
