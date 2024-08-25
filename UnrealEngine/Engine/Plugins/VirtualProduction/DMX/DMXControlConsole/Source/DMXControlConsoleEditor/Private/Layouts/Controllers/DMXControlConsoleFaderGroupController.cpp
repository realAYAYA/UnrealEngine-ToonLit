// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroupController.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/StableSort.h"
#include "DMXControlConsoleCellAttributeController.h"
#include "DMXControlConsoleElementController.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchFunctionFader.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "DMXControlConsoleMatrixCellController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Styling/SlateTypes.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderGroupController"

void UDMXControlConsoleFaderGroupController::Possess(UDMXControlConsoleFaderGroup* InFaderGroup)
{
	if (!InFaderGroup)
	{
		return;
	}

	UDMXControlConsoleFaderGroupController* OldController = Cast<UDMXControlConsoleFaderGroupController>(InFaderGroup->GetFaderGroupController());
	if (OldController == this)
	{
		return;
	}

	InFaderGroup->SetFaderGroupController(this);
	if (!InFaderGroup->GetOnFixturePatchChanged().IsBoundToObject(this))
	{
		InFaderGroup->GetOnFixturePatchChanged().AddUObject(this, &UDMXControlConsoleFaderGroupController::OnFaderGroupFixturePatchChanged);
	}

	GenerateElementControllers(InFaderGroup);

	FaderGroups.AddUnique(InFaderGroup);
	SyncControllerEditorColor();
}

void UDMXControlConsoleFaderGroupController::Possess(TArray<UDMXControlConsoleFaderGroup*> InFaderGroups)
{
	InFaderGroups.RemoveAll([this](const UDMXControlConsoleFaderGroup* FaderGroup)
		{
			if (FaderGroup)
			{
				const UDMXControlConsoleFaderGroupController* OldController = Cast<UDMXControlConsoleFaderGroupController>(FaderGroup->GetFaderGroupController());
				return OldController == this;
			}
			return true;
		});

	for (UDMXControlConsoleFaderGroup* FaderGroup : InFaderGroups)
	{
		if (!FaderGroup)
		{
			continue;
		}

		FaderGroup->SetFaderGroupController(this);
		FaderGroup->GetOnFixturePatchChanged().AddUObject(this, &UDMXControlConsoleFaderGroupController::OnFaderGroupFixturePatchChanged);
		GenerateElementControllers(FaderGroup);
	}
	
	FaderGroups.Append(InFaderGroups);
	SyncControllerEditorColor();
}

void UDMXControlConsoleFaderGroupController::UnPossess(UDMXControlConsoleFaderGroup* InFaderGroup)
{
	if (!InFaderGroup || !FaderGroups.Contains(InFaderGroup))
	{
		return;
	}
	
	const TArray<UDMXControlConsoleElementController*> Controllers = GetAllElementControllersFromFaderGroup(InFaderGroup);
	for (UDMXControlConsoleElementController* Controller : Controllers)
	{
		if (Controller)
		{
			Controller->Modify();
			Controller->Destroy();
		}
	}

	InFaderGroup->SetFaderGroupController(nullptr);
	if (InFaderGroup->GetOnFixturePatchChanged().IsBoundToObject(this))
	{
		InFaderGroup->GetOnFixturePatchChanged().RemoveAll(this);
	}

	FaderGroups.Remove(InFaderGroup);
	SyncControllerEditorColor();
}

