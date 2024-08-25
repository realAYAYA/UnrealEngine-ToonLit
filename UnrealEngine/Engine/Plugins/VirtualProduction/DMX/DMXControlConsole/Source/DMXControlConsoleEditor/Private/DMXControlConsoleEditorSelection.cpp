// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorSelection.h"

#include "Algo/Sort.h"
#include "Algo/StableSort.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Models/DMXControlConsoleEditorModel.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorSelection"

FDMXControlConsoleEditorSelection::FDMXControlConsoleEditorSelection(UDMXControlConsoleEditorModel* InEditorModel)
	: EditorModel(InEditorModel)
{}

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleFaderGroupController* FaderGroupController, bool bNotifySelectionChange)
{
	if (FaderGroupController && FaderGroupController->IsActive())
	{
		SelectedFaderGroupControllers.AddUnique(FaderGroupController);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroupController::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleElementController* ElementController, bool bNotifySelectionChange)
{
	if (ElementController && ElementController->IsActive())
	{
		SelectedElementControllers.AddUnique(ElementController);

		UDMXControlConsoleFaderGroupController& FaderGroupController = ElementController->GetOwnerFaderGroupControllerChecked();
		SelectedFaderGroupControllers.AddUnique(&FaderGroupController);

		UpdateMultiSelectAnchor(UDMXControlConsoleElementController::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::AddToSelection(const TArray<UObject*> Objects, bool bNotifySelectionChange)
{
	if (Objects.IsEmpty())
	{
		return;
	}

	for (UObject* Object : Objects)
	{
		if (UDMXControlConsoleFaderGroupController* FaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(Object))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			AddToSelection(FaderGroupController, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Object))
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			AddToSelection(ElementController, bNotifyFaderSelectionChange);
		}
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::AddAllElementControllersFromFaderGroupControllerToSelection(UDMXControlConsoleFaderGroupController* FaderGroupController, bool bOnlyMatchingFilter, bool bNotifySelectionChange)
{
	if (FaderGroupController && FaderGroupController->IsActive())
	{
		const TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroupController->GetAllElementControllers();
		for (UDMXControlConsoleElementController* ElementController : AllElementControllers)
		{
			if (!ElementController || !ElementController->IsActive())
			{
				continue;
			}

			if (bOnlyMatchingFilter && !ElementController->IsMatchingFilter())
			{
				continue;
			}

			SelectedElementControllers.AddUnique(ElementController);
		}

		SelectedFaderGroupControllers.AddUnique(FaderGroupController);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroupController::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleFaderGroupController* FaderGroupController, bool bNotifySelectionChange)
{
	if (FaderGroupController && SelectedFaderGroupControllers.Contains(FaderGroupController))
	{
		constexpr bool bNotifyFadersSelectionChange = false;
		ClearElementControllersSelection(FaderGroupController, bNotifyFadersSelectionChange);
		SelectedFaderGroupControllers.Remove(FaderGroupController);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroupController::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleElementController* ElementController, bool bNotifySelectionChange)
{
	if (ElementController && SelectedElementControllers.Contains(ElementController))
	{
		SelectedElementControllers.Remove(ElementController);

		UpdateMultiSelectAnchor(UDMXControlConsoleElementController::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(const TArray<UObject*> Objects, bool bNotifySelectionChange)
{
	if (Objects.IsEmpty())
	{
		return;
	}

	for (UObject* Object : Objects)
	{
		if (UDMXControlConsoleFaderGroupController* FaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(Object))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			RemoveFromSelection(FaderGroupController, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Object))
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			RemoveFromSelection(ElementController, bNotifyFaderSelectionChange);
		}
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::Multiselect(UObject* ElementControllerOrFaderGroupControllerObject)
{
	const UClass* MultiSelectClass = ElementControllerOrFaderGroupControllerObject->GetClass();
	if (!ensureMsgf(MultiSelectClass == UDMXControlConsoleFaderGroupController::StaticClass() || ElementControllerOrFaderGroupControllerObject->IsA(UDMXControlConsoleElementController::StaticClass()), TEXT("Invalid type when trying to multiselect")))
	{
		return;
	}

	constexpr bool bNotifySelectionChange = false;
	RemoveInvalidObjectsFromSelection(bNotifySelectionChange);

	// Normal selection if nothing is selected or there's no valid anchor
	if (!MultiSelectAnchor.IsValid() ||
		(SelectedFaderGroupControllers.IsEmpty() && SelectedElementControllers.IsEmpty()))
	{
		if (UDMXControlConsoleFaderGroupController* FaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(ElementControllerOrFaderGroupControllerObject))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			AddToSelection(FaderGroupController, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(ElementControllerOrFaderGroupControllerObject))
		{
			constexpr bool bFaderSelectionChange = false;
			AddToSelection(ElementController, bFaderSelectionChange);
		}
		return;
	}

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	TArray<UObject*> ElementControllersAndFaderGroupControllers;
	const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
	for (UDMXControlConsoleFaderGroupController* AnyFaderGroupController : AllFaderGroupControllers)
	{
		if (!AnyFaderGroupController)
		{
			continue;
		}

		ElementControllersAndFaderGroupControllers.AddUnique(AnyFaderGroupController);
		for (UDMXControlConsoleElementController* AnyElementController : AnyFaderGroupController->GetAllElementControllers())
		{
			ElementControllersAndFaderGroupControllers.AddUnique(AnyElementController);
		}
	}

	const int32 IndexOfFaderGroupControllerAnchor = ElementControllersAndFaderGroupControllers.IndexOfByPredicate([this](const UObject* Object)
		{
			return MultiSelectAnchor == Object;
		});
	const int32 IndexOfElementControllerAnchor = ElementControllersAndFaderGroupControllers.IndexOfByPredicate([this](const UObject* Object)
		{
			return MultiSelectAnchor == Object;
		});

	const int32 IndexOfAnchor = FMath::Max(IndexOfFaderGroupControllerAnchor, IndexOfElementControllerAnchor);
	if (!ensureAlwaysMsgf(IndexOfAnchor != INDEX_NONE, TEXT("No previous selection when multi selecting, cannot multiselect.")))
	{
		return;
	}

	const int32 IndexOfSelection = ElementControllersAndFaderGroupControllers.IndexOfByKey(ElementControllerOrFaderGroupControllerObject);

	const int32 StartIndex = FMath::Min(IndexOfAnchor, IndexOfSelection);
	const int32 EndIndex = FMath::Max(IndexOfAnchor, IndexOfSelection);

	SelectedFaderGroupControllers.Reset();
	SelectedElementControllers.Reset();
	for (int32 IndexToSelect = StartIndex; IndexToSelect <= EndIndex; IndexToSelect++)
	{
		if (!ensureMsgf(ElementControllersAndFaderGroupControllers.IsValidIndex(IndexToSelect), TEXT("Invalid index when multiselecting")))
		{
			break;
		}

		if (UDMXControlConsoleFaderGroupController* FaderGroupControllerToSelect = Cast<UDMXControlConsoleFaderGroupController>(ElementControllersAndFaderGroupControllers[IndexToSelect]))
		{
			if (FaderGroupControllerToSelect  && FaderGroupControllerToSelect->IsActive() && FaderGroupControllerToSelect->IsMatchingFilter())
			{
				SelectedFaderGroupControllers.AddUnique(FaderGroupControllerToSelect);
			}
		}
		else if (UDMXControlConsoleElementController* ElementControllerToSelect = Cast<UDMXControlConsoleElementController>(ElementControllersAndFaderGroupControllers[IndexToSelect]))
		{
			if (ElementControllerToSelect && ElementControllerToSelect->IsActive() && ElementControllerToSelect->IsMatchingFilter())
			{
				SelectedElementControllers.AddUnique(ElementControllerToSelect);
			}
		}
	}
	if (!SelectedElementControllers.IsEmpty())
	{
		// Always select the fader group controller of the first selected element controller
		UDMXControlConsoleElementController* FirstSelectedElementController = CastChecked<UDMXControlConsoleElementController>(SelectedElementControllers[0]);
		SelectedFaderGroupControllers.AddUnique(&FirstSelectedElementController->GetOwnerFaderGroupControllerChecked());
	}

	OnSelectionChanged.Broadcast();
}

void FDMXControlConsoleEditorSelection::ReplaceInSelection(UDMXControlConsoleFaderGroupController* FaderGroupController)
{
	if (!FaderGroupController || !IsSelected(FaderGroupController))
	{
		return;
	}

	RemoveFromSelection(FaderGroupController);

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroupController*> AllActiveFaderGroupControllers = ActiveLayout->GetAllActiveFaderGroupControllers();
	if (AllActiveFaderGroupControllers.Num() <= 1)
	{
		return;
	}

	const int32 Index = AllActiveFaderGroupControllers.IndexOfByKey(FaderGroupController);

	int32 NewIndex = Index - 1;
	if (!AllActiveFaderGroupControllers.IsValidIndex(NewIndex))
	{
		NewIndex = Index + 1;
	}

	UDMXControlConsoleFaderGroupController* NewSelectedFaderGroupController = AllActiveFaderGroupControllers.IsValidIndex(NewIndex) ? AllActiveFaderGroupControllers[NewIndex] : nullptr;
	if (!NewSelectedFaderGroupController)
	{
		return;
	}

	AddToSelection(NewSelectedFaderGroupController);
}

void FDMXControlConsoleEditorSelection::ReplaceInSelection(UDMXControlConsoleElementController* ElementController)
{
	if (!ElementController || !IsSelected(ElementController))
	{
		return;
	}

	RemoveFromSelection(ElementController);

	const UDMXControlConsoleFaderGroupController& FaderGroupController = ElementController->GetOwnerFaderGroupControllerChecked();
	const TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroupController.GetAllElementControllers();
	if (AllElementControllers.Num() <= 1)
	{
		return;
	}

	const int32 IndexToReplace = AllElementControllers.IndexOfByKey(ElementController);
	int32 NewIndex = IndexToReplace - 1;
	if (!AllElementControllers.IsValidIndex(NewIndex))
	{
		NewIndex = IndexToReplace + 1;
	}

	UDMXControlConsoleElementController* NewSelectedElementController = AllElementControllers.IsValidIndex(NewIndex) ? AllElementControllers[NewIndex] : nullptr;
	AddToSelection(NewSelectedElementController);
}

bool FDMXControlConsoleEditorSelection::IsSelected(UDMXControlConsoleFaderGroupController* FaderGroupController) const
{
	return SelectedFaderGroupControllers.Contains(FaderGroupController);
}

bool FDMXControlConsoleEditorSelection::IsSelected(UDMXControlConsoleElementController* ElementController) const
{
	return SelectedElementControllers.Contains(ElementController);
}

void FDMXControlConsoleEditorSelection::SelectAll(bool bOnlyMatchingFilter)
{
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	ClearSelection(false);

	const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
	for (UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
	{
		if (FaderGroupController && FaderGroupController->IsActive())
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			AddAllElementControllersFromFaderGroupControllerToSelection(FaderGroupController, bOnlyMatchingFilter, bNotifyFaderSelectionChange);
		}
	}

	OnSelectionChanged.Broadcast();
}

void FDMXControlConsoleEditorSelection::RemoveInvalidObjectsFromSelection(bool bNotifySelectionChange)
{
	SelectedFaderGroupControllers.Remove(nullptr);
	SelectedElementControllers.Remove(nullptr);

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::ClearElementControllersSelection(UDMXControlConsoleFaderGroupController* FaderGroupController, bool bNotifySelectionChange)
{
	if (!FaderGroupController || !SelectedFaderGroupControllers.Contains(FaderGroupController))
	{
		return;
	}

	TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroupController->GetAllElementControllers();

	auto IsFaderGroupOwnerLambda = [AllElementControllers](const TWeakObjectPtr<UObject> SelectedObject)
	{
		const UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedObject);
		if (!SelectedElementController)
		{
			return true;
		}

		if (AllElementControllers.Contains(SelectedElementController))
		{
			return true;
		}

		return false;
	};

	SelectedElementControllers.RemoveAll(IsFaderGroupOwnerLambda);

	if (!MultiSelectAnchor.IsValid() || MultiSelectAnchor->IsA(UDMXControlConsoleElementController::StaticClass()))
	{
		UpdateMultiSelectAnchor(UDMXControlConsoleElementController::StaticClass());
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::ClearSelection(bool bNotifySelectionChange)
{
	SelectedFaderGroupControllers.Reset();
	SelectedElementControllers.Reset();

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

UDMXControlConsoleFaderGroupController* FDMXControlConsoleEditorSelection::GetFirstSelectedFaderGroupController(bool bReverse) const
{
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return nullptr;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return nullptr;
	}

	auto SortSelectedFaderGroupControllersLambda = [ActiveLayout](TWeakObjectPtr<UObject> FaderGroupControllerObjectA, TWeakObjectPtr<UObject> FaderGroupControllerObjectB)
		{
			const UDMXControlConsoleFaderGroupController* FaderGroupControllerA = Cast<UDMXControlConsoleFaderGroupController>(FaderGroupControllerObjectA);
			const UDMXControlConsoleFaderGroupController* FaderGroupControllerB = Cast<UDMXControlConsoleFaderGroupController>(FaderGroupControllerObjectB);

			if (!FaderGroupControllerA || !FaderGroupControllerB)
			{
				return false;
			}

			const int32 RowIndexA = ActiveLayout->GetFaderGroupControllerRowIndex(FaderGroupControllerA);
			const int32 RowIndexB = ActiveLayout->GetFaderGroupControllerRowIndex(FaderGroupControllerB);

			if (RowIndexA != RowIndexB)
			{
				return RowIndexA < RowIndexB;
			}

			const int32 IndexA = ActiveLayout->GetFaderGroupControllerColumnIndex(FaderGroupControllerA);
			const int32 IndexB = ActiveLayout->GetFaderGroupControllerColumnIndex(FaderGroupControllerB);

			return IndexA < IndexB;
		};

	TArray<TWeakObjectPtr<UObject>> CurrentSelectedFaderGroupControllers = GetSelectedFaderGroupControllers();
	Algo::Sort(CurrentSelectedFaderGroupControllers, SortSelectedFaderGroupControllersLambda);
	
	const TWeakObjectPtr<UObject> FirstFaderGroupController = bReverse ? CurrentSelectedFaderGroupControllers.Last() : CurrentSelectedFaderGroupControllers[0];
	return Cast<UDMXControlConsoleFaderGroupController>(FirstFaderGroupController);
}

UDMXControlConsoleElementController* FDMXControlConsoleEditorSelection::GetFirstSelectedElementController(bool bReverse) const
{
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return nullptr;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return nullptr;
	}

	const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
	if (FaderGroupControllers.IsEmpty())
	{
		return nullptr;
	}

	const auto SortSelectedElementControllerLambda = [FaderGroupControllers](const TWeakObjectPtr<UObject>& ElementControllerObjectA, const TWeakObjectPtr<UObject>& ElementControllerObjectB)
		{
			const UDMXControlConsoleElementController* ElementControllerA = Cast<UDMXControlConsoleElementController>(ElementControllerObjectA);
			const UDMXControlConsoleElementController* ElementControllerB = Cast<UDMXControlConsoleElementController>(ElementControllerObjectB);
			if (!ElementControllerA || !ElementControllerB)
			{
				return false;
			}

			const UDMXControlConsoleFaderGroupController& FaderGroupControllerA = ElementControllerA->GetOwnerFaderGroupControllerChecked();
			const UDMXControlConsoleFaderGroupController& FaderGroupControllerB = ElementControllerB->GetOwnerFaderGroupControllerChecked();

			const int32 FaderGroupControllerIndexA = FaderGroupControllers.IndexOfByKey(&FaderGroupControllerA);
			const int32 FaderGroupControllerIndexB = FaderGroupControllers.IndexOfByKey(&FaderGroupControllerB);

			if (FaderGroupControllerIndexA != FaderGroupControllerIndexB)
			{
				return FaderGroupControllerIndexA < FaderGroupControllerIndexB;
			}

			const int32 IndexA = FaderGroupControllerA.GetAllElementControllers().IndexOfByKey(ElementControllerA);
			const int32 IndexB = FaderGroupControllerB.GetAllElementControllers().IndexOfByKey(ElementControllerB);

			return IndexA < IndexB;
		};

	TArray<TWeakObjectPtr<UObject>> CurrentSelectedElementControllers = GetSelectedElementControllers();
	Algo::Sort(CurrentSelectedElementControllers, SortSelectedElementControllerLambda);
	
	const TWeakObjectPtr<UObject> FirstElementController = bReverse ? CurrentSelectedElementControllers.Last() : CurrentSelectedElementControllers[0];
	return Cast<UDMXControlConsoleElementController>(FirstElementController);
}

TArray<UDMXControlConsoleElementController*> FDMXControlConsoleEditorSelection::GetSelectedElementControllersFromFaderGroupController(UDMXControlConsoleFaderGroupController* FaderGroupController) const
{
	TArray<UDMXControlConsoleElementController*> CurrentSelectedElementControllers;

	if (!FaderGroupController)
	{
		return CurrentSelectedElementControllers;
	}

	TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroupController->GetAllElementControllers();
	for (UDMXControlConsoleElementController* ElementController : AllElementControllers)
	{
		if (!ElementController)
		{
			continue;
		}

		if (!SelectedElementControllers.Contains(ElementController))
		{
			continue;
		}

		CurrentSelectedElementControllers.Add(ElementController);
	}

	return CurrentSelectedElementControllers;
}

TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FDMXControlConsoleEditorSelection::GetSelectedFaderGroups(bool bSort) const
{
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> SelectedFaderGroups;
	for (const TWeakObjectPtr<UObject>& SelectedFaderGroupControllerObject : SelectedFaderGroupControllers)
	{
		UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(SelectedFaderGroupControllerObject);
		if (!SelectedFaderGroupController || !SelectedFaderGroupController->IsMatchingFilter())
		{
			continue;
		}

		SelectedFaderGroups.Append(SelectedFaderGroupController->GetFaderGroups());
	}

	if (bSort)
	{
		const auto SortFaderGroupsByAbsoluteAddressLambda = [](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& Item)
			{
				const UDMXEntityFixturePatch* FixturePatch = Item.IsValid() ? Item->GetFixturePatch() : nullptr;
				if (FixturePatch) 
				{
					return  (int64)FixturePatch->GetUniverseID() * DMX_UNIVERSE_SIZE + FixturePatch->GetStartingChannel();
				}

				return TNumericLimits<int64>::Max();
			};

		Algo::StableSortBy(SelectedFaderGroups, SortFaderGroupsByAbsoluteAddressLambda);
	}

	return SelectedFaderGroups;
}

TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> FDMXControlConsoleEditorSelection::GetSelectedElements(bool bSort) const
{
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> SelectedElements;
	for (const TWeakObjectPtr<UObject>& SelectedElementControllerObject : SelectedElementControllers)
	{
		UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedElementControllerObject);
		if (!SelectedElementController || !SelectedElementController->IsMatchingFilter())
		{
			continue;
		}

		SelectedElements.Append(SelectedElementController->GetElements());
	}

	if (bSort)
	{
		const auto SortElementsByStartingAddressLambda = [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
			{
				return InElement->GetStartingAddress();
			};

		Algo::SortBy(SelectedElements, SortElementsByStartingAddressLambda);
	}

	return SelectedElements;
}

void FDMXControlConsoleEditorSelection::UpdateMultiSelectAnchor(UClass* PreferedClass)
{
	if (!ensureMsgf(PreferedClass == UDMXControlConsoleFaderGroupController::StaticClass() || PreferedClass == UDMXControlConsoleElementController::StaticClass(), TEXT("Invalid class when trying to update multi select anchor")))
	{
		return;
	}

	if (PreferedClass == UDMXControlConsoleFaderGroupController::StaticClass() && !SelectedFaderGroupControllers.IsEmpty())
	{
		MultiSelectAnchor = SelectedFaderGroupControllers.Last();
	}
	else if (!SelectedElementControllers.IsEmpty())
	{
		MultiSelectAnchor = SelectedElementControllers.Last();
	}
	else if (!SelectedFaderGroupControllers.IsEmpty())
	{
		MultiSelectAnchor = SelectedFaderGroupControllers.Last();
	}
	else
	{
		MultiSelectAnchor = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
