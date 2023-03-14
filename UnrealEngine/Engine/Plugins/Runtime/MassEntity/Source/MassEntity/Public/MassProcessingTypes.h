// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtilsTypes.h"
#include "InstancedStruct.h"
#include "MassProcessingTypes.generated.h"

#ifndef MASS_DO_PARALLEL
#define MASS_DO_PARALLEL !UE_SERVER
#endif // MASS_DO_PARALLEL

#define WITH_MASSENTITY_DEBUG (!(UE_BUILD_SHIPPING || UE_BUILD_SHIPPING_WITH_EDITOR || UE_BUILD_TEST) && WITH_STRUCTUTILS_DEBUG && 1)

struct FMassEntityManager;
class UMassProcessor;
class UMassCompositeProcessor;
struct FMassCommandBuffer;

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EProcessorExecutionFlags : uint8
{
	None = 0 UMETA(Hidden),
	Standalone = 1 << 0,
	Server = 1 << 1,
	Client = 1 << 2,
	All = Standalone | Server | Client UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EProcessorExecutionFlags);

USTRUCT()
struct FProcessorAuxDataBase
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct MASSENTITY_API FMassProcessingContext
{
	GENERATED_BODY()

	TSharedPtr<FMassEntityManager> EntityManager;
	
	UPROPERTY()
	float DeltaSeconds = 0.f;

	UPROPERTY()
	FInstancedStruct AuxData;

	/** 
	 * If set to "true" the MassExecutor will flush commands at the end of given execution function. 
	 * If "false" the caller is responsible for manually flushing the commands.
	 */
	UPROPERTY()
	bool bFlushCommandBuffer = true; 
		
	TSharedPtr<FMassCommandBuffer> CommandBuffer;
	
	FMassProcessingContext() = default;
	FMassProcessingContext(FMassEntityManager& InEntityManager, const float InDeltaSeconds);
	FMassProcessingContext(TSharedPtr<FMassEntityManager>& InEntityManager, const float InDeltaSeconds);
	~FMassProcessingContext();
};

/** 
 *  Runtime-usable array of MassProcessor copies
 */
USTRUCT()
struct MASSENTITY_API FMassRuntimePipeline
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UMassProcessor>> Processors;

	void Reset();
	void Initialize(UObject& Owner);
	
	/** Creates runtime copies of the given UMassProcessors collection. */
	void SetProcessors(TArray<UMassProcessor*>&& InProcessors);

	/** Creates runtime copies of UMassProcessors given in InProcessors input parameter, using InOwner as new UMassProcessors' outer. */
	void CreateFromArray(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner);

	/** Calls CreateFromArray and calls Initialize on all processors afterwards. */
	void InitializeFromArray(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner);
	
	/** Creates runtime instances of UMassProcessors for each processor class given via InProcessorClasses. 
	 *  The instances will be created with InOwner as outer. */
	void InitializeFromClassArray(TConstArrayView<TSubclassOf<UMassProcessor>> InProcessorClasses, UObject& InOwner);

	/** Creates a runtime instance of every processors in the given InProcessors array. If a processor of that class
	 *  already exists in Processors array it gets overridden. Otherwise it gets added to the end of the collection.*/
	void AppendOrOverrideRuntimeProcessorCopies(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner);

	/** Creates a runtime instance of every processors in the given array if there's no processor of that class in Processors already.
	 *  Call this function when adding processors to an already configured FMassRuntimePipeline instance. If you're creating 
	 *  one from scratch calling any of the InitializeFrom* methods will be more efficient (and will produce same results)
	 *  or call AppendOrOverrideRuntimeProcessorCopies.*/
	void AppendUniqueRuntimeProcessorCopies(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner);

	/** Adds InProcessor to Processors without any additional checks */
	void AppendProcessor(UMassProcessor& Processor);

	/** Creates an instance of ProcessorClass and adds it to Processors without any additional checks */
	void AppendProcessor(TSubclassOf<UMassProcessor> ProcessorClass, UObject& InOwner);

	/** goes through Processor looking for a UMassCompositeProcessor instance which GroupName matches the one given as the parameter */
	UMassCompositeProcessor* FindTopLevelGroupByName(const FName GroupName);

	bool HasProcessorOfExactClass(TSubclassOf<UMassProcessor> InClass) const;
	bool IsEmpty() const { return Processors.IsEmpty();}

	MASSENTITY_API friend uint32 GetTypeHash(const FMassRuntimePipeline& Instance);
};

UENUM()
enum class EMassProcessingPhase : uint8
{
	PrePhysics,
	StartPhysics,
	DuringPhysics,
	EndPhysics,
	PostPhysics,
	FrameEnd,
	MAX,
};