void UDMXControlConsoleFaderGroupController::Group()
{
	if (!HasFixturePatch())
	{
		return;
	}

	ClearElementControllers();

	TMap<FName, TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>> AttributeNameToElementsMap;
	TMap<int32, TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>> CellIDToElementsMap;
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
	{
		if (!FaderGroup.IsValid())
		{
			continue;
		}

		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements = FaderGroup->GetElements();
		for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
		{
			if (!Element)
			{
				continue;
			}

			if (UDMXControlConsoleFixturePatchFunctionFader* FunctionFader = Cast<UDMXControlConsoleFixturePatchFunctionFader>(Element.GetObject()))
			{
				const FName& AttributeName = FunctionFader->GetAttributeName().Name;
				if (AttributeNameToElementsMap.Contains(AttributeName))
				{
					AttributeNameToElementsMap[AttributeName].Add(Element);
				}
				else
				{
					AttributeNameToElementsMap.Add(AttributeName, { Element });
				}
			}
			else if (UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(Element.GetObject()))
			{
				const int32 CellID = MatrixCell->GetCellID();
				if (CellIDToElementsMap.Contains(CellID))
				{
					CellIDToElementsMap[CellID].Add(Element);
				}
				else
				{
					CellIDToElementsMap.Add(CellID, { Element });
				}
			}
		}
	}

	// Create a single element controller for each attribute name
	for (const TTuple<FName, TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>>& AttributeNameToElements : AttributeNameToElementsMap)
	{
		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = AttributeNameToElements.Value;
		if (Elements.IsEmpty())
		{
			continue;
		}

		float ControllerValue = 0.f;
		float ControllerMinValue = 0.f;
		float ControllerMaxValue = 1.f;

		UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Elements[0].GetObject());
		if (Fader)
		{
			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			ControllerValue = Fader->GetValue() / ValueRange;
			ControllerMinValue = Fader->GetMinValue() / ValueRange;
			ControllerMaxValue = Fader->GetMaxValue() / ValueRange;
		}

		UDMXControlConsoleElementController* NewController = CreateElementController(AttributeNameToElements.Value, AttributeNameToElements.Key.ToString());
		if (NewController)
		{
			constexpr bool bSyncElements = false;
			NewController->SetValue(ControllerValue, bSyncElements);
			NewController->SetMinValue(ControllerMinValue, bSyncElements);
			NewController->SetMaxValue(ControllerMaxValue, bSyncElements);
		}
	}

	// Create a single element controller for each cell id
	for (const TTuple<int32, TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>>& CellIDToElements : CellIDToElementsMap)
	{
		UDMXControlConsoleElementController* NewController = CreateElementController(CellIDToElements.Value, FString::FromInt(CellIDToElements.Key));
		if (UDMXControlConsoleMatrixCellController* MatrixCellController = Cast<UDMXControlConsoleMatrixCellController>(NewController))
		{
			MatrixCellController->Group();
		}
	}

	SortElementControllersByStartingAddress();

	OnControllerGrouped.Broadcast();
}

void UDMXControlConsoleFaderGroupController::SortFaderGroupsByAbsoluteAddress()
{
	Algo::StableSortBy(FaderGroups,
		[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& Item)
		{
			const UDMXEntityFixturePatch* FixturePatch = Item.IsValid() ? Item->GetFixturePatch() : nullptr;
			if (FixturePatch)
			{
				return  (int64)FixturePatch->GetUniverseID() * DMX_UNIVERSE_SIZE + FixturePatch->GetStartingChannel();
			}

			return TNumericLimits<int64>::Max();
		});
}

