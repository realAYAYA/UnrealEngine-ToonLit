// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleMatrixCellController.h"

#include "DMXControlConsoleCellAttributeController.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "IDMXControlConsoleFaderGroupElement.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleMatrixCellController"

void UDMXControlConsoleMatrixCellController::Possess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
{
	UDMXControlConsoleElementController::Possess(InElement);

	GenerateCellAttributeControllers(InElement);
}

void UDMXControlConsoleMatrixCellController::Possess(TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements)
{
	UDMXControlConsoleElementController::Possess(InElements);

	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : InElements)
	{
		if (Element)
		{
			GenerateCellAttributeControllers(Element);
		}
	}
}

void UDMXControlConsoleMatrixCellController::UnPossess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
{
	if (!InElement || !Elements.Contains(InElement))
	{
		return;
	}

	const TArray<UDMXControlConsoleCellAttributeController*> Controllers = GetAllCellAttributeControllersFromElement(InElement);
	for (UDMXControlConsoleCellAttributeController* Controller : Controllers)
	{
		if (Controller)
		{
			Controller->Modify();
			Controller->Destroy();
		}
	}

	InElement->SetElementController(nullptr);
	Elements.Remove(InElement);
}

void UDMXControlConsoleMatrixCellController::Destroy()
{
	ClearCellAttributeControllers();
	UDMXControlConsoleElementController::Destroy();
}

void UDMXControlConsoleMatrixCellController::Group()
{
	ClearCellAttributeControllers();

	TMap<FName, TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>> AttributeNameToElementsMap;
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(Element.GetObject());
		if (!MatrixCell)
		{
			continue;
		}

		const TArray<UDMXControlConsoleFaderBase*>& Faders = MatrixCell->GetFaders();
		for (UDMXControlConsoleFaderBase* Fader : Faders)
		{
			if (UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(Fader))
			{
				const FName& AttributeName = CellAttributeFader->GetAttributeName().Name;
				if (AttributeNameToElementsMap.Contains(AttributeName))
				{
					AttributeNameToElementsMap[AttributeName].Add(Fader);
				}
				else
				{
					AttributeNameToElementsMap.Add(AttributeName, { Fader });
				}
			}
		}
	}

	// Create a single cell attribute controller for each attribute name
	for (const TTuple<FName, TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>>& AttributeNameToElements : AttributeNameToElementsMap)
	{
		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& ElementsToGroup = AttributeNameToElements.Value;
		if (ElementsToGroup.IsEmpty())
		{
			continue;
		}

		float ControllerValue = 0.f;
		float ControllerMinValue = 0.f;
		float ControllerMaxValue = 1.f;

		UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(ElementsToGroup[0].GetObject());
		if (Fader)
		{
			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			ControllerValue = Fader->GetValue() / ValueRange;
			ControllerMinValue = Fader->GetMinValue() / ValueRange;
			ControllerMaxValue = Fader->GetMaxValue() / ValueRange;
		}

		UDMXControlConsoleCellAttributeController* NewController = CreateCellAttributeController(AttributeNameToElements.Value, AttributeNameToElements.Key.ToString());
		if (NewController)
		{
			constexpr bool bSyncElements = false;
			NewController->SetValue(ControllerValue, bSyncElements);
			NewController->SetMinValue(ControllerMinValue, bSyncElements);
			NewController->SetMaxValue(ControllerMaxValue, bSyncElements);
		}
	}

	SortCellAttributeControllersByStartingAddress();
}

