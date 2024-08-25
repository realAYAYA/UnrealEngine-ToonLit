// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolCommon.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "DMXControlConsoleData.generated.h"

class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFaderGroupController;
class UDMXControlConsoleFaderGroupRow;
class UDMXEntity;
class UDMXEntityFixturePatch;
class UDMXLibrary;


UENUM(BlueprintType)
enum class EDMXControlConsoleStopDMXMode : uint8
{
	SendDefaultValues UMETA(DisplayName = "Send Default Values"),
	SendZeroValues UMETA(DisplayName = "Send Zero Values"),
	DoNotSendValues UMETA(DisplayName = "Keep Last Mapped Values")
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDMXControlConsoleFaderGroupDelegate, const UDMXControlConsoleFaderGroup*);

/** This class is responsible to hold all the data of a DMX Control Console */
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

	/** Returns the Fader Group matching the given Fixture Patch or nullptr if it's no longer valid. */
	UDMXControlConsoleFaderGroup* FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const;

	/** Generates sorted Fader Groups based on the DMX Control Console's current DMX Library */
	void GenerateFromDMXLibrary();

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

	/** Sets the stop DMX mode for this control console */
	void SetStopDMXMode(EDMXControlConsoleStopDMXMode NewStopDMXMode);

	/** Gets the current stop DMX mode of this control console */
	EDMXControlConsoleStopDMXMode GetStopDMXMode() const { return StopDMXMode; }

	/** Updates DMX Output Ports */
	void UpdateOutputPorts(const TArray<FDMXOutputPortSharedRef> InOutputPorts);

	/** Clears FaderGroupRows array from data */
	void Clear(bool bOnlyPatchedFaderGroups = false);

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR 
	//~ End UObject interface

	/** Gets a reference to OnFaderGroupAdded delegate */
	FDMXControlConsoleFaderGroupDelegate& GetOnFaderGroupAdded() { return OnFaderGroupAdded; }

	/** Gets a reference to OnFaderGroupRemoved delegate */
	FDMXControlConsoleFaderGroupDelegate& GetOnFaderGroupRemoved() { return OnFaderGroupRemoved; }

	/** Called when the DMX Library has been reloaded */
	FSimpleMulticastDelegate& GetOnDMXLibraryReloaded() { return OnDMXLibraryReloadedDelegate; }

#if WITH_EDITOR
	/** Called when the DMX Library has been changed */
	FSimpleMulticastDelegate& GetOnDMXLibraryChanged() { return OnDMXLibraryChangedDelegate; }
#endif // WITH_EDITOR 

#if WITH_EDITORONLY_DATA
	/** The current editor filter string */
	UPROPERTY()
	FString FilterString;
#endif // WITH_EDITORONLY_DATA

	// Property Name getters
	FORCEINLINE static FName GetDMXLibraryPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleData, SoftDMXLibraryPtr); }

	// ~ Begin FTickableGameObject interface
	virtual void Tick(float InDeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	// ~ End FTickableGameObject interface

private:
	/** Clears the DMX Control Console to its default */
	void ClearAll();

	/** Clears Patched Fader Groups from data */
	void ClearPatchedFaderGroups();

	/** Called when a Fixture Patch was added to a DMX Library */
	void OnFixturePatchAddedToLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities);

	/** Called when a Fader Group is added to the Control Console */
	FDMXControlConsoleFaderGroupDelegate OnFaderGroupAdded;

	/** Called when a Fader Group is removed from the Control Console */
	FDMXControlConsoleFaderGroupDelegate OnFaderGroupRemoved;

	/** Called when the DMX Library has been reloaded */
	FSimpleMulticastDelegate OnDMXLibraryReloadedDelegate;

#if WITH_EDITOR
	/** Called when the DMX Library has been changed */
	FSimpleMulticastDelegate OnDMXLibraryChangedDelegate;
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

	/** The stop DMX mode currently in use by the console */
	UPROPERTY()
	EDMXControlConsoleStopDMXMode StopDMXMode = EDMXControlConsoleStopDMXMode::DoNotSendValues;

#if WITH_EDITORONLY_DATA
	/** True if the Control Console ticks in Editor */
	bool bSendDMXInEditor = true;
#endif // WITH_EDITORONLY_DATA
};
