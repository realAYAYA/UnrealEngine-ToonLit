// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Bindings/MVVMCompiledBindingLibrary.h"

namespace UE::MVVM { struct FMVVMConstFieldVariant; }
template <typename T> class TSubclassOf;


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
	FCompiledBindingLibraryCompiler(UBlueprint* Context);

public:
	/** */
	TValueOrError<FFieldIdHandle, FText> AddFieldId(const UClass* SourceClass, FName FieldName);
	
	/** */
	TValueOrError<FFieldPathHandle, FText> AddFieldPath(TArrayView<const UE::MVVM::FMVVMConstFieldVariant> FieldPath, bool bRead);

	/** */
	TValueOrError<FFieldPathHandle, FText> AddObjectFieldPath(TArrayView<const UE::MVVM::FMVVMConstFieldVariant> FieldPath, const UClass* ExpectedType, bool bRead);

	/** */
	TValueOrError<FFieldPathHandle, FText> AddConversionFunctionFieldPath(const UClass* SourceClass, const UFunction* Function);

	/** */
	TValueOrError<FBindingHandle, FText> AddBinding(FFieldPathHandle Source, FFieldPathHandle Destination);

	/** */
	TValueOrError<FBindingHandle, FText> AddBinding(FFieldPathHandle Source, FFieldPathHandle Destination, FFieldPathHandle ConversionFunction);

	/** */
	TValueOrError<FBindingHandle, FText> AddComplexBinding(FFieldPathHandle Destination, FFieldPathHandle ConversionFunction);

	struct FCompileResult
	{
		FCompileResult(FGuid LibraryId);
		FMVVMCompiledBindingLibrary Library;
		TMap<FFieldPathHandle, FMVVMVCompiledFieldPath> FieldPaths;
		TMap<FBindingHandle, FMVVMVCompiledBinding> Bindings;
		TMap<FFieldIdHandle, UE::FieldNotification::FFieldId> FieldIds;
	};

	/** */
	TValueOrError<FCompileResult, FText> Compile(FGuid LibraryId);

private:
	/** */
	TValueOrError<FFieldPathHandle, FText> AddFieldPathImpl(TArrayView<const UE::MVVM::FMVVMConstFieldVariant> FieldPath, bool bRead);
	TValueOrError<FBindingHandle, FText> AddBindingImpl(FFieldPathHandle Source, FFieldPathHandle Destination, FFieldPathHandle ConversionFunction, bool bIsComplexBinding);

	TPimplPtr<Private::FCompiledBindingLibraryCompilerImpl> Impl;
};

} //namespace UE::MVVM

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "MVVMSubsystem.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMFieldVariant.h"
#endif
