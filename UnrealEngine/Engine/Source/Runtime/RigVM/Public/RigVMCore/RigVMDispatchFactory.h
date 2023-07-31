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
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

#include "RigVMDispatchFactory.generated.h"

/**
 * A factory to generate a template and its dispatch functions
 */
USTRUCT()
struct RIGVM_API FRigVMDispatchFactory
{
	GENERATED_BODY()

public:

	FORCEINLINE FRigVMDispatchFactory()
		: FactoryScriptStruct(nullptr)
		, CachedTemplate(nullptr)
	{}
	
	FORCEINLINE virtual ~FRigVMDispatchFactory() {}

	// returns the name of the factory template
	FName GetFactoryName() const;

	// returns the struct for this dispatch factory
	FORCEINLINE UScriptStruct* GetScriptStruct() const { return FactoryScriptStruct; };

#if WITH_EDITOR
	
	// returns the title of the node for a given type set.
	FORCEINLINE virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const { return GetScriptStruct()->GetDisplayNameText().ToString(); }

	// returns the color of the node for a given type set.
	virtual FLinearColor GetNodeColor() const;

	// returns the tooltip for the node
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const;

	// returns the default value for an argument
	FORCEINLINE virtual FString GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const { return FString(); }

	// returns the tooltip for an argument
	FORCEINLINE virtual FText GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const { return FText(); }

	// returns the names of the input aggregate arguments
	FORCEINLINE virtual TArray<FName> GetAggregateInputArguments() const { return TArray<FName>(); }

	// returns the names of the output aggregate arguments
	FORCEINLINE virtual TArray<FName> GetAggregateOutputArguments() const { return TArray<FName>(); }

	// returns the next name to be used for an aggregate pin
	virtual FName GetNextAggregateName(const FName& InLastAggregatePinName) const;

	// Returns the display name text for an argument 
	FORCEINLINE virtual FText GetDisplayNameForArgument(const FName& InArgumentName) const { return FText::FromName(InArgumentName); }

	// Returns meta data on the property of the permutations 
	FORCEINLINE virtual FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const { return FString(); }

	// Returns the category this factory is under
	virtual FString GetCategory() const;

	// Returns the keywords used for looking up this factory
	virtual FString GetKeywords() const;

#endif

	// Returns the execute context support for this dispatch factory
	FORCEINLINE virtual UScriptStruct* GetExecuteContextStruct() const { return FRigVMExecuteContext::StaticStruct(); }

	// registered needed types during registration of the factory
	FORCEINLINE virtual void RegisterDependencyTypes() const {}

	// returns opaque arguments expected to be passed to this factory in FRigVMExtendedExecuteContext::OpaqueArguments
	FORCEINLINE virtual TArray<TPair<FName,FString>> GetOpaqueArguments() const { return TArray<TPair<FName,FString>>(); }

	// returns the arguments of the template
	FORCEINLINE virtual TArray<FRigVMTemplateArgument> GetArguments() const { return TArray<FRigVMTemplateArgument>(); }

	// returns the delegate to react to new types being added to an argument.
	// this happens if types are being loaded later after this factory has already been deployed
	FORCEINLINE virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const { return FRigVMTemplateTypeMap(); }

	// returns the upgrade info to use for this factory
	FORCEINLINE virtual FRigVMStructUpgradeInfo GetUpgradeInfo(const FRigVMTemplateTypeMap& InTypes) const { return FRigVMStructUpgradeInfo(); }

	// returns the dispatch function for a given type set
	FRigVMFunctionPtr GetDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const;

	// builds and returns the template
	const FRigVMTemplate* GetTemplate() const;

	// returns the name of the permutation for a given set of types
	FString GetPermutationName(const FRigVMTemplateTypeMap& InTypes) const;

protected:

	// returns the name of the permutation for a given set of types
	FString GetPermutationNameImpl(const FRigVMTemplateTypeMap& InTypes) const;

	FORCEINLINE virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const { return nullptr; }

	static const FString DispatchPrefix;

	UScriptStruct* FactoryScriptStruct;
	mutable const FRigVMTemplate* CachedTemplate;
	friend struct FRigVMTemplate;
	friend struct FRigVMRegistry;
};
