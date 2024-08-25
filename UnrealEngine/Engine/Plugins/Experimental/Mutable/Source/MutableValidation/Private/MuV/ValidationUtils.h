// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MuCO/CustomizableObjectInstance.h"
#include "UObject/Object.h"

#include "ValidationUtils.generated.h"

// Forward declarations
class UCustomizableObject;
class UCustomizableObjectInstance;
class USkeletalMeshComponent;
struct FCompilationOptions;


/**
 * Prepare the asset registry so we can later use it to search assets. It is required by Mutable to compile.
 */
void PrepareAssetRegistry();


/**
 * Hold the thread for the time specified while ticking the engine.
 * @param ToWaitSeconds The time in seconds we want to hold the execution of the thread
 */
void Wait(const double ToWaitSeconds);


/**
 * Compiles a CO synchronously
 * @param InCustomizableObject The Customizable Object we want to synchronously compile
 * @param bLogMutableLogs Enables or disables the logging of Log category logs relevant to the CO compilation. Required to avoid a MongoDB limitation with the duplication of MongoDB document names.
 * @param InCompilationOptionsOverride The configuration for the compilation of the CO we want to use instead of the one part of the CO.
 * @return True if the compilation was successful and false if it failed.
 */
bool CompileCustomizableObject(UCustomizableObject* InCustomizableObject, const bool bLogMutableLogs = true, const FCompilationOptions* InCompilationOptionsOverride = nullptr);


/**
 * Logs some configuration data related to how mutable will compile and then generate instances. We do this so we can later
 * Isolate tests using different configurations.
 * @note Add new logs each time you add a way to change the configuration of the test from the .xml testing file
 */
void LogMutableSettings();


/**
 * Helping class that handles the async update of the provided instance. It will also wait for the mips of it so they get streamed.
 */
UCLASS()
class UCOIUpdater : public UObject
{
	GENERATED_BODY()
	
public:
	
	/**
	 * Updates the provided customizable object instance.
	 * @param InInstance Instance to be compiled
	 * @return True if the update was successful and false otherwise
	 */
	bool UpdateInstance(UCustomizableObjectInstance* InInstance);
	

private:
	
	/**
	 * Callback executed when the instance being updated finishes it's mesh update.
	 * @param Result The result of the updating operation
	 */
	UFUNCTION()
	void OnInstanceUpdateResult(const FUpdateContext& Result);

	/** The instance that is currently being handled by this class. */
	UPROPERTY()
	TObjectPtr<UCustomizableObjectInstance> Instance;

	/** The components of the Instance that we are currently waiting for their mips to be streamed in */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> ComponentsBeingUpdated;

	/** Flag used to control if we are updating and instance or not. Once it gets set to false then the update gets halted and the program continues */
	bool bIsInstanceBeingUpdated = false;

	/** False if the instance did update successfully, true if it failed */
	bool bInstanceFailedUpdate = false;
};
