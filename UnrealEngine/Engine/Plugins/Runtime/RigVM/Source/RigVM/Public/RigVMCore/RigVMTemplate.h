// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "RigVMFunction.h"
#include "RigVMTypeIndex.h"
#include "RigVMTypeUtils.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UnrealNames.h"

#include "RigVMTemplate.generated.h"

class FProperty;
struct FRigVMDispatchFactory;
struct FRigVMTemplate;
struct FRigVMDispatchContext;
struct FRigVMUserDefinedTypeResolver;
struct FRigVMRegistry;

typedef TMap<FName, TRigVMTypeIndex> FRigVMTemplateTypeMap;

// FRigVMTemplate_NewArgumentTypeDelegate is deprecated, use FRigVMTemplate_GetPermutationsFromArgumentTypeDelegate
DECLARE_DELEGATE_RetVal_TwoParams(FRigVMTemplateTypeMap, FRigVMTemplate_NewArgumentTypeDelegate, const FName& /* InArgumentName */, TRigVMTypeIndex /* InTypeIndexToAdd */);
DECLARE_DELEGATE_RetVal(FRigVMDispatchFactory*, FRigVMTemplate_GetDispatchFactoryDelegate);

struct RIGVM_API FRigVMTemplateDelegates
{
	FRigVMTemplate_NewArgumentTypeDelegate NewArgumentTypeDelegate;
	FRigVMTemplate_GetDispatchFactoryDelegate GetDispatchFactoryDelegate;
};

USTRUCT()
struct RIGVM_API FRigVMTemplateArgumentType
{
	GENERATED_BODY()

	UPROPERTY()
	FName CPPType;
	
	UPROPERTY()
	TObjectPtr<UObject> CPPTypeObject; 

	FRigVMTemplateArgumentType()
		: CPPType(NAME_None)
		, CPPTypeObject(nullptr)
	{
		CPPType = RigVMTypeUtils::GetWildCardCPPTypeName();
		CPPTypeObject = RigVMTypeUtils::GetWildCardCPPTypeObject();
	}

	FRigVMTemplateArgumentType(const FName& InCPPType, UObject* InCPPTypeObject = nullptr);
	
	FRigVMTemplateArgumentType(UClass* InClass, RigVMTypeUtils::EClassArgType InClassArgType = RigVMTypeUtils::EClassArgType::AsObject)
	: CPPType(*RigVMTypeUtils::CPPTypeFromObject(InClass, InClassArgType))
	, CPPTypeObject(InClass)
	{
	}

	FRigVMTemplateArgumentType(UScriptStruct* InScriptStruct)
	: CPPType(*RigVMTypeUtils::GetUniqueStructTypeName(InScriptStruct))
	, CPPTypeObject(InScriptStruct)
	{
	}

	FRigVMTemplateArgumentType(UEnum* InEnum)
	: CPPType(*RigVMTypeUtils::CPPTypeFromEnum(InEnum))
	, CPPTypeObject(InEnum)
	{
	}

	bool IsValid() const
	{
		return !CPPType.IsNone();
	}

	static FRigVMTemplateArgumentType Array()
	{
		return FRigVMTemplateArgumentType(RigVMTypeUtils::GetWildCardArrayCPPTypeName(), RigVMTypeUtils::GetWildCardCPPTypeObject());
	}

	bool operator == (const FRigVMTemplateArgumentType& InOther) const
	{
		return CPPType == InOther.CPPType;
	}

	bool operator != (const FRigVMTemplateArgumentType& InOther) const
	{
		return CPPType != InOther.CPPType;
	}

	RIGVM_API friend FORCEINLINE uint32 GetTypeHash(const FRigVMTemplateArgumentType& InType)
	{
		return GetTypeHash(InType.CPPType);
	}

	FName GetCPPTypeObjectPath() const
	{
		if(CPPTypeObject)
		{
			return *CPPTypeObject->GetPathName();
		}
		return NAME_None;
	}

	bool IsWildCard() const
	{
		return CPPTypeObject == RigVMTypeUtils::GetWildCardCPPTypeObject() ||
			CPPType == RigVMTypeUtils::GetWildCardCPPTypeName() ||
			CPPType == RigVMTypeUtils::GetWildCardArrayCPPTypeName();
	}

