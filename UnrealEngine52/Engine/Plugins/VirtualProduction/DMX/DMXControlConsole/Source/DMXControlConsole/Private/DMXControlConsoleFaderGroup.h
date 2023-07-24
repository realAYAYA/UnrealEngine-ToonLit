// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DMXControlConsoleFaderGroup.generated.h"

struct FDMXAttributeName;
struct FDMXCell;
struct FDMXFixtureFunction;
class IDMXControlConsoleFaderGroupElement;
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

	/** Gets the Faders array of this Fader Group */
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> GetElements() const { return Elements; };

	/** Gets all single faders from Faders array Fader Group */
	TArray<UDMXControlConsoleFaderBase*> GetAllFaders() const;

	/** Gets this Fader Group's index according to its Fader Group Row owner */
	int32 GetIndex() const;

	/** Gets the owner Fader Group Row of this Fader Group */
	UDMXControlConsoleFaderGroupRow& GetOwnerFaderGroupRowChecked() const;

	/** Gets the name of this Fader Group */
	FString GetFaderGroupName() const { return FaderGroupName; }

	/** Sets the name of the Fader Group */
	void SetFaderGroupName(const FString& NewName);

	/** Automatically generates a Fader Group based on Fixture Patch Ref property */
	void GenerateFromFixturePatch(UDMXEntityFixturePatch* InFixturePatch);

	/** Gets current binded Fixture Patch reference, if valid */
	UDMXEntityFixturePatch* GetFixturePatch() const { return CachedWeakFixturePatch.Get(); }

	/** Gets wheter this Fader Group is binded to a Fixture Patch */
	bool HasFixturePatch() const;

	/** Gets wheter this Fader Group has Matrix Faders */
	bool HasMatrixProperties() const;

	/** Gets a universeID to fragment map for the current list of Raw Faders */
	TMap<int32, TMap<int32, uint8>> GetUniverseToFragmentMap() const;

	/** Gets an AttributeName to values map for the current list of Fixture Patch Function Faders */
	TMap<FDMXAttributeName, int32> GetAttributeMap() const;

	/** Gets a cell coordinate to AttributeName/Value map for the current list of Fixture Patch Matrix Faders */
	TMap<FIntPoint, TMap<FDMXAttributeName, float>> GetMatrixCoordinateToAttributeMap() const;

	/** Resets this Fader Group to its default parameters */
	void Reset();

	/** Destroys this Fader Group */
	void Destroy();

	/** Resets bForceRefresh condition */
	void ForceRefresh();

	/** True if this Fader Group needs to be refreshed */
	bool HasForceRefresh() const { return bForceRefresh; }

#if WITH_EDITOR
	/** Gets Fader Group color for Editor representation */
	const FLinearColor& GetEditorColor() const { return EditorColor; }
#endif // WITH_EDITOR

	// Property Name getters
	FORCEINLINE static FName GetElementsPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, Elements); }
	FORCEINLINE static FName GetFaderGroupNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, FaderGroupName); }
	FORCEINLINE static FName GetSoftFixturePatchPtrPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, SoftFixturePatchPtr); }
	FORCEINLINE static FName GetCachedWeakFixturePatchPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, CachedWeakFixturePatch); }
#if WITH_EDITOR
	FORCEINLINE static FName GetEditorColorPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, EditorColor); }
#endif // WITH_EDITOR

protected:
	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

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

	/** Name identifier of this Fader Group */
	UPROPERTY(EditAnywhere, Category = "DMX Fader Group")
	FString FaderGroupName;

	/** Fixture Patch this Fader Group is based on */
	UPROPERTY()
	TSoftObjectPtr<UDMXEntityFixturePatch> SoftFixturePatchPtr;

	/** Cached fixture patch for faster access */
	UPROPERTY(Transient)
	TWeakObjectPtr<UDMXEntityFixturePatch> CachedWeakFixturePatch;

	/** Elements in this Fader Group */
	UPROPERTY()
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements;

#if WITH_EDITORONLY_DATA
	/** Color for Fader Group representation on the Editor */
	UPROPERTY(EditAnywhere, Category = "DMX Fader Group")
	FLinearColor EditorColor = FLinearColor::White;
#endif

	/** Shows wheter this Fader Group needs to be refreshed or not */
	bool bForceRefresh = false;
};
