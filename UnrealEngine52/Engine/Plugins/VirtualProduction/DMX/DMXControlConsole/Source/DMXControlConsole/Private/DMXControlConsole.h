// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DMXControlConsole.generated.h"

class UDMXControlConsoleData;


/** The DMX Control Console */
UCLASS(BlueprintType)
class DMXCONTROLCONSOLE_API UDMXControlConsole
	: public UObject
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDMXControlConsole();

	/** Gets the Control Console data used in this console */
	UDMXControlConsoleData* GetControlConsoleData() const { return ControlConsoleData; }

	/** Copies the provided Control Console Data into this instance */
	void CopyControlConsoleData(UDMXControlConsoleData* InControlConsoleData);

	/** Returns an event that is broadcast when the control console asset is saved */
#if WITH_EDITOR
	DECLARE_EVENT_OneParam(UDMXControlConsole, FDMXControlConsoleSavedEvent, const UDMXControlConsole*)
	FDMXControlConsoleSavedEvent& GetOnControlConsoleSaved() { return OnControlConsoleSaved; }
#endif // WITH_EDITOR

protected:
	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject interface

private:
	/** Control Console reference */
	UPROPERTY(VisibleAnywhere, Category = "DMX Control Console")
	TObjectPtr<UDMXControlConsoleData> ControlConsoleData;

	/** Called when the asset is saved */
#if WITH_EDITOR
	FDMXControlConsoleSavedEvent OnControlConsoleSaved;
#endif // WITH_EDITOR
};
