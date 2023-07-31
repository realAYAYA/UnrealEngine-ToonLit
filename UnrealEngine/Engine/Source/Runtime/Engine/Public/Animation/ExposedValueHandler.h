// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

#include "ExposedValueHandler.generated.h"

struct FPropertyAccessLibrary;
class UClass;
struct FAnimationBaseContext;

UENUM()
enum class EPostCopyOperation : uint8
{
	None,

	LogicalNegateBool,
};

USTRUCT()
struct FExposedValueCopyRecord
{
	GENERATED_BODY()

	FExposedValueCopyRecord() = default;

	FExposedValueCopyRecord(int32 InCopyIndex, EPostCopyOperation InPostCopyOperation)
		: CopyIndex(InCopyIndex)
		, PostCopyOperation(InPostCopyOperation)
	{
	}

	UPROPERTY()
	int32 CopyIndex = INDEX_NONE;

	UPROPERTY()
	EPostCopyOperation PostCopyOperation = EPostCopyOperation::None;
};

// An exposed value updater
USTRUCT()
struct ENGINE_API FExposedValueHandler
{
	GENERATED_USTRUCT_BODY()

	FExposedValueHandler()
		: Function(nullptr)
		, PropertyAccessLibrary(nullptr)
		, BoundFunction(NAME_None)
	{
	}

	// Direct data access to property in anim instance
	UPROPERTY()
	TArray<FExposedValueCopyRecord> CopyRecords;

	// function pointer if BoundFunction != NAME_None
	UPROPERTY()
	TObjectPtr<UFunction> Function;

	// Cached property access library ptr
	const FPropertyAccessLibrary* PropertyAccessLibrary;

	// The function to call to update associated properties (can be NAME_None)
	UPROPERTY()
	FName BoundFunction;

	// Helper function to bind an array of handlers.
	static void ClassInitialization(TArray<FExposedValueHandler>& Handlers, UClass* InClass);

	// Bind copy records and cache UFunction if necessary
	void Initialize(UClass* InClass, const FPropertyAccessLibrary& InPropertyAccessLibrary);

	// Execute the function and copy records
	void Execute(const FAnimationBaseContext& Context) const;
};