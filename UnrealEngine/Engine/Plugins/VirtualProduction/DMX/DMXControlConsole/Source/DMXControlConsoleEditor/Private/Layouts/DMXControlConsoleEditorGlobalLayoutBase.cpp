// Copyright Epic Games, Inc. All Rights Reserved.

#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "Controllers/DMXControlConsoleCellAttributeController.h"
#include "Controllers/DMXControlConsoleElementController.h"
#include "Controllers/DMXControlConsoleFaderGroupController.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorGlobalLayoutRow.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntity.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorGlobalLayoutBase"

UDMXControlConsoleFaderGroupController* UDMXControlConsoleEditorGlobalLayoutBase::AddToLayout(UDMXControlConsoleFaderGroup* InFaderGroup, const FString& ControllerName, const int32 RowIndex, const int32 ColumnIndex)
{
	if (!InFaderGroup)
	{
		return nullptr;
	}

	const TArray<UDMXControlConsoleFaderGroup*> FaderGroupAsArray = { InFaderGroup };
	UDMXControlConsoleFaderGroupController* NewController = AddToLayout(FaderGroupAsArray, ControllerName, RowIndex, ColumnIndex);
	return NewController;
}

UDMXControlConsoleFaderGroupController* UDMXControlConsoleEditorGlobalLayoutBase::AddToLayout(const TArray<UDMXControlConsoleFaderGroup*> InFaderGroups, const FString& ControllerName, const int32 RowIndex, const int32 ColumnIndex)
{
	if (InFaderGroups.IsEmpty() || !LayoutRows.IsValidIndex(RowIndex))
	{
		return nullptr;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRows[RowIndex];
	if (!LayoutRow)
	{
		return nullptr;
	}

	LayoutRow->Modify();
	return LayoutRow->CreateFaderGroupController(InFaderGroups, ControllerName, ColumnIndex);
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::AddNewRowToLayout(const int32 RowIndex)
{
	if (RowIndex > LayoutRows.Num())
	{
		return nullptr;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = NewObject<UDMXControlConsoleEditorGlobalLayoutRow>(this, NAME_None, RF_Transactional);
	const int32 ValidRowIndex = RowIndex < 0 ? LayoutRows.Num() : RowIndex;
	LayoutRows.Insert(LayoutRow, ValidRowIndex);

	return LayoutRow;
}

UDMXControlConsoleEditorLayouts& UDMXControlConsoleEditorGlobalLayoutBase::GetOwnerEditorLayoutsChecked() const
{
	UDMXControlConsoleEditorLayouts* Outer = Cast<UDMXControlConsoleEditorLayouts>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get layout owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::GetLayoutRow(const UDMXControlConsoleFaderGroupController* FaderGroupController) const
{
	const int32 RowIndex = GetFaderGroupControllerRowIndex(FaderGroupController);
	return LayoutRows.IsValidIndex(RowIndex) ? LayoutRows[RowIndex] : nullptr;
}

TArray<UDMXControlConsoleFaderGroupController*> UDMXControlConsoleEditorGlobalLayoutBase::GetAllFaderGroupControllers() const
{
	TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers;
	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow)
		{
			AllFaderGroupControllers.Append(LayoutRow->GetFaderGroupControllers());
		}
	}

	return AllFaderGroupControllers;
}

void UDMXControlConsoleEditorGlobalLayoutBase::AddToActiveFaderGroupControllers(UDMXControlConsoleFaderGroupController* FaderGroupController)
{
	if (FaderGroupController)
	{
		ActiveFaderGroupControllers.AddUnique(FaderGroupController);
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::RemoveFromActiveFaderGroupControllers(UDMXControlConsoleFaderGroupController* FaderGroupController)
{
	if (FaderGroupController)
	{
		ActiveFaderGroupControllers.Remove(FaderGroupController);
	}
}

TArray<UDMXControlConsoleFaderGroupController*> UDMXControlConsoleEditorGlobalLayoutBase::GetAllActiveFaderGroupControllers() const
{
	TArray<UDMXControlConsoleFaderGroupController*> AllActiveFaderGroupControllers = GetAllFaderGroupControllers();
	AllActiveFaderGroupControllers.RemoveAll([](const UDMXControlConsoleFaderGroupController* FaderGroupController)
		{
			return FaderGroupController && !FaderGroupController->IsActive();
		});

	return AllActiveFaderGroupControllers;
}

void UDMXControlConsoleEditorGlobalLayoutBase::SetActiveFaderGroupControllersInLayout(bool bActive)
{
	const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = GetAllFaderGroupControllers();
	for (UDMXControlConsoleFaderGroupController* FaderGroupController : AllFaderGroupControllers)
	{
		if (!FaderGroupController)
		{
			continue;
		}

		const bool bActivate = ActiveFaderGroupControllers.Contains(FaderGroupController) ? bActive : !bActive;
		FaderGroupController->Modify();
		FaderGroupController->SetIsActive(bActivate);
	}
}

int32 UDMXControlConsoleEditorGlobalLayoutBase::GetFaderGroupControllerRowIndex(const UDMXControlConsoleFaderGroupController* FaderGroupController) const
{
	if (!FaderGroupController)
	{
		return INDEX_NONE;
	}

	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow &&
			LayoutRow->GetFaderGroupControllers().Contains(FaderGroupController))
		{
			return LayoutRows.IndexOfByKey(LayoutRow);
		}
	}

	return INDEX_NONE;
}

int32 UDMXControlConsoleEditorGlobalLayoutBase::GetFaderGroupControllerColumnIndex(const UDMXControlConsoleFaderGroupController* FaderGroupController) const
{
	if (!FaderGroupController)
	{
		return INDEX_NONE;
	}

	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow && 
			LayoutRow->GetFaderGroupControllers().Contains(FaderGroupController))
		{
			return LayoutRow->GetIndex(FaderGroupController);
		}
	}

	return INDEX_NONE;
}

