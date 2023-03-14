// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Bindings/MVVMCompiledBindingLibrary.h"
#include "MVVMSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Templates/TypeHash.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMFieldVariant.h"


namespace UE::MVVM::Private
{
	class FCompiledBindingLibraryCompilerImpl;
} //namespace UE::MVVM::Private


namespace UE::MVVM
{

/**  */
class MODELVIEWVIEWMODELBLUEPRINT_API FCompiledBindingLibraryCompiler
{
public:
	/** */
	struct FFieldPathHandle
	{
	public:
		explicit FFieldPathHandle()
			: Id(0)
		{
		}

		static FFieldPathHandle MakeHandle()
		{
			FFieldPathHandle Handle;
			++IdGenerator;
			Handle.Id = IdGenerator;
			return Handle;
		}

		bool IsValid() const
		{
			return Id != 0;
		}

		bool operator==(const FFieldPathHandle& Other) const
		{
			return Id == Other.Id;
		}

		bool operator!=(const FFieldPathHandle& Other) const
		{
			return Id != Other.Id;
		}

		friend uint32 GetTypeHash(const FFieldPathHandle& Handle)
		{
			return ::GetTypeHash(Handle.Id);
		}

	private:
		static int32 IdGenerator;
		int32 Id;
	};


	/** */
	struct FBindingHandle
	{
	public:
		explicit FBindingHandle()
			: Id(0)
		{
		}

		static FBindingHandle MakeHandle()
		{
			FBindingHandle Handle;
			++IdGenerator;
			Handle.Id = IdGenerator;
			return Handle;
		}

		bool IsValid() const
		{
			return Id != 0;
		}

		bool operator==(const FBindingHandle& Other) const
		{
			return Id == Other.Id;
		}

		bool operator!=(const FBindingHandle& Other) const
		{
			return Id != Other.Id;
		}

		friend uint32 GetTypeHash(const FBindingHandle& Handle)
		{
			return ::GetTypeHash(Handle.Id);
		}

	private:
		static int32 IdGenerator;
		int32 Id;
	};

	/** */
	struct FFieldIdHandle
	{
	public:
		explicit FFieldIdHandle()
			: Id(0)
		{
		}

		static FFieldIdHandle MakeHandle()
		{
			FFieldIdHandle Handle;
			++IdGenerator;
			Handle.Id = IdGenerator;
			return Handle;
		}

		bool IsValid() const
		{
			return Id != 0;
		}

		bool operator==(const FFieldIdHandle& Other) const
		{
			return Id == Other.Id;
		}

		bool operator!=(const FFieldIdHandle& Other) const
		{
			return Id != Other.Id;
		}

		friend uint32 GetTypeHash(const FFieldIdHandle& Handle)
		{
			return ::GetTypeHash(Handle.Id);
		}

	private:
		static int32 IdGenerator;
		int32 Id;
	};

public:
	FCompiledBindingLibraryCompiler();

public:
	/** */
	TValueOrError<FFieldIdHandle, FText> AddFieldId(TSubclassOf<UObject> SourceClass, FName FieldName);
	
	/** */
	TValueOrError<FFieldPathHandle, FText> AddFieldPath(TArrayView<const UE::MVVM::FMVVMConstFieldVariant> FieldPath, bool bRead);

	/** */
	TValueOrError<FFieldPathHandle, FText> AddObjectFieldPath(TArrayView<const UE::MVVM::FMVVMConstFieldVariant> FieldPath, UClass* ExpectedType, bool bRead);

	/** */
	TValueOrError<FFieldPathHandle, FText> AddConversionFunctionFieldPath(TSubclassOf<UObject> SourceClass, const UFunction* Function);

	/** */
	TValueOrError<FBindingHandle, FText> AddBinding(FFieldPathHandle Source, FFieldPathHandle Destination);

	/** */
	TValueOrError<FBindingHandle, FText> AddBinding(FFieldPathHandle Source, FFieldPathHandle Destination, FFieldPathHandle ConversionFunction);

	/** */
	TValueOrError<FBindingHandle, FText> AddBinding(TArrayView<const FFieldPathHandle> Sources, FFieldPathHandle Destination, FFieldPathHandle ConversionFunction);

	struct FCompileResult
	{
		FMVVMCompiledBindingLibrary Library;
		TMap<FFieldPathHandle, FMVVMVCompiledFieldPath> FieldPaths;
		TMap<FBindingHandle, FMVVMVCompiledBinding> Bindings;
		TMap<FFieldIdHandle, FMVVMVCompiledFieldId> FieldIds;
	};

	/** */
	TValueOrError<FCompileResult, FText> Compile();

private:
	/** */
	TValueOrError<FFieldPathHandle, FText> AddFieldPathImpl(TArrayView<const UE::MVVM::FMVVMConstFieldVariant> FieldPath, bool bRead);

	TPimplPtr<Private::FCompiledBindingLibraryCompilerImpl> Impl;
};

} //namespace UE::MVVM