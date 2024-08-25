// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelStreamingPersistenceManager.h"
#include "LevelStreamingPersistenceModule.h"
#include "LevelStreamingPersistenceSettings.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "PropertyBag.h"
#include "PropertyPathHelpers.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/Package.h"

#if !UE_BUILD_SHIPPING
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLevelStreamingPersistenceManager, All, All);

/*
 * Achives used by ULevelStreamingPersistenceManager
 */

// By default, LevelStreamingPersistenceManager uses tagged property serialization, which is more flexible as it
// allows to load a previously serialized snapshot of ULevelStreamingPersistenceManager even if 
// LevelStreamingPersistenceSettings Properties has changed afterwards.
// Setting this flag to false will use less memory, with the disadvantage of not being resilient to changes
static const bool GUseTaggedPropertySerialization = true;

class FPersistentPropertiesArchive : public FObjectAndNameAsStringProxyArchive
{
public:
	FPersistentPropertiesArchive(FArchive& InArchive, const TArray<const FProperty*>& InPersistentProperties)
		: FObjectAndNameAsStringProxyArchive(InArchive, /*bLoadIfFindFails*/false)
		, PersistentProperties(InPersistentProperties)
	{
		check(InArchive.IsPersistent());
		check(InArchive.IsFilterEditorOnly());
		check(InArchive.ShouldSkipBulkData());
		SetIsLoading(InArchive.IsLoading());
		SetIsSaving(InArchive.IsSaving());
		SetIsTextFormat(InArchive.IsTextFormat());
		SetWantBinaryPropertySerialization(InArchive.WantBinaryPropertySerialization());
		SetIsPersistent(true);
		FArchiveProxy::SetFilterEditorOnly(true);
		ArShouldSkipBulkData = true;

		if (WantBinaryPropertySerialization())
		{
			// Custom property lists only work with binary serialization, not tagged property serialization.
			BuildSerializedPropertyList();
			ArUseCustomPropertyList = !CustomPropertyList.IsEmpty();
			ArCustomPropertyList = ArUseCustomPropertyList ? &CustomPropertyList[0] : nullptr;
		}
	}

	virtual bool ShouldSkipProperty(const FProperty* InProperty) const
	{
		if (FObjectAndNameAsStringProxyArchive::ShouldSkipProperty(InProperty))
		{
			return true;
		}

		if (!ArUseCustomPropertyList)
		{
			return !PersistentProperties.Contains(InProperty);
		}

		return false;
	}

	virtual FArchive& operator<<(FLazyObjectPtr& Value) override { return FArchiveUObject::SerializeLazyObjectPtr(*this, Value); }

private:

	void BuildSerializedPropertyList()
	{
		for (const FProperty* Property : PersistentProperties)
		{
			CustomPropertyList.Emplace(const_cast<FProperty*>(Property));
		}

		// Link changed properties
		if (!CustomPropertyList.IsEmpty())
		{
			for (int i = 0; i < CustomPropertyList.Num() - 1; ++i)
			{
				CustomPropertyList[i].PropertyListNext = &CustomPropertyList[i + 1];
			}
		}
	}

	const TArray<const FProperty*>& PersistentProperties;
	FLevelStreamingPersistentPropertyArray CustomPropertyList;
};

class FPersistentPropertiesWriter : public FMemoryWriter
{
public:
	FPersistentPropertiesWriter(bool bInUseTaggedPropertySerialization, TArray<uint8, TSizedDefaultAllocator<32>>& InBytes)
		: FMemoryWriter(InBytes, true)
	{
		SetFilterEditorOnly(true);
		ArShouldSkipBulkData = true;
		SetIsTextFormat(false);
		SetWantBinaryPropertySerialization(!bInUseTaggedPropertySerialization);
	}
};

class FPersistentPropertiesReader : public FMemoryReader
{
public:
	FPersistentPropertiesReader(bool bInUseTaggedPropertySerialization, const TArray<uint8>& InBytes)
		: FMemoryReader(InBytes, true)
	{
		SetFilterEditorOnly(true);
		ArShouldSkipBulkData = true;
		SetIsTextFormat(false);
		SetWantBinaryPropertySerialization(!bInUseTaggedPropertySerialization);
	}
};

/*
 * ULevelStreamingPersistenceManager implementation
 */

ULevelStreamingPersistenceManager::ULevelStreamingPersistenceManager(const FObjectInitializer& ObjectInitializer)
	: PersistenceModule(&ILevelStreamingPersistenceModule::Get())
	, bUseTaggedPropertySerialization(GUseTaggedPropertySerialization)
{
}

bool ULevelStreamingPersistenceManager::DoesSupportWorldType(const EWorldType::Type InWorldType) const
{
	return InWorldType == EWorldType::PIE || InWorldType == EWorldType::Game;
}

bool ULevelStreamingPersistenceManager::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!UWorldSubsystem::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	UWorld* World = Cast<UWorld>(Outer);
	if (!World || !World->IsGameWorld() || (World->GetNetMode() == NM_Client))
	{
		return false;
	}

	UWorldPartition* WorldPartition = World->GetWorldPartition();
	// @todo_ow: Make UWorldPartition::IsStreamingEnabled not dependant of WorldPartition's World member
	if (WorldPartition && (!WorldPartition->bEnableStreaming || !World->GetWorldSettings()->SupportsWorldPartitionStreaming()))
	{
		return false;
	}

	return true;
}

void ULevelStreamingPersistenceManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FLevelStreamingDelegates::OnLevelBeginMakingInvisible.AddUObject(this, &ULevelStreamingPersistenceManager::OnLevelBeginMakingInvisible);
	FLevelStreamingDelegates::OnLevelBeginMakingVisible.AddUObject(this, &ULevelStreamingPersistenceManager::OnLevelBeginMakingVisible);

	PersistentPropertiesInfo = NewObject<ULevelStreamingPersistentPropertiesInfo>(this);
	PersistentPropertiesInfo->Initialize();
}

void ULevelStreamingPersistenceManager::Deinitialize()
{
	FLevelStreamingDelegates::OnLevelBeginMakingInvisible.RemoveAll(this);
	FLevelStreamingDelegates::OnLevelBeginMakingVisible.RemoveAll(this);
}

bool ULevelStreamingPersistenceManager::TrySetPropertyValueFromString(const FString& InObjectPathName, const FName InPropertyName, const FString& InPropertyValue)
{
	const FString ObjectPathName = GetResolvedObjectPathName(InObjectPathName);
	FLevelStreamingPersistentObjectPropertyBag* PropertyBag = GetPropertyBag(ObjectPathName);
	if (PropertyBag && PropertyBag->SetPropertyValueFromString(InPropertyName, InPropertyValue))
	{
		// If object is loaded, copy property value to object
		auto ObjectPropertyPair = GetObjectPropertyPair(ObjectPathName, InPropertyName);
		CopyPropertyBagValueToObject(PropertyBag, ObjectPropertyPair.Key, ObjectPropertyPair.Value);
		return true;
	}
	return false;
}