void UDMXControlConsoleEditorGlobalLayoutBase::SetLayoutMode(const EDMXControlConsoleLayoutMode NewLayoutMode)
{
	if (LayoutMode == NewLayoutMode)
	{
		return;
	}

	LayoutMode = NewLayoutMode;

	const UDMXControlConsoleEditorLayouts& OwnerEditorLayouts = GetOwnerEditorLayoutsChecked();
	OwnerEditorLayouts.OnLayoutModeChanged.Broadcast();
}

bool UDMXControlConsoleEditorGlobalLayoutBase::ContainsFaderGroupController(const UDMXControlConsoleFaderGroupController* FaderGroupController) const
{
	return GetFaderGroupControllerRowIndex(FaderGroupController) != INDEX_NONE;
}

bool UDMXControlConsoleEditorGlobalLayoutBase::ContainsFaderGroup(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	if (!FaderGroup)
	{
		return false;
	}

	const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = GetAllFaderGroupControllers();
	const bool bIsFaderGroupPossessedByAnyControllerInLayout = Algo::AnyOf(AllFaderGroupControllers, 
		[FaderGroup](const UDMXControlConsoleFaderGroupController* FaderGroupController)
		{
			return FaderGroupController && FaderGroupController->GetFaderGroups().Contains(FaderGroup);
		});

	return bIsFaderGroupPossessedByAnyControllerInLayout;
}

