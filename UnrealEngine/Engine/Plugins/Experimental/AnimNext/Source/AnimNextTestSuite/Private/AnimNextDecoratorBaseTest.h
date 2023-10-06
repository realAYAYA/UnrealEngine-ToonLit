// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/DecoratorSharedData.h"

#include "AnimNextDecoratorBaseTest.generated.h"

USTRUCT()
struct FDecoratorA_BaseSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 DecoratorUID;

	FDecoratorA_BaseSharedData();
};

USTRUCT()
struct FDecoratorAB_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 DecoratorUID;

	FDecoratorAB_AddSharedData();
};

USTRUCT()
struct FDecoratorAC_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 DecoratorUID;

	FDecoratorAC_AddSharedData();
};

USTRUCT()
struct FDecoratorSerialization_BaseSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Integer = 0;

	UPROPERTY()
	int32 IntegerArray[4] = { 0, 0, 0, 0 };

	UPROPERTY()
	TArray<int32> IntegerTArray;

	UPROPERTY()
	FVector Vector = FVector::ZeroVector;

	UPROPERTY()
	FVector VectorArray[2] = { FVector::ZeroVector, FVector::ZeroVector };

	UPROPERTY()
	TArray<FVector> VectorTArray;

	UPROPERTY()
	FString String;

	UPROPERTY()
	FName Name;
};

USTRUCT()
struct FDecoratorSerialization_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Integer = 0;

	UPROPERTY()
	int32 IntegerArray[4] = { 0, 0, 0, 0 };

	UPROPERTY()
	TArray<int32> IntegerTArray;

	UPROPERTY()
	FVector Vector = FVector::ZeroVector;

	UPROPERTY()
	FVector VectorArray[2] = { FVector::ZeroVector, FVector::ZeroVector };

	UPROPERTY()
	TArray<FVector> VectorTArray;

	UPROPERTY()
	FString String;

	UPROPERTY()
	FName Name;
};

USTRUCT()
struct FDecoratorNativeSerialization_AddSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Integer = 0;

	UPROPERTY()
	int32 IntegerArray[4] = { 0, 0, 0, 0 };

	UPROPERTY()
	TArray<int32> IntegerTArray;

	UPROPERTY()
	FVector Vector = FVector::ZeroVector;

	UPROPERTY()
	FVector VectorArray[2] = { FVector::ZeroVector, FVector::ZeroVector };

	UPROPERTY()
	TArray<FVector> VectorTArray;

	UPROPERTY()
	FString String;

	UPROPERTY()
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