bool ULevelStreamingPersistenceManager::GetPropertyValueAsString(const FString& InObjectPathName, const FName InPropertyName, FString& OutPropertyValue)
{
	const FString ObjectPathName = GetResolvedObjectPathName(InObjectPathName);
	if (FLevelStreamingPersistentObjectPropertyBag* PropertyBag = GetPropertyBag(ObjectPathName))
	{
		return PropertyBag->GetPropertyValueAsString(InPropertyName, OutPropertyValue);
	}
	return false;
}

void ULevelStreamingPersistenceManager::OnLevelBeginMakingInvisible(UWorld* World, const ULevelStreaming* InStreamingLevel, ULevel* InLoadedLevel)
{
	if (IsValid(InLoadedLevel) && (World == GetWorld()) && IsEnabled())
	{
		SaveLevelPersistentPropertyValues(InLoadedLevel);
	}
}

bool ULevelStreamingPersistenceManager::SaveLevelPersistentPropertyValues(const ULevel * InLevel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULevelStreamingPersistenceManager::SaveLevelPersistentPropertyValues);

	check(IsValid(InLevel) && IsEnabled());
	const FString LevelPathName = InLevel->GetPathName();
	FLevelStreamingPersistentPropertyValues& LevelProperties = LevelsPropertyValues.FindOrAdd(LevelPathName);

	auto SavePrivateProperties = [this, &LevelProperties]()
	{
		int32 SavedCount = 0;
		for (auto& [ObjectPath, ObjectPrivatePropertyValues] : LevelProperties.ObjectsPrivatePropertyValues)
		{
			if (const UObject* Object = FindObject<UObject>(nullptr, *ObjectPath); IsValid(Object))
			{
				ObjectPrivatePropertyValues.PayloadData.Reset();
				ObjectPrivatePropertyValues.PersistentProperties.Reset();
				if (DiffWithSnapshot(Object, ObjectPrivatePropertyValues.Snapshot, ObjectPrivatePropertyValues.PersistentProperties))
				{
					FPersistentPropertiesWriter WriterAr(bUseTaggedPropertySerialization, ObjectPrivatePropertyValues.PayloadData);
					FPersistentPropertiesArchive Ar(WriterAr, ObjectPrivatePropertyValues.PersistentProperties);
					Object->SerializeScriptProperties(Ar);
					SavedCount += (ObjectPrivatePropertyValues.PersistentProperties.Num() > 0) ? 1 : 0;
				}
			}
		}
		return SavedCount;
	};

	auto SavePublicProperties = [this, &LevelProperties]()
	{
		int32 SavedCount = 0;
		for (auto& [ObjectPath, ObjectPublicPropertyValues] : LevelProperties.ObjectsPublicPropertyValues)
		{
			if (const UObject* Object = FindObject<UObject>(nullptr, *ObjectPath); IsValid(Object))
			{
				if (ensure(ObjectPublicPropertyValues.PropertyBag.IsValid()))
				{
					ObjectPublicPropertyValues.PersistentProperties.Reset();
					for (const FProperty* BagProperty : ObjectPublicPropertyValues.PropertiesToPersist)
					{
						if (FProperty* ObjectProperty = Object->GetClass()->FindPropertyByName(BagProperty->GetFName()))
						{
							if (ObjectPublicPropertyValues.PropertyBag.CopyPropertyValueFromObject(Object, ObjectProperty))
							{
								ObjectPublicPropertyValues.PersistentProperties.Add(BagProperty);
								++SavedCount;
							}
						}
					}
				}
			}
		}
		return SavedCount;
	};

	const int32 SavedCount = SavePrivateProperties() + SavePublicProperties();
	return SavedCount > 0;
}

void ULevelStreamingPersistenceManager::OnLevelBeginMakingVisible(UWorld* World, const ULevelStreaming* InStreamingLevel, ULevel* InLoadedLevel)
{
	if (IsValid(InLoadedLevel) && (World == GetWorld()) && IsEnabled())
	{
		RestoreLevelPersistentPropertyValues(InLoadedLevel);
	}
}

