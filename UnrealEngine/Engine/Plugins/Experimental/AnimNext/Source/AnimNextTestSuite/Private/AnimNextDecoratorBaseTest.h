// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/DecoratorSharedData.h"

#include "AnimNextDecoratorBaseTest.generated.h"

USTRUCT()
struct FDecoratorA_BaseSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Inline))
	uint32 DecoratorUID;

	FDecoratorA_BaseSharedData();
};

USTRUCT()
struct FDecoratorAB_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Inline))
	uint32 DecoratorUID;

	FDecoratorAB_AddSharedData();
};

USTRUCT()
struct FDecoratorAC_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Inline))
	uint32 DecoratorUID;

	FDecoratorAC_AddSharedData();
};

USTRUCT()
struct FDecoratorSerialization_BaseSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Inline))
	int32 Integer = 0;

	UPROPERTY(meta = (Inline))
	int32 IntegerArray[4] = { 0, 0, 0, 0 };

	UPROPERTY(meta = (Inline))
	TArray<int32> IntegerTArray;

	UPROPERTY(meta = (Inline))
	FVector Vector = FVector::ZeroVector;

	UPROPERTY(meta = (Inline))
	FVector VectorArray[2] = { FVector::ZeroVector, FVector::ZeroVector };

	UPROPERTY(meta = (Inline))
	TArray<FVector> VectorTArray;

	UPROPERTY(meta = (Inline))
	FString String;

	UPROPERTY(meta = (Inline))
	FName Name;
};

USTRUCT()
struct FDecoratorSerialization_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Inline))
	int32 Integer = 0;

	UPROPERTY(meta = (Inline))
	int32 IntegerArray[4] = { 0, 0, 0, 0 };

	UPROPERTY(meta = (Inline))
	TArray<int32> IntegerTArray;

	UPROPERTY(meta = (Inline))
	FVector Vector = FVector::ZeroVector;

	UPROPERTY(meta = (Inline))
	FVector VectorArray[2] = { FVector::ZeroVector, FVector::ZeroVector };

	UPROPERTY(meta = (Inline))
	TArray<FVector> VectorTArray;

	UPROPERTY(meta = (Inline))
	FString String;

	UPROPERTY(meta = (Inline))
	FName Name;
};

USTRUCT()
struct FDecoratorNativeSerialization_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Inline))
	int32 Integer = 0;

	UPROPERTY(meta = (Inline))
	int32 IntegerArray[4] = { 0, 0, 0, 0 };

	UPROPERTY(meta = (Inline))
	TArray<int32> IntegerTArray;

	UPROPERTY(meta = (Inline))
	FVector Vector = FVector::ZeroVector;

	UPROPERTY(meta = (Inline))
	FVector VectorArray[2] = { FVector::ZeroVector, FVector::ZeroVector };

	UPROPERTY(meta = (Inline))
	TArray<FVector> VectorTArray;

	UPROPERTY(meta = (Inline))
	FString String;

	UPROPERTY(meta = (Inline))
	FName Name;

	bool bSerializeCalled = false;

	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FDecoratorNativeSerialization_AddSharedData> : public TStructOpsTypeTraitsBase2<FDecoratorNativeSerialization_AddSharedData>
{
	enum
	{
		WithSerializer = true,
	};
};
