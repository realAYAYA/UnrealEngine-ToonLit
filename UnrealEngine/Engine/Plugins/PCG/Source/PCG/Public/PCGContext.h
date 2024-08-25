// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGNode.h" // IWYU pragma: keep
#include "Helpers/PCGAsyncState.h"

#include "UObject/GCObject.h"

#include "PCGContext.generated.h"

class UPCGComponent;
class UPCGGraphInterface;
class UPCGSettingsInterface;
class UPCGSpatialData;
struct FPCGGraphCache;
struct FPCGSettingsOverridableParam;
struct FPCGStack;

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

	virtual ~FPCGContext() = default;

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

	EPCGExecutionPhase CurrentPhase = EPCGExecutionPhase::NotExecuted;
	int32 BypassedOutputCount = 0;

	/** The current call stack. */
	const FPCGStack* Stack = nullptr;

	const UPCGSettingsInterface* GetInputSettingsInterface() const;
	
	// After initializing the context, we can call this method to prepare for parameter override
	// It will create a copy of the original settings if there is indeed a possible override.
	void InitializeSettings();

	// If we any any parameter override, it will read from the params and override matching values
	// in the settings copy.
	void OverrideSettings();

	// Returns true if the given property has been overriden
	bool IsValueOverriden(const FName PropertyName);

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

	/** Time slicing is not enabled by default. */
	virtual bool TimeSliceIsEnabled() const { return false; }

#if WITH_EDITOR
	/** Log warnings and errors to be displayed on node in graph editor. */
	void LogVisual(ELogVerbosity::Type InVerbosity, const FText& InMessage) const;

	/** True if any issues were logged during last execution. */
	bool HasVisualLogs() const;
#endif // WITH_EDITOR

	/** Gathers references to objects to prevent them from being garbage collected. Extend resources to be collected with AddExtractStructReferencedObjects. */
	// Implementation note: this is NOT the same as a struct using WithAddStructReferencedObjects since we are not holding contexts
	//  in properties in any case. This will be called from the graph executor when needed and is implemented to look like normal reference traversal.
	void AddStructReferencedObjects(FReferenceCollector& Collector);

	/** Caution: most use cases should use GetInputSettings, because they contain the overridden values. Use this one if you really need to get the original pointer. */
	template<typename SettingsType>
	const SettingsType* GetOriginalSettings() const 
	{
		return PCGContextHelpers::GetInputSettings<SettingsType>(Node, InputData);
	}

protected:
	virtual UObject* GetExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) { return nullptr; }
	virtual void* GetUnsafeExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) { return nullptr; }
	virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) {}

private:
	// Copy of the settings that will be used to apply overrides.
	TObjectPtr<UPCGSettings> SettingsWithOverride = nullptr;

	// List of params that were in effect overriden
	TArray<const FPCGSettingsOverridableParam*> OverriddenParams;
};