UDMXControlConsoleCellAttributeController* UDMXControlConsoleMatrixCellController::CreateCellAttributeController(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement, const FString& InControllerName)
{
	if (!InElement)
	{
		return nullptr;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> ElementAsArray = { InElement };
	UDMXControlConsoleCellAttributeController* CellAttributeController = CreateCellAttributeController(ElementAsArray, InControllerName);
	return CellAttributeController;
}

UDMXControlConsoleCellAttributeController* UDMXControlConsoleMatrixCellController::CreateCellAttributeController(const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements, const FString& InControllerName)
{
	if (InElements.IsEmpty())
	{
		return nullptr;
	}

	UDMXControlConsoleCellAttributeController* CellAttributeController = NewObject<UDMXControlConsoleCellAttributeController>(this, NAME_None, RF_Transactional);
	CellAttributeController->Possess(InElements);

	const FString NewName = InControllerName.IsEmpty() ? FString::FromInt(CellAttributeControllers.Num() + 1) : InControllerName;
	CellAttributeController->SetUserName(NewName);

	CellAttributeControllers.Add(CellAttributeController);
	return CellAttributeController;
}

void UDMXControlConsoleMatrixCellController::GenerateCellAttributeControllers()
{
	ClearCellAttributeControllers();
	UpdateCellAttributeControllers();
}

void UDMXControlConsoleMatrixCellController::DeleteCellAttributeController(UDMXControlConsoleCellAttributeController* CellAttributeController)
{
	if (!ensureMsgf(CellAttributeController, TEXT("Invalid cell attribute controller, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(CellAttributeControllers.Contains(CellAttributeController), TEXT("'%s' matrix cell controller is not owner of '%s'. Cannot delete cell attribute controller correctly."), *GetName(), *CellAttributeController->GetUserName()))
	{
		return;
	}

	CellAttributeControllers.Remove(CellAttributeController);
}

void UDMXControlConsoleMatrixCellController::ClearCellAttributeControllers()
{
	for (UDMXControlConsoleCellAttributeController* CellAttributeController : CellAttributeControllers)
	{
		if (CellAttributeController)
		{
			CellAttributeController->Modify();
			CellAttributeController->ClearElements();
		}
	}
	CellAttributeControllers.Reset();
}

void UDMXControlConsoleMatrixCellController::SortCellAttributeControllersByStartingAddress() const
{
	// Sort elements
	for (UDMXControlConsoleCellAttributeController* CellAttributeController : CellAttributeControllers)
	{
		if (CellAttributeController)
		{
			CellAttributeController->SortElementsByStartingAddress();
		}
	}

	// Sort controllers
	Algo::Sort(CellAttributeControllers,
		[](const UDMXControlConsoleCellAttributeController* ItemA, const UDMXControlConsoleCellAttributeController* ItemB)
		{
			if (!ItemA || !ItemB)
			{
				return false;
			}

			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& ElementsA = ItemA->GetElements();
			const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& ElementsB = ItemB->GetElements();
			if (ElementsA.IsEmpty() || ElementsB.IsEmpty())
			{
				return false;
			}

			const TScriptInterface<IDMXControlConsoleFaderGroupElement> ElementA = ElementsA[0];
			const TScriptInterface<IDMXControlConsoleFaderGroupElement> ElementB = ElementsB[0];
			if (!ElementA || !ElementB)
			{
				return false;
			}

			const int32 StartingAddressA = ElementA->GetStartingAddress();
			const int32 StartingAddressB = ElementB->GetStartingAddress();

			return StartingAddressA < StartingAddressB;
		});
}

TArray<UDMXControlConsoleCellAttributeController*> UDMXControlConsoleMatrixCellController::GetAllCellAttributeControllersFromElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement) const
{
	TArray<UDMXControlConsoleCellAttributeController*> AllCellAttributeControllers;
	if (!InElement || !Elements.Contains(InElement))
	{
		return AllCellAttributeControllers;
	}

	const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(InElement.GetObject());
	if (!MatrixCell)
	{
		return AllCellAttributeControllers;
	}

	const TArray<UDMXControlConsoleFaderBase*>& Faders = MatrixCell->GetFaders();
	for (const UDMXControlConsoleFaderBase* Fader : Faders)
	{
		if (!Fader)
		{
			continue;
		}

		UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Fader->GetElementController());
		if (!ElementController || !CellAttributeControllers.Contains(ElementController))
		{
			continue;
		}

		if (UDMXControlConsoleCellAttributeController* CellAttributeController = Cast<UDMXControlConsoleCellAttributeController>(ElementController))
		{
			AllCellAttributeControllers.AddUnique(CellAttributeController);
		}
	}

	return AllCellAttributeControllers;
}

void UDMXControlConsoleMatrixCellController::GenerateCellAttributeControllers(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
{
	if (!InElement)
	{
		return;
	}

	const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(InElement.GetObject());
	if (!MatrixCell)
	{
		return;
	}

	// Create cell attribute controllers for faders with no controller
	const TArray<UDMXControlConsoleFaderBase*>& Faders = MatrixCell->GetFaders();
	for (UDMXControlConsoleFaderBase* Fader : Faders)
	{
		if (!Fader)
		{
			continue;
		}

		UDMXControlConsoleCellAttributeController* CellAttributeController = Cast<UDMXControlConsoleCellAttributeController>(Fader->GetElementController());
		if (CellAttributeController && CellAttributeControllers.Contains(CellAttributeController))
		{
			continue;
		}

		UDMXControlConsoleCellAttributeController* NewController = CreateCellAttributeController(Fader, Fader->GetFaderName());
		if (NewController)
		{
			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			float ControllerValue = Fader->GetValue() / ValueRange;
			float ControllerMinValue = Fader->GetMinValue() / ValueRange;
			float ControllerMaxValue = Fader->GetMaxValue() / ValueRange;

			NewController->SetValue(ControllerValue);
			NewController->SetMinValue(ControllerMinValue);
			NewController->SetMaxValue(ControllerMaxValue);
		}
	}
}

void UDMXControlConsoleMatrixCellController::UpdateCellAttributeControllers()
{
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (Element)
		{
			GenerateCellAttributeControllers(Element);
		}
	}

	// Remove all element controllers with no elements
	CellAttributeControllers.RemoveAll([](const UDMXControlConsoleCellAttributeController* CellAttributeController)
		{
			return CellAttributeController && CellAttributeController->GetElements().IsEmpty();
		});

	SortCellAttributeControllersByStartingAddress();
}

#undef LOCTEXT_NAMESPACE