bool ULevelStreamingPersistenceManager::RestoreLevelPersistentPropertyValues(const ULevel* InLevel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULevelStreamingPersistenceManager::RestoreLevelPersistentPropertyValues);

	check(IsValid(InLevel) && IsEnabled());

	auto CreatePrivatePropertiesSnapshot = [this](UObject* Object, FLevelStreamingPersistentPropertyValues& LevelProperties)
	{
		int CreatedCount = 0;
		const UClass* ObjectClass = Object->GetClass();
		if (PersistentPropertiesInfo->HasProperties(ULevelStreamingPersistentPropertiesInfo::PropertyType_Private, ObjectClass))
		{
			FLevelStreamingPersistentObjectPrivateProperties* ObjectPrivatePropertyValues = LevelProperties.ObjectsPrivatePropertyValues.Find(Object->GetPathName());
			if (!ObjectPrivatePropertyValues || !ObjectPrivatePropertyValues->Snapshot.IsValid())
			{
				if (!ObjectPrivatePropertyValues)
				{
					ObjectPrivatePropertyValues = &LevelProperties.ObjectsPrivatePropertyValues.Add(Object->GetPathName());
					ObjectPrivatePropertyValues->SourceClassPathName = ObjectClass->GetPathName();
				}
			}
			
			if (!ObjectPrivatePropertyValues->Snapshot.IsValid() && BuildSnapshot(Object, ObjectPrivatePropertyValues->Snapshot))
			{
				++CreatedCount;
			}
			
			// Clean-up private entries
			if (!ObjectPrivatePropertyValues->Snapshot.IsValid())
			{
				LevelProperties.ObjectsPrivatePropertyValues.Remove(Object->GetPathName());
			}
		}
		return CreatedCount;
	};

	auto CreatePublicPropertiesEntry = [this](const UObject* Object, FLevelStreamingPersistentPropertyValues& LevelProperties)
	{
		int32 CreatedCount = 0;
		check(IsValid(Object));
		const UClass* ObjectClass = Object->GetClass();

		if (PersistentPropertiesInfo->HasProperties(ULevelStreamingPersistentPropertiesInfo::PropertyType_Public, ObjectClass))
		{
			FLevelStreamingPersistentObjectPublicProperties* ObjectPublicPropertyValues = LevelProperties.ObjectsPublicPropertyValues.Find(Object->GetPathName());

			// Build PropertiesToPersist if necessary
			TArray<FProperty*, TInlineAllocator<32>> SerializedObjectProperties;
			if (!ObjectPublicPropertyValues || ObjectPublicPropertyValues->PropertiesToPersist.IsEmpty())
			{
				PersistentPropertiesInfo->ForEachProperty(ULevelStreamingPersistentPropertiesInfo::PropertyType_Public, ObjectClass, [this, Object, &SerializedObjectProperties](FProperty* ObjectProperty)
				{
					if (PersistenceModule->ShouldPersistProperty(Object, ObjectProperty))
					{
						SerializedObjectProperties.Add(ObjectProperty);
					}
				});

				if (!SerializedObjectProperties.IsEmpty())
				{
					check(!ObjectPublicPropertyValues || ObjectPublicPropertyValues->PropertyBag.IsValid());
					if (!ObjectPublicPropertyValues)
					{
						// Create entry if necessary and initialize PropertyBag
						ObjectPublicPropertyValues = &LevelProperties.ObjectsPublicPropertyValues.Add(Object->GetPathName());
						ObjectPublicPropertyValues->SourceClassPathName = ObjectClass->GetPathName();
						ObjectPublicPropertyValues->PropertyBag.Initialize([this, ObjectClass]() { return PersistentPropertiesInfo->GetPropertyBagFromClass(ULevelStreamingPersistentPropertiesInfo::PropertyType_Public, ObjectClass); });
					}
					if (ObjectPublicPropertyValues->PropertyBag.IsValid())
					{
						check(ObjectPublicPropertyValues->PropertiesToPersist.IsEmpty());
						for (FProperty* ObjectProperty : SerializedObjectProperties)
						{
							if (const FProperty* BagProperty = ObjectPublicPropertyValues->PropertyBag.GetCompatibleProperty(ObjectProperty))
							{
								ObjectPublicPropertyValues->PropertiesToPersist.Add(BagProperty);
								++CreatedCount;
							}
						}
					}
				}
			}

			// Clean-up public entries
			if (ObjectPublicPropertyValues && (!ObjectPublicPropertyValues->PropertyBag.IsValid() || ObjectPublicPropertyValues->PropertiesToPersist.IsEmpty()))
			{
				LevelProperties.ObjectsPublicPropertyValues.Remove(Object->GetPathName());
			}
		}
		return CreatedCount;
	};

	int32 CreatedCount = 0;
	const FString LevelPathName = InLevel->GetPathName();
	FLevelStreamingPersistentPropertyValues* LevelProperties = LevelsPropertyValues.Find(LevelPathName);
	const bool bIsNewLevelEntry = (LevelProperties == nullptr);

	// The first time, create a snapshot of persistent properties
	if (bIsNewLevelEntry || !LevelProperties->bIsMakingVisibleCacheValid)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ULevelStreamingPersistenceManager::CreateLevelPropertiesEntries);

		FLevelStreamingPersistentPropertyValues& NewLevelProperties = LevelsPropertyValues.FindOrAdd(LevelPathName);
		LevelProperties = &NewLevelProperties;

		TSet<FString> ValidObjects;
		ForEachObjectWithOuter(InLevel, [this, bIsNewLevelEntry, &NewLevelProperties, &ValidObjects, &CreatedCount, &CreatePrivatePropertiesSnapshot, &CreatePublicPropertiesEntry](UObject* Object)
		{
			int32 EntryCreatedCount = CreatePrivatePropertiesSnapshot(Object, NewLevelProperties);
			EntryCreatedCount += CreatePublicPropertiesEntry(Object, NewLevelProperties);
			if (!bIsNewLevelEntry && (EntryCreatedCount > 0))
			{
				ValidObjects.Add(Object->GetPathName());
			}
			CreatedCount += EntryCreatedCount;
		}, true, RF_NoFlags, EInternalObjectFlags::Garbage);

		// Clean-up object entries that don't exist in the level
		if (ValidObjects.Num() > 0)
		{
			for (auto It = LevelProperties->ObjectsPrivatePropertyValues.CreateIterator(); It; ++It)
			{
				if (!ValidObjects.Contains(It.Key()))
				{
					It.RemoveCurrent();
				}
			}
			for (auto It = LevelProperties->ObjectsPublicPropertyValues.CreateIterator(); It; ++It)
			{
				if (!ValidObjects.Contains(It.Key()))
				{
					It.RemoveCurrent();
				}
			}
		}

		// Make level entry's as valid
		LevelProperties->bIsMakingVisibleCacheValid = true;
	}

	// Restore persistent properties
	int32 RestoredCount = 0;
	if (LevelProperties)
	{
		for (const auto& [ObjectPath, ObjectPrivatePropertyValues] : LevelProperties->ObjectsPrivatePropertyValues)
		{
			if (UObject* Object = FindObject<UObject>(nullptr, *ObjectPath))
			{
				RestoredCount += RestorePrivateProperties(Object, ObjectPrivatePropertyValues);
			}
		}
		for (const auto& [ObjectPath, ObjectPublicPropertyValues] : LevelProperties->ObjectsPublicPropertyValues)
		{
			if (UObject* Object = FindObject<UObject>(nullptr, *ObjectPath))
			{
				RestoredCount += RestorePublicProperties(Object, ObjectPublicPropertyValues);
			}
		}
	}
	
	return ((RestoredCount + CreatedCount) > 0);
}

int32 ULevelStreamingPersistenceManager::RestorePrivateProperties(UObject* Object, const FLevelStreamingPersistentObjectPrivateProperties& PersistentObjectProperties) const
{
	int32 RestoredCount = 0;
	check(IsValid(Object));

	// Restore private properties
	if (!PersistentObjectProperties.PayloadData.IsEmpty())
	{
		FPersistentPropertiesReader ReaderAr(bUseTaggedPropertySerialization, PersistentObjectProperties.PayloadData);
		FPersistentPropertiesArchive Ar(ReaderAr, PersistentObjectProperties.PersistentProperties);
		Object->SerializeScriptProperties(Ar);
		PersistentObjectProperties.ForEachPersistentProperty([this, Object](const FProperty* ObjectProperty)
		{
			PersistenceModule->PostRestorePersistedProperty(Object, ObjectProperty);
		});
		++RestoredCount;
	}
	return RestoredCount;
};

int32 ULevelStreamingPersistenceManager::RestorePublicProperties(UObject* Object, const FLevelStreamingPersistentObjectPublicProperties& ObjectsPublicPropertyValues) const
{
	int RestoredCount = 0;
	check(IsValid(Object));
	const UClass* ObjectClass = Object->GetClass();
	// Restore public properties
	if (ObjectsPublicPropertyValues.PropertyBag.IsValid())
	{
		ObjectsPublicPropertyValues.ForEachPersistentProperty([this, Object, ObjectClass, &ObjectsPublicPropertyValues, &RestoredCount](const FProperty* BagProperty)
		{
			FProperty* ObjectProperty = ObjectClass->FindPropertyByName(BagProperty->GetFName());
			if (CopyPropertyBagValueToObject(&ObjectsPublicPropertyValues.PropertyBag, Object, ObjectProperty))
			{
				++RestoredCount;
			}
		});
	}
	return RestoredCount;
};

FLevelStreamingPersistentObjectPropertyBag* ULevelStreamingPersistenceManager::GetPropertyBag(const FString& InObjectPathName)
{
	FString LevelPathName;
	FString ObjectShortPathName;
	if (!SplitObjectPath(InObjectPathName, LevelPathName, ObjectShortPathName))
	{
		return nullptr;
	}
	FLevelStreamingPersistentPropertyValues* LevelProperties = LevelsPropertyValues.Find(LevelPathName);
	FLevelStreamingPersistentObjectPublicProperties* ObjectPublicPropertyValues = LevelProperties ? LevelProperties->ObjectsPublicPropertyValues.Find(InObjectPathName) : nullptr;
	return ObjectPublicPropertyValues ? &ObjectPublicPropertyValues->PropertyBag : nullptr;
}

