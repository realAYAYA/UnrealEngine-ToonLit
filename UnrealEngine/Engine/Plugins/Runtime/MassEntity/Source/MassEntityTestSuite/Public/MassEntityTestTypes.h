// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "AITestsCommon.h"
#include "Math/RandomStream.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Misc/MTAccessDetector.h"
#include "MassExternalSubsystemTraits.h"
#include "MassProcessingPhaseManager.h"
#include "MassEntityTestTypes.generated.h"


struct FMassEntityManager;
struct FMassProcessingPhaseManager;
namespace UE::Mass::Testing
{
	struct FMassTestProcessingPhaseManager;
}

USTRUCT()
struct FTestFragment_Float : public FMassFragment
{
	GENERATED_BODY()
	float Value = 0;

	FTestFragment_Float(const float InValue = 0.f) : Value(InValue) {}
};

USTRUCT()
struct FTestFragment_Int : public FMassFragment
{
	GENERATED_BODY()
	int32 Value = 0;

	FTestFragment_Int(const int32 InValue = 0) : Value(InValue) {}

	static constexpr int32 TestIntValue = 123456;
};

USTRUCT()
struct FTestFragment_Bool : public FMassFragment
{
	GENERATED_BODY()
	bool bValue = false;

	FTestFragment_Bool(const bool bInValue = false) : bValue(bInValue) {}
};

USTRUCT()
struct FTestFragment_Large : public FMassFragment
{
	GENERATED_BODY()
	uint8 Value[64];

	FTestFragment_Large(uint8 Fill = 0)
	{
		FMemory::Memset(Value, Fill, 64);
	}
};

USTRUCT()
struct FTestFragment_Array : public FMassFragment
{
	GENERATED_BODY()
	TArray<int32> Value;

	FTestFragment_Array(uint8 Num = 0)
	{
		Value.Reserve(Num);
	}
};

USTRUCT()
struct FTestChunkFragment_Int : public FMassChunkFragment
{
	GENERATED_BODY()
	int32 Value = 0;

	FTestChunkFragment_Int(const int32 InValue = 0) : Value(InValue) {}
};

USTRUCT()
struct FTestSharedFragment_Int : public FMassSharedFragment
{
	GENERATED_BODY()
	int32 Value = 0;

	FTestSharedFragment_Int(const int32 InValue = 0) : Value(InValue) {}
};

USTRUCT()
struct FTestSharedFragment_Float : public FMassSharedFragment
{
	GENERATED_BODY()
	float Value = 0.f;

	FTestSharedFragment_Float(const float InValue = 0) : Value(InValue) {}
};

/** @todo rename to FTestTag */
USTRUCT()
struct FTestFragment_Tag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestTag_A : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestTag_B : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestTag_C : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestTag_D : public FMassTag
{
	GENERATED_BODY()
};


UCLASS()
class MASSENTITYTESTSUITE_API UMassTestProcessorBase : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassTestProcessorBase();
	FMassProcessorExecutionOrder& GetMutableExecutionOrder() { return ExecutionOrder; }
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual bool ShouldAllowQueryBasedPruning(const bool bRuntimeMode = true) const { return false; }
	/** 
	 * ConfigureQueries is called in PostInitProperties, so there's no point in calling anything here, 
	 * since the EntityQuery configuration will take place after a processor instance gets created
	 * (so after PostInitProperties has already been called). Use EntityQuery directly instead. 
	 */
	virtual void ConfigureQueries() override {}

	using FExecutionFunction = TFunction<void(FMassEntityManager& EntityManager, FMassExecutionContext& Context)>;
	FExecutionFunction ExecutionFunction;

	/** 
	 * By default ExecutionFunction is configured to pass this function over to EntityQuery.ForEachEntityChunk call. 
	 * Note that this function won't be used if you override ExecutionFunction's default value.
	 */
	FMassExecuteFunction ForEachEntityChunkExecutionFunction;


	void SetShouldAllowMultipleInstances(const bool bInShouldAllowDuplicated) { bAllowMultipleInstances = bInShouldAllowDuplicated; }

	/** public on purpose, this is a test processor, no worries about access*/
	FMassEntityQuery EntityQuery;
};

UCLASS()
class MASSENTITYTESTSUITE_API UMassTestProcessor_A : public UMassTestProcessorBase
{
	GENERATED_BODY()
};

UCLASS()
class MASSENTITYTESTSUITE_API UMassTestProcessor_B : public UMassTestProcessorBase
{
	GENERATED_BODY()
};

UCLASS()
class MASSENTITYTESTSUITE_API UMassTestProcessor_C : public UMassTestProcessorBase
{
	GENERATED_BODY()
};

UCLASS()
class MASSENTITYTESTSUITE_API UMassTestProcessor_D : public UMassTestProcessorBase
{
	GENERATED_BODY()
};

UCLASS()
class MASSENTITYTESTSUITE_API UMassTestProcessor_E : public UMassTestProcessorBase
{
	GENERATED_BODY()
};