	bool IsArray() const
	{
		return RigVMTypeUtils::IsArrayType(CPPType.ToString());
	}

	FString GetBaseCPPType() const
	{
		if(IsArray())
		{
			return RigVMTypeUtils::BaseTypeFromArrayType(CPPType.ToString());
		}
		return CPPType.ToString();
	}

	FRigVMTemplateArgumentType& ConvertToArray() 
	{
		CPPType = *RigVMTypeUtils::ArrayTypeFromBaseType(CPPType.ToString());
		return *this;
	}

	FRigVMTemplateArgumentType& ConvertToBaseElement() 
	{
		CPPType = *RigVMTypeUtils::BaseTypeFromArrayType(CPPType.ToString());
		return *this;
	}
};

/**
 * The template argument represents a single parameter
 * in a function call and all of its possible types
 */
struct RIGVM_API FRigVMTemplateArgument
{
	DECLARE_DELEGATE_RetVal_OneParam(bool, FTypeFilter, const TRigVMTypeIndex&);
	
	enum EArrayType
	{
		EArrayType_SingleValue,
		EArrayType_ArrayValue,
		EArrayType_ArrayArrayValue,
		EArrayType_Mixed,
		EArrayType_Invalid
	};

	enum ETypeCategory : uint8
	{
		ETypeCategory_Execute,
		ETypeCategory_SingleAnyValue,
		ETypeCategory_ArrayAnyValue,
		ETypeCategory_ArrayArrayAnyValue,
		ETypeCategory_SingleSimpleValue,
		ETypeCategory_ArraySimpleValue,
		ETypeCategory_ArrayArraySimpleValue,
		ETypeCategory_SingleMathStructValue,
		ETypeCategory_ArrayMathStructValue,
		ETypeCategory_ArrayArrayMathStructValue,
		ETypeCategory_SingleScriptStructValue,
		ETypeCategory_ArrayScriptStructValue,
		ETypeCategory_ArrayArrayScriptStructValue,
		ETypeCategory_SingleEnumValue,
		ETypeCategory_ArrayEnumValue,
		ETypeCategory_ArrayArrayEnumValue,
		ETypeCategory_SingleObjectValue,
		ETypeCategory_ArrayObjectValue,
		ETypeCategory_ArrayArrayObjectValue,
		ETypeCategory_Invalid
	};

	// default constructor
	FRigVMTemplateArgument();

	FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection);
	FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, TRigVMTypeIndex InType);
	FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<TRigVMTypeIndex>& InTypeIndices);
	FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<ETypeCategory>& InTypeCategories, TFunction<bool(const TRigVMTypeIndex&)> InFilterType = nullptr);

	// returns the name of the argument
	const FName& GetName() const { return Name; }

	// returns the direction of the argument
	ERigVMPinDirection GetDirection() const { return Direction; }

	// returns true if this argument supports a given type across a set of permutations
	bool SupportsTypeIndex(TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex = nullptr) const;

	// returns the flat list of types (including duplicates) of this argument
	void GetAllTypes(TArray<TRigVMTypeIndex>& OutTypes) const;
	
	TRigVMTypeIndex GetTypeIndex(const int32 InIndex) const;
	int32 GetNumTypes() const;
	void AddTypeIndex(const TRigVMTypeIndex InTypeIndex);
	void RemoveType(const int32 InIndex);
	void ForEachType(TFunction<bool(const TRigVMTypeIndex InType)>&& InCallback) const;
	int32 FindTypeIndex(const TRigVMTypeIndex InTypeIndex) const;

	template <typename Predicate>
	int32 IndexOfByPredicate(Predicate Pred) const
	{
		if (!bUseCategories)
		{
			return TypeIndices.IndexOfByPredicate(Pred);
		}
		if (FilterType)
		{
			bool bFound = false;
			int32 ValidIndex = 0;
			CategoryViews(TypeCategories).ForEachType([this, &ValidIndex, &bFound, Pred](const TRigVMTypeIndex Type)
			{
				if (FilterType(Type))
				{
					if (Pred(Type))
					{
						bFound = true;
						return false;
					}
					ValidIndex++;
				}
				return true;
			});
			if (bFound)
			{
				return ValidIndex;
			}
			return INDEX_NONE;
		}
		return CategoryViews(TypeCategories).IndexOfByPredicate(Pred);
	}

	// returns an array of all of the supported types
	TArray<TRigVMTypeIndex> GetSupportedTypeIndices(const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

#if WITH_EDITOR
	// returns an array of all supported types as strings. this is used for automated testing only.
	TArray<FString> GetSupportedTypeStrings(const TArray<int32>& InPermutationIndices = TArray<int32>()) const;
#endif
	
	// returns true if an argument is singleton (same type for all variants)
	bool IsSingleton(const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

	// returns true if this argument is an execute
	bool IsExecute() const;

	// returns true if the argument uses an array container
	EArrayType GetArrayType() const;

	RIGVM_API friend uint32 GetTypeHash(const FRigVMTemplateArgument& InArgument);

	// Get the map of types to permutation indices
	const TArray<int32>& GetPermutations(const TRigVMTypeIndex InType) const;
	void InvalidatePermutations(const TRigVMTypeIndex InType);

protected:

	int32 Index = INDEX_NONE;
	FName Name = NAME_None;
	ERigVMPinDirection Direction = ERigVMPinDirection::IO;

	TArray<TRigVMTypeIndex> TypeIndices;
	mutable TMap<TRigVMTypeIndex, TArray<int32>> TypeToPermutations;

	bool bUseCategories = false;
	TArray<ETypeCategory> TypeCategories;
	TFunction<bool(const TRigVMTypeIndex&)> FilterType;

	FRigVMTemplateArgument(FProperty* InProperty);

	// constructor from a property. this forces the type to be created
	FRigVMTemplateArgument(FProperty* InProperty, FRigVMRegistry& InRegistry);

	void EnsureValidExecuteType(FRigVMRegistry& InRegistry);
	void UpdateTypeToPermutations();

	friend struct FRigVMTemplate;
	friend struct FRigVMDispatchFactory;
	friend class URigVMController;
	friend struct FRigVMRegistry;
	friend struct FRigVMStructUpgradeInfo;
	friend class URigVMCompiler;

private:
	struct CategoryViews
	{
		CategoryViews() = delete;
		CategoryViews(const TArray<ETypeCategory>& InCategories);
		
		void ForEachType(TFunction<bool(const TRigVMTypeIndex InType)>&& InCallback) const;

		TRigVMTypeIndex GetTypeIndex(int32 InIndex) const;
		
		int32 FindIndex(const TRigVMTypeIndex InTypeIndex) const;

		template <typename Predicate>
		int32 IndexOfByPredicate(Predicate Pred) const
		{
			int32 Offset = 0;
			for (const TArrayView<const TRigVMTypeIndex>& TypeView: Types)
			{
				const int32 Found = TypeView.IndexOfByPredicate(Pred);
				if (Found != INDEX_NONE)
				{
					return Found + Offset;
				}
				Offset += TypeView.Num();
			}
			return INDEX_NONE;
		}
		
	private:
		TArray<TArrayView<const TRigVMTypeIndex>> Types;
	};
};

/**
 * FRigVMTemplateArgumentInfo 
 */

struct RIGVM_API FRigVMTemplateArgumentInfo
{
	using ArgumentCallback = TFunction<FRigVMTemplateArgument(const FName /*InName*/, ERigVMPinDirection /*InDirection*/)>;
	using TypeFilterCallback = TFunction<bool(const TRigVMTypeIndex& InNewType)>;
	
	FName Name = NAME_None;
	ERigVMPinDirection Direction = ERigVMPinDirection::Invalid;
	TFunction<FRigVMTemplateArgument(const FName /*InName*/, ERigVMPinDirection /*InDirection*/)> FactoryCallback = [](const FName, ERigVMPinDirection) { return FRigVMTemplateArgument(); };
	
	FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection, TRigVMTypeIndex InTypeIndex);
	FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection, const TArray<TRigVMTypeIndex>& InTypeIndices);
	FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection, const TArray<FRigVMTemplateArgument::ETypeCategory>& InTypeCategories, TypeFilterCallback InTypeFilter = nullptr);
	FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection);
	FRigVMTemplateArgumentInfo(const FName InName, ERigVMPinDirection InDirection, ArgumentCallback&& InCallback);

	FRigVMTemplateArgument GetArgument() const;
	
	static FName ComputeTemplateNotation(
		const FName InTemplateName,
		const TArray<FRigVMTemplateArgumentInfo>& InInfos);
	
	static TArray<TRigVMTypeIndex> GetTypesFromCategories(
		const TArray<FRigVMTemplateArgument::ETypeCategory>& InTypeCategories,
		const FRigVMTemplateArgument::FTypeFilter& InTypeFilter = {});	
};

