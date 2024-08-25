// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "RigVMCore/RigVMFunction.h"
#include "RigVMCore/RigVMStructUpgradeInfo.h"
#include "RigVMCore/RigVMTemplate.h"
#include "RigVMCore/RigVMTypeIndex.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StructOnScope.h"

#include "RigVMDispatchFactory.generated.h"

struct FRigVMDispatchFactory;
class URigVMPin;

/**
 * A context used for inquiring from dispatch factories
 */
struct RIGVM_API FRigVMDispatchContext
{
	FRigVMDispatchContext()
		: Instance()
		, Subject(nullptr)
		, StringRepresentation()
	{}
	
	TSharedPtr<FStructOnScope> Instance;
	const UObject* Subject;
	FString StringRepresentation;
};

/**
 * A factory to generate a template and its dispatch functions
 */
USTRUCT()
struct FRigVMDispatchFactory
{
	GENERATED_BODY()

public:

	FRigVMDispatchFactory()
		: FactoryScriptStruct(nullptr)
		, CachedTemplate(nullptr)
	{
		ArgumentNamesMutex = new FCriticalSection();
	}
	
	virtual ~FRigVMDispatchFactory()
	{
		delete ArgumentNamesMutex;
	}

	// returns the name of the factory template
	RIGVM_API FName GetFactoryName() const;

	// returns the struct for this dispatch factory
	UScriptStruct* GetScriptStruct() const { return FactoryScriptStruct; };

#if WITH_EDITOR
	
	// returns the title of the node for a given type set.
	virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const { return GetScriptStruct()->GetDisplayNameText().ToString(); }

	// returns the color of the node for a given type set.
	RIGVM_API virtual FLinearColor GetNodeColor() const;

	// returns the tooltip for the node
	RIGVM_API virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const;

	// returns the default value for an argument
	RIGVM_API virtual FString GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const;

	// returns the tooltip for an argument
	virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const { return FText(); }

	// returns the names of the input aggregate arguments
	virtual TArray<FName> GetAggregateInputArguments() const { return TArray<FName>(); }

	// returns the names of the output aggregate arguments
	virtual TArray<FName> GetAggregateOutputArguments() const { return TArray<FName>(); }

	// returns the next name to be used for an aggregate pin
	RIGVM_API virtual FName GetNextAggregateName(const FName& InLastAggregatePinName) const;

	// Returns the display name text for an argument 
	RIGVM_API virtual FName GetDisplayNameForArgument(const FName& InArgumentName) const;

	// Returns meta data on the property of the permutations 
	RIGVM_API virtual FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const;

	// Returns true if the factory provides metadata for a given argument
	bool HasArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
	{
		return !GetArgumentMetaData(InArgumentName, InMetaDataKey).IsEmpty();
	}

	// Returns the category this factory is under
	RIGVM_API virtual FString GetCategory() const;

	// Returns the keywords used for looking up this factory
	RIGVM_API virtual FString GetKeywords() const;

	// Returns true if the argument is lazy
	RIGVM_API bool IsLazyInputArgument(const FName& InArgumentName) const;

#endif

	// Returns the name to use for the branch info / argument based on the operand index.
	// operand index count may be different than the number of arguments, since fixed size array
	// arguments get unrolled.
	RIGVM_API virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const;

	// returns true if the dispatch is a control flow dispatch
	bool IsControlFlowDispatch(const FRigVMDispatchContext& InContext) const { return !GetControlFlowBlocks(InContext).IsEmpty(); }

	// returns the control flow blocks of this dispatch
	RIGVM_API const TArray<FName>& GetControlFlowBlocks(const FRigVMDispatchContext& InContext) const;

	// returns true if a given control flow block needs to be sliced
	virtual const bool IsControlFlowBlockSliced(const FName& InBlockName) const { return false; }

	// Returns the execute context support for this dispatch factory
	virtual UScriptStruct* GetExecuteContextStruct() const { return FRigVMExecuteContext::StaticStruct(); }

	// Returns truf if this factory supports a given executecontext struct
	RIGVM_API bool SupportsExecuteContextStruct(const UScriptStruct* InExecuteContextStruct) const;

	// registered needed types during registration of the factory
	virtual void RegisterDependencyTypes() const {}

	// returns the arguments of the template
	RIGVM_API virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const;

	// returns the execute arguments of the template
	RIGVM_API TArray<FRigVMExecuteArgument> GetExecuteArguments(const FRigVMDispatchContext& InContext) const;

	// this function is deprecated, please use GetPermutationsFromArgumentType
	// returns the new permutation argument types after a new type is defined for one argument
	// this happens if types are being loaded later after this factory has already been deployed (like UUserDefinedStruct)
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const { return FRigVMTemplateTypeMap(); }

