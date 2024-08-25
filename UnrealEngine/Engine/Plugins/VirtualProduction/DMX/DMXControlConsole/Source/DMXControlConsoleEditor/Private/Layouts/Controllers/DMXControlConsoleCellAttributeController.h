// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleElementController.h"

#include "DMXControlConsoleCellAttributeController.generated.h"

class UDMXControlConsoleFaderGroupController;
class UDMXControlConsoleMatrixCellController;


/** A controller for handling one or more faders at once in a matrix cell */
UCLASS()
class UDMXControlConsoleCellAttributeController
	: public UDMXControlConsoleElementController
{
	GENERATED_BODY()

public:
	//~ Being UDMXControlConsoleElementController interface
	virtual UDMXControlConsoleFaderGroupController& GetOwnerFaderGroupControllerChecked() const override;
	virtual int32 GetIndex() const override;
	virtual void Destroy() override;
	//~ End UDMXControlConsoleElementController interface

	/** Returns the Matrix Cell Controller this Controller resides in */
	UDMXControlConsoleMatrixCellController& GetOwnerMatrixCellControllerChecked() const;
};
