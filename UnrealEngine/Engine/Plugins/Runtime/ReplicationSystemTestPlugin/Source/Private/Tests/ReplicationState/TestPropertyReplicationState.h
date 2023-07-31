// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkAutomationTest.h"
#include "UObject/ObjectMacros.h"
#include "TestPropertyReplicationState.generated.h"

UCLASS()
class UTestPropertyReplicationState_TestClass : public UObject
{
	GENERATED_BODY()

public:
	UTestPropertyReplicationState_TestClass() : UObject() {};

	UPROPERTY(Transient, Replicated)
	int IntA;

	UPROPERTY(Transient, Replicated)
	int IntB;

	UPROPERTY(Transient, Replicated)
	int8 IntC;
};

UCLASS()
class UTestPropertyReplicationState_TestClassWithRepNotify : public UObject
{
	GENERATED_BODY()
public:
	UTestPropertyReplicationState_TestClassWithRepNotify() : UObject() {};

	UFUNCTION()
	void OnRep_IntA(int32 OldInt);

	UPROPERTY(Transient, ReplicatedUsing=OnRep_IntA)
	int32 IntA;
};


USTRUCT()
struct FTestPropertyReplicationState_FullyReplicatedStruct
{
	GENERATED_BODY()

	UPROPERTY()
	int32 IntA;

	UPROPERTY()
	int32 IntB;

	bool operator==(const FTestPropertyReplicationState_FullyReplicatedStruct& Other) const
	{
		return (IntA == Other.IntA) & (IntB == Other.IntB);
	}
};

// Set WithIdenticalViaEquality trait on FTestPropertyReplicationState_FullyReplicatedStruct to optimize the equality test 
template<>
struct TStructOpsTypeTraits<FTestPropertyReplicationState_FullyReplicatedStruct> : public TStructOpsTypeTraitsBase2<FTestPropertyReplicationState_FullyReplicatedStruct>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};

USTRUCT()
struct FTestPropertyReplicationState_NotFullyReplicatedStruct
{
	GENERATED_BODY()

	UPROPERTY()
	int32 IntA;

	UPROPERTY(NotReplicated)
	int32 IntB;

	UPROPERTY()
	int32 IntC;
};

USTRUCT()
struct FTestPropertyReplicationState_StructWithArrayOfNotFullyReplicatedStruct
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTestPropertyReplicationState_NotFullyReplicatedStruct> DynamicArrayOfNotFullyReplicatedStruct;
};

UCLASS()
class UTestPropertyReplicationState_TestClassWithInitAndCArrays : public UObject
{
	GENERATED_BODY()

public:
	UTestPropertyReplicationState_TestClassWithInitAndCArrays() : UObject() {}

	UPROPERTY(Replicated)
	FTestPropertyReplicationState_FullyReplicatedStruct InitArrayOfFullyReplicatedStruct[3];

	UPROPERTY(Replicated)
	FTestPropertyReplicationState_NotFullyReplicatedStruct InitArrayOfNotFullyReplicatedStruct[3];

	UPROPERTY(Replicated)
	FTestPropertyReplicationState_FullyReplicatedStruct ArrayOfFullyReplicatedStruct[3];

	UPROPERTY(Replicated)
	FTestPropertyReplicationState_NotFullyReplicatedStruct ArrayOfNotFullyReplicatedStruct[3];

	UPROPERTY(Replicated)
	FTestPropertyReplicationState_StructWithArrayOfNotFullyReplicatedStruct StructWithArrayOfNotFullyReplicatedStruct;
};
