// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMCore/RigVMTemplate.h"
#include "RigVMTemplateNode.generated.h"

USTRUCT()
struct RIGVMDEVELOPER_API FRigVMTemplatePreferredType
{
	GENERATED_BODY()

public:
	
	FRigVMTemplatePreferredType()
		: Argument(NAME_None)
		, TypeIndex(INDEX_NONE)
	{}
	
	FRigVMTemplatePreferredType(const FName& InArgument, TRigVMTypeIndex InTypeIndex)
		: Argument(InArgument)
		, TypeIndex(InTypeIndex)
	{
#if UE_RIGVM_DEBUG_TYPEINDEX
		UpdateStringFromIndex();
#endif
	}

	FORCEINLINE const FName& GetArgument() const { return Argument; }
	FORCEINLINE TRigVMTypeIndex GetTypeIndex() const
	{
#if UE_RIGVM_DEBUG_TYPEINDEX
		const FRigVMTemplateArgumentType& Type = FRigVMRegistry::Get().GetType(TypeIndex);
		return FRigVMRegistry::Get().GetTypeIndex(Type);
#else
		return TypeIndex;
#endif
	}

	bool operator==(const FRigVMTemplatePreferredType& Other) const
	{
		return Argument == Other.Argument && TypeIndex == Other.TypeIndex;
	}
	
	void UpdateStringFromIndex();
	void UpdateIndexFromString();

protected:
	
	UPROPERTY()
	FName Argument;

	UPROPERTY()
	int32 TypeIndex;

	UPROPERTY()
	FString TypeString;

	friend class URigVMTemplateNode;
	friend class URigVMController;
	friend class UControlRigBlueprint;
};

/**
 * The Template Node represents an unresolved function.
 * Template nodes can morph into all functions implementing
 * the template's template.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMTemplateNode : public URigVMNode
{
	GENERATED_BODY()

public:

	// default constructor
	URigVMTemplateNode();

	// UObject interface
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostLoad() override;

	// URigVMNode interface
	virtual FString GetNodeTitle() const override;
	virtual FName GetMethodName() const;
	virtual  FText GetToolTipText() const override;
	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;
	virtual TArray<URigVMPin*> GetAggregateInputs() const override;
	virtual TArray<URigVMPin*> GetAggregateOutputs() const override;
	TArray<URigVMPin*> GetAggregatePins(const ERigVMPinDirection& InDirection) const;
	virtual FName GetNextAggregateName(const FName& InLastAggregatePinName) const override;

	// Returns the UStruct for this unit node
	// (the struct declaring the RIGVM_METHOD)
	UFUNCTION(BlueprintCallable, Category = RigVMUnitNode)
	virtual UScriptStruct* GetScriptStruct() const;

	// Returns the notation of the node
	UFUNCTION(BlueprintPure, Category = Template)
	virtual FName GetNotation() const;

	UFUNCTION(BlueprintCallable, Category = Template)
	virtual bool IsSingleton() const;

	// returns true if a pin supports a given type
	bool SupportsType(const URigVMPin* InPin, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex = nullptr);

	// returns true if a pin supports a given type after filtering
	bool FilteredSupportsType(const URigVMPin* InPin, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex = nullptr, bool bAllowFloatingPointCasts = true);

	// returns the resolved functions for the template
	TArray<const FRigVMFunction*> GetResolvedPermutations() const;

	// returns the template used for this node
	virtual const FRigVMTemplate* GetTemplate() const;

	// returns the resolved function or nullptr if there are still unresolved pins left
	const FRigVMFunction* GetResolvedFunction() const;

	// returns true if the template node is resolved
	UFUNCTION(BlueprintPure, Category = Template)
	bool IsResolved() const;

	// returns true if the template is fully unresolved
	UFUNCTION(BlueprintPure, Category = Template)
	bool IsFullyUnresolved() const;

	// returns a default value for pin if it is known
	FString GetInitialDefaultValueForPin(const FName& InRootPinName, const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

	// returns the display name for a pin
	FName GetDisplayNameForPin(const FName& InRootPinName, const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

	// returns the indeces of the filtered permutations
	const TArray<int32>& GetFilteredPermutationsIndices() const;

	// returns the filtered types of this pin
	TArray<TRigVMTypeIndex> GetFilteredTypesForPin(URigVMPin* InPin) const;

	// Tries to reduce the input types to a single type, if all are compatible
	// Will prioritize the InPreferredType if available
	TRigVMTypeIndex TryReduceTypesToSingle(const TArray<TRigVMTypeIndex>& InTypes, const TRigVMTypeIndex PreferredType = TRigVMTypeIndex()) const;

	// returns true if updating pin filters with InTypes would result in different filters 
	bool PinNeedsFilteredTypesUpdate(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices);
	bool PinNeedsFilteredTypesUpdate(URigVMPin* InPin, URigVMPin* LinkedPin);

	// updates the filtered permutations given a link or the types for a pin
	bool UpdateFilteredPermutations(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices);
	bool UpdateFilteredPermutations(URigVMPin* InPin, URigVMPin* LinkedPin);

	// initializes the filtered permutations to all possible permutations
	void InitializeFilteredPermutations();

	// Initializes the filtered permutations and preferred permutation from the types of the pins
	void InitializeFilteredPermutationsFromTypes(bool bAllowCasting);

	// Converts the preferred types per pin from index to string
	void ConvertPreferredTypesToString();

	// Converts the preferred types per pin from string to indices
	void ConvertPreferredTypesToTypeIndex();

	// Returns the preferred type, or an invalid type if non was found
	TRigVMTypeIndex GetPreferredType(const FName& ArgumentName) const;

protected:

	virtual void InvalidateCache() override;
	
	TArray<int32> GetNewFilteredPermutations(URigVMPin* InPin, URigVMPin* LinkedPin);
	TArray<int32> GetNewFilteredPermutations(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices);

	TArray<int32> FindPermutationsForTypes(const TArray<FRigVMTemplatePreferredType>& ArgumentTypes, bool bAllowCasting = false) const;
	TArray<FRigVMTemplatePreferredType> GetPreferredTypesForPermutation(const int32 InPermutationIndex) const;
	FRigVMTemplateTypeMap GetTypesForPermutation(const int32 InPermutationIndex) const;

	UPROPERTY()
	FName TemplateNotation;

	UPROPERTY()
	FString ResolvedFunctionName;

	// Indicates a preferred permutation using the types of the arguments
	// Each element is in the format "ArgumentName:CPPType"
	UPROPERTY()
	TArray<FString> PreferredPermutationTypes_DEPRECATED;

	UPROPERTY()
	TArray<FRigVMTemplatePreferredType> PreferredPermutationPairs;

	TArray<int32> FilteredPermutations;

	mutable const FRigVMTemplate* CachedTemplate;
	mutable const FRigVMFunction* CachedFunction;
	mutable int32 ResolvedPermutation;

	friend class URigVMController;
	friend class UControlRigBlueprint;
	friend struct FRigVMSetTemplateFilteredPermutationsAction;
	friend struct FRigVMSetPreferredTemplatePermutationsAction;
	friend struct FRigVMRemoveNodeAction;
	friend class FRigVMTemplatesLibraryNode3Test;
	friend class URigVMCompiler;
};