const FLevelStreamingPersistentObjectPropertyBag* ULevelStreamingPersistenceManager::GetPropertyBag(const FString& InObjectPathName) const
{
	return const_cast<ULevelStreamingPersistenceManager*>(this)->GetPropertyBag(InObjectPathName);
}

bool ULevelStreamingPersistenceManager::CopyPropertyBagValueToObject(const FLevelStreamingPersistentObjectPropertyBag* InPropertyBag, UObject* InObject, FProperty* InObjectProperty) const
{
	if (InPropertyBag->CopyPropertyValueToObject(InObject, InObjectProperty))
	{
		PersistenceModule->PostRestorePersistedProperty(InObject, InObjectProperty);
		return true;
	}
	return false;
}

TPair<UObject*, FProperty*> ULevelStreamingPersistenceManager::GetObjectPropertyPair(const FString& InObjectPathName, const FName InPropertyName) const
{
	UObject* Object = FindObject<UObject>(nullptr, *InObjectPathName);
	FProperty* ObjectProperty = ::IsValid(Object) ? Object->GetClass()->FindPropertyByName(InPropertyName) : nullptr;
	return TPair<UObject*, FProperty*>(Object, ObjectProperty);
}

const FString ULevelStreamingPersistenceManager::GetResolvedObjectPathName(const FString& InObjectPathName) const
{
	if (UWorldPartition* WorldPartition = GetWorld()->GetWorldPartition())
	{
		const FSoftObjectPath Path(InObjectPathName);
		const FSoftObjectPath WorldPath(Path.GetAssetPath(), FString());
		const UWorld* World = Cast<UWorld>(WorldPath.ResolveObject());
		if (World && (World == GetWorld()))
		{
			FSoftObjectPath ResolvedPath;
			if (FWorldPartitionHelpers::ConvertEditorPathToRuntimePath(FSoftObjectPath(InObjectPathName), ResolvedPath))
			{
				return ResolvedPath.ToString();
			}
		}
	}
	return InObjectPathName;
}

bool ULevelStreamingPersistenceManager::SplitObjectPath(const FString& InObjectPathName, FString& OutLevelPathName, FString& OutShortObjectPathName) const
{
	FSoftObjectPath ObjectPath(InObjectPathName);
	if (ObjectPath.GetSubPathString().StartsWith(TEXT("PersistentLevel.")))
	{
		TStringBuilder<FName::StringBufferSize> Builder;
		Builder << ObjectPath.GetAssetPath() << SUBOBJECT_DELIMITER_CHAR << TEXT("PersistentLevel");
		OutLevelPathName = Builder.ToString();
		OutShortObjectPathName = ObjectPath.GetSubPathString().RightChop(16);
		return true;
	}
	return false;
}


bool ULevelStreamingPersistenceManager::BuildSnapshot(const UObject* InObject, FLevelStreamingPersistentObjectPropertyBag& OutSnapshot) const
{
	int SavedCount = 0;
	const UClass* ObjectClass = InObject->GetClass();
	if (PersistentPropertiesInfo->HasProperties(ULevelStreamingPersistentPropertiesInfo::PropertyType_Private, ObjectClass))
	{
		if (OutSnapshot.Initialize([this, ObjectClass]() { return PersistentPropertiesInfo->GetPropertyBagFromClass(ULevelStreamingPersistentPropertiesInfo::PropertyType_Private, ObjectClass); }))
		{
			PersistentPropertiesInfo->ForEachProperty(ULevelStreamingPersistentPropertiesInfo::PropertyType_Private, ObjectClass, [this, InObject, &SavedCount, &OutSnapshot](FProperty* ObjectProperty)
			{
				if (OutSnapshot.CopyPropertyValueFromObject(InObject, ObjectProperty))
				{
					++SavedCount;
				}
			});
		}
		check(OutSnapshot.IsValid());
	}
	return SavedCount > 0;
}

bool ULevelStreamingPersistenceManager::DiffWithSnapshot(const UObject* InObject, const FLevelStreamingPersistentObjectPropertyBag& InSnapshot, TArray<const FProperty*>& OutChangedProperties) const
{
	if (ensure(InSnapshot.IsValid()))
	{
		const UClass* ObjectClass = InObject->GetClass();

		// Find changed properties
		PersistentPropertiesInfo->ForEachProperty(ULevelStreamingPersistentPropertiesInfo::PropertyType_Private, ObjectClass, [this, InObject, &InSnapshot, &OutChangedProperties](FProperty* ObjectProperty)
		{
			if (PersistenceModule->ShouldPersistProperty(InObject, ObjectProperty))
			{
				bool bIsIdentical = true;
				if (InSnapshot.ComparePropertyValueWithObject(InObject, ObjectProperty, bIsIdentical) && !bIsIdentical)
				{
					OutChangedProperties.Add(ObjectProperty);
				}
			}
		});

		return OutChangedProperties.Num() > 0;
	}

	return false;
}

bool ULevelStreamingPersistenceManager::SerializeTo(TArray<uint8>& OutPayload)
{
	// Serialize to archive and gather custom versions
	TArray<uint8> PayloadData;
	FMemoryWriter PayloadAr(PayloadData, true);
	if (SerializeManager(PayloadAr))
	{
		// Serialize custom versions
		TArray<uint8> HeaderData;
		FMemoryWriter HeaderAr(HeaderData);
		FCustomVersionContainer CustomVersions = PayloadAr.GetCustomVersions();
		CustomVersions.Serialize(HeaderAr);

		// Append data
		OutPayload = MoveTemp(HeaderData);
		OutPayload.Append(PayloadData);
		return true;
	}
	return false;
}

bool ULevelStreamingPersistenceManager::InitializeFrom(const TArray<uint8>& InPayload)
{
	FMemoryReader PayloadAr(InPayload, true);

	// Serialize custom versions
	FCustomVersionContainer CustomVersions;
	CustomVersions.Serialize(PayloadAr);
	PayloadAr.SetCustomVersions(CustomVersions);

	// Serialize payload
	return SerializeManager(PayloadAr);
}

