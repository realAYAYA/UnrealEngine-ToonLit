// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/Controllers/DMXControlConsoleControllerBase.h"

#include "DMXControlConsoleFaderGroupController.generated.h"

enum class ECheckBoxState : uint8;
class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleData;
class UDMXControlConsoleEditorGlobalLayoutBase;
class UDMXControlConsoleEditorGlobalLayoutRow;
class UDMXControlConsoleElementController;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


/**
 * A controller for handling one or more fader groups at once. 
 * A fader group can be possessed by one controller at a time.
 */
UCLASS(AutoExpandCategories = ("DMX Fader Group Controller"))
class UDMXControlConsoleFaderGroupController
	: public UDMXControlConsoleControllerBase
{
	GENERATED_BODY()

public:
	/** Possesses the given Fader Group. A Fader Group Controller can possess more than one Fader Group at once. */
	void Possess(UDMXControlConsoleFaderGroup* InFaderGroup);

	/** Possesses the given array of Fader Groups. A Fader Group Controller can possess more than one Fader Group at once. */
	void Possess(TArray<UDMXControlConsoleFaderGroup*> InFaderGroups);

	/** Unpossesses the given Fader Group, if valid */
	void UnPossess(UDMXControlConsoleFaderGroup* InFaderGroup);

	/** Groups all the possessed Fader Groups to minimize the number of Element Controllers */
	void Group();

	/** Sorts the array of Fader Groups by their absolute address */
	void SortFaderGroupsByAbsoluteAddress();

	/** Returns the Layout Row this Controller resides in */
	UDMXControlConsoleEditorGlobalLayoutRow& GetOwnerLayoutRowChecked() const;

	/** Creates a Controller for the given Element */
	UDMXControlConsoleElementController* CreateElementController(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement, const FString& InControllerName = "");

	/** Creates a Controller for the given array of Elements */
	UDMXControlConsoleElementController* CreateElementController(const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements, const FString& InControllerName = "");

	/** Generates an Element Controller for each Element of each Fader Group possesed by the Controller */
	void GenerateElementControllers();

	/** Deletes the given Element Controller */
	void DeleteElementController(UDMXControlConsoleElementController* ElementController);

	/** Sorts the array of Element Controllers by the starting address of their Elements */
	void SortElementControllersByStartingAddress() const;

	/** Gets the array of Element Controllers for this Fader Group Controller */
	TArray<UDMXControlConsoleElementController*> GetElementControllers() const { return ElementControllers; }

	/** Gets all single (even nested) Element Controllers for this Fader Group Controller */
	TArray<UDMXControlConsoleElementController*> GetAllElementControllers() const;

	/** Gets an array of all Element Controllers which posses Elements of the given Fader Group Controller */
	TArray<UDMXControlConsoleElementController*> GetAllElementControllersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets the array of Fader Groups in this Controller */
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& GetFaderGroups() const { return FaderGroups; }

	/** Gets the name of the Controller */
	const FString& GetUserName() const { return UserName; };

	/** Generates a string using the names of all the Fader Groups in the Controller */
	FString GenerateUserNameByFaderGroupsNames() const;

	/** Sets the name of the Controller */
	void SetUserName(const FString& NewName);

	/** True if the Controller has any patched Fader Group */
	bool HasFixturePatch() const;

	/** Sets the lock state of this Controller */
	void SetLocked(bool bLock);

	/** Gets Fader Group Controller color for Editor representation */
	const FLinearColor& GetEditorColor() const { return EditorColor; }

	/** Gets the expansion state of the Fader Group Controller */
	bool IsExpanded() const { return bIsExpanded; }

	/** Sets the expansion state of the Fader Group Controller */
	void SetIsExpanded(bool bExpanded, bool bNotify = true);

	/** Gets the activity state of the Fader Group Controller */
	bool IsActive() const;

	/** Sets the activity state of the Fader Group Controller */
	void SetIsActive(bool bActive) { bIsActive = bActive; }

	/** True if any of the Fader Groups in the Controller matches the Control Console filtering system */
	bool IsMatchingFilter() const;

	/** True if the Controller is in the currently active layout */
	bool IsInActiveLayout() const;

	/** Gets the enable state of the controller according to the possesed fader groups */
	ECheckBoxState GetEnabledState() const;

	/** Destroys the Controller */
	virtual void Destroy();

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	//~ End UObject interface

	/** Called when all the Fader Groups in this Controller have been grouped */
	FSimpleMulticastDelegate& GetOnControllerGrouped() { return OnControllerGrouped; }

	/** Called after the Fixture Patch of a Fader Group in the Controller has changed */
	FSimpleMulticastDelegate& GetOnFixturePatchChanged() { return OnFixturePatchChanged; }

	/** Called when the Controller expansion state changes */
	FSimpleMulticastDelegate& GetOnFaderGroupControllerExpanded() { return OnFaderGroupControllerExpanded; }

	// Property Name getters
	FORCEINLINE static FName GetUserNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroupController, UserName); }
	FORCEINLINE static FName GetFaderGroupsPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroupController, FaderGroups); }
	FORCEINLINE static FName GetElementControllersPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroupController, ElementControllers); }
	FORCEINLINE static FName GetEditorColorPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroupController, EditorColor); }

private:
	/** Clears all the Fader Groups in this Controller */
	void ClearFaderGroups();

	/** Clears all the Element Controllers in this Controller */
	void ClearElementControllers();

	/** Generates an Element Controller for each Element of the given Fader Group*/
	void GenerateElementControllers(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Updates the Element Controllers array to ensure that each Element has its own Controller */
	void UpdateElementControllers();

	/** Called when the Fixture Patch of a Fader Group possessed by the Controller has changed */
	void OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch);

	/** Syncs the Controller's Editor Color according to its patched Fader Groups */
	void SyncControllerEditorColor();

	/** Called when the Controller has been grouped */
	FSimpleMulticastDelegate OnControllerGrouped;

	/** Called after the Fixture Patch of a Fader Group in the Controller has changed */
	FSimpleMulticastDelegate OnFixturePatchChanged;

	/** Called when the Controller expansion state changes */
	FSimpleMulticastDelegate OnFaderGroupControllerExpanded;

	/** The current name of this Controller */
	UPROPERTY(EditAnywhere, Category = "DMX Fader Group Controller")
	FString UserName;

	/** The array of Fader Groups in this Controller */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups;

	/** The array of Controllers for the Elements in this Fader Group Controllers */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleElementController>> ElementControllers;

	/** Color for Fader Group Controller representation on the Editor */
	UPROPERTY(EditAnywhere, Category = "DMX Fader Group Controller")
	FLinearColor EditorColor = FLinearColor::White;

	/** Fader Group Controller expansion state saved from the Editor */
	UPROPERTY()
	bool bIsExpanded = true;

	/** In Editor activity state of the Fader Group Controller */
	UPROPERTY()
	bool bIsActive = false;
};
