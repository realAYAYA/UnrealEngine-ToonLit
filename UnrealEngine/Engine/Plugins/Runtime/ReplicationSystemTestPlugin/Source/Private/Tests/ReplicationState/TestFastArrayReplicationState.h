// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkAutomationTest.h"
#include "UObject/ObjectMacros.h"
#include "Tests/ReplicationSystem/ReplicatedTestObject.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "TestFastArrayReplicationState.generated.h"

USTRUCT()
struct FTestFastArrayReplicationState_FastArrayItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	// Bool
	UPROPERTY()
	bool  bRepBool;

	UPROPERTY(NotReplicated)
	int32 NotRepInt32;

	// Integers
	UPROPERTY()
	int32 RepInt32;

	UPROPERTY()
	TObjectPtr<UObject> ObjectRef = nullptr;

	// Callbacks
	void PostReplicatedAdd(const struct FTestFastArrayReplicationState_FastArraySerializer& InArraySerializer);
	void PostReplicatedChange(const struct FTestFastArrayReplicationState_FastArraySerializer& InArraySerializer);
	void PreReplicatedRemove(const struct FTestFastArrayReplicationState_FastArraySerializer& InArraySerializer);
};

USTRUCT()
struct FTestFastArrayReplicationState_FastArraySerializer : public FIrisFastArraySerializer
{
	GENERATED_BODY()

	FTestFastArrayReplicationState_FastArraySerializer()
		: FIrisFastArraySerializer()
		, bHitReplicatedAdd(false)
		, bHitReplicatedChange(false)
		, bHitReplicatedRemove(false)
		, bHitPostReplicatedReceive(false)
		, bPostReplicatedReceiveWasHitWithUnresolvedReferences(false)
	{
	}

	// Test of TIrisFastArrayEditor interface
	// This is just to see if it works out as expected
	typedef TArray<FTestFastArrayReplicationState_FastArrayItem> ItemArrayType;
	const ItemArrayType& GetItemArray() const { return Items; }
	ItemArrayType& GetItemArray() { return Items; }

	typedef UE::Net::TIrisFastArrayEditor<FTestFastArrayReplicationState_FastArraySerializer> FFastArrayEditor;
	FFastArrayEditor Edit() { return FFastArrayEditor(*this); }	

	uint8 bHitReplicatedAdd    : 1;
	uint8 bHitReplicatedChange : 1;
	uint8 bHitReplicatedRemove : 1;
	uint8 bHitPostReplicatedReceive : 1;

	bool bPostReplicatedReceiveWasHitWithUnresolvedReferences;
	
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
	{
		bHitPostReplicatedReceive = 1U;
		bPostReplicatedReceiveWasHitWithUnresolvedReferences = Parameters.bHasMoreUnmappedReferences;
	}

protected:

	UPROPERTY()
	TArray<FTestFastArrayReplicationState_FastArrayItem> Items;
};


USTRUCT()
struct FTestFastArrayReplicationState_FastArray : public FTestFastArrayReplicationState_FastArraySerializer
{
	GENERATED_BODY()

	FTestFastArrayReplicationState_FastArray() : FTestFastArrayReplicationState_FastArraySerializer()
	{
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FTestFastArrayReplicationState_FastArrayItem, FTestFastArrayReplicationState_FastArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits< FTestFastArrayReplicationState_FastArray > : public TStructOpsTypeTraitsBase2< FTestFastArrayReplicationState_FastArray >
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

USTRUCT()
struct FTestFastArrayReplicationState_FastArrayWithExtraProperty : public FTestFastArrayReplicationState_FastArraySerializer
{
	GENERATED_BODY()

	FTestFastArrayReplicationState_FastArrayWithExtraProperty()
		: FTestFastArrayReplicationState_FastArraySerializer()
	{
	}

	UPROPERTY()
	int32 ExtraInt;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FTestFastArrayReplicationState_FastArrayItem, FTestFastArrayReplicationState_FastArrayWithExtraProperty>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits< FTestFastArrayReplicationState_FastArrayWithExtraProperty > : public TStructOpsTypeTraitsBase2< FTestFastArrayReplicationState_FastArrayWithExtraProperty >
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

UCLASS()
class UTestFastArrayReplicationState_FastArray_TestClassFastArray : public UReplicatedTestObject
{
	GENERATED_BODY()
public:
	UTestFastArrayReplicationState_FastArray_TestClassFastArray();

	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Fragments, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;

	UPROPERTY(Replicated)
	FTestFastArrayReplicationState_FastArray FastArray;
};

UCLASS()
class UTestFastArrayReplicationState_FastArray_TestClassFastArrayWithExtraProperty : public UReplicatedTestObject
{
	GENERATED_BODY()
public:
	UTestFastArrayReplicationState_FastArray_TestClassFastArrayWithExtraProperty();

	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Fragments, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;

	UPROPERTY(Replicated)
	FTestFastArrayReplicationState_FastArrayWithExtraProperty FastArray;
};