bool ULevelStreamingPersistenceManager::SerializeManager(FArchive& InArchive)
{
	if (!InArchive.IsPersistent())
	{
		UE_LOG(LogLevelStreamingPersistenceManager, Warning, TEXT("Archive must be persistent to serialize LevelStreamingPersistenceManager"));
		return false;
	}

	FObjectAndNameAsStringProxyArchive Ar(InArchive, false);
	Ar.SetIsLoading(InArchive.IsLoading());
	Ar.SetIsSaving(InArchive.IsSaving());
	Ar.SetIsPersistent(true);
	Ar.SetFilterEditorOnly(true);
	Ar.SetIsTextFormat(false);
	Ar.SetWantBinaryPropertySerialization(!bUseTaggedPropertySerialization);
	Ar.ArShouldSkipBulkData = true;

	bool bLocalUseTaggedPropertySerialization = bUseTaggedPropertySerialization;
	Ar << bLocalUseTaggedPropertySerialization;

	if (bLocalUseTaggedPropertySerialization != bUseTaggedPropertySerialization)
	{
		UE_LOG(LogLevelStreamingPersistenceManager, Error, TEXT("Tagged property serialization mismatch : Can't serialize LevelStreamingPersistenceManager"));
		return false;
	}

	if (Ar.IsLoading())
	{
		TMap<FString, FLevelStreamingPersistentPropertyValues> LocalLevelsPropertyValues;
		Ar << LocalLevelsPropertyValues;

		for (auto& [LocalLevelPathName, LocalLevelPropertyValues] : LocalLevelsPropertyValues)
		{
			FLevelStreamingPersistentPropertyValues& LevelProperties = LevelsPropertyValues.FindOrAdd(LocalLevelPathName);
			for (auto& [ObjectPath, LocalPrivatePropertyValues] : LocalLevelPropertyValues.ObjectsPrivatePropertyValues)
			{
				if (LocalPrivatePropertyValues.Sanitize(*this))
				{
					check(!LocalPrivatePropertyValues.Snapshot.IsValid());
					// Override existing entry with the loaded/sanitized version 
					LevelProperties.ObjectsPrivatePropertyValues.Add(ObjectPath, MoveTemp(LocalPrivatePropertyValues));
					LevelProperties.bIsMakingVisibleCacheValid = false;
				}
			}

			for (auto& [ObjectPath, LocalPublicPropertyValues] : LocalLevelPropertyValues.ObjectsPublicPropertyValues)
			{
				if (LocalPublicPropertyValues.Sanitize(*this))
				{
					check(LocalPublicPropertyValues.PropertiesToPersist.IsEmpty());
					// Override existing entry with the loaded/sanitized version 
					LevelProperties.ObjectsPublicPropertyValues.Add(ObjectPath, MoveTemp(LocalPublicPropertyValues));
					LevelProperties.bIsMakingVisibleCacheValid = false;
				}
			}

			// If level is loaded, run restore logic right away
			if (ULevel* Level = FindObject<ULevel>(nullptr, *LocalLevelPathName))
			{
				RestoreLevelPersistentPropertyValues(Level);
			}
		}
	}
	else if (Ar.IsSaving())
	{
		Ar << LevelsPropertyValues;
	}

	return true;
}

#if !UE_BUILD_SHIPPING
void ULevelStreamingPersistenceManager::DumpContent() const
{
	UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("World Persistence Content for %s"), *GetWorld()->GetName());

	TMap<const UClass*, UObject*> TemporaryObjects;
	auto GetTemporaryObjectForClass = [&TemporaryObjects](const UClass* InClass)
	{
		UObject*& Object = TemporaryObjects.FindOrAdd(InClass);
		if (!Object)
		{
			Object = NewObject<UObject>(GetTransientPackage(), InClass);
		}
		return Object;
	};

	for (auto& [LevelPathName, LevelPropertyValues] : LevelsPropertyValues)
	{
		if (LevelPropertyValues.ObjectsPrivatePropertyValues.Num())
		{
			UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("[+] Level Private Properties of %s"), *LevelPathName);
			for (const auto& [ObjectPathName, ObjectPropertyValues] : LevelPropertyValues.ObjectsPrivatePropertyValues)
			{
				if (!ObjectPropertyValues.PayloadData.IsEmpty())
				{
					if (const UClass* Class = ObjectPropertyValues.GetSourceClass())
					{
						if (UObject* TempObject = GetTemporaryObjectForClass(Class))
						{
							FString ObjectLevelPathName;
							FString ObjectShortPathName;
							const bool bUseShortName = SplitObjectPath(ObjectPathName, ObjectLevelPathName, ObjectShortPathName);

							UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("  [+] Object Private Properties of %s"), bUseShortName ? *ObjectShortPathName : *ObjectPathName);

							if (RestorePrivateProperties(TempObject, ObjectPropertyValues))
							{
								for (const FProperty* Property : ObjectPropertyValues.PersistentProperties)
								{
									FString PropertyValue;
									if (PropertyPathHelpers::GetPropertyValueAsString(TempObject, Property->GetName(), PropertyValue))
									{
										UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("   - Property[%s] = %s"), *Property->GetName(), *PropertyValue);
									}
								}
							}
						}
					}
				}
			}
		}

		if (LevelPropertyValues.ObjectsPublicPropertyValues.Num())
		{
			UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("[+] Level Public Properties of %s"), *LevelPathName);
			for (auto& [ObjectPathName, ObjectPublicPropertyValues] : LevelPropertyValues.ObjectsPublicPropertyValues)
			{
				if (ObjectPublicPropertyValues.PropertyBag.IsValid())
				{
					FString ObjectLevelPathName;
					FString ObjectShortPathName;
					const bool bUseShortName = SplitObjectPath(ObjectPathName, ObjectLevelPathName, ObjectShortPathName);

					UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("  [+] Object Public Properties of %s"), bUseShortName ? *ObjectShortPathName : *ObjectPathName);
					ObjectPublicPropertyValues.PropertyBag.DumpContent([](const FProperty* Property, const FString& PropertyValue)
					{
						UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("   - Property[%s] = %s"), *Property->GetName(), *PropertyValue);
					}, &ObjectPublicPropertyValues.PersistentProperties);
				}
			}
		}
	}
}
#endif

/*
 * FLevelStreamingPersistentObjectPrivateProperties implementation
 */

FLevelStreamingPersistentObjectPrivateProperties::FLevelStreamingPersistentObjectPrivateProperties(FLevelStreamingPersistentObjectPrivateProperties&& InOther)
	: Snapshot(MoveTemp(InOther.Snapshot))
	, SourceClassPathName(MoveTemp(InOther.SourceClassPathName))
	, PayloadData(MoveTemp(InOther.PayloadData))
	, PersistentProperties(InOther.PersistentProperties)
{
}

void FLevelStreamingPersistentObjectPrivateProperties::ForEachPersistentProperty(TFunctionRef<void(const FProperty*)> Func) const
{
	for (const FProperty* Property : PersistentProperties)
	{
		Func(Property);
	}
}

FArchive& operator<<(FArchive& Ar, FLevelStreamingPersistentObjectPrivateProperties& PrivateProperties)
{
	PrivateProperties.Serialize(Ar);
	return Ar;
}

void FLevelStreamingPersistentObjectPrivateProperties::Serialize(FArchive& Ar)
{
	Ar << SourceClassPathName;
	Ar << PayloadData;

	uint32 PersistentPropertiesCount = PersistentProperties.Num();
	Ar << PersistentPropertiesCount;

	const UClass* Class = GetSourceClass();
	UE_CLOG(!Class, LogLevelStreamingPersistenceManager, Verbose, TEXT("FLevelStreamingPersistentObjectPrivateProperties : Could not find class %s"), *SourceClassPathName);

	if (Ar.IsLoading())
	{
		PersistentProperties.Reserve(PersistentPropertiesCount);
		for (uint32 i = 0; i < PersistentPropertiesCount; ++i)
		{
			FString PropertyName;
			Ar << PropertyName;

			if (const FProperty* ObjectProperty = Class ? Class->FindPropertyByName(FName(PropertyName)) : nullptr)
			{
				PersistentProperties.Add(ObjectProperty);
			}
			else
			{
				UE_CLOG(Class, LogLevelStreamingPersistenceManager, Verbose, TEXT("FLevelStreamingPersistentObjectPrivateProperties : Could not find private property %s"), *PropertyName);
			}
		}
	}
	else if (Ar.IsSaving())
	{
		for (const FProperty* Property : PersistentProperties)
		{
			FString PropertyName = Property->GetName();
			Ar << PropertyName;
		}
	}
}