UCLASS()
class MASSENTITYTESTSUITE_API UMassTestProcessor_F : public UMassTestProcessorBase
{
	GENERATED_BODY()
};

UCLASS()
class UMassTestProcessor_Floats : public UMassTestProcessorBase
{
	GENERATED_BODY()
public:
	TArrayView<FTestFragment_Float> Floats;
	UMassTestProcessor_Floats();
};

UCLASS()
class UMassTestProcessor_Ints : public UMassTestProcessorBase
{
	GENERATED_BODY()
public:
	TArrayView<FTestFragment_Int> Ints;
	UMassTestProcessor_Ints();
};

UCLASS()
class UMassTestProcessor_FloatsInts : public UMassTestProcessorBase
{
	GENERATED_BODY()
public:
	TArrayView<FTestFragment_Float> Floats;
	TArrayView<FTestFragment_Int> Ints;
	UMassTestProcessor_FloatsInts();
};

UCLASS()
class MASSENTITYTESTSUITE_API UMassTestStaticCounterProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassTestStaticCounterProcessor();
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override
	{
		++StaticCounter;
	}
	virtual void ConfigureQueries() override {}
	virtual bool ShouldAllowQueryBasedPruning(const bool bRuntimeMode = true) const { return false; }

	static int StaticCounter;
};

struct MASSENTITYTESTSUITE_API FExecutionTestBase : FAITestBase
{
	TSharedPtr<FMassEntityManager> EntityManager;
	bool bMakeWorldEntityManagersOwner = false;

	virtual bool SetUp() override;
};

struct MASSENTITYTESTSUITE_API FEntityTestBase : FExecutionTestBase
{
	FMassArchetypeHandle EmptyArchetype;
	FMassArchetypeHandle FloatsArchetype;
	FMassArchetypeHandle IntsArchetype;
	FMassArchetypeHandle FloatsIntsArchetype;

	FInstancedStruct InstanceInt;

	virtual bool SetUp() override;
};


struct MASSENTITYTESTSUITE_API FProcessingPhasesTestBase : FEntityTestBase
{
	using Super = FEntityTestBase;

	TSharedPtr<UE::Mass::Testing::FMassTestProcessingPhaseManager> PhaseManager;
	FMassProcessingPhaseConfig PhasesConfig[int(EMassProcessingPhase::MAX)];
	int32 TickIndex = -1;
	FGraphEventRef CompletionEvent;
	float DeltaTime = 1.f / 30;
	UWorld* World = nullptr;

	FProcessingPhasesTestBase();
	virtual bool SetUp() override;
	virtual bool Update() override;
	virtual void TearDown() override;
	virtual void VerifyLatentResults() override;
	virtual bool PopulatePhasesConfig() = 0;
};

template<typename T>
void ShuffleDataWithRandomStream(FRandomStream& Rand, TArray<T>& Data)
{
	for (int i = 0; i < Data.Num(); ++i)
	{
		const int32 NewIndex = Rand.RandRange(0, Data.Num() - 1);
		Data.Swap(i, NewIndex);
	}
}

UCLASS()
class UMassTestWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void Write(int32 InNumber);
	int32 Read() const;

private:
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(AccessDetector);
	int Number = 0;
};

template<>
struct TMassExternalSubsystemTraits<UMassTestWorldSubsystem>
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeRead = true,
		ThreadSafeWrite = false,
	};
};

UCLASS()
class UMassTestEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
};

template<>
struct TMassExternalSubsystemTraits<UMassTestEngineSubsystem>
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeRead = true,
		ThreadSafeWrite = false,
	};
};

UCLASS()
class UMassTestLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
};

template<>
struct TMassExternalSubsystemTraits<UMassTestLocalPlayerSubsystem>
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeRead = true,
		ThreadSafeWrite = false,
	};
}; 

UCLASS()
class UMassTestGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
};

template<>
struct TMassExternalSubsystemTraits<UMassTestGameInstanceSubsystem>
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeRead = true,
		ThreadSafeWrite = false,
	};
};

namespace UE::Mass::Testing
{
/** Test-time TaskGraph task for triggering processing phases. */
struct FMassTestPhaseTickTask
{
	FMassTestPhaseTickTask(const TSharedRef<FMassProcessingPhaseManager>& InPhaseManager, const EMassProcessingPhase InPhase, const float InDeltaTime);

	static TStatId GetStatId();
	static ENamedThreads::Type GetDesiredThread();
	static ESubsequentsMode::Type GetSubsequentsMode();

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

private:
	const TSharedRef<FMassProcessingPhaseManager> PhaseManager;
	const EMassProcessingPhase Phase = EMassProcessingPhase::MAX;
	const float DeltaTime = 0.f;
};

/** The main point of this FMassProcessingPhaseManager extension is to disable world-based ticking, even if a world is available. */
struct FMassTestProcessingPhaseManager : public FMassProcessingPhaseManager
{
	void Start(const TSharedPtr<FMassEntityManager>& InEntityManager);
	void OnNewArchetype(const FMassArchetypeHandle& NewArchetype);
};

} // namespace UE::Mass::Testing
