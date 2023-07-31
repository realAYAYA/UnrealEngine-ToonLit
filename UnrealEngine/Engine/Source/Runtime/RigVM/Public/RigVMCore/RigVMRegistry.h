// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ChunkedArray.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/PlatformCrt.h"
#include "RigVMCore/RigVMTraits.h"
#include "RigVMDispatchFactory.h"
#include "RigVMFunction.h"
#include "RigVMMemory.h"
#include "RigVMTemplate.h"
#include "RigVMTypeIndex.h"
#include "Templates/EnableIf.h"
#include "Templates/IsEnum.h"
#include "Templates/Models.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/GCObject.h"

class FProperty;
class UObject;
struct FRigVMDispatchFactory;

/**
 * The FRigVMRegistry is used to manage all known function pointers
 * for use in the RigVM. The Register method is called automatically
 * when the static struct is initially constructed for each USTRUCT
 * hosting a RIGVM_METHOD enabled virtual function.
 * 
 * Inheriting from FGCObject to ensure that all type objects cannot be GCed
 */
struct RIGVM_API FRigVMRegistry : public FGCObject
{
public:

	DECLARE_MULTICAST_DELEGATE(FOnRigVMRegistryChanged);
	
	~FRigVMRegistry();
	
	// Returns the singleton registry
	static FRigVMRegistry& Get();

	// FGCObject overrides
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	
	// Registers a function given its name.
	// The name will be the name of the struct and virtual method,
	// for example "FMyStruct::MyVirtualMethod"
	void Register(const TCHAR* InName, FRigVMFunctionPtr InFunctionPtr, UScriptStruct* InStruct = nullptr, const TArray<FRigVMFunctionArgument>& InArguments = TArray<FRigVMFunctionArgument>());

	// Registers a dispatch factory given its struct.
	const FRigVMDispatchFactory* RegisterFactory(UScriptStruct* InFactoryStruct);

	// Initializes the registry by storing the defaults
	void InitializeIfNeeded();
	
	// Refreshes the list and finds the function pointers
	// based on the names.
	void RefreshEngineTypes();

