// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStylusState.h"

#include "EditorSubsystem.h"
#include "TickableEditorObject.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

#include "IStylusInputModule.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogStylusInput, Log, All);

/**
 * Module to handle Wacom-style tablet input using styluses.
 */
class STYLUSINPUT_API IStylusInputModule : public IModuleInterface
{
public:

	/**
	 * Retrieve the module instance.
	 */
	static inline IStylusInputModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IStylusInputModule>("StylusInput");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("StylusInput");
	}
};

// This is the interface that all platform-specific implementations must implement.
class IStylusInputInterfaceInternal
{
public:
	virtual void Tick() = 0;

	virtual IStylusInputDevice* GetInputDevice(int32 Index) const = 0;
	virtual int32 NumInputDevices() const = 0;
};


UCLASS()
class STYLUSINPUT_API UStylusInputSubsystem : 
	public UEditorSubsystem, 
	public FTickableEditorObject
{
	GENERATED_BODY()
public:
	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Retrieve the input device that is at the given index, or nullptr if not found. Corresponds to the StylusIndex in IStylusMessageHandler. */
	const IStylusInputDevice* GetInputDevice(int32 Index) const;

	/** Return the number of active input devices. */
	int32 NumInputDevices() const; 

	/** Add a message handler to receive messages from the stylus. */
	void AddMessageHandler(IStylusMessageHandler& MessageHandler);

	/** Remove a previously registered message handler. */
	void RemoveMessageHandler(IStylusMessageHandler& MessageHandler);

	// FTickableEditorObject implementation
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UStylusInputSubsystem, STATGROUP_Tickables); }

private:
	TSharedPtr<IStylusInputInterfaceInternal> InputInterface;
	TArray<IStylusMessageHandler*> MessageHandlers;

	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& Args);
};