// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolCommon.h"

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "DMXControlConsoleData.generated.h"

class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFaderGroupRow;
class UDMXLibrary;


/** The DMX Control Console */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleData 
	: public UObject
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Adds a Fader Group Row to this DMX Control Console */
	UDMXControlConsoleFaderGroupRow* AddFaderGroupRow(const int32 RowIndex);

	/** Removes a Fader Group Row from this DMX Control Console */
	void DeleteFaderGroupRow(const TObjectPtr<UDMXControlConsoleFaderGroupRow>& FaderGroupRow);

	/** Gets this DMX Control Console's Fader Group Rows array */
	const TArray<UDMXControlConsoleFaderGroupRow*>& GetFaderGroupRows() const { return FaderGroupRows; }

	/** Gets an array of all Fader Groups in this DMX Control Console */
	TArray<UDMXControlConsoleFaderGroup*> GetAllFaderGroups() const;

	/** Generates sorted Fader Groups based on the DMX Control Console's current DMX Library */
	void GenerateFromDMXLibrary();

	/** Gets this DMX Control Console's DMXLibrary */
	UDMXLibrary* GetDMXLibrary() const { return DMXLibrary.Get(); }

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

	/** Resets the DMX Control Console to its default */
	void Reset();

	// Property Name getters
	FORCEINLINE static FName GetDMXLibraryPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleData, DMXLibrary); }
	FORCEINLINE static FName GetFaderGroupRowsPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleData, FaderGroupRows); }

protected:
	// ~ Begin FTickableGameObject interface
	virtual void Tick(float InDeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	// ~ End FTickableGameObject interface

private:
	/** Clears FaderGroupRows array */
	void ClearFaderGroupRows();

	/** Library used to generate Fader Groups */
	UPROPERTY(EditAnywhere, Category = "DMX Control Console")
	TWeakObjectPtr<UDMXLibrary> DMXLibrary;

	/** DMX Control Console's Fader Group Rows array */
	UPROPERTY(VisibleAnywhere, Category = "DMX Control Console")
	TArray<TObjectPtr<UDMXControlConsoleFaderGroupRow>> FaderGroupRows;

	/** Output ports to output dmx to */
	TArray<FDMXOutputPortSharedRef> OutputPorts;

	/** True when this object is ticking */
	bool bSendDMX = false;

#if WITH_EDITOR
	/** True if the Control Console ticks in Editor */
	bool bSendDMXInEditor = true;
#endif // WITH_EDITOR
};
