// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FieldNotification/FieldId.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMFieldContext.h"
#include "Types/MVVMFunctionContext.h"
#include "Types/MVVMFieldVariant.h"
#include "Types/MVVMObjectVariant.h"
#include "UObject/FieldPath.h"
#include "UObject/Object.h"

#include "MVVMCompiledBindingLibrary.generated.h"

struct FMVVMCompiledBindingLibrary;
namespace UE::MVVM
{
	class FCompiledBindingLibraryCompiler;
}


/** The info to fetch a list of FProperty or UFunction from a Class that will be needed by bindings. */
USTRUCT()
struct MODELVIEWVIEWMODEL_API FMVVMVCompiledFields
{
	GENERATED_BODY()
	friend UE::MVVM::FCompiledBindingLibraryCompiler;

public:
	int32 GetPropertyNum() const
	{
		return ClassOrScriptStruct ? NumberOfProperties : 0;
	}

	int32 GetFunctionNum() const
	{
		return ClassOrScriptStruct ? NumberOfFunctions : 0;
	}

	int32 GetFieldIdNum() const
	{
		return ClassOrScriptStruct ? NumberOfFieldIds : 0;
	}

	FName GetPropertyName(TArrayView<FName> Names, int32 Index) const
	{
		check(Index < NumberOfProperties && Index >= 0);
		check(Names.IsValidIndex(LibraryStartIndex + Index));
		return Names[LibraryStartIndex + Index];
	}

	FName GetFunctionName(TArrayView<FName> Names, int32 Index) const
	{
		check(Index < NumberOfFunctions && Index >= 0);
		check(Names.IsValidIndex(LibraryStartIndex + NumberOfProperties + Index));
		return Names[LibraryStartIndex + NumberOfProperties + Index];
	}

	FName GetFieldIdName(TArrayView<FName> Names, int32 Index) const
	{
		check(Index < NumberOfFieldIds && Index >= 0);
		check(Names.IsValidIndex(LibraryStartIndex + NumberOfProperties + NumberOfFunctions + Index));
		return Names[LibraryStartIndex + NumberOfProperties + NumberOfFunctions + Index];
	}

	const UStruct* GetStruct() const
	{
		return ClassOrScriptStruct;
	}

	/** Find the FProperty from the  Class.*/
	FProperty* GetProperty(FName PropertyName) const;

	/** Find the UFunction from the Class.*/
	UFunction* GetFunction(FName FunctionName) const;

	/** Find the FFieldId from the Class.*/
	UE::FieldNotification::FFieldId GetFieldId(FName FieldName) const;

private:
	UPROPERTY()
	TObjectPtr<const UStruct> ClassOrScriptStruct = nullptr;

	UPROPERTY()
	int16 LibraryStartIndex = 0;

	UPROPERTY()
	int16 NumberOfProperties = 0;

	UPROPERTY()
	int16 NumberOfFunctions = 0;
	
	UPROPERTY()
	int16 NumberOfFieldIds = 0;
};


/** Contains the info to evaluate a path for a specific library. */
USTRUCT()
struct FMVVMVCompiledFieldPath
{
	GENERATED_BODY()

	friend FMVVMCompiledBindingLibrary;
	friend UE::MVVM::FCompiledBindingLibraryCompiler;

	bool IsValid() const
	{
		return Num >= 0;
	}

private:
	using IndexType = int16;

	UPROPERTY()
	int16 StartIndex = INDEX_NONE;

	UPROPERTY()
	int16 Num = INDEX_NONE;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid CompiledBindingLibraryId;
#endif
};


/** Contains the FieldId index. */
USTRUCT()
struct FMVVMVCompiledFieldId
{
	GENERATED_BODY()

	friend FMVVMCompiledBindingLibrary;
	friend UE::MVVM::FCompiledBindingLibraryCompiler;

	bool IsValid() const
	{
		return FieldIdIndex >= 0;
	}

private:
	using IndexType = int16;

	UPROPERTY()
	int16 FieldIdIndex = INDEX_NONE;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid CompiledBindingLibraryId;
#endif
};


/** */
USTRUCT()
struct FMVVMCompiledLoadedPropertyOrFunctionIndex
{
	GENERATED_BODY()

	friend FMVVMCompiledBindingLibrary;
	friend UE::MVVM::FCompiledBindingLibraryCompiler;

	FMVVMCompiledLoadedPropertyOrFunctionIndex()
	{
		bIsObjectProperty = false;
		bIsScriptStructProperty = false;
		bIsProperty = false;
	}

private:
	UPROPERTY()
	int16 Index = INDEX_NONE;

	/** Is the property or the return property of the UFunction is a FObjectPropertyBase */
	UPROPERTY()
	uint8 bIsObjectProperty : 1;

	/** Is the property or the return property of the UFunction is a FStructProperty */
	UPROPERTY()
	uint8 bIsScriptStructProperty : 1;