/**
 * The template is used to group multiple rigvm functions
 * that share the same notation. Templates can then be used
 * to build polymorphic nodes (RigVMTemplateNode) that can
 * take on any of the permutations supported by the template.
 */
struct RIGVM_API FRigVMTemplate
{
public:

	typedef FRigVMTemplateTypeMap FTypeMap;
	typedef TPair<FName, TRigVMTypeIndex> FTypePair;

	// Default constructor
	FRigVMTemplate();

	// returns true if this is a valid template
	bool IsValid() const;

	// Returns the notation of this template
	const FName& GetNotation() const;

	// Returns the name of the template
	FName GetName() const;

	// returns true if this template can merge another one
	bool Merge(const FRigVMTemplate& InOther);

	// returns the number of args of this template
	int32 NumArguments() const { return Arguments.Num(); }

	// returns an argument for a given index
	const FRigVMTemplateArgument* GetArgument(int32 InIndex) const { return &Arguments[InIndex]; }

	// returns an argument given a name (or nullptr)
	const FRigVMTemplateArgument* FindArgument(const FName& InArgumentName) const;

	// returns the number of args of this template
	int32 NumExecuteArguments(const FRigVMDispatchContext& InContext) const;

	// returns an argument for a given index
	const FRigVMExecuteArgument* GetExecuteArgument(int32 InIndex, const FRigVMDispatchContext& InContext) const;

	// returns an argument given a name (or nullptr)
	const FRigVMExecuteArgument* FindExecuteArgument(const FName& InArgumentName, const FRigVMDispatchContext& InContext) const;

	// returns the top level execute context struct this template uses
	const UScriptStruct* GetExecuteContextStruct() const;

	// returns true if this template supports a given execute context struct
	bool SupportsExecuteContextStruct(const UScriptStruct* InExecuteContextStruct) const;

	// returns true if a given arg supports a type
	bool ArgumentSupportsTypeIndex(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex = nullptr) const;

	// returns the number of permutations supported by this template
	int32 NumPermutations() const { return Permutations.Num(); }

	// returns the first / primary permutation of the template
	const FRigVMFunction* GetPrimaryPermutation() const;

	// returns a permutation given an index
	const FRigVMFunction* GetPermutation(int32 InIndex) const;

	// returns a permutation given an index and creates it using the backing factory if needed
	const FRigVMFunction* GetOrCreatePermutation(int32 InIndex);

	// returns true if a given function is a permutation of this template
	bool ContainsPermutation(const FRigVMFunction* InPermutation) const;

	// returns the index of the permutation within the template of a given function (or INDEX_NONE)
	int32 FindPermutation(const FRigVMFunction* InPermutation) const;

	// returns the index of the permutation within the template of a given set of types
	int32 FindPermutation(const FTypeMap& InTypes) const;

	// returns true if the template was able to resolve to single permutation
	bool FullyResolve(FTypeMap& InOutTypes, int32& OutPermutationIndex) const;

	// returns true if the template was able to resolve to at least one permutation
	bool Resolve(FTypeMap& InOutTypes, TArray<int32> & OutPermutationIndices, bool bAllowFloatingPointCasts) const;

	// Will return the hash of the input type map, if it is a valid type map. Otherwise, will return 0.
	// It is only a valid type map if it includes all arguments, and non of the types is a wildcard.
	uint32 GetTypesHashFromTypes(const FTypeMap& InTypes) const;

	// returns true if the template was able to resolve to at least one permutation
	bool ContainsPermutation(const FTypeMap& InTypes) const;

	// returns true if the template can resolve an argument to a new type
	bool ResolveArgument(const FName& InArgumentName, const TRigVMTypeIndex InTypeIndex, FTypeMap& InOutTypes) const;

