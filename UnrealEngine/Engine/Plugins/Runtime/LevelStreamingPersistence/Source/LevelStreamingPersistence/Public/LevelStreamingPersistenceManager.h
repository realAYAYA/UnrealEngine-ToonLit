// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "PropertyBag.h"
#include "Containers/StaticArray.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelStreamingPersistentObjectPropertyBag.h"
#include "LevelStreamingPersistenceModule.h"
#include "LevelStreamingPersistenceSettings.h"
#include "LevelStreamingPersistenceManager.generated.h"

class ULevel;
class UWorld;
class ULevelStreaming;
using FLevelStreamingPersistentPropertyArray = TArray<FCustomPropertyListNode, TInlineAllocator<32>>;

struct FLevelStreamingPersistentObjectPrivateProperties
{
	FLevelStreamingPersistentObjectPrivateProperties() = default;
	FLevelStreamingPersistentObjectPrivateProperties(FLevelStreamingPersistentObjectPrivateProperties&& Other);

	// Initial snapshot used detect changed properties
	FLevelStreamingPersistentObjectPropertyBag Snapshot;

	// Object source class
	FString SourceClassPathName;
	// Payload data containing serialized changed properties
	TArray<uint8> PayloadData;
	// Persistent Properties
	TArray<const FProperty*> PersistentProperties;

	const UClass* GetSourceClass() const { return LoadObject<UClass>(nullptr, *SourceClassPathName); }
	void ForEachPersistentProperty(TFunctionRef<void(const FProperty*)> Func) const;
	bool Sanitize(const ULevelStreamingPersistenceManager& InManager);
	void Serialize(FArchive& Ar);
	friend FArchive& operator<<(FArchive& Ar, FLevelStreamingPersistentObjectPrivateProperties& PrivateProperties);
};

struct FLevelStreamingPersistentObjectPublicProperties
{
	FLevelStreamingPersistentObjectPublicProperties() = default;
	FLevelStreamingPersistentObjectPublicProperties(FLevelStreamingPersistentObjectPublicProperties&& Other);

	// Properties to persist (computed once)
	TArray<const FProperty*> PropertiesToPersist;

	// Object source class
	FString SourceClassPathName;
	// Public properties
	FLevelStreamingPersistentObjectPropertyBag PropertyBag;
	// Persistent Properties
	TArray<const FProperty*> PersistentProperties;

	const UClass* GetSourceClass() const { return LoadObject<UClass>(nullptr, *SourceClassPathName); }
	void ForEachPersistentProperty(TFunctionRef<void(const FProperty*)> Func) const;
	bool Sanitize(const ULevelStreamingPersistenceManager& InManager);
	void Serialize(FArchive& Ar);
	friend FArchive& operator<<(FArchive& Ar, FLevelStreamingPersistentObjectPublicProperties& PublicProperties);
};

struct FLevelStreamingPersistentPropertyValues
{
	bool bIsMakingVisibleCacheValid = false;
	TMap<FString, FLevelStreamingPersistentObjectPrivateProperties> ObjectsPrivatePropertyValues;
	TMap<FString, FLevelStreamingPersistentObjectPublicProperties> ObjectsPublicPropertyValues;

	void Serialize(FArchive& Ar);
	friend FArchive& operator<<(FArchive& Ar, FLevelStreamingPersistentPropertyValues& Properties);
};

// Helper class to access FLevelStreamingPersistentProperty's Properties
UCLASS()
class ULevelStreamingPersistentPropertiesInfo : public UObject
{
	GENERATED_BODY()

public:
	void Initialize();
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	enum EPropertyType
	{
		PropertyType_Public,
		PropertyType_Private,
		PropertyType_Count
	};

	// Configurable persistent properties helper methods
	const UPropertyBag* GetPropertyBagFromClass(EPropertyType InAccessSpecifier, const UClass* InClass) const;
	void ForEachProperty(EPropertyType InAccessSpecifier, const UClass* InClass, TFunctionRef<void(FProperty*)> Func) const;
	bool HasProperties(EPropertyType InAccessSpecifier, const UClass* InClass) const;
	bool HasProperty(EPropertyType InAccessSpecifier, const FProperty* InProperty) const;

private:
	/* Acceleration maps to find properties for a given class */
	TStaticArray<TMap<TObjectPtr<const UClass>, TSet<FProperty*>>, PropertyType_Count> ClassesProperties;
	TStaticArray<TMap<const UClass*, FInstancedPropertyBag>, PropertyType_Count> ObjectClassToPropertyBag;
};