bool FLevelStreamingPersistentObjectPrivateProperties::Sanitize(const ULevelStreamingPersistenceManager& InManager)
{
	check(PersistentProperties.IsEmpty() == PayloadData.IsEmpty());

	if (!PersistentProperties.IsEmpty())
	{
		TArray<const FProperty*> LocalPersistentProperties = MoveTemp(PersistentProperties);
		PersistentProperties.Reserve(LocalPersistentProperties.Num());
		for (const FProperty* PersistentProperty : LocalPersistentProperties)
		{
			if (InManager.PersistentPropertiesInfo->HasProperty(ULevelStreamingPersistentPropertiesInfo::PropertyType_Private, PersistentProperty))
			{
				PersistentProperties.Add(PersistentProperty);
			}
			else if (!InManager.bUseTaggedPropertySerialization)
			{
				UE_LOG(LogLevelStreamingPersistenceManager, Verbose, TEXT("FLevelStreamingPersistentObjectPrivateProperties : ULevelStreamingPersistenceManager doesn't use tagged property serialization and found an invalid persistent property %s. All private properties for this object will be ignored."), *PersistentProperty->GetName());
				PersistentProperties.Reset();
				return false;
			}
		}
	}
	return !PersistentProperties.IsEmpty();
}

/*
 * FLevelStreamingPersistentObjectPublicProperties implementation
 */

FLevelStreamingPersistentObjectPublicProperties::FLevelStreamingPersistentObjectPublicProperties(FLevelStreamingPersistentObjectPublicProperties&& InOther)
	: PropertiesToPersist(MoveTemp(InOther.PropertiesToPersist))
	, SourceClassPathName(MoveTemp(InOther.SourceClassPathName))
	, PropertyBag(MoveTemp(InOther.PropertyBag))
	, PersistentProperties(InOther.PersistentProperties)

{
}

void FLevelStreamingPersistentObjectPublicProperties::ForEachPersistentProperty(TFunctionRef<void(const FProperty*)> Func) const
{
	for (const FProperty* Property : PersistentProperties)
	{
		Func(Property);
	}
}

FArchive& operator<<(FArchive& Ar, FLevelStreamingPersistentObjectPublicProperties& PublicProperties)
{
	PublicProperties.Serialize(Ar);
	return Ar;
}

void FLevelStreamingPersistentObjectPublicProperties::Serialize(FArchive& Ar)
{
	Ar << SourceClassPathName;
	Ar << PropertyBag;

	uint32 PersistentPropertiesCount = PersistentProperties.Num();
	Ar << PersistentPropertiesCount;
	
	if (Ar.IsLoading())
	{
		PersistentProperties.Reserve(PersistentPropertiesCount);
		for (uint32 i = 0; i < PersistentPropertiesCount; ++i)
		{
			FString PropertyName;
			Ar << PropertyName;
			if (const FProperty* Property = PropertyBag.FindPropertyByName(FName(PropertyName)))
			{
				PersistentProperties.Add(Property);
			}
			else
			{
				UE_LOG(LogLevelStreamingPersistenceManager, Verbose, TEXT("FLevelStreamingPersistentObjectPublicProperties : Could not find public property %s in property bag."), *PropertyName);
			}
		}
	}
	else if (Ar.IsSaving())
	{
		for (const FProperty* Property : PersistentProperties)
		{
			FString PropertyName = Property->GetName();
			Ar << PropertyName;
		}
	}
}

bool FLevelStreamingPersistentObjectPublicProperties::Sanitize(const ULevelStreamingPersistenceManager& InManager)
{
	if (!PropertyBag.IsValid())
	{
		return false;
	}

	const UClass* SourceClass = GetSourceClass();
	const UPropertyBag* DefaultClass = SourceClass ? InManager.PersistentPropertiesInfo->GetPropertyBagFromClass(ULevelStreamingPersistentPropertiesInfo::PropertyType_Public, SourceClass) : nullptr;
	if (!DefaultClass)
	{
		return false;
	}

	FLevelStreamingPersistentObjectPropertyBag LocalPropertyBag = MoveTemp(PropertyBag);
	TArray<const FProperty*> LocalPersistentProperties = MoveTemp(PersistentProperties);
	PropertyBag.Initialize([&InManager, SourceClass]() { return InManager.PersistentPropertiesInfo->GetPropertyBagFromClass(ULevelStreamingPersistentPropertiesInfo::PropertyType_Public, SourceClass); });

	for (const FProperty* LocalProperty : LocalPersistentProperties)
	{
		if (const FProperty* Property = PropertyBag.FindPropertyByName(LocalProperty->GetFName()))
		{
			if (PropertyBag.CopyPropertyValueFromPropertyBag(LocalPropertyBag, LocalProperty))
			{
				PersistentProperties.Add(Property);
			}
		}
	}

	if (PersistentProperties.IsEmpty())
	{
		return false;
	}

	return true;
}

/*
 * FLevelStreamingPersistentPropertyValues implementation
 */

FArchive& operator<<(FArchive& Ar, FLevelStreamingPersistentPropertyValues& Properties)
{
	Properties.Serialize(Ar);
	return Ar;
}

void FLevelStreamingPersistentPropertyValues::Serialize(FArchive& Ar)
{
	Ar << ObjectsPrivatePropertyValues;
	Ar << ObjectsPublicPropertyValues;
}

/*
 * ULevelStreamingPersistentPropertiesInfo implementation
 */

void ULevelStreamingPersistentPropertiesInfo::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULevelStreamingPersistentPropertiesInfo* This = CastChecked<ULevelStreamingPersistentPropertiesInfo>(InThis);
	for (int32 AccessSpecifierIndex = 0; AccessSpecifierIndex < PropertyType_Count; ++AccessSpecifierIndex)
	{
		for (auto& It : This->ClassesProperties[AccessSpecifierIndex])
		{
			Collector.AddReferencedObject(It.Key);
		}
	}
	Super::AddReferencedObjects(InThis, Collector);
}