	/** Is the index in LoadedProperties or the index is in LoadedFunctions. */
	UPROPERTY()
	uint8 bIsProperty : 1;
};


/** Contains a single combination to execute a binding for a specific library. */
USTRUCT()
struct FMVVMVCompiledBinding
{
	GENERATED_BODY()

	friend FMVVMCompiledBindingLibrary;
	friend UE::MVVM::FCompiledBindingLibraryCompiler;

public:
	bool IsValid() const
	{
		return SourceFieldPath.IsValid() && DestinationFieldPath.IsValid();
	}

	FMVVMVCompiledFieldPath GetSourceFieldPath() const
	{
		return SourceFieldPath;
	}

	FMVVMVCompiledFieldPath GetDestinationFieldPath() const
	{
		return DestinationFieldPath;
	}

	FMVVMVCompiledFieldPath GetConversionFunctionFieldPath() const
	{
		return ConversionFunctionFieldPath;
	}

private:
	using IndexType = int16;

	UPROPERTY()
	FMVVMVCompiledFieldPath SourceFieldPath; // todo: make this an array to support multiple input to conversion functions

	UPROPERTY()
	FMVVMVCompiledFieldPath DestinationFieldPath;

	UPROPERTY()
	FMVVMVCompiledFieldPath ConversionFunctionFieldPath;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid CompiledBindingLibraryId;
#endif
};


/** Library of all the compiled bindings. */
USTRUCT()
struct MODELVIEWVIEWMODEL_API FMVVMCompiledBindingLibrary
{
	GENERATED_BODY()
	friend UE::MVVM::FCompiledBindingLibraryCompiler;

public:
	FMVVMCompiledBindingLibrary();

public:
	/** */
	enum class EExecutionFailingReason
	{
		IncompatibleLibrary,
		InvalidSource,
		InvalidDestination,
		InvalidConversionFunction,
		InvalidCast,
	};

	/** */
	enum class EConversionFunctionType
	{
		Simple, 	// accept an single argument and returns a single property
		Complex,	// returns a single property. The argument are fetch from inside the function 
	};

	/** Fetch the FProperty and UFunction. */
	void Load();

	/**
	 * Execute a binding, in one direction.
	 * The ExecutionSource is the View (UserWidget) instance.
	 * The Source of the binding (ViewModel, UserWidget, Widget, ...) can be provided if already known.
	 * The Destination or the binding (ViewModel, UserWIdget, Widget, ...) can be provided if already known.
	 */
	TValueOrError<void, EExecutionFailingReason> Execute(UObject* ExecutionSource, const FMVVMVCompiledBinding& Binding, EConversionFunctionType ConversionType) const;
	TValueOrError<void, EExecutionFailingReason> ExecuteWithSource(UObject* ExecutionSource, const FMVVMVCompiledBinding& Binding, UObject* Source) const;

	/**
	 * Evaluate the path to find the container use by a binding.
	 * ExecutionSource is the View (UserWidget) instance.
	 * @returns the container and the last segment of the path. The last segment can be a property or a function.
	 */
	TValueOrError<UE::MVVM::FFieldContext, void> EvaluateFieldPath(UObject* ExecutionSource, const FMVVMVCompiledFieldPath& FieldPath) const;

	/** Get the loaded FieldId. */
	TValueOrError<UE::FieldNotification::FFieldId, void> GetFieldId(const FMVVMVCompiledFieldId& FieldId) const;

	/** Return a readable version of the FieldPath. */
	TValueOrError<FString, FString> FieldPathToString(const FMVVMVCompiledFieldPath& FieldPath) const;

private:
	TValueOrError<void, EExecutionFailingReason> ExecuteImpl(UObject* ExecutionSource, const FMVVMVCompiledBinding& Binding, UObject* Source, EConversionFunctionType ConversionType) const;
	TValueOrError<void, EExecutionFailingReason> ExecuteImpl(UE::MVVM::FFieldContext& Source, UE::MVVM::FFieldContext& Destination, UE::MVVM::FFunctionContext& ConversionFunction, EConversionFunctionType ConversionType) const;

	TValueOrError<UE::MVVM::FMVVMFieldVariant, void> GetFinalFieldFromPathImpl(const FMVVMVCompiledFieldPath& FieldPath) const;

private:
	TArray<FProperty*> LoadedProperties;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFunction>> LoadedFunctions;

	TArray<UE::FieldNotification::FFieldId> LoadedFieldIds;

	UPROPERTY()
	TArray<FMVVMCompiledLoadedPropertyOrFunctionIndex> FieldPaths;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid CompiledBindingLibraryId;
#endif

	/** Only needed for loading the FProperty/UFunction. */
	UPROPERTY()
	TArray<FMVVMVCompiledFields> CompiledFields;

	/** Only needed for loading the FProperty/UFunction. */
	UPROPERTY()
	TArray<FName> CompiledFieldNames;
};