	// Update the registry when types are renamed
	void OnAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath);
	
	// Update the registry when old types are removed
    void OnAssetRemoved(const FAssetData& InAssetData);

	// Update the registry when new types are added to the attribute system so that they can be selected
	// on Attribute Nodes
	void OnAnimationAttributeTypesChanged(const UScriptStruct* InStruct, bool bIsAdded);
	
	// Clear the registry
	void Reset();

	// Adds a type if it doesn't exist yet and returns its index.
	// This function is not thead-safe
	FORCEINLINE TRigVMTypeIndex FindOrAddType(const FRigVMTemplateArgumentType& InType)
	{
		return FindOrAddType_Internal(InType, false);
	}

	// Removes a type from the registry, and updates all dependent templates
	// which also creates invalid permutations in templates that we should ignore
	bool RemoveType(const FAssetData& InAssetData);

	// Returns the type index given a type
	TRigVMTypeIndex GetTypeIndex(const FRigVMTemplateArgumentType& InType) const;

	// Returns the type index given a cpp type and a type object
	FORCEINLINE TRigVMTypeIndex GetTypeIndex(const FName& InCPPType, UObject* InCPPTypeObject) const
	{
		return GetTypeIndex(FRigVMTemplateArgumentType(InCPPType, InCPPTypeObject));
	}

	// Returns the type index given an enum
	template <
		typename T,
		typename TEnableIf<TIsEnum<T>::Value>::Type* = nullptr
	>
	FORCEINLINE TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
	{
		FRigVMTemplateArgumentType Type(StaticEnum<T>());
		if(bAsArray)
		{
			Type.ConvertToArray();
		}
		return GetTypeIndex(Type);
	}

	// Returns the type index given a struct
	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
	{
		FRigVMTemplateArgumentType Type(TBaseStructure<T>::Get());
		if(bAsArray)
		{
			Type.ConvertToArray();
		}
		return GetTypeIndex(Type);
	}

	// Returns the type index given a struct
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
	>
	FORCEINLINE TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
	{
		FRigVMTemplateArgumentType Type(T::StaticStruct());
		if(bAsArray)
		{
			Type.ConvertToArray();
		}
		return GetTypeIndex(Type);
	}

	// Returns the type index given an object
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUClass, T>::Value>::Type * = nullptr
	>
	FORCEINLINE TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
	{
		FRigVMTemplateArgumentType Type(T::StaticClass());
		if(bAsArray)
		{
			Type.ConvertToArray();
		}
		return GetTypeIndex(Type);
	}

	// Returns the type given its index
	const FRigVMTemplateArgumentType& GetType(TRigVMTypeIndex InTypeIndex) const;

	// Returns the number of types
	FORCEINLINE int32 NumTypes() const { return Types.Num(); }

	// Returns the type given only its cpp type
	const FRigVMTemplateArgumentType& FindTypeFromCPPType(const FString& InCPPType) const;

	// Returns the type index given only its cpp type
	TRigVMTypeIndex GetTypeIndexFromCPPType(const FString& InCPPType) const;

	// Returns true if the type is an array
	bool IsArrayType(TRigVMTypeIndex InTypeIndex) const;

	// Returns true if the type is an execute type
	bool IsExecuteType(TRigVMTypeIndex InTypeIndex) const;

	// Converts the given execute context type to the base execute context type
	bool ConvertExecuteContextToBaseType(TRigVMTypeIndex& InOutTypeIndex) const;

	// Returns the dimensions of the array 
	int32 GetArrayDimensionsForType(TRigVMTypeIndex InTypeIndex) const;

	// Returns true if the type is a wildcard type
	bool IsWildCardType(TRigVMTypeIndex InTypeIndex) const;

	// Returns true if the types can be matched.
	bool CanMatchTypes(TRigVMTypeIndex InTypeIndexA, TRigVMTypeIndex InTypeIndexB, bool bAllowFloatingPointCasts) const;

	// Returns the list of compatible types for a given type
	const TArray<TRigVMTypeIndex>& GetCompatibleTypes(TRigVMTypeIndex InTypeIndex) const;

	// Returns all compatible types given a category
	const TArray<TRigVMTypeIndex>& GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory InCategory) const;

	// Returns the type index of the array matching the given element type index
	TRigVMTypeIndex GetArrayTypeFromBaseTypeIndex(TRigVMTypeIndex InTypeIndex) const;

	// Returns the type index of the element matching the given array type index
	TRigVMTypeIndex GetBaseTypeFromArrayTypeIndex(TRigVMTypeIndex InTypeIndex) const;

	// Returns the function given its name (or nullptr)
	const FRigVMFunction* FindFunction(const TCHAR* InName) const;

	// Returns the function given its backing up struct and method name
	const FRigVMFunction* FindFunction(UScriptStruct* InStruct, const TCHAR* InName) const;

	// Returns all current rigvm functions
	const TChunkedArray<FRigVMFunction>& GetFunctions() const;

	// Returns a template pointer given its notation (or nullptr)
	const FRigVMTemplate* FindTemplate(const FName& InNotation, bool bIncludeDeprecated = false) const;

	// Returns all current rigvm functions
	const TChunkedArray<FRigVMTemplate>& GetTemplates() const;

	// Defines and retrieves a template given its arguments
	const FRigVMTemplate* GetOrAddTemplateFromArguments(
		const FName& InName,
		const TArray<FRigVMTemplateArgument>& InArguments,
		const FRigVMTemplateDelegates& InDelegates);

	// Returns a dispatch factory given its name (or nullptr)
	FRigVMDispatchFactory* FindDispatchFactory(const FName& InFactoryName) const;

	// Returns a dispatch factory given its static struct (or nullptr)
	FRigVMDispatchFactory* FindOrAddDispatchFactory(UScriptStruct* InFactoryStruct);

	// Returns all dispatch factories
	const TArray<FRigVMDispatchFactory*>& GetFactories() const;

	static const TArray<UScriptStruct*>& GetMathTypes();

	// Notifies other system that types have been added/removed, and template permutations have been updated
	FOnRigVMRegistryChanged& OnRigVMRegistryChanged() { return OnRigVMRegistryChangedDelegate; }