void ULevelStreamingPersistentPropertiesInfo::Initialize()
{
	auto FillClassesProperties = [this](const FString& InPropertyPath, bool bIsPublic)
	{
		auto& OutClassesProperties = ClassesProperties[bIsPublic ? PropertyType_Public : PropertyType_Private];
		FSoftObjectPath(InPropertyPath).TryLoad();
		if (FProperty* Property = TFieldPath<FProperty>(*InPropertyPath).Get())
		{
			if (const UClass* Class = Property->GetOwnerClass())
			{
				OutClassesProperties.FindOrAdd(Class).Add(Property);
			}
		}
		else
		{
			UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("Could not resolve property path %s."), *InPropertyPath);
		}
	};

	auto CreateClassDefaultPropertyBag = [this]()
	{
		for (int32 AccessSpecifierIndex = 0; AccessSpecifierIndex < PropertyType_Count; ++AccessSpecifierIndex)
		{
			EPropertyType AccessSpecifier = EPropertyType(AccessSpecifierIndex);
			TMap<const UClass*, FInstancedPropertyBag>& ClassesDefaults = ObjectClassToPropertyBag[AccessSpecifier];
			for (auto& [Class, ClassProperties] : ClassesProperties[AccessSpecifier])
			{
				check(Class);
				check(!ClassesDefaults.Contains(Class));

				TArray<FPropertyBagPropertyDesc> Descs;
				ForEachProperty(AccessSpecifier, Class, [&Descs](FProperty* Property)
				{
					Descs.Emplace(Property->GetFName(), Property);
#if WITH_EDITORONLY_DATA
					// Get rid of metadata to save memory
					Descs.Last().MetaData.Empty();
#endif
				});

				FInstancedPropertyBag& PropertyBag = ClassesDefaults.FindOrAdd(Class);
				PropertyBag.AddProperties(Descs);
				check(PropertyBag.GetPropertyBagStruct());
			}
		}
	};

	for (const FLevelStreamingPersistentProperty& PersistentProperty : GetDefault<ULevelStreamingPersistenceSettings>()->Properties)
	{
		FillClassesProperties(PersistentProperty.Path, PersistentProperty.bIsPublic);
	}

	CreateClassDefaultPropertyBag();
}

const UPropertyBag* ULevelStreamingPersistentPropertiesInfo::GetPropertyBagFromClass(EPropertyType InAccessSpecifier, const UClass* InClass) const
{
	if (ensure(InAccessSpecifier < PropertyType_Count))
	{
		const UClass* Class = InClass;
		while (Class)
		{
			if (const FInstancedPropertyBag* InstancedPropertyBag = ObjectClassToPropertyBag[InAccessSpecifier].Find(Class))
			{
				return InstancedPropertyBag->GetPropertyBagStruct();
			}
			Class = Class->GetSuperClass();
		}
	}
	return nullptr;
}

bool ULevelStreamingPersistentPropertiesInfo::HasProperty(EPropertyType InAccessSpecifier, const FProperty* InProperty) const
{
	if (ensure(InAccessSpecifier < PropertyType_Count))
	{
		if (const UClass* Class = InProperty->GetOwnerClass())
		{
			if (const TSet<FProperty*>* ClassProperties = ClassesProperties[InAccessSpecifier].Find(Class))
			{
				return ClassProperties->Contains(InProperty);
			}
		}
	}
	return false;
}

void ULevelStreamingPersistentPropertiesInfo::ForEachProperty(EPropertyType InAccessSpecifier, const UClass* InClass, TFunctionRef<void(FProperty*)> Func) const
{
	if (ensure(InAccessSpecifier < PropertyType_Count))
	{
		const UClass* Class = InClass;
		while (Class)
		{
			if (const TSet<FProperty*>* ClassProperties = ClassesProperties[InAccessSpecifier].Find(Class))
			{
				for (FProperty* Property : *ClassProperties)
				{
					Func(Property);
				}
			}
			Class = Class->GetSuperClass();
		}
	}
}

bool ULevelStreamingPersistentPropertiesInfo::HasProperties(EPropertyType InAccessSpecifier, const UClass* InClass) const
{
	if (ensure(InAccessSpecifier < PropertyType_Count))
	{
		const UClass* Class = InClass;
		while (Class)
		{
			if (ClassesProperties[InAccessSpecifier].Contains(Class))
			{
				return true;
			}
			Class = Class->GetSuperClass();
		}
	}
	return false;
}

#if !UE_BUILD_SHIPPING

/*
 * Level Streaming Persistence Manager Console command helper
 */
namespace LSPMConsoleCommandHelper
{
	template<typename PropertyType>
	static bool GetValueFromString(const FString& InPropertyValue, PropertyType& OutResult) { return false; }

	template<typename PropertyType>
	static FString GetValueToString(const PropertyType& InPropertyValue) { return FString(TEXT("<unknown>")); }

	template<typename PropertyType>
	static bool TrySetPropertyValueFromString(ULevelStreamingPersistenceManager* InLevelStreamingPersistenceManager, const FString& InObjectPath, const FName InPropertyName, const FString& InPropertyValue)
	{
		PropertyType ValueFromString;
		if (GetValueFromString(InPropertyValue, ValueFromString))
		{
			if (InLevelStreamingPersistenceManager->TrySetPropertyValue(InObjectPath, InPropertyName, ValueFromString))
			{
				PropertyType Result;
				if (InLevelStreamingPersistenceManager->GetPropertyValue(InObjectPath, InPropertyName, Result))
				{
					FString ResultToString = GetValueToString(Result);
					UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("SetPropertyValue succeded : Property[%s] = %s for object %s"), *InPropertyName.ToString(), *ResultToString, *InObjectPath);
					return true;
				}
			}
		}
		return false;
	}

	template<typename PropertyType>
	static bool GetPropertyValueAsString(ULevelStreamingPersistenceManager* InLevelStreamingPersistenceManager, const FString& InObjectPath, const FName InPropertyName, FString& OutPropertyValue)
	{
		PropertyType Result;
		if (InLevelStreamingPersistenceManager->GetPropertyValue(InObjectPath, InPropertyName, Result))
		{
			OutPropertyValue = GetValueToString(Result);
			UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("GetPropertyValue succeded : Property[%s] = %s for object %s"), *InPropertyName.ToString(), *OutPropertyValue, *InObjectPath);
			return true;
		}
		return false;
	}

	template<> bool GetValueFromString(const FString& InPropertyValue, bool& OutResult) { OutResult = InPropertyValue.ToBool(); return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, int32& OutResult) { LexFromString(OutResult, *InPropertyValue); return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, int64& OutResult) { LexFromString(OutResult, *InPropertyValue); return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, float& OutResult) { LexFromString(OutResult, *InPropertyValue); return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, double& OutResult) { LexFromString(OutResult, *InPropertyValue); return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, FName& OutResult) { OutResult = FName(InPropertyValue); return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, FString& OutResult) { OutResult = InPropertyValue; return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, FText& OutResult) { OutResult.FromString(InPropertyValue); return true; }
	template<> bool GetValueFromString(const FString& InPropertyValue, FVector& OutResult) { return OutResult.InitFromString(InPropertyValue); }
	template<> bool GetValueFromString(const FString& InPropertyValue, FRotator& OutResult) { return OutResult.InitFromString(InPropertyValue); }
	template<> bool GetValueFromString(const FString& InPropertyValue, FTransform& OutResult) { return OutResult.InitFromString(InPropertyValue); }
	template<> bool GetValueFromString(const FString& InPropertyValue, FColor& OutResult) { return OutResult.InitFromString(InPropertyValue); }
	template<> bool GetValueFromString(const FString& InPropertyValue, FLinearColor& OutResult) { return OutResult.InitFromString(InPropertyValue); }

	template<> FString GetValueToString(const bool& InPropertyValue) { return LexToString(InPropertyValue); }
	template<> FString GetValueToString(const int32& InPropertyValue) { return LexToString(InPropertyValue); }
	template<> FString GetValueToString(const int64& InPropertyValue) { return LexToString(InPropertyValue); }
	template<> FString GetValueToString(const float& InPropertyValue) { return LexToString(InPropertyValue); }
	template<> FString GetValueToString(const double& InPropertyValue) { return LexToString(InPropertyValue); }
	template<> FString GetValueToString(const FName& InPropertyValue) { return InPropertyValue.ToString(); }
	template<> FString GetValueToString(const FString& InPropertyValue) { return InPropertyValue; }
	template<> FString GetValueToString(const FText& InPropertyValue) { return InPropertyValue.ToString(); }
	template<> FString GetValueToString(const FVector& InPropertyValue) { return InPropertyValue.ToString(); }
	template<> FString GetValueToString(const FRotator& InPropertyValue) { return InPropertyValue.ToString(); }
	template<> FString GetValueToString(const FTransform& InPropertyValue) { return InPropertyValue.ToString(); }
	template<> FString GetValueToString(const FColor& InPropertyValue) { return InPropertyValue.ToString(); }
	template<> FString GetValueToString(const FLinearColor& InPropertyValue) { return InPropertyValue.ToString(); }

	static FString GetFilename(UWorld* InWorld)
	{
		FString Filename = FPaths::ProjectSavedDir() / TEXT("LevelStreamingPersistence") / FString::Printf(TEXT("LevelStreamingPersistence-%s.bin"), *InWorld->GetName());
		return Filename;
	}

	void ForEachLevelStreamingPersistenceManager(TFunctionRef<bool(ULevelStreamingPersistenceManager*)> Func)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld())
			{
				if (ULevelStreamingPersistenceManager* LevelStreamingPersistenceManager = World->GetSubsystem<ULevelStreamingPersistenceManager>())
				{
					if (!Func(LevelStreamingPersistenceManager))
					{
						break;
					}
				}
			}
		}
	}

} // namespace LSPMConsoleCommandHelper

