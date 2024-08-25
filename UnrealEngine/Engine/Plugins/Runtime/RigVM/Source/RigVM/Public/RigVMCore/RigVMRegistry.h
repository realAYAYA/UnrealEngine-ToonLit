// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ChunkedArray.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "RigVMCore/RigVMTraits.h"
#include "RigVMDispatchFactory.h"
#include "RigVMFunction.h"
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
class IPlugin;
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

	virtual ~FRigVMRegistry() override;
	
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

	// Register a predicate contained in the input struct
	void RegisterPredicate(UScriptStruct* InStruct, const TCHAR* InName, const TArray<FRigVMFunctionArgument>& InArguments);

	// How to register an object's class when passed to RegisterObjectTypes
	enum class ERegisterObjectOperation
	{
		Class,

		ClassAndParents,

		ClassAndChildren,
	};

	// Register a set of allowed object types
	void RegisterObjectTypes(TConstArrayView<TPair<UClass*, ERegisterObjectOperation>> InClasses);

	// Refreshes the list and finds the function pointers
	// based on the names.
	void RefreshEngineTypes();

	// Refreshes the list and finds the function pointers
	// based on the names.
	void RefreshEngineTypesIfRequired();

	// Update the registry when types are renamed
	void OnAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath);
	
	// Update the registry when old types are removed
    void OnAssetRemoved(const FAssetData& InAssetData);

	// Removes all types associated with a plugin that's being unloaded. 
	void OnPluginUnloaded(IPlugin& InPlugin);
	
	// Update the registry when new types are added to the attribute system so that they can be selected
	// on Attribute Nodes
	void OnAnimationAttributeTypesChanged(const UScriptStruct* InStruct, bool bIsAdded);
	
	// Clear the registry
	void Reset();

	// Adds a type if it doesn't exist yet and returns its index.
	// This function is thread-safe
	TRigVMTypeIndex FindOrAddType(const FRigVMTemplateArgumentType& InType, bool bForce = false);

	// Removes a type from the registry, and updates all dependent templates
	// which also creates invalid permutations in templates that we should ignore
	bool RemoveType(const FSoftObjectPath& InObjectPath, const UClass* InObjectClass);

	// Returns the type index given a type
	TRigVMTypeIndex GetTypeIndex(const FRigVMTemplateArgumentType& InType) const;

	// Returns the type index given a cpp type and a type object
	TRigVMTypeIndex GetTypeIndex(const FName& InCPPType, UObject* InCPPTypeObject) const
	{
		return GetTypeIndex(FRigVMTemplateArgumentType(InCPPType, InCPPTypeObject));
	}

	// Returns the type index given an enum
	template <
		typename T,
		typename TEnableIf<TIsEnum<T>::Value>::Type* = nullptr
	>
	TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
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
	TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
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
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
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
		typename TEnableIf<TModels_V<CRigVMUClass, T>>::Type * = nullptr
	>
	TRigVMTypeIndex GetTypeIndex(bool bAsArray = false) const
	{
		FRigVMTemplateArgumentType Type(T::StaticClass(), RigVMTypeUtils::EClassArgType::AsObject);
		if(bAsArray)
		{
			Type.ConvertToArray();
		}
		return GetTypeIndex(Type);
	}

	// Returns the type given its index
	const FRigVMTemplateArgumentType& GetType(TRigVMTypeIndex InTypeIndex) const;

	// Returns the number of types
	int32 NumTypes() const { return Types.Num(); }

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
	const FRigVMFunction* FindFunction(const TCHAR* InName, const FRigVMUserDefinedTypeResolver& InTypeResolver = FRigVMUserDefinedTypeResolver()) const;

	// Returns the function given its backing up struct and method name
	const FRigVMFunction* FindFunction(UScriptStruct* InStruct, const TCHAR* InName, const FRigVMUserDefinedTypeResolver& InResolvalInfo = FRigVMUserDefinedTypeResolver()) const;

	// Returns all current rigvm functions
	const TChunkedArray<FRigVMFunction>& GetFunctions() const;

	// Returns a template pointer given its notation (or nullptr)
	const FRigVMTemplate* FindTemplate(const FName& InNotation, bool bIncludeDeprecated = false) const;

	// Returns all current rigvm functions
	const TChunkedArray<FRigVMTemplate>& GetTemplates() const;

	// Defines and retrieves a template given its arguments
	const FRigVMTemplate* GetOrAddTemplateFromArguments(
		const FName& InName,
		const TArray<FRigVMTemplateArgumentInfo>& InInfos,
		const FRigVMTemplateDelegates& InDelegates);

	// Adds a new template given its arguments
	const FRigVMTemplate* AddTemplateFromArguments(
		const FName& InName,
		const TArray<FRigVMTemplateArgumentInfo>& InInfos,
		const FRigVMTemplateDelegates& InDelegates);

	// Returns a dispatch factory given its name (or nullptr)
	FRigVMDispatchFactory* FindDispatchFactory(const FName& InFactoryName) const;

	// Returns a dispatch factory given its static struct (or nullptr)
	FRigVMDispatchFactory* FindOrAddDispatchFactory(UScriptStruct* InFactoryStruct);

	// Returns a dispatch factory given its static struct (or nullptr)
	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	FRigVMDispatchFactory* FindOrAddDispatchFactory()
	{
		return FindOrAddDispatchFactory(T::StaticStruct());
	}

	// Returns a dispatch factory's singleton function name if that exists
	FString FindOrAddSingletonDispatchFunction(UScriptStruct* InFactoryStruct);

	// Returns a dispatch factory's singleton function name if that exists
	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	FString FindOrAddSingletonDispatchFunction()
	{
		return FindOrAddSingletonDispatchFunction(T::StaticStruct());
	}

	// Returns all dispatch factories
	const TArray<FRigVMDispatchFactory*>& GetFactories() const;

	// Given a struct name, return the predicates
	const TArray<FRigVMFunction>* GetPredicatesForStruct(const FName& InStructName) const;

	static const TArray<UScriptStruct*>& GetMathTypes();

	// Notifies other system that types have been added/removed, and template permutations have been updated
	FOnRigVMRegistryChanged& OnRigVMRegistryChanged() { return OnRigVMRegistryChangedDelegate; }

	// Returns a unique hash per type index
	uint32 GetHashForType(TRigVMTypeIndex InTypeIndex) const;
	uint32 GetHashForScriptStruct(const UScriptStruct* InScriptStruct, bool bCheckTypeIndex = true) const;
	uint32 GetHashForStruct(const UStruct* InStruct) const;
	uint32 GetHashForEnum(const UEnum* InEnum, bool bCheckTypeIndex = true) const;
	uint32 GetHashForProperty(const FProperty* InProperty) const;