private:

	static const FName TemplateNameMetaName;

	// disable default constructor
	FRigVMRegistry() {}
	// disable copy constructor
	FRigVMRegistry(const FRigVMRegistry&) = delete;
	// disable assignment operator
	FRigVMRegistry& operator= (const FRigVMRegistry &InOther) = delete;

	struct FTypeInfo
	{
		FTypeInfo()
			: Type()
			, BaseTypeIndex(INDEX_NONE)
			, ArrayTypeIndex(INDEX_NONE)
			, bIsArray(false)
			, bIsExecute(false)
		{}
		
		FRigVMTemplateArgumentType Type;
		TRigVMTypeIndex BaseTypeIndex;
		TRigVMTypeIndex ArrayTypeIndex;
		bool bIsArray;
		bool bIsExecute;
	};

	TRigVMTypeIndex FindOrAddType_Internal(const FRigVMTemplateArgumentType& InType, bool bForce);

	FORCEINLINE static EObjectFlags DisallowedFlags()
	{
		return RF_BeginDestroyed | RF_FinishDestroyed;
	}

	FORCEINLINE static EObjectFlags NeededFlags()
	{
		return RF_Public;
	}

	static bool IsAllowedType(const FProperty* InProperty);
	static bool IsAllowedType(const UEnum* InEnum);
	static bool IsAllowedType(const UStruct* InStruct);
	static bool IsAllowedType(const UClass* InClass);

	void RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory InCategory, TRigVMTypeIndex InTypeIndex);
	
	void RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory InCategory, TRigVMTypeIndex InTypeIndex);

	// memory for all (known) types
	TArray<FTypeInfo> Types;
	TMap<FRigVMTemplateArgumentType, TRigVMTypeIndex> TypeToIndex;

	// memory for all functions
	// We use TChunkedArray because we need the memory locations to be stable, since we only ever add and never remove.
	TChunkedArray<FRigVMFunction> Functions;

	// memory for all non-deprecated templates
	TChunkedArray<FRigVMTemplate> Templates;

	// memory for all deprecated templates
	TChunkedArray<FRigVMTemplate> DeprecatedTemplates;

	// memory for all dispatch factories
	TArray<FRigVMDispatchFactory*> Factories;

	// name lookup for functions
	TMap<FName, int32> FunctionNameToIndex;

	// name lookup for non-deprecated templates
	TMap<FName, int32> TemplateNotationToIndex;

	// name lookup for deprecated templates
	TMap<FName, int32> DeprecatedTemplateNotationToIndex;

	// Maps storing the default types per type category
	TMap<FRigVMTemplateArgument::ETypeCategory, TArray<TRigVMTypeIndex>> TypesPerCategory;

	// Lookup per type category to know which argument to keep in sync
	TMap<FRigVMTemplateArgument::ETypeCategory, TArray<TPair<int32,int32>>> ArgumentsPerCategory;

	// Name loop up for user defined types since they can be deleted.
	// When that happens, it won't be safe to reload deleted assets so only type names are reliable
	TMap<FSoftObjectPath, TRigVMTypeIndex> UserDefinedTypeToIndex;
	
	// Notifies other system that types have been added/removed, and template permutations have been updated
	FOnRigVMRegistryChanged OnRigVMRegistryChangedDelegate;
	
	friend struct FRigVMStruct;
	friend struct FRigVMTemplate;
	friend struct FRigVMTemplateArgument;
};