UCLASS()
class LEVELSTREAMINGPERSISTENCE_API ULevelStreamingPersistenceManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin WorldSubsystem
	ULevelStreamingPersistenceManager(const FObjectInitializer&);
	~ULevelStreamingPersistenceManager() {}
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End WorldSubsystem

	// Serialization
	bool SerializeTo(TArray<uint8>& OutPayload);
	bool InitializeFrom(const TArray<uint8>& InPayload);

	// Sets property value and creates the entry if necessary, returns true on success.
	template <typename ClassType, typename PropertyType>
	bool SetPropertyValue(const FString& InObjectPathName, const FName InPropertyName, const PropertyType& InPropertyValue);

	// Sets the property value on existing entries, returns true on success.
	template <typename PropertyType>
	bool TrySetPropertyValue(const FString& InObjectPathName, const FName InPropertyName, const PropertyType& InPropertyValue);

	// Gets the property value if found, returns true on success.
	template<typename PropertyType>
	bool GetPropertyValue(const FString& InObjectPathName, const FName InPropertyName, PropertyType& OutPropertyValue) const;

	// Sets the property value converted from the provided string value on existing entries, returns true on success.
	bool TrySetPropertyValueFromString(const FString& InObjectPathName, const FName InPropertyName, const FString& InPropertyValue);

	// Gets the property value and converts it to a string if found, returns true on success.
	bool GetPropertyValueAsString(const FString& InObjectPathName, const FName InPropertyName, FString& OutPropertyValue);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const;

private:
	// Level streaming visibility callbacks
	void OnLevelBeginMakingInvisible(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel);
	void OnLevelBeginMakingVisible(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel);

	// Save persistent properties
	bool SaveLevelPersistentPropertyValues(const ULevel* InLevel);

	// Restore persistent properties
	bool RestoreLevelPersistentPropertyValues(const ULevel* InLevel) const;
	int32 RestorePrivateProperties(UObject* InObject, const FLevelStreamingPersistentObjectPrivateProperties& InPersistentProperties) const;
	int32 RestorePublicProperties(UObject* InObject, const FLevelStreamingPersistentObjectPublicProperties& InPersistentProperties) const;

	// Snapshot of persistent properties
	bool BuildSnapshot(const UObject* InObject, FLevelStreamingPersistentObjectPropertyBag& OutSnapshot) const;
	bool DiffWithSnapshot(const UObject* InObject, const FLevelStreamingPersistentObjectPropertyBag& InSnapshot, TArray<const FProperty*>& OutChangedProperties) const;

	bool IsEnabled() const { return bIsEnabled; }
	bool SplitObjectPath(const FString& InObjectPathName, FString& OutLevelPathName, FString& OutShortObjectPathName) const;
	const FString GetResolvedObjectPathName(const FString& InObjectPathName) const;
	bool CopyPropertyBagValueToObject(const FLevelStreamingPersistentObjectPropertyBag* InPropertyBag, UObject* InObject, FProperty* InObjectProperty) const;
	TPair<UObject*, FProperty*> GetObjectPropertyPair(const FString& InObjectPathName, const FName InPropertyName) const;
#if !UE_BUILD_SHIPPING
	void DumpContent() const;
#endif
	const FLevelStreamingPersistentObjectPropertyBag* GetPropertyBag(const FString& InObjectPathName) const;
	FLevelStreamingPersistentObjectPropertyBag* GetPropertyBag(const FString& InObjectPathName);
	bool SerializeManager(FArchive& Ar);

	template <typename ClassType>
	FLevelStreamingPersistentObjectPropertyBag* GetOrCreatePropertyBag(const FString& InObjectPathName, const FName InPropertyName);

	// Console commands
	static class FAutoConsoleVariableRef EnableCommand;
	static bool bIsEnabled;
#if !UE_BUILD_SHIPPING
	static class FAutoConsoleCommand SaveToFileCommand;
	static class FAutoConsoleCommand LoadFromFileCommand;
	static class FAutoConsoleCommand DumpContentCommand;
	static class FAutoConsoleCommand SetPropertyValueCommand;
	static class FAutoConsoleCommand GetPropertyValueCommand;