private:

	static const FName TemplateNameMetaName;

	FRigVMRegistry();

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
			, Hash(UINT32_MAX)
		{}
		
		FRigVMTemplateArgumentType Type;
		TRigVMTypeIndex BaseTypeIndex;
		TRigVMTypeIndex ArrayTypeIndex;
		bool bIsArray;
		bool bIsExecute;
		uint32 Hash;
	};

	// Initialize the base types
	void Initialize();
	
	TRigVMTypeIndex FindOrAddType_NoLock(const FRigVMTemplateArgumentType& InType, bool bForce);

	void RefreshEngineTypes_NoLock();

	static EObjectFlags DisallowedFlags()
	{
		return RF_BeginDestroyed | RF_FinishDestroyed;
	}

	static EObjectFlags NeededFlags()
	{
		return RF_Public;
	}

	bool IsAllowedType(const FProperty* InProperty) const;
	bool IsAllowedType(const UEnum* InEnum) const;
	bool IsAllowedType(const UStruct* InStruct) const;
	bool IsAllowedType(const UClass* InClass) const;

	void RegisterTypeInCategory(const FRigVMTemplateArgument::ETypeCategory InCategory, const TRigVMTypeIndex InTypeIndex);
	void PropagateTypeAddedToCategory(const FRigVMTemplateArgument::ETypeCategory InCategory, const TRigVMTypeIndex InTypeIndex);
	
	void RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory InCategory, TRigVMTypeIndex InTypeIndex);

	const FRigVMFunction* FindFunction_NoLock(const TCHAR* InName, const FRigVMUserDefinedTypeResolver& InTypeResolver = FRigVMUserDefinedTypeResolver()) const;
	const FRigVMTemplate* FindTemplate_NoLock(const FName& InNotation, bool bIncludeDeprecated) const;
	FRigVMDispatchFactory* FindDispatchFactory_NoLock(const FName& InFactoryName) const;

	const FRigVMTemplate* AddTemplateFromArguments_NoLock(
		const FName& InName,
		const TArray<FRigVMTemplateArgumentInfo>& InInfos,
		const FRigVMTemplateDelegates& InDelegates);

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

	// lookup all the predicate functions of this struct
	TMap<FName, TArray<FRigVMFunction>> StructNameToPredicates;

	// name lookup for non-deprecated templates
	TMap<FName, int32> TemplateNotationToIndex;

	// name lookup for deprecated templates
	TMap<FName, int32> DeprecatedTemplateNotationToIndex;

	// Maps storing the default types per type category
	TMap<FRigVMTemplateArgument::ETypeCategory, TArray<TRigVMTypeIndex>> TypesPerCategory;

	// Lookup per type category to know which template to keep in sync
	TMap<FRigVMTemplateArgument::ETypeCategory, TArray<int32>> TemplatesPerCategory;

	// Name loop up for user defined types since they can be deleted.
	// When that happens, it won't be safe to reload deleted assets so only type names are reliable
	TMap<FSoftObjectPath, TRigVMTypeIndex> UserDefinedTypeToIndex;
	
	// All allowed classes
	TSet<TObjectPtr<const UClass>> AllowedClasses;

	// Notifies other system that types have been added/removed, and template permutations have been updated
	FOnRigVMRegistryChanged OnRigVMRegistryChangedDelegate;

	// If this is true the registry is currently refreshing all types
	bool bIsRefreshingEngineTypes;

	// This is true if the engine has ever refreshed the engine types
	bool bEverRefreshedEngineTypes;

	static FCriticalSection FindOrAddTypeMutex;
	static FCriticalSection FunctionRegistryMutex;
	static FCriticalSection FactoryRegistryMutex;
	static FCriticalSection TemplateRegistryMutex;

	static FCriticalSection DispatchFunctionMutex;
	static FCriticalSection DispatchPredicatesMutex;
	
	friend struct FRigVMStruct;
	friend struct FRigVMTemplate;
	friend struct FRigVMTemplateArgument;
	friend struct FRigVMDispatchFactory;
};
