// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGNode.h" // IWYU pragma: keep
#include "Helpers/PCGAsyncState.h"

#include "PCGContext.generated.h"

class UPCGComponent;
class UPCGGraphInterface;
class UPCGSettingsInterface;
class UPCGSpatialData;
struct FPCGGraphCache;
struct FPCGSettingsOverridableParam;

namespace PCGContextHelpers
{
	template<typename SettingsType>
	const SettingsType* GetInputSettings(const UPCGNode* Node, const FPCGDataCollection& InputData)
	{
		if (Node && Node->GetSettings())
		{
			return Cast<SettingsType>(InputData.GetSettings(Node->GetSettings()));
		}
		else
		{
			return InputData.GetSettings<SettingsType>();
		}
	}
}

UENUM()
enum class EPCGExecutionPhase : uint8
{
		NotExecuted = 0,
		PrepareData,
		Execute,
		PostExecute,
		Done
};

USTRUCT(BlueprintType)
struct PCG_API FPCGContext
{
	GENERATED_BODY()

	virtual ~FPCGContext();

	FPCGDataCollection InputData;
	FPCGDataCollection OutputData;
	TWeakObjectPtr<UPCGComponent> SourceComponent = nullptr;

	FPCGAsyncState AsyncState;
	FPCGCrc DependenciesCrc;

	// TODO: replace this by a better identification mechanism
	const UPCGNode* Node = nullptr;
	FPCGTaskId TaskId = InvalidPCGTaskId;
	FPCGTaskId CompiledTaskId = InvalidPCGTaskId;
	bool bIsPaused = false;

	// Used to preven settings override being deleted, needs to be false when going through blueprint calls with a context
	bool bShouldUnrootSettingsOnDelete = true;

	EPCGExecutionPhase CurrentPhase = EPCGExecutionPhase::NotExecuted;
	int32 BypassedOutputCount = 0;

	const UPCGSettingsInterface* GetInputSettingsInterface() const;
	
	// After initializing the context, we can call this method to prepare for parameter override
	// It will create a copy of the original settings if there is indeed a possible override.
	void InitializeSettings();

	// If we any any parameter override, it will read from the params and override matching values
	// in the settings copy.
	void OverrideSettings();

	// Return the seed, possibly overriden by params, and combined with the source component (if any).
	int GetSeed() const;

	// Return the settings casted in the wanted type.
	// If there is any override, those settings will already contains all the overriden values.
	template<typename SettingsType>
	const SettingsType* GetInputSettings() const
	{
		return SettingsWithOverride ? Cast<SettingsType>(SettingsWithOverride) : GetOriginalSettings<SettingsType>();
	}

	FString GetTaskName() const;
	FString GetComponentName() const;
	bool ShouldStop() const { return AsyncState.ShouldStop(); }

	AActor* GetTargetActor(const UPCGSpatialData* InSpatialData) const;

#if WITH_EDITOR
	/** Log warnings and errors to be displayed on node in graph editor. */
	void LogVisual(ELogVerbosity::Type InVerbosity, const FText& InMessage) const;

	/** True if any issues were logged during last execution. */
	bool HasVisualLogs() const;
#endif // WITH_EDITOR

protected:
	virtual UObject* GetExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) { return nullptr; }
	virtual void* GetUnsafeExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) { return nullptr; }

private:
	template<typename SettingsType>
	const SettingsType* GetOriginalSettings() const 
	{
		return PCGContextHelpers::GetInputSettings<SettingsType>(Node, InputData);
	}

	// Copy of the settings that will be used to apply overrides.
	TObjectPtr<UPCGSettings> SettingsWithOverride = nullptr;
};
