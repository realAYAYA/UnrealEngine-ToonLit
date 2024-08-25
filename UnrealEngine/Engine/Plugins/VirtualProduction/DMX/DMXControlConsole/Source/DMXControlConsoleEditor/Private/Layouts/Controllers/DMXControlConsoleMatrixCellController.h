// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleElementController.h"

#include "DMXControlConsoleMatrixCellController.generated.h"

class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleCellAttributeController;

/**
 * A controller for handling one or more matrix cells at once.
 * A matrix cell can be possessed by one controller at a time.
 */
UCLASS(AutoExpandCategories = ("DMX Element Controller", "DMX Element Controller|Oscillator"))
class UDMXControlConsoleMatrixCellController
	: public UDMXControlConsoleElementController
{
	GENERATED_BODY()

public:
	//~ Being UDMXControlConsoleElementController interface
	virtual void Possess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement) override;
	virtual void Possess(TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements) override;
	virtual void UnPossess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement) override;
	virtual void Destroy() override;
	//~ End UDMXControlConsoleElementController interface

	/** Groups all the possessed Elements to minimize the number of Cell Attribute Controllers */
	void Group();
	
	/** Creates a Cell Attribute Controller for the given Element */
	UDMXControlConsoleCellAttributeController* CreateCellAttributeController(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement, const FString& InControllerName = "");

	/** Creates a Cell Attribute Controller for the given array of Elements */
	UDMXControlConsoleCellAttributeController* CreateCellAttributeController(const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements, const FString& InControllerName = "");

	/** Generates a Cell Attribute Controller for each Element of each Matrix Cell possesed by the Controller */
	void GenerateCellAttributeControllers();

	/** Deletes the given Cell Attribute Controller */
	void DeleteCellAttributeController(UDMXControlConsoleCellAttributeController* CellAttributeController);

	/** Clears all the Cell Attribute Controllers in this Controller */
	void ClearCellAttributeControllers();

	/** Sorts the array of Cell Attribute Controllers by the starting address of their Elements */
	void SortCellAttributeControllersByStartingAddress() const;

	/** Gets the array of Cell Attribute Controllers for this Matrix Cell Controller */
	TArray<UDMXControlConsoleCellAttributeController*> GetCellAttributeControllers() const { return CellAttributeControllers; }

	/** Gets an array of all Cell Attribute Controllers which posses Faders of the given Element */
	TArray<UDMXControlConsoleCellAttributeController*> GetAllCellAttributeControllersFromElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement) const;

	// Property Name getters
	FORCEINLINE static FName GetCellAttributeControllersPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleMatrixCellController, CellAttributeControllers); }

private:
	/** Generates a Cell Attribute Controller for each Fader of the given Element */
	void GenerateCellAttributeControllers(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement);

	/** Updates the Cell Attribute Controllers array to ensure that each Element has its own Controller */
	void UpdateCellAttributeControllers();

	/** The array of Cell Attribute Controllers for this Matrix Cell Controller */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleCellAttributeController>> CellAttributeControllers;
};