	// returns the new permutations argument types after a new type is defined for one argument
	virtual bool GetPermutationsFromArgumentType(const FName& InArgumentName, const TRigVMTypeIndex& InTypeIndex, TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>>& OutPermutations) const
	{
		FRigVMTemplateTypeMap Permutation = OnNewArgumentType(InArgumentName, InTypeIndex);
		if (!Permutation.IsEmpty())
		{
			OutPermutations.Add(Permutation);
			return true;
		}
		return false;
	}

	// returns the upgrade info to use for this factory
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo(const FRigVMTemplateTypeMap& InTypes, const FRigVMDispatchContext& InContext) const { return FRigVMStructUpgradeInfo(); }

	// returns the dispatch function for a given type set
	RIGVM_API FRigVMFunctionPtr GetOrCreateDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const;

	// builds and returns the template
	RIGVM_API const FRigVMTemplate* GetTemplate() const;

	// returns the name of the factory template
	RIGVM_API FName GetTemplateNotation() const;

	// returns the name of the permutation for a given set of types
	RIGVM_API FString GetPermutationName(const FRigVMTemplateTypeMap& InTypes) const;

	// returns true if the dispatch uses the same function ptr for all permutations
	virtual bool IsSingleton() const { return false; } 

protected:

	// for each type defined in the primary argument, this function will call GetPermutationsFromArgumentType to construct an array of arguments with the appropiate permutations
	RIGVM_API TArray<FRigVMTemplateArgumentInfo> BuildArgumentListFromPrimaryArgument(const TArray<FRigVMTemplateArgumentInfo>& InInfos, const FName& InPrimaryArgumentName) const;

	// returns the name of the permutation for a given set of types
	RIGVM_API FString GetPermutationNameImpl(const FRigVMTemplateTypeMap& InTypes) const;

	RIGVM_API FRigVMFunctionPtr CreateDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const;
	FRigVMFunctionPtr CreateDispatchFunction_NoLock(const FRigVMTemplateTypeMap& InTypes) const;

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const { return nullptr; }
	
	RIGVM_API TArray<FRigVMFunction> CreateDispatchPredicates(const FRigVMTemplateTypeMap& InTypes) const;
	TArray<FRigVMFunction> CreateDispatchPredicates_NoLock(const FRigVMTemplateTypeMap& InTypes) const;
	
	virtual TArray<FRigVMFunction> GetDispatchPredicatesImpl(const FRigVMTemplateTypeMap& InTypes) const { return TArray<FRigVMFunction>(); }

	RIGVM_API virtual const TArray<FRigVMExecuteArgument>& GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const;

	RIGVM_API static bool CopyProperty(
		const FProperty* InTargetProperty,
		uint8* InTargetPtr,
		const FProperty* InSourceProperty,
		const uint8* InSourcePtr);

	template <
	typename T,
	typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	static FString GetDefaultValueForStruct(const T& InValue)
	{
		static FString ValueString;
		if(ValueString.IsEmpty())
		{
			TBaseStructure<T>::Get()->ExportText(ValueString, &InValue, &InValue, nullptr, PPF_None, nullptr);
		}
		return ValueString;
	}

	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	static FString GetDefaultValueForStruct(const T& InValue)
	{
		static FString ValueString;
		if(ValueString.IsEmpty())
		{
			T::StaticStruct()->ExportText(ValueString, &InValue, &InValue, nullptr, PPF_None, nullptr);
		}
		return ValueString;
	}

#if WITH_EDITOR
	bool CheckArgumentType(bool bCondition, const FName& InArgumentName) const
	{
		if(!bCondition)
		{
			UE_LOG(LogRigVM, Error, TEXT("Fatal: '%s' Argument '%s' has incorrect type."), *GetFactoryName().ToString(), *InArgumentName.ToString())
		}
		return bCondition;
	}
#endif

	RIGVM_API virtual const TArray<FName>& GetControlFlowBlocks_Impl(const FRigVMDispatchContext& InContext) const;

	RIGVM_API static const FString DispatchPrefix;
	RIGVM_API static const FString TrueString;

	const TArray<FName>* UpdateArgumentNameCache(int32 InNumberOperands) const;
	const TArray<FName>* UpdateArgumentNameCache_NoLock(int32 InNumberOperands) const;

	UScriptStruct* FactoryScriptStruct;
	mutable const FRigVMTemplate* CachedTemplate;
	static FCriticalSection GetTemplateMutex;
	mutable TMap<int32, TSharedPtr<TArray<FName>>> ArgumentNamesMap;
	FCriticalSection* ArgumentNamesMutex;
	friend struct FRigVMTemplate;
	friend struct FRigVMRegistry;
	friend struct FRigVMFunction;
	friend class URigVM;
};
