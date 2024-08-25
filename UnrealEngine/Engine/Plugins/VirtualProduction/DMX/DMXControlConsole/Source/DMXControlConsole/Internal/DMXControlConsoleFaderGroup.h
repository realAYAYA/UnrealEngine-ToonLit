// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "DMXControlConsoleFaderGroup.generated.h"

struct FDMXAttributeName;
struct FDMXCell;
struct FDMXFixtureFunction;
class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleControllerBase;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroupRow;
class UDMXControlConsoleFixturePatchFunctionFader;
class UDMXControlConsoleFixturePatchMatrixCell;
class UDMXControlConsoleRawFader;
class UDMXEntity;
class UDMXEntityFixturePatch;
class UDMXLibrary;


/** A Group of Faders in the DMX Control Console */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleFaderGroup
	: public UObject
{
	GENERATED_BODY()

	// Allow the DMXControlConsoleFixturePatchMatrixCell to read Fader Group Data
	friend UDMXControlConsoleFixturePatchMatrixCell;

	DECLARE_MULTICAST_DELEGATE_OneParam(FDMXControlConsoleElementDelegate, IDMXControlConsoleFaderGroupElement*);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FDMXOnFaderGroupFixturePatchChangedDelegate, UDMXControlConsoleFaderGroup*, UDMXEntityFixturePatch*);

public:
	/** Adds a raw fader to this Fader Group */
	UDMXControlConsoleRawFader* AddRawFader();

	/** Adds a fixture patch function fader to this Fader Group */
	UDMXControlConsoleFixturePatchFunctionFader* AddFixturePatchFunctionFader(const FDMXFixtureFunction& FixtureFunction, const int32 InUniverseID, const int32 StartingChannel);

	/** Adds a fixture patch matrix cell to this Fader Group */
	UDMXControlConsoleFixturePatchMatrixCell* AddFixturePatchMatrixCell(const FDMXCell& Cell, const int32 InUniverseID, const int32 StartingChannel);

	/** Deletes an Element from this Fader Group */
	void DeleteElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element);

	/** Clears all Elements in this Group */
	void ClearElements();

	/** Gets the Elements array of this Fader Group */
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> GetElements(bool bSortByUniverseAndAddress = false) const;

	/** Gets all single faders from the Faders array of this Fader Group */
	TArray<UDMXControlConsoleFaderBase*> GetAllFaders() const;

	/** Sorts the array of Elements by their starting address */
	void SortElementsByStartingAddress() const;

	/** Gets this Fader Group's index according to its Fader Group Row owner */
	int32 GetIndex() const;

	/** Gets the owner Fader Group Row of this Fader Group */
	UDMXControlConsoleFaderGroupRow& GetOwnerFaderGroupRowChecked() const;

	/** Gets the Controller of this Fader Group, if valid */
	UDMXControlConsoleControllerBase* GetFaderGroupController() const;

	/** Sets the Controller of this Fader Group */
	void SetFaderGroupController(UDMXControlConsoleControllerBase* NewController);

	/** Gets the name of this Fader Group */
	FString GetFaderGroupName() const { return FaderGroupName; }

	/** Sets the name of the Fader Group */
	void SetFaderGroupName(const FString& NewName);

	/** Automatically generates a Fader Group based on Fixture Patch Ref property */
	void GenerateFromFixturePatch(UDMXEntityFixturePatch* InFixturePatch);

	/** Gets the Fixture Patch used by this Fader Group, if valid */
	UDMXEntityFixturePatch* GetFixturePatch() const { return CachedWeakFixturePatch.Get(); }

	/** Reloads the Fixture Patch used by this Fader Group, if valid */
	void ReloadFixturePatch();

	/** Gets wheter this Fader Group is bound to a Fixture Patch */
	bool HasFixturePatch() const;

	/** Gets wheter this Fader Group has Matrix Faders */
	bool HasMatrixProperties() const;

	/** Gets a universeID to fragment map for the current list of Raw Faders */
	TMap<int32, TMap<int32, uint8>> GetUniverseToFragmentMap() const;

	/** Gets an AttributeName to values map for the current list of Fixture Patch Function Faders */
	TMap<FDMXAttributeName, int32> GetAttributeMap() const;

	/** Gets a cell coordinate to AttributeName/Value map for the current list of Fixture Patch Matrix Faders */
	TMap<FIntPoint, TMap<FDMXAttributeName, float>> GetMatrixCoordinateToAttributeMap() const;

	/** Duplicates this Fader Group if there's no Fixture Patch data */
	UDMXControlConsoleFaderGroup* Duplicate() const;

	/** Clears this Fader Group and all its elements */
	void Clear();

	/** Destroys this Fader Group */
	void Destroy();

	/** True if the Fader Group can't send DMX data */
	bool IsEnabled() const { return bIsEnabled; }

	/** Sets the enable state of this Fader Group */
	void SetEnabled(bool bEnable);

