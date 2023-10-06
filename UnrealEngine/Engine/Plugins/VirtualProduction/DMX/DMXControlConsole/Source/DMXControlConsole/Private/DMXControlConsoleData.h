// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolCommon.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "DMXControlConsoleData.generated.h"

class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFaderGroupRow;
class UDMXEntity;
class UDMXEntityFixturePatch;
class UDMXLibrary;


DECLARE_MULTICAST_DELEGATE_OneParam(FDMXControlConsoleFaderGroupDelegate, const UDMXControlConsoleFaderGroup*);

/** The DMX Control Console */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleData 
	: public UObject
	, public FTickableGameObject
{
	GENERATED_BODY()

	// Allow the DMXControlConsoleFaderGroupRow to read Control Console Data
	friend UDMXControlConsoleFaderGroupRow;

public:
	/** Adds a Fader Group Row to this DMX Control Console */
	UDMXControlConsoleFaderGroupRow* AddFaderGroupRow(const int32 RowIndex);

	/** Removes a Fader Group Row from this DMX Control Console */
	void DeleteFaderGroupRow(const TObjectPtr<UDMXControlConsoleFaderGroupRow>& FaderGroupRow);

	/** Gets this DMX Control Console's Fader Group Rows array */
	const TArray<UDMXControlConsoleFaderGroupRow*>& GetFaderGroupRows() const { return FaderGroupRows; }

	/** Gets an array of all Fader Groups in this DMX Control Console */
	TArray<UDMXControlConsoleFaderGroup*> GetAllFaderGroups() const;

#if WITH_EDITOR
	/** Gets an array of all active Fader Groups in this DMX Control Console */
	TArray<UDMXControlConsoleFaderGroup*> GetAllActiveFaderGroups() const;
#endif // WITH_EDITOR 

	/** Generates sorted Fader Groups based on the DMX Control Console's current DMX Library */
	void GenerateFromDMXLibrary();

	/** Finds the Fader Group matching the given Fixture Patch, if valid */
	UDMXControlConsoleFaderGroup* FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const;

	/** Gets this DMX Control Console's DMXLibrary */
	UDMXLibrary* GetDMXLibrary() const { return CachedWeakDMXLibrary.Get(); }

	/** Sends DMX on this DMX Control Console on tick */
	void StartSendingDMX();

	/** Stops DMX on this DMX Control Console on tick */
	void StopSendingDMX();

	/** Gets if DMX is sending DMX data or not */
	bool IsSendingDMX() const { return bSendDMX; }

#if WITH_EDITOR
	/** Sets if the console can send DMX in Editor */
	void SetSendDMXInEditorEnabled(bool bSendDMXInEditorEnabled) { bSendDMXInEditor = bSendDMXInEditorEnabled; }
#endif // WITH_EDITOR 

	/** Updates DMX Output Ports */
	void UpdateOutputPorts(const TArray<FDMXOutputPortSharedRef> InOutputPorts);

	/** Clears FaderGroupRows array from data */
	void Clear();

	/** Clears Patched Fader Groups from data */
	void ClearPatchedFaderGroups();

	/** Clears the DMX Control Console to its default */
	void ClearAll(bool bOnlyPatchedFaderGroups = false);

	/** Called when a Fixture Patch was added to a DMX Library */
	void OnFixturePatchAddedToLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities);

	/** Gets a reference to OnFaderGroupAdded delegate */
	FDMXControlConsoleFaderGroupDelegate& GetOnFaderGroupAdded() { return OnFaderGroupAdded; }

	/** Gets a reference to OnFaderGroupRemoved delegate */
	FDMXControlConsoleFaderGroupDelegate& GetOnFaderGroupRemoved() { return OnFaderGroupRemoved; }

#if WITH_EDITOR
	/** Called when the DMX Library has been changed */
	static FSimpleMulticastDelegate& GetOnDMXLibraryChanged() { return OnDMXLibraryChanged; }
#endif // WITH_EDITOR 

#if WITH_EDITORONLY_DATA
	/** The current editor filter string */
	UPROPERTY()
	FString FilterString;
#endif // WITH_EDITORONLY_DATA

	// Property Name getters
	FORCEINLINE static FName GetDMXLibraryPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleData, SoftDMXLibraryPtr); }

protected:
	//~ Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR 
	//~ End UObject interface

	// ~ Begin FTickableGameObject interface
	virtual void Tick(float InDeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	// ~ End FTickableGameObject interface

private:
	/** Called when a Fader Group is added to the Control Console */
	FDMXControlConsoleFaderGroupDelegate OnFaderGroupAdded;

	/** Called when a Fader Group is removed from the Control Console */
	FDMXControlConsoleFaderGroupDelegate OnFaderGroupRemoved;

#if WITH_EDITOR
	/** Called when the DMX Library has been changed */
	static FSimpleMulticastDelegate OnDMXLibraryChanged;
#endif // WITH_EDITOR 

	/** Library used to generate Fader Groups */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "DMX Library", ShowDisplayNames), Category = "DMX Control Console")
	TSoftObjectPtr<UDMXLibrary> SoftDMXLibraryPtr;

	/** Cached DMX Library for faster access */
	UPROPERTY()
	TWeakObjectPtr<UDMXLibrary> CachedWeakDMXLibrary;

	/** DMX Control Console's Fader Group Rows array */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleFaderGroupRow>> FaderGroupRows;

	/** Output ports to output dmx to */
	TArray<FDMXOutputPortSharedRef> OutputPorts;

	/** True when this object is ticking */
	bool bSendDMX = false;

#if WITH_EDITORONLY_DATA
	/** True if the Control Console ticks in Editor */
	bool bSendDMXInEditor = true;
#endif // WITH_EDITORONLY_DATA
};
