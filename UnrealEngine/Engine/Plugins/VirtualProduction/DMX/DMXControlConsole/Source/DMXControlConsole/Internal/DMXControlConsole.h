// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "DMXControlConsole.generated.h"

class UDMXControlConsoleData;
class UDMXControlConsoleEditorDataBase;
class UDMXControlConsoleEditorLayoutsBase;


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

#if WITH_EDITOR
	/** Returns an event that is broadcast when the control console asset is saved */
	DECLARE_EVENT_OneParam(UDMXControlConsole, FDMXControlConsoleSavedEvent, const UDMXControlConsole*)
	FDMXControlConsoleSavedEvent& GetOnControlConsoleSaved() { return OnControlConsoleSaved; }
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Control Console Editor Layouts reference */
	UPROPERTY()
	TObjectPtr<UDMXControlConsoleEditorLayoutsBase> ControlConsoleEditorLayouts;

	/** Control Console Editor Data reference */
	UPROPERTY()
	TObjectPtr<UDMXControlConsoleEditorDataBase> ControlConsoleEditorData;
#endif // WITH_EDITORONLY_DATA

protected:
	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject interface

private:
	/** Control Console Data reference */
	UPROPERTY(VisibleAnywhere, Category = "DMX Control Console")
	TObjectPtr<UDMXControlConsoleData> ControlConsoleData;

	/** Called when the asset is saved */
#if WITH_EDITOR
	FDMXControlConsoleSavedEvent OnControlConsoleSaved;
#endif // WITH_EDITOR
};