#if WITH_EDITOR
	/** True if Fader Group matches Control Console filtering system */
	bool IsMatchingFilter() const { return bIsMatchingFilter; }

	/** Sets wheter Fader Group matches Control Console filtering system */
	void SetIsMatchingFilter(bool bMatches) { bIsMatchingFilter = bMatches; }

	/** Sets in Editor visibility state of all elements to true */
	void ShowAllElementsInEditor();
#endif // WITH_EDITOR

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

	/** Gets a reference to OnElementAdded delegate */
	FDMXControlConsoleElementDelegate& GetOnElementAdded() { return OnElementAdded; }

	/** Gets a reference to OnElementRemoved delegate */
	FDMXControlConsoleElementDelegate& GetOnElementRemoved() { return OnElementRemoved; }

	/** Gets a reference to OnFixturePatchChanged delegate */
	FDMXOnFaderGroupFixturePatchChangedDelegate& GetOnFixturePatchChanged() { return OnFixturePatchChangedDelegate; }

	// Property Name getters
	FORCEINLINE static FName GetElementsPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, Elements); }
	FORCEINLINE static FName GetFaderGroupNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, FaderGroupName); }
	FORCEINLINE static FName GetSoftFixturePatchPtrPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, SoftFixturePatchPtr); }
	FORCEINLINE static FName GetCachedWeakFixturePatchPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, CachedWeakFixturePatch); }
	FORCEINLINE static FName GetIsEnabledPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, bIsEnabled); }

#if WITH_EDITORONLY_DATA
	/** Last string from Editor filtering */
	UPROPERTY()
	FString FilterString;
#endif // WITH_EDITORONLY_DATA

private:	
	/** Called when a Fixture Patch was removed from a DMX Library */
	void OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities);

	/** Called when the Fixture Patch is changed inside its DMX Library */
	void OnFixturePatchChanged(const UDMXEntityFixturePatch* InFixturePatch);

	/** Updates FaderGroup properties according to the given FixturePatch */
	void UpdateFaderGroupFromFixturePatch(UDMXEntityFixturePatch* InFixturePatch);

	/** Updates FixtureFunctionFaders properties according to the given FixturePatch */
	void UpdateFixturePatchFunctionFaders(UDMXEntityFixturePatch* InFixturePatch);

	/** Updates MatrixCells properties according to the given FixturePatch */
	void UpdateFixturePatchMatrixCells(UDMXEntityFixturePatch* InFixturePatch);

	/** Subscribes this Fader Group to Fixture Patch delegates */
	void SubscribeToFixturePatchDelegates();

	/** Gets the next Universe and Address available for a new fader */
	void GetNextAvailableUniverseAndAddress(int32& OutUniverse, int32& OutAddress) const;

	/** Called when an Element is added to the Fader Group */
	FDMXControlConsoleElementDelegate OnElementAdded;

	/** Called when an Element is removed from the Fader Group */
	FDMXControlConsoleElementDelegate OnElementRemoved;

	/** Called when Fixture Patch is changed */
	FDMXOnFaderGroupFixturePatchChangedDelegate OnFixturePatchChangedDelegate;

	/** Name identifier of this Fader Group */
	UPROPERTY(EditAnywhere, Category = "DMX Fader Group")
	FString FaderGroupName;

	/** Fixture Patch this Fader Group is based on */
	UPROPERTY()
	TSoftObjectPtr<UDMXEntityFixturePatch> SoftFixturePatchPtr;

	/** Cached fixture patch for faster access */
	UPROPERTY(Transient)
	TWeakObjectPtr<UDMXEntityFixturePatch> CachedWeakFixturePatch;

	/** Soft reference to the Controller of this Fader Group */
	UPROPERTY()
	TSoftObjectPtr<UDMXControlConsoleControllerBase> SoftControllerPtr;

	/** Cached reference to the Controller of this Fader Group, for fast access */
	UPROPERTY(Transient)
	TWeakObjectPtr<UDMXControlConsoleControllerBase> CachedWeakFaderGroupController;

	/** Elements in this Fader Group */
	UPROPERTY()
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements;

	/** If true, the Fader Group doesn't send DMX */
	UPROPERTY(EditAnywhere, Category = "DMX Fader Group")
	bool bIsEnabled = true;

#if WITH_EDITORONLY_DATA
	/** True if Fader Group matches Control Console filtering system */
	bool bIsMatchingFilter = true;
#endif
};
