// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "AITestsCommon.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "Misc/MTAccessDetector.h"
#include "MassExternalSubsystemTraits.h"
#include "MassEntityTestTypes.generated.h"


class UWorld;
struct FMassEntityManager;

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
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override 
	{
		ExecutionFunction(EntityManager, Context);
	}
	virtual void ConfigureQueries() override
	{
		RequirementsFunction(EntityQuery);
	}

	TFunction<void(FMassEntityManager& EntityManager, FMassExecutionContext& Context)> ExecutionFunction;
	TFunction<void(FMassEntityQuery& Query)> RequirementsFunction;

	FMassEntityQuery& TestGetQuery() { return EntityQuery; }

protected:
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

struct MASSENTITYTESTSUITE_API FExecutionTestBase : FAITestBase
{
	TSharedPtr<FMassEntityManager> EntityManager;
	UWorld* World = nullptr;

	virtual bool SetUp() override;
};

const int TestIntValue = 123456;

struct MASSENTITYTESTSUITE_API FEntityTestBase : FExecutionTestBase
{
	FMassArchetypeHandle EmptyArchetype;
	FMassArchetypeHandle FloatsArchetype;
	FMassArchetypeHandle IntsArchetype;
	FMassArchetypeHandle FloatsIntsArchetype;

	FInstancedStruct InstanceInt;

	virtual bool SetUp() override;
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