UDMXControlConsoleFaderGroupController* UDMXControlConsoleEditorGlobalLayoutBase::FindFaderGroupControllerByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const
{
	if (!InFixturePatch)
	{
		return nullptr;
	}

	const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = GetAllFaderGroupControllers();
	UDMXControlConsoleFaderGroupController* const* FaderGroupControllerPtr =
		Algo::FindByPredicate(AllFaderGroupControllers,
			[InFixturePatch](const UDMXControlConsoleFaderGroupController* FaderGroupController)
			{
				if (!FaderGroupController)
				{
					return false;
				}

				const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
				const TWeakObjectPtr<UDMXControlConsoleFaderGroup>* FaderGroupPtr =
					Algo::FindByPredicate(FaderGroups,
						[InFixturePatch](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
						{
							return FaderGroup.IsValid() && FaderGroup->GetFixturePatch() == InFixturePatch;
						});

				return FaderGroupPtr != nullptr;
			});

	return FaderGroupControllerPtr ? *FaderGroupControllerPtr : nullptr;
}

void UDMXControlConsoleEditorGlobalLayoutBase::GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData)
{
	if (!ControlConsoleData)
	{
		return;
	}

	LayoutRows.Reset(LayoutRows.Num());

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsoleData->GetFaderGroupRows();
	for (const UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = NewObject<UDMXControlConsoleEditorGlobalLayoutRow>(this, NAME_None, RF_Transactional);
		for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroupRow->GetFaderGroups())
		{
			if (!FaderGroup)
			{
				continue;
			}

			if (ContainsFaderGroup(FaderGroup))
			{
				continue;
			}

			LayoutRow->Modify();
			LayoutRow->CreateFaderGroupController(FaderGroup, FaderGroup->GetFaderGroupName());
		}

		LayoutRows.Add(LayoutRow);
	}

	// Remove all active controllers no more contained by the layout
	ActiveFaderGroupControllers.RemoveAll(
		[this](const TWeakObjectPtr<UDMXControlConsoleFaderGroupController>& ActiveFaderGroupController)
		{
			return !ActiveFaderGroupController.IsValid() || !ContainsFaderGroupController(ActiveFaderGroupController.Get());
		});

	const UDMXControlConsoleEditorLayouts& OwnerEditorLayouts = GetOwnerEditorLayoutsChecked();
	// The default layout can't contain not patched fader group controllers
	if (&OwnerEditorLayouts.GetDefaultLayoutChecked() == this)
	{
		CleanLayoutFromUnpatchedFaderGroupControllers();
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::ClearAll(const bool bOnlyPatchedFaderGroups)
{
	if (bOnlyPatchedFaderGroups)
	{
		for (UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
		{
			if (!LayoutRow)
			{
				continue;
			}

			const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = LayoutRow->GetFaderGroupControllers();
			for (UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
			{
				if (!FaderGroupController || !FaderGroupController->HasFixturePatch())
				{
					continue;
				}

				LayoutRow->Modify();
				LayoutRow->DeleteFaderGroupController(FaderGroupController);
			}
		}

		ClearEmptyLayoutRows();
	}
	else
	{
		LayoutRows.Reset();
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::ClearEmptyLayoutRows()
{
	LayoutRows.RemoveAll([](const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
		{
			return LayoutRow && LayoutRow->GetFaderGroupControllers().IsEmpty();
		});
}

void UDMXControlConsoleEditorGlobalLayoutBase::Register(UDMXControlConsoleData* ControlConsoleData)
{
	if (!ensureMsgf(ControlConsoleData, TEXT("Invalid control console data, cannot register layout correctly.")))
	{
		return;
	}

	if (!ensureMsgf(!bIsRegistered, TEXT("Layout already registered to dmx library delegates.")))
	{
		return;
	}

	UDMXControlConsoleEditorLayouts& OwnerEditorLayouts = GetOwnerEditorLayoutsChecked();
	if (!OwnerEditorLayouts.GetOnActiveLayoutChanged().IsBoundToObject(this))
	{
		OwnerEditorLayouts.GetOnActiveLayoutChanged().AddUObject(this, &UDMXControlConsoleEditorGlobalLayoutBase::OnActiveLayoutchanged);
	}

	if (&OwnerEditorLayouts.GetDefaultLayoutChecked() == this)
	{
		if (!UDMXLibrary::GetOnEntitiesRemoved().IsBoundToObject(this))
		{
			UDMXLibrary::GetOnEntitiesRemoved().AddUObject(this, &UDMXControlConsoleEditorGlobalLayoutBase::OnFixturePatchRemovedFromLibrary);
		}

		if (!ControlConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
		{
			ControlConsoleData->GetOnFaderGroupAdded().AddUObject(this, &UDMXControlConsoleEditorGlobalLayoutBase::OnFaderGroupAddedToData, ControlConsoleData);
		}
	}

	bIsRegistered = true;
}

void UDMXControlConsoleEditorGlobalLayoutBase::Unregister(UDMXControlConsoleData* ControlConsoleData)
{
	if (!ensureMsgf(ControlConsoleData, TEXT("Invalid control console data, cannot register layout correctly.")))
	{
		return;
	}

	if (!ensureMsgf(bIsRegistered, TEXT("Layout already unregistered from dmx library delegates.")))
	{
		return;
	}

	UDMXControlConsoleEditorLayouts& OwnerEditorLayouts = GetOwnerEditorLayoutsChecked();
	if (OwnerEditorLayouts.GetOnActiveLayoutChanged().IsBoundToObject(this))
	{
		OwnerEditorLayouts.GetOnActiveLayoutChanged().RemoveAll(this);
	}

	if (&OwnerEditorLayouts.GetDefaultLayoutChecked() == this)
	{
		if (UDMXLibrary::GetOnEntitiesRemoved().IsBoundToObject(this))
		{
			UDMXLibrary::GetOnEntitiesRemoved().RemoveAll(this);
		}

		if (ControlConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
		{
			ControlConsoleData->GetOnFaderGroupAdded().RemoveAll(this);
		}
	}

	bIsRegistered = false;
}

void UDMXControlConsoleEditorGlobalLayoutBase::BeginDestroy()
{
	Super::BeginDestroy();

	ensureMsgf(!bIsRegistered, TEXT("Layout still registered to dmx library delegates before being destroyed."));
}

void UDMXControlConsoleEditorGlobalLayoutBase::PostLoad()
{
	Super::PostLoad();

	ClearEmptyLayoutRows();
}

void UDMXControlConsoleEditorGlobalLayoutBase::OnActiveLayoutchanged(const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout)
{
	if (ActiveLayout != this)
	{
		return;
	}

	const auto SynchElementControllerValueLambda =
		[](UDMXControlConsoleElementController* ElementController, UDMXControlConsoleFaderBase* Fader)
		{
			if (ElementController && Fader)
			{
				const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
				const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
				const float NormalizedMaxValue = Fader->GetMaxValue() / ValueRange;
				const float NormalizedMinValue = Fader->GetMinValue() / ValueRange;
				const float NormalizedValue = Fader->GetValue() / ValueRange;

				ElementController->SetMaxValue(NormalizedMaxValue);
				ElementController->SetMinValue(NormalizedMinValue);
				ElementController->SetValue(NormalizedValue);
			}
		};

	const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = GetAllFaderGroupControllers();
	for (UDMXControlConsoleFaderGroupController* FaderGroupController : AllFaderGroupControllers)
	{
		if (!FaderGroupController)
		{
			continue;
		}

		const TArray<UDMXControlConsoleElementController*> ElementControllers = FaderGroupController->GetAllElementControllers();
		TArray<UDMXControlConsoleCellAttributeController*> CellAttributeControllers;
		Algo::TransformIf(ElementControllers, CellAttributeControllers,
			[](UDMXControlConsoleElementController* ElementController)
			{
				return IsValid(Cast<UDMXControlConsoleCellAttributeController>(ElementController));
			},
			[](UDMXControlConsoleElementController* ElementController)
			{
				return Cast<UDMXControlConsoleCellAttributeController>(ElementController);
			}
		);

		// Synch cell attribute controllers before their matrix cell controllers
		for (UDMXControlConsoleCellAttributeController* CellAttributeController : CellAttributeControllers)
		{
			if (!CellAttributeController)
			{
				continue;
			}

			// Ensure that all elements are possessed by controllers in the active layout
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = CellAttributeController->GetElements();
			for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
			{
				if (Element)
				{
					CellAttributeController->Possess(Element);
				}
			}

			if (Elements.Num() == 1)
			{
				UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Elements[0].GetObject());
				SynchElementControllerValueLambda(CellAttributeController, Fader);
			}
		}

		for (UDMXControlConsoleElementController* ElementController : ElementControllers)
		{
			if (!ElementController || CellAttributeControllers.Contains(ElementController))
			{
				continue;
			}

			// Ensure that all elements are possessed by controllers in the active layout
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = ElementController->GetElements();
			for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
			{
				if (Element)
				{
					ElementController->Possess(Element);
				}
			}

			// Synch the controller value only if there's one element
			if (Elements.Num() == 1)
			{
				UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Elements[0].GetObject());
				SynchElementControllerValueLambda(ElementController, Fader);
			}
		}

		// Ensure that all fader groups are possessed by controllers in the active layout
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
		{
			if (FaderGroup.IsValid())
			{
				FaderGroupController->Possess(FaderGroup.Get());
			}
		}
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities)
{
	if (Entities.IsEmpty())
	{
		return;
	}

	for (const UDMXEntity* Entity : Entities)
	{
		const UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(Entity);
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroupController* FaderGroupController = FindFaderGroupControllerByFixturePatch(FixturePatch);
		if (FaderGroupController && FaderGroupController->GetFaderGroups().IsEmpty())
		{
			FaderGroupController->Modify();
			FaderGroupController->Destroy();
		}
	}

	ClearEmptyLayoutRows();
}

void UDMXControlConsoleEditorGlobalLayoutBase::OnFaderGroupAddedToData(const UDMXControlConsoleFaderGroup* FaderGroup, UDMXControlConsoleData* ControlConsoleData)
{
	if (FaderGroup && FaderGroup->HasFixturePatch())
	{
		Modify();
		GenerateLayoutByControlConsoleData(ControlConsoleData);
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::CleanLayoutFromUnpatchedFaderGroupControllers()
{
	const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = GetAllFaderGroupControllers();
	for (UDMXControlConsoleFaderGroupController* FaderGroupController : AllFaderGroupControllers)
	{
		if (!FaderGroupController)
		{
			continue;
		}

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
		const bool bAreAllFaderGroupsUnpatched = Algo::AllOf(FaderGroups,
			[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
			{
				return FaderGroup.IsValid() && !FaderGroup->HasFixturePatch();
			});

		if (bAreAllFaderGroupsUnpatched)
		{
			RemoveFromActiveFaderGroupControllers(FaderGroupController);

			FaderGroupController->Modify();
			FaderGroupController->Destroy();
		}
	}

	ClearEmptyLayoutRows();
}

#undef LOCTEXT_NAMESPACE