	// returns the types for a specific permutation
	FRigVMTemplateTypeMap GetTypesForPermutation(const int32 InPermutationIndex) const;

	// returns true if a given argument is valid for a template
	static bool IsValidArgumentForTemplate(const ERigVMPinDirection InDirection);

	// returns the prefix for an argument in the notation
	static const FString& GetDirectionPrefix(const ERigVMPinDirection InDirection);

	// returns the notation of an argument
	static FString GetArgumentNotation(const FName InName, const ERigVMPinDirection InDirection);

	// recomputes the notation from its arguments
	void ComputeNotationFromArguments(const FString& InTemplateName);

	// returns an array of structs in the inheritance order of a given struct
	static TArray<UStruct*> GetSuperStructs(UStruct* InStruct, bool bIncludeLeaf = true);

	// converts the types provided by a string (like "A:float,B:int32") into a type map
	FTypeMap GetArgumentTypesFromString(const FString& InTypeString, const FRigVMUserDefinedTypeResolver* InTypeResolver = nullptr) const;

	// converts the types provided to a string (like "A:float,B:int32")
	static FString GetStringFromArgumentTypes(const FTypeMap& InTypes); 

#if WITH_EDITOR

	// Returns the color based on the permutation's metadata
	FLinearColor GetColor(const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

	// Returns the tooltip based on the permutation's metadata
	FText GetTooltipText(const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

	// Returns the display name text for an argument 
	FText GetDisplayNameForArgument(const FName& InArgumentName, const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

	// Returns meta data on the property of the permutations 
	FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey, const TArray<int32>& InPermutationIndices = TArray<int32>()) const;

	FString GetCategory() const;
	FString GetKeywords() const;

#endif

	// Updates the template's argument types. This only affects templates which have category based
	// arguments and will resolve the other arguments to the expected types.
	bool UpdateArgumentTypes();

	// Invalidates template permutations whenever a type such as a user defined struct is removed
	void HandleTypeRemoval(TRigVMTypeIndex InTypeIndex);
	
	// Returns the delegate to be able to react to type changes dynamically
	// This delegate is deprecated
	FRigVMTemplate_NewArgumentTypeDelegate& OnNewArgumentType() { return Delegates.NewArgumentTypeDelegate; }

	// Returns the factory this template was created by
	const FRigVMDispatchFactory* GetDispatchFactory() const
	{
		return UsesDispatch() ? Delegates.GetDispatchFactoryDelegate.Execute() : nullptr;
	}

	// Returns true if this template is backed by a dispatch factory
	bool UsesDispatch() const
	{
		return Delegates.GetDispatchFactoryDelegate.IsBound();
	}

	void RecomputeTypesHashToPermutations();

	void UpdateTypesHashToPermutation(const int32 InPermutation);

	RIGVM_API friend uint32 GetTypeHash(const FRigVMTemplate& InTemplate);

private:

	// Constructor from a struct, a template name and a function index
	FRigVMTemplate(UScriptStruct* InStruct, const FString& InTemplateName, int32 InFunctionIndex = INDEX_NONE);

	// Constructor from a template name and argument infos
	FRigVMTemplate(const FName& InTemplateName, const TArray<FRigVMTemplateArgumentInfo>& InInfos);

	static FLinearColor GetColorFromMetadata(FString InMetadata);

	void InvalidateHash() { Hash = UINT32_MAX; }
	const TArray<FRigVMExecuteArgument>& GetExecuteArguments(const FRigVMDispatchContext& InContext) const;

	const FRigVMFunction* GetPermutation_NoLock(int32 InIndex) const;
	const FRigVMFunction* GetOrCreatePermutation_NoLock(int32 InIndex);

	int32 Index;
	FName Notation;
	TArray<FRigVMTemplateArgument> Arguments;
	mutable TArray<FRigVMExecuteArgument> ExecuteArguments;
	TArray<int32> Permutations;
	TMap<uint32, int32> TypesHashToPermutation;
	mutable uint32 Hash;

	FRigVMTemplateDelegates Delegates;

	friend struct FRigVMRegistry;
	friend class URigVMController;
	friend class URigVMLibraryNode;
	friend struct FRigVMDispatchFactory;
};
