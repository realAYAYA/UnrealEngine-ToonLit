// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsole.h"

#include "UObject/Object.h"

#include "DMXControlConsoleEditorModel.generated.h"

class UDMXControlConsole;


/** Model of the console currently being edited in the control console editor.  */
UCLASS(Config = DMXEditor)
class UDMXControlConsoleEditorModel final
	: public UObject
{
	GENERATED_BODY()

public:
	/** Returns the edited console */
	UDMXControlConsole* GetEditorConsole() const { return EditorConsole; }
	
	/** Loads the console stored in config or creates a transient console if none is stored in config */
	void LoadConsoleFromConfig();

	/** Creates a new console in the transient package. */
	void CreateNewConsole();

	/** Saves the console. Creates a new asset if the asset was never saved. */
	void SaveConsole();

	/** Saves the console as new asset. */
	void SaveConsoleAs();

	/** Loads a console. Returns the loaded console, or nullptr if the console could not be loaded. */
	void LoadConsole(const FAssetData& AssetData);

	/** 
	 * Creates a new Control Console asset from provided source control console in desired package name.
	 * 
	 * @param SavePackagePath				The package path
	 * @param SaveAssetName					The desired asset name
	 * @param SourceControlConsole			The control console copied into the new console
	 * @return								The newly created console, or nullptr if no console could be created
	 */
	[[nodiscard]] UDMXControlConsole* CreateNewConsoleAsset(const FString& SavePackagePath, const FString& SaveAssetName, UDMXControlConsoleData* SourceControlConsole) const;

	/** Returns a delegate broadcast whenever a console is saved */
	FSimpleMulticastDelegate& GetOnConsoleSaved() { return OnConsoleSavedDelegate; }

	/** Returns a delegate broadcast whenever a console is loaded */
	FSimpleMulticastDelegate& GetOnConsoleLoaded() { return OnConsoleLoadedDelegate; }

protected:
	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	//~ End UObject interface

private:
	/** Saves the current editor console to config */
	void SaveConsoleToConfig();

	/** Finalizes loading a  console. Useful to pass in the result of a dialog. */
	void FinalizeLoadConsole(UDMXControlConsole* ControlConsoleToLoad);

	/** Called when enter pressed a console in the Load Dialog */
	void OnLoadDialogEnterPressedConsole(const TArray<FAssetData>& ControlConsoleAssets);
	
	/** Called when the load dialog selected a console */
	void OnLoadDialogSelectedConsole(const FAssetData& ControlConsoleAssets);

	/** Opens a Save Dialog, returns true if the user's dialog interaction results in a valid OutPackageName. */
	[[nodiscard]] bool OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName) const;

	/** Prompts user to specify a console package name to save to. Returns true if a valid package name was acquired. */
	[[nodiscard]] bool PromptSaveConsolePackage(FString& OutSavePackagePath, FString& OutSaveAssetName) const;

	/** Called at the very end of engine initialization, right before the engine starts ticking. */
	void OnFEngineLoopInitComplete();

	/** Delegate that needs be broadcast whenever a console is saved */
	FSimpleMulticastDelegate OnConsoleSavedDelegate;

	/** Delegate that needs be broadcast whenever a new console is loaded */
	FSimpleMulticastDelegate OnConsoleLoadedDelegate;

	/** The currently edited console */
	UPROPERTY(Transient)
	TObjectPtr<UDMXControlConsole> EditorConsole;

	/** Control console saved in config */
	UPROPERTY(Config)
	FSoftObjectPath DefaultConsolePath;
};
