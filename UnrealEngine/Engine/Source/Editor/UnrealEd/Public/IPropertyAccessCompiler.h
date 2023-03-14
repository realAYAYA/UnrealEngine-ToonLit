// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "UObject/ObjectMacros.h"
#include "Features/IModularFeature.h"

class UObject;
struct FPropertyAccessLibrary;

// DEPRECATED - The various batching of property copy
UENUM()
enum class EPropertyAccessBatchType : uint8
{
	// Copies designed to be called one at a time via ProcessCopy
	Unbatched,

	// Copies designed to be processed in one call to ProcessCopies
	Batched,
};

// Handle used to track property accesses that are being compiled
struct FPropertyAccessHandle
{
public:
	// Get the internal index
	int32 GetId() const { return Id; }

	// Check this handle for validity
	bool IsValid() const { return Id != INDEX_NONE; }

	FPropertyAccessHandle() = default;
	
	FPropertyAccessHandle(int32 InId)
		: Id(InId)
	{
	}

	inline bool operator==(const FPropertyAccessHandle& Other) const
	{
		return Id == Other.Id;
	} 

private:
	// Index used to track accesses by external systems
	int32 Id = INDEX_NONE;
};

inline uint32 GetTypeHash(const FPropertyAccessHandle& Value)
{
	return GetTypeHash(Value.GetId());
}

// Handle used to describe property accesses that have been compiled
struct FCompiledPropertyAccessHandle
{
public:
	FCompiledPropertyAccessHandle() = default;
	
	// Get the index into the batch
	int32 GetId() const { return Id; }

	// Get the index of the batch
	int32 GetBatchId() const { return BatchId; }

	// Check this handle for validity
	bool IsValid() const { return Id != INDEX_NONE && BatchId != INDEX_NONE; }

private:
	friend class FPropertyAccessLibraryCompiler;

	FCompiledPropertyAccessHandle(int32 InId, int32 InBatchId)
		: Id(InId)
		, BatchId(InBatchId)
	{
	}

	// Index into the batch
	int32 Id = INDEX_NONE;

	// Index of the batch
	int32 BatchId = INDEX_NONE;
};

// A helper used to compile a property access library
class IPropertyAccessLibraryCompiler
{
public:
	virtual ~IPropertyAccessLibraryCompiler() {}

	// Begin compilation - reset the library to its default state
	virtual void BeginCompilation() = 0;
	
	UE_DEPRECATED(5.0, "Please use BeginCompilation without a class arg")
	virtual void BeginCompilation(const UClass* InClass) {}

	// Add a copy to the property access library we are compiling
	// @return an integer handle to the pending copy. This can be resolved to a true copy index by calling MapCopyIndex
	virtual FPropertyAccessHandle AddCopy(TArrayView<FString> InSourcePath, TArrayView<FString> InDestPath, const FName& InContextId, UObject* InAssociatedObject = nullptr) = 0;
	
	UE_DEPRECATED(5.0, "Please use AddCopy with a context ID that returns a handle")
	virtual int32 AddCopy(TArrayView<FString> InSourcePath, TArrayView<FString> InDestPath, EPropertyAccessBatchType InBatchType, UObject* InAssociatedObject = nullptr)
	{
		FPropertyAccessHandle Handle = AddCopy(InSourcePath, InDestPath, FName(NAME_None), InAssociatedObject); 
		return Handle.GetId();
	}

	// Post-process the library to finish compilation. @return true if compilation succeeded.
	virtual bool FinishCompilation() = 0;

	// Iterate any errors we have with compilation
	virtual void IterateErrors(TFunctionRef<void(const FText&, UObject*)> InFunction) const = 0;

	// Maps the initial copy handle to a true handle, post compilation
	virtual FCompiledPropertyAccessHandle GetCompiledHandle(FPropertyAccessHandle InHandle) const = 0;

	UE_DEPRECATED(5.0, "Please use GetCompiledHandle")
	virtual int32 MapCopyIndex(int32 InIndex) const
	{
		FCompiledPropertyAccessHandle CompiledHandle = GetCompiledHandle(FPropertyAccessHandle(InIndex));
		return CompiledHandle.GetId();
	}
};