#endif

	// Per-level Persistent property values
	mutable TMap<FString, FLevelStreamingPersistentPropertyValues> LevelsPropertyValues;

	// Persistence Module
	ILevelStreamingPersistenceModule* PersistenceModule = nullptr;

	// Persistent Properties Info
	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingPersistentPropertiesInfo> PersistentPropertiesInfo;

	// Whether to use tagget property serialization
	bool bUseTaggedPropertySerialization;

	friend struct FLevelStreamingPersistentObjectPrivateProperties;
	friend struct FLevelStreamingPersistentObjectPublicProperties;
	friend class FPersistenceModule;
};

template <typename PropertyType>
bool ULevelStreamingPersistenceManager::TrySetPropertyValue(const FString& InObjectPathName, const FName InPropertyName, const PropertyType& InPropertyValue)
{
	const FString ObjectPathName = GetResolvedObjectPathName(InObjectPathName);
	FLevelStreamingPersistentObjectPropertyBag* PropertyBag = GetPropertyBag(ObjectPathName);
	if (PropertyBag && PropertyBag->SetPropertyValue(InPropertyName, InPropertyValue))
	{
		// If object is loaded, copy property value to object
		auto ObjectPropertyPair = GetObjectPropertyPair(ObjectPathName, InPropertyName);
		CopyPropertyBagValueToObject(PropertyBag, ObjectPropertyPair.Key, ObjectPropertyPair.Value);
		return true;
	}
	return false;
}

template <typename ClassType, typename PropertyType>
bool ULevelStreamingPersistenceManager::SetPropertyValue(const FString& InObjectPathName, const FName InPropertyName, const PropertyType& InPropertyValue)
{
	const FString ObjectPathName = GetResolvedObjectPathName(InObjectPathName);
	FLevelStreamingPersistentObjectPropertyBag* PropertyBag = GetOrCreatePropertyBag<ClassType>(ObjectPathName, InPropertyName);
	if (PropertyBag && PropertyBag->SetPropertyValue(InPropertyName, InPropertyValue))
	{
		// If object is loaded, copy property value to object
		auto ObjectPropertyPair = GetObjectPropertyPair(ObjectPathName, InPropertyName);
		CopyPropertyBagValueToObject(PropertyBag, ObjectPropertyPair.Key, ObjectPropertyPair.Value);
		return true;
	}
	return false;
}

template<typename PropertyType>
bool ULevelStreamingPersistenceManager::GetPropertyValue(const FString& InObjectPathName, const FName InPropertyName, PropertyType& OutPropertyValue) const
{
	const FString ObjectPathName = GetResolvedObjectPathName(InObjectPathName);
	const FLevelStreamingPersistentObjectPropertyBag* PropertyBag = GetPropertyBag(ObjectPathName);
	return PropertyBag ? PropertyBag->GetPropertyValue(InPropertyName, OutPropertyValue) : false;
}

template <typename ClassType>
FLevelStreamingPersistentObjectPropertyBag* ULevelStreamingPersistenceManager::GetOrCreatePropertyBag(const FString& InObjectPathName, const FName InPropertyName)
{
	const FString ObjectPathName = GetResolvedObjectPathName(InObjectPathName);
	const UClass* ObjectClass = ClassType::StaticClass();

	FString LevelPathName;
	FString ObjectShortPathName;
	if (!SplitObjectPath(ObjectPathName, LevelPathName, ObjectShortPathName))
	{
		return nullptr;
	}

	if (FProperty* ObjectProperty = ObjectClass->FindPropertyByName(InPropertyName))
	{
		if (PersistentPropertiesInfo->HasProperty(ULevelStreamingPersistentPropertiesInfo::PropertyType_Public, ObjectProperty))
		{
			FLevelStreamingPersistentPropertyValues& LevelProperties = LevelsPropertyValues.FindOrAdd(LevelPathName);

			FLevelStreamingPersistentObjectPublicProperties& ObjectPublicPropertyValues = LevelProperties.ObjectsPublicPropertyValues.FindOrAdd(ObjectPathName);
			if (ObjectPublicPropertyValues.PropertyBag.Initialize([this, ObjectClass]() { return PersistentPropertiesInfo->GetPropertyBagFromClass(ULevelStreamingPersistentPropertiesInfo::PropertyType_Public, ObjectClass); }))
			{
				return &ObjectPublicPropertyValues.PropertyBag;
			}
		}
	}
	return nullptr;
}
