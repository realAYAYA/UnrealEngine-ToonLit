// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleData.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"

#include "DMXControlConsoleEditorModel.generated.h"

class FDMXControlConsoleEditorSelection;
class UDMXControlConsole;
class UDMXControlConsoleEditorData;
class UDMXControlConsoleEditorLayouts;
class UDMXControlConsoleFaderGroupController;

namespace UE::DMX::Private { class FDMXControlConsoleEditorToolkit; }
namespace UE::DMX::Private { class FDMXControlConsoleGlobalFilterModel; }


/** Model of the console currently being edited in the control console editor.  */
UCLASS()
class UDMXControlConsoleEditorModel 
	: public UObject
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FDMXControlConsoleFaderGroupControllerDelegate, const UDMXControlConsoleFaderGroupController*);

public:
	/** Initializes the model */
	void Initialize(const TSharedPtr<UE::DMX::Private::FDMXControlConsoleEditorToolkit>& InToolkit);

	/** Returns the edited Control Console */
	UDMXControlConsole* GetControlConsole() const { return ControlConsole.IsValid() ? ControlConsole.Get() : nullptr; }

	/** Returns the edited Control Console data */
	UDMXControlConsoleData* GetControlConsoleData() const;

	/** Returns the edited Control Console editor data */
	UDMXControlConsoleEditorData* GetControlConsoleEditorData() const;

	/** Returns the edited Control Console layouts */
	UDMXControlConsoleEditorLayouts* GetControlConsoleLayouts() const;

	/** Gets a reference to the Selection Handler*/
	TSharedRef<FDMXControlConsoleEditorSelection> GetSelectionHandler();

	/** Gets a reference to the Filter Model */
	TSharedRef<UE::DMX::Private::FDMXControlConsoleGlobalFilterModel> GetGlobalFilterModel();

	/** Scrolls the given Fader Group Controller into view */
	void ScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController) const;

	/** Requests the Editor Model to be updated */
	void RequestUpdateEditorModel();

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	//~ End UObject interface

	/** Returns a delegate broadcast whenever a Fader Group Controller needs to be scrolled into view */
	FDMXControlConsoleFaderGroupControllerDelegate& GetOnScrollFaderGroupControllerIntoView() { return OnScrollFaderGroupControllerIntoView; }

	/** Returns a delegate broadcast whenever Editor Model has changed */
	FSimpleMulticastDelegate& GetOnEditorModelUpdated() { return OnEditorModelUpdated; }

private:
	/** Updates the Editor Model */
	void UpdateEditorModel();

	/** Binds Editor Model to the current DMX Library changes */
	void BindToDMXLibraryChanges();

	/** Unbinds Editor Model from the current DMX Library changes */
	void UnbindFromDMXLibraryChanges();

	/** Initializes current Control Console's Editor Data, if not valid */
	void InitializeEditorData() const;

	/** Initializes current Control Console's Editor Layouts, if not valid */
	void InitializeEditorLayouts() const;

	/** Registers current Editor Layouts to delegates */
	void RegisterEditorLayouts() const;

	/** Unregisters current Editor Layouts from delegates */
	void UnregisterEditorLayouts() const;

	/** Called when the DMX Library of the current Control Console has been changed */
	void OnDMXLibraryChanged();

	/** Called before the engine is shut down */
	void OnEnginePreExit();

	/** Called when a Fader Group Controller needs to be scrolled into view */
	FDMXControlConsoleFaderGroupControllerDelegate OnScrollFaderGroupControllerIntoView;

	/** Called when Editor Model has been updated */
	FSimpleMulticastDelegate OnEditorModelUpdated;

	/** Timer handle in use while updating Editor Model is requested but not carried out yet */
	FTimerHandle UpdateEditorModelTimerHandle;

	/** The global filter model for the current edited Control Console */
	TSharedPtr<UE::DMX::Private::FDMXControlConsoleGlobalFilterModel> GlobalFilterModel;

	/** Selection handler for the current edited Control Console */
	TSharedPtr<FDMXControlConsoleEditorSelection> SelectionHandler;

	/** Weak reference to the Control Console asset toolkit */
	TWeakPtr<UE::DMX::Private::FDMXControlConsoleEditorToolkit> WeakToolkit;

	/** Weak reference to the current edited Control Console */
	TWeakObjectPtr<UDMXControlConsole> ControlConsole;
};