FAutoConsoleCommand ULevelStreamingPersistenceManager::DumpContentCommand(
	TEXT("s.LevelStreamingPersistence.Debug.DumpContent"),
	TEXT("Dump the content of LevelStreamingPersistenceManager"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		LSPMConsoleCommandHelper::ForEachLevelStreamingPersistenceManager([](ULevelStreamingPersistenceManager* Manager)
		{
			Manager->DumpContent();
			return true;
		});
	})
);

FAutoConsoleCommand ULevelStreamingPersistenceManager::SetPropertyValueCommand(
	TEXT("s.LevelStreamingPersistence.Debug.SetPropertyValue"),
	TEXT("Set the persistent property's value for a given object"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
	{
		if (InArgs.Num() < 3)
		{
			return;
		}

		LSPMConsoleCommandHelper::ForEachLevelStreamingPersistenceManager([&InArgs](ULevelStreamingPersistenceManager* Manager)
		{
			const FString& ObjectPath = InArgs[0];
			const FName PropertyName = FName(InArgs[1]);
			const FString& PropertyValue = InArgs[2];

			if (LSPMConsoleCommandHelper::TrySetPropertyValueFromString<bool>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<int32>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<int64>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<float>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<double>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FName>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FString>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FText>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FVector>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FRotator>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FTransform>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FColor>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::TrySetPropertyValueFromString<FLinearColor>(Manager, ObjectPath, PropertyName, PropertyValue))
			{
				return false;
			}
			else if (Manager->TrySetPropertyValueFromString(ObjectPath, PropertyName, PropertyValue))
			{
				FString Result;
				if (Manager->GetPropertyValueAsString(ObjectPath, PropertyName, Result))
				{
					UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("GetPropertyValueAsString succeded : Property[%s] = %s for object %s"), *PropertyName.ToString(), *Result, *ObjectPath);
					return false;
				}
			}			
			return true;
		});
	})
);

FAutoConsoleCommand ULevelStreamingPersistenceManager::GetPropertyValueCommand(
	TEXT("s.LevelStreamingPersistence.Debug.GetPropertyValue"),
	TEXT("Get the persistent property's value for a given object"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
	{
		if (InArgs.Num() < 2)
		{
			return;
		}

		LSPMConsoleCommandHelper::ForEachLevelStreamingPersistenceManager([&InArgs](ULevelStreamingPersistenceManager* Manager)
		{
			const FString& ObjectPath = InArgs[0];
			const FName PropertyName = FName(InArgs[1]);

			FString PropertyValue;
			if (LSPMConsoleCommandHelper::GetPropertyValueAsString<bool>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<int32>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<int64>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<float>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<double>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FName>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FString>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FText>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FVector>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FRotator>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FTransform>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FColor>(Manager, ObjectPath, PropertyName, PropertyValue) ||
				LSPMConsoleCommandHelper::GetPropertyValueAsString<FLinearColor>(Manager, ObjectPath, PropertyName, PropertyValue))
			{
				return false;
			}
			else
			{
				FString Result;
				if (Manager->GetPropertyValueAsString(ObjectPath, PropertyName, Result))
				{
					UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("GetPropertyValueAsString succeded : Property[%s] = %s for object %s"), *PropertyName.ToString(), *Result, *ObjectPath);
					return false;
				}
			}
			return true;
		});
	})
);

FAutoConsoleCommand ULevelStreamingPersistenceManager::SaveToFileCommand(
	TEXT("s.LevelStreamingPersistence.Debug.SaveToFile"),
	TEXT("Save the content of LevelStreamingPersistenceManager to a file"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		LSPMConsoleCommandHelper::ForEachLevelStreamingPersistenceManager([](ULevelStreamingPersistenceManager* Manager)
		{
			FString Filename = LSPMConsoleCommandHelper::GetFilename(Manager->GetWorld());
			TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*Filename));
			if (FileWriter.IsValid())
			{
				TArray<uint8> Payload;
				if (Manager->SerializeTo(Payload))
				{
					UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("SaveToFile %s succeeded"), *Filename);
					*FileWriter << Payload;
					return false;
				}
			}
			return true;
		});
	})
);

FAutoConsoleCommand ULevelStreamingPersistenceManager::LoadFromFileCommand(
	TEXT("s.LevelStreamingPersistence.Debug.LoadFromFile"),
	TEXT("Load from a file and initializes LevelStreamingPersistenceManager"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		LSPMConsoleCommandHelper::ForEachLevelStreamingPersistenceManager([](ULevelStreamingPersistenceManager* Manager)
		{
			FString Filename = LSPMConsoleCommandHelper::GetFilename(Manager->GetWorld());
			TUniquePtr<FArchive> FileReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*Filename));
			if (FileReader.IsValid())
			{
				TArray<uint8> Payload;
				*FileReader << Payload;
				if (Manager->InitializeFrom(Payload))
				{
					UE_LOG(LogLevelStreamingPersistenceManager, Log, TEXT("LoadFromFile %s succeeded"), *Filename);
					return false;
				}
			}
			return true;
		});
	})
);

#endif // !UE_BUILD_SHIPPING

bool ULevelStreamingPersistenceManager::bIsEnabled = true;
FAutoConsoleVariableRef ULevelStreamingPersistenceManager::EnableCommand(
	TEXT("s.LevelStreamingPersistence.Enabled"),
	ULevelStreamingPersistenceManager::bIsEnabled,
	TEXT("Turn on/off to enable/disable world persistent subsystem."),
	ECVF_Default);
