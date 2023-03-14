// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "RigVMFunction.h"
#include "RigVMTraits.h"
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

typedef TMap<FName, TRigVMTypeIndex> FRigVMTemplateTypeMap;

DECLARE_DELEGATE_RetVal_ThreeParams(FRigVMTemplateTypeMap, FRigVMTemplate_NewArgumentTypeDelegate, const FRigVMTemplate* /* InTemplate */, const FName& /* InArgumentName */, TRigVMTypeIndex /* InTypeIndexToAdd */);
DECLARE_DELEGATE_RetVal_TwoParams(FRigVMFunctionPtr, FRigVMTemplate_RequestDispatchFunctionDelegate, const FRigVMTemplate* /* InTemplate */,  const FRigVMTemplateTypeMap& /* InTypes */);
DECLARE_DELEGATE_RetVal(FRigVMDispatchFactory*, FRigVMTemplate_GetDispatchFactoryDelegate);

struct RIGVM_API FRigVMTemplateDelegates
{
	FRigVMTemplate_NewArgumentTypeDelegate NewArgumentTypeDelegate;
	FRigVMTemplate_GetDispatchFactoryDelegate GetDispatchFactoryDelegate;
	FRigVMTemplate_RequestDispatchFunctionDelegate RequestDispatchFunctionDelegate;
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

	FRigVMTemplateArgumentType(const FName& InCPPType, UObject* InCPPTypeObject = nullptr)
		: CPPType(InCPPType)
		, CPPTypeObject(InCPPTypeObject)
	{
		// InCppType is unreliable because not all caller knows that
		// we use generated unique names for user defined structs
		// so here we override the CppType name with the actual name used in the registry
		CPPType = *RigVMTypeUtils::PostProcessCPPType(CPPType.ToString(), CPPTypeObject);
		
		check(!CPPType.IsNone());
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

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMTemplateArgumentType& InType)
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

	FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, TRigVMTypeIndex InType);
	FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<TRigVMTypeIndex>& InTypeIndices);
	FRigVMTemplateArgument(const FName& InName, ERigVMPinDirection InDirection, const TArray<ETypeCategory>& InTypeCategories, const FTypeFilter& InTypeFilter = {});

	// Serialize
	void Serialize(FArchive& Ar);

	// returns the name of the argument
	const FName& GetName() const { return Name; }

	// returns the direction of the argument
	ERigVMPinDirection GetDirection() const { return Direction; }

	// returns true if this argument supports a given type across a set of permutations
	bool SupportsTypeIndex(TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex = nullptr) const;

	// returns the flat list of types (including duplicates) of this argument
	const TArray<TRigVMTypeIndex>& GetTypeIndices() const;

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

protected:

	int32 Index;
	FName Name;
	ERigVMPinDirection Direction;
	TArray<TRigVMTypeIndex> TypeIndices;

	TMap<TRigVMTypeIndex, TArray<int32>> TypeToPermutations;
	TArray<ETypeCategory> TypeCategories;

	// constructor from a property. this forces the type to be created
	FRigVMTemplateArgument(FProperty* InProperty);

	void EnsureValidExecuteType();
	void UpdateTypeToPermutations();

	friend struct FRigVMTemplate;
	friend class URigVMController;
	friend struct FRigVMRegistry;
	friend struct FRigVMStructUpgradeInfo;
	friend struct FRigVMSetLibraryTemplateAction;
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

	// Serialize
	void Serialize(FArchive& Ar);

	// returns true if this is a valid template
	bool IsValid() const;

	// Returns the notation of this template
	const FName& GetNotation() const;

	// Returns the name of the template
	FName GetName() const;

	// returns true if this template is compatible with another one
	bool IsCompatible(const FRigVMTemplate& InOther) const;

	// returns true if this template can merge another one
	bool Merge(const FRigVMTemplate& InOther);

	// returns the number of args of this template
	int32 NumArguments() const { return Arguments.Num(); }

	// returns an argument for a given index
	const FRigVMTemplateArgument* GetArgument(int32 InIndex) const { return &Arguments[InIndex]; }

		// returns an argument given a name (or nullptr)
	const FRigVMTemplateArgument* FindArgument(const FName& InArgumentName) const;

	// returns true if a given arg supports a type
	bool ArgumentSupportsTypeIndex(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex = nullptr) const;

	// returns the number of permutations supported by this template
	int32 NumPermutations() const { return Permutations.Num(); }

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

	// returns true if the template can resolve an argument to a new type
	bool ResolveArgument(const FName& InArgumentName, const TRigVMTypeIndex InTypeIndex, FTypeMap& InOutTypes) const;

	// returns the types for a specific permutation
	FRigVMTemplateTypeMap GetTypesForPermutation(const int32 InPermutationIndex) const;

	// returns true if a given argument is valid for a template
	static bool IsValidArgumentForTemplate(const FRigVMTemplateArgument& InArgument);

	// returns the prefix for an argument in the notation
	static const FString& GetArgumentNotationPrefix(const FRigVMTemplateArgument& InArgument);

	// returns the notation of an argument
	static FString GetArgumentNotation(const FRigVMTemplateArgument& InArgument);

	// recomputes the notation from its arguments
	void ComputeNotationFromArguments(const FString& InTemplateName);

	// returns an array of structs in the inheritance order of a given struct
	static TArray<UStruct*> GetSuperStructs(UStruct* InStruct, bool bIncludeLeaf = true);

	// converts the types provided by a string (like "A:float,B:int32") into a type map
	FTypeMap GetArgumentTypesFromString(const FString& InTypeString) const;

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

	// Adds a new argument to the template. The template needs to rely on the delegate to ask the
	// template factory how to deal with the new type.
	bool AddTypeForArgument(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex);

	// Invalidates template permutations whenever a type such as a user defined struct is removed
	void HandleTypeRemoval(TRigVMTypeIndex InTypeIndex);
	
	// Returns the delegate to be able to react to type changes dynamically
	FRigVMTemplate_NewArgumentTypeDelegate& OnNewArgumentType() { return Delegates.NewArgumentTypeDelegate; }

	// Returns the factory this template was created by
	FORCEINLINE const FRigVMDispatchFactory* GetDispatchFactory() const
	{
		if(Delegates.GetDispatchFactoryDelegate.IsBound())
		{
			return Delegates.GetDispatchFactoryDelegate.Execute();
		}
		return nullptr;
	}

	// Returns true if this template is backed by a dispatch factory
	FORCEINLINE bool UsesDispatch() const
	{
		return Delegates.RequestDispatchFunctionDelegate.IsBound() &&
			Delegates.GetDispatchFactoryDelegate.IsBound();
	}  

private:

	// Constructor from a struct, a template name and a function index
	FRigVMTemplate(UScriptStruct* InStruct, const FString& InTemplateName, int32 InFunctionIndex);

	// Constructor from a template name, arguments and a function index
	FRigVMTemplate(const FName& InTemplateName, const TArray<FRigVMTemplateArgument>& InArguments, int32 InFunctionIndex);

	static FLinearColor GetColorFromMetadata(FString InMetadata);

	int32 Index;
	FName Notation;
	TArray<FRigVMTemplateArgument> Arguments;
	TArray<int32> Permutations;

	FRigVMTemplateDelegates Delegates;

	friend struct FRigVMRegistry;
	friend class URigVMController;
	friend class URigVMLibraryNode;
	friend struct FRigVMSetLibraryTemplateAction;
	friend struct FRigVMDispatchFactory;
};