UDMXControlConsoleEditorGlobalLayoutRow& UDMXControlConsoleFaderGroupController::GetOwnerLayoutRowChecked() const
{
	UDMXControlConsoleEditorGlobalLayoutRow* Outer = Cast<UDMXControlConsoleEditorGlobalLayoutRow>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get controller owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleElementController* UDMXControlConsoleFaderGroupController::CreateElementController(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement, const FString& InControllerName)
{
	if (!InElement)
	{
		return nullptr;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> ElementAsArray = { InElement };
	UDMXControlConsoleElementController* ElementController = CreateElementController(ElementAsArray, InControllerName);
	return ElementController;
}

UDMXControlConsoleElementController* UDMXControlConsoleFaderGroupController::CreateElementController(const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements, const FString& InControllerName)
{
	if (InElements.IsEmpty())
	{
		return nullptr;
	}

	const bool bIsAnyElementAMatrixCell = Algo::AnyOf(InElements,
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			return Element && IsValid(Cast<UDMXControlConsoleFixturePatchMatrixCell>(Element.GetObject()));
		});

	UDMXControlConsoleElementController* ElementController = nullptr;
	if (bIsAnyElementAMatrixCell)
	{
		ElementController = NewObject<UDMXControlConsoleMatrixCellController>(this, NAME_None, RF_Transactional);
	}
	else
	{
		ElementController = NewObject<UDMXControlConsoleElementController>(this, NAME_None, RF_Transactional);
	}
	
	const FString NewName = InControllerName.IsEmpty() ? FString::FromInt(ElementControllers.Num() + 1) : InControllerName;
	ElementController->SetUserName(NewName);
	ElementController->Possess(InElements);

	ElementControllers.Add(ElementController);
	return ElementController;
}

void UDMXControlConsoleFaderGroupController::GenerateElementControllers()
{
	ClearElementControllers();
	UpdateElementControllers();
}

void UDMXControlConsoleFaderGroupController::DeleteElementController(UDMXControlConsoleElementController* ElementController)
{
	if (!ensureMsgf(ElementController, TEXT("Invalid element controller, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(ElementControllers.Contains(ElementController), TEXT("'%s' fader group is not owner of '%s'. Cannot delete element controller correctly."), *GetName(), *ElementController->GetUserName()))
	{
		return;
	}

	ElementControllers.Remove(ElementController);
}

void UDMXControlConsoleFaderGroupController::SortElementControllersByStartingAddress() const
{
	// Sort elements
	for (UDMXControlConsoleElementController* ElementController : ElementControllers)
	{
		if (ElementController)
		{
			ElementController->SortElementsByStartingAddress();
		}
	}

	// Sort controllers
	Algo::StableSortBy(ElementControllers, 
		[](const UDMXControlConsoleElementController* Item)
		{
			if (!Item)
			{
				return TNumericLimits<int32>::Max();
			}

			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = Item->GetElements();
			if (Elements.IsEmpty())
			{
				return TNumericLimits<int32>::Max();
			}

			const TScriptInterface<IDMXControlConsoleFaderGroupElement> Element = Elements[0];
			if (Element)
			{
				const int32 StartingAddress = Element->GetStartingAddress();
				return StartingAddress;
			}

			return TNumericLimits<int32>::Max();
		});
}

TArray<UDMXControlConsoleElementController*> UDMXControlConsoleFaderGroupController::GetAllElementControllers() const
{
	TArray<UDMXControlConsoleElementController*> AllElementControllers;
	for (UDMXControlConsoleElementController* ElementController : ElementControllers)
	{
		if (!ElementController)
		{
			continue;
		}

		AllElementControllers.Add(ElementController);
		if (UDMXControlConsoleMatrixCellController* MatrixCellController = Cast<UDMXControlConsoleMatrixCellController>(ElementController))
		{
			AllElementControllers.Append(MatrixCellController->GetCellAttributeControllers());
		}
	}

	return AllElementControllers;
}

TArray<UDMXControlConsoleElementController*> UDMXControlConsoleFaderGroupController::GetAllElementControllersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const
{
	TArray<UDMXControlConsoleElementController*> AllElementControllers;
	if (!FaderGroup || !FaderGroups.Contains(FaderGroup))
	{
		return AllElementControllers;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements = FaderGroup->GetElements();
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element)
		{
			continue;
		}

		UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Element->GetElementController());
		if (!ElementController || !ElementControllers.Contains(ElementController))
		{
			continue;
		}

		AllElementControllers.AddUnique(ElementController);
		if (UDMXControlConsoleMatrixCellController* MatrixCellController = Cast<UDMXControlConsoleMatrixCellController>(ElementController))
		{
			AllElementControllers.Append(MatrixCellController->GetCellAttributeControllers());
		}
	}

	return AllElementControllers;
}

FString UDMXControlConsoleFaderGroupController::GenerateUserNameByFaderGroupsNames() const
{
	FString NewName = FaderGroups.Num() > 1 ? TEXT("Group_") : TEXT("");
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
	{
		if (!FaderGroup.IsValid())
		{
			continue;
		}

		const FString& FaderGroupName = FaderGroup->GetFaderGroupName();
		if (NewName.Contains(FaderGroupName))
		{
			continue;
		}

		NewName.Append(FaderGroupName);
		if (FaderGroups.Last() != FaderGroup)
		{
			NewName.Append(TEXT("_"));
		}
	}

	return NewName;
}

void UDMXControlConsoleFaderGroupController::SetUserName(const FString& NewName)
{
	UserName = NewName;
}

bool UDMXControlConsoleFaderGroupController::HasFixturePatch() const
{
	const bool bIsAnyFaderGroupPatched = Algo::AnyOf(FaderGroups,
		[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			return FaderGroup.IsValid() && FaderGroup->HasFixturePatch();
		});

	return bIsAnyFaderGroupPatched;
}

void UDMXControlConsoleFaderGroupController::SetLocked(bool bLock)
{
	bIsLocked = bLock;

	for (UDMXControlConsoleElementController* ElementController : ElementControllers)
	{
		if (!ElementController)
		{
			continue;
		}

		ElementController->Modify();
		ElementController->SetLocked(bIsLocked);
	}
}

void UDMXControlConsoleFaderGroupController::SetIsExpanded(bool bExpanded, bool bNotify)
{
	bIsExpanded = bExpanded;
	if (bNotify)
	{
		OnFaderGroupControllerExpanded.Broadcast();
	}
}

bool UDMXControlConsoleFaderGroupController::IsActive() const
{
	return HasFixturePatch() ? bIsActive : true;
}

bool UDMXControlConsoleFaderGroupController::IsMatchingFilter() const
{
	const bool bIsAnyFaderGroupMatchingFilter = Algo::AnyOf(FaderGroups,
		[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			return FaderGroup.IsValid() && FaderGroup->IsMatchingFilter();
		});

	return bIsAnyFaderGroupMatchingFilter;
}

bool UDMXControlConsoleFaderGroupController::IsInActiveLayout() const
{
	UDMXControlConsoleEditorGlobalLayoutRow& OwnerLayoutRow = GetOwnerLayoutRowChecked();
	UDMXControlConsoleEditorGlobalLayoutBase& OwnerLayout = OwnerLayoutRow.GetOwnerLayoutChecked();
	UDMXControlConsoleEditorLayouts& OwnerEditorLayouts = OwnerLayout.GetOwnerEditorLayoutsChecked();
	return OwnerEditorLayouts.GetActiveLayout() == &OwnerLayout;
}

ECheckBoxState UDMXControlConsoleFaderGroupController::GetEnabledState() const
{
	const bool bAreAllFaderGorupsEnabled = Algo::AllOf(FaderGroups,
		[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			return FaderGroup.IsValid() && FaderGroup->IsEnabled();
		});

	if (bAreAllFaderGorupsEnabled)
	{
		return ECheckBoxState::Checked;
	}

	const bool bIsAnyFaderGroupEnabled = Algo::AnyOf(FaderGroups,
		[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			return FaderGroup.IsValid() && FaderGroup->IsEnabled();
		});

	return bIsAnyFaderGroupEnabled ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked;
}

void UDMXControlConsoleFaderGroupController::Destroy()
{
	ClearElementControllers();
	ClearFaderGroups();

	UDMXControlConsoleEditorGlobalLayoutRow& OwnerLayoutRow = GetOwnerLayoutRowChecked();
	OwnerLayoutRow.PreEditChange(UDMXControlConsoleEditorGlobalLayoutRow::StaticClass()->FindPropertyByName(UDMXControlConsoleEditorGlobalLayoutRow::GetFaderGroupControllersPropertyName()));
	OwnerLayoutRow.DeleteFaderGroupController(this);
	OwnerLayoutRow.PostEditChange();
}

void UDMXControlConsoleFaderGroupController::PostInitProperties()
{
	Super::PostInitProperties();

	UserName = GetName();
}

void UDMXControlConsoleFaderGroupController::ClearFaderGroups()
{
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
	{
		if (!FaderGroup.IsValid())
		{
			continue;
		}

		FaderGroup->SetFaderGroupController(nullptr);
		if (FaderGroup->GetOnFixturePatchChanged().IsBoundToObject(this))
		{
			FaderGroup->GetOnFixturePatchChanged().RemoveAll(this);
		}
	}

	FaderGroups.Reset();
}

void UDMXControlConsoleFaderGroupController::ClearElementControllers()
{
	for (UDMXControlConsoleElementController* ElementController : ElementControllers)
	{
		if (!ElementController)
		{
			continue;
		}

		ElementController->Modify();
		if (UDMXControlConsoleMatrixCellController* MatrixCellController = Cast<UDMXControlConsoleMatrixCellController>(ElementController))
		{
			MatrixCellController->ClearCellAttributeControllers();
		}

		ElementController->ClearElements();
	}
	ElementControllers.Reset(ElementControllers.Num());
}

void UDMXControlConsoleFaderGroupController::GenerateElementControllers(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!FaderGroup)
	{
		return;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements = FaderGroup->GetElements();
	// Create element controllers for elements with no controller
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element)
		{
			continue;
		}

		UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Element->GetElementController());
		if (ElementController && ElementControllers.Contains(ElementController))
		{
			continue;
		}

		const UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
		FString NewControllerName = TEXT("");
		float ControllerValue = 0.f;
		float ControllerMinValue = 0.f;
		float ControllerMaxValue = 1.f;
		if (Fader)
		{
			NewControllerName = Fader->GetFaderName();

			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			ControllerValue = Fader->GetValue() / ValueRange;
			ControllerMinValue = Fader->GetMinValue() / ValueRange;
			ControllerMaxValue = Fader->GetMaxValue() / ValueRange;
		}

		UDMXControlConsoleElementController* NewController = CreateElementController(Element, NewControllerName);
		if (NewController)
		{
			NewController->SetValue(ControllerValue);
			NewController->SetMinValue(ControllerMinValue);
			NewController->SetMaxValue(ControllerMaxValue);
		}
	}
}

void UDMXControlConsoleFaderGroupController::UpdateElementControllers()
{
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
	{
		if (FaderGroup.IsValid())
		{
			GenerateElementControllers(FaderGroup.Get());
		}
	}

	// Remove all element controllers with no elements
	ElementControllers.RemoveAll([](const UDMXControlConsoleElementController* ElementController)
		{
			return ElementController && ElementController->GetElements().IsEmpty();
		});

	SortElementControllersByStartingAddress();
}

void UDMXControlConsoleFaderGroupController::OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch)
{
	if (!FaderGroup || !FixturePatch || !FaderGroups.Contains(FaderGroup))
	{
		return;
	}

	GenerateElementControllers(FaderGroup);
	SyncControllerEditorColor();
	if (FaderGroups.Num() > 1)
	{
		Group();
	}

	OnFixturePatchChanged.Broadcast();
}

void UDMXControlConsoleFaderGroupController::SyncControllerEditorColor()
{
	if (FaderGroups.IsEmpty())
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = FaderGroups[0]->GetFixturePatch();
	if (FixturePatch)
	{
		EditorColor = FixturePatch->EditorColor;
	}
}

#undef LOCTEXT_NAMESPACE
