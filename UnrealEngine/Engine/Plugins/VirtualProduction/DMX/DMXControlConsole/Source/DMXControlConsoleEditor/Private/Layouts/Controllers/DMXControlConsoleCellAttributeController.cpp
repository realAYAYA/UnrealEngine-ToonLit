// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleCellAttributeController.h"

#include "DMXControlConsoleFaderGroupController.h"
#include "DMXControlConsoleMatrixCellController.h"


UDMXControlConsoleFaderGroupController& UDMXControlConsoleCellAttributeController::GetOwnerFaderGroupControllerChecked() const
{
	const UDMXControlConsoleMatrixCellController& OwnerMatrixCellController = GetOwnerMatrixCellControllerChecked();
	return OwnerMatrixCellController.GetOwnerFaderGroupControllerChecked();
}

int32 UDMXControlConsoleCellAttributeController::GetIndex() const
{
	const UDMXControlConsoleMatrixCellController& OwnerMatrixCellController = GetOwnerMatrixCellControllerChecked();

	const TArray<UDMXControlConsoleCellAttributeController*> Controllers = OwnerMatrixCellController.GetCellAttributeControllers();
	const int32 Index = Controllers.IndexOfByKey(this);
	return Index;
}

void UDMXControlConsoleCellAttributeController::Destroy()
{
	ClearElements();

	UDMXControlConsoleMatrixCellController& OwnerMatrixCellController = GetOwnerMatrixCellControllerChecked();

	OwnerMatrixCellController.PreEditChange(UDMXControlConsoleMatrixCellController::StaticClass()->FindPropertyByName(UDMXControlConsoleMatrixCellController::GetCellAttributeControllersPropertyName()));
	OwnerMatrixCellController.DeleteCellAttributeController(this);
	OwnerMatrixCellController.PostEditChange();
}

UDMXControlConsoleMatrixCellController& UDMXControlConsoleCellAttributeController::GetOwnerMatrixCellControllerChecked() const
{
	UDMXControlConsoleMatrixCellController* OwnerMatrixCellController = Cast<UDMXControlConsoleMatrixCellController>(GetOuter());
	checkf(OwnerMatrixCellController, TEXT("Invalid outer for '%s', cannot get controller owner correctly."), *GetName());

	return *OwnerMatrixCellController;
}
