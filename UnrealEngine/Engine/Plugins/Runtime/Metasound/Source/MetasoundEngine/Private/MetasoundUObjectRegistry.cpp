// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundUObjectRegistry.h"

#include "Algo/Copy.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundSettings.h"
#include "MetasoundSource.h"
#include "MetasoundTrace.h"
#include "UObject/Object.h"

namespace Metasound
{
	class FMetasoundUObjectRegistry : public IMetasoundUObjectRegistry
	{
		public:
			FMetasoundUObjectRegistry() = default;

			void RegisterUClassInterface(TUniquePtr<IMetasoundUObjectRegistryEntry>&& InEntry) override
			{
				METASOUND_LLM_SCOPE;

				if (InEntry.IsValid())
				{
					Frontend::FInterfaceRegistryKey Key = Frontend::GetInterfaceRegistryKey(InEntry->GetInterfaceVersion());

					EntriesByInterface.Add(Key, InEntry.Get());
					EntriesByName.Add(InEntry->GetInterfaceVersion().Name, InEntry.Get());
					Entries.Add(InEntry.Get());
					Storage.Add(MoveTemp(InEntry));
				}
			}

			TArray<const IMetasoundUObjectRegistryEntry*> FindInterfaceEntriesByName(FName InName) const override
			{
				TArray<const IMetasoundUObjectRegistryEntry*> EntriesWithName;
				EntriesByName.MultiFind(InName, EntriesWithName);

				return EntriesWithName;
			}

			TArray<UClass*> FindSupportedInterfaceClasses(const FMetasoundFrontendVersion& InInterfaceVersion) const override
			{
				TArray<UClass*> Classes;

				TArray<const IMetasoundUObjectRegistryEntry*> EntriesForInterface;
				EntriesByInterface.MultiFind(Frontend::GetInterfaceRegistryKey(InInterfaceVersion), EntriesForInterface);

				for (const IMetasoundUObjectRegistryEntry* Entry : EntriesForInterface)
				{
					if (nullptr != Entry)
					{
						if (UClass* Class = Entry->GetUClass())
						{
							Classes.Add(Class);
						}
					}
				}

				return Classes;
			}

			UObject* NewObject(UClass* InClass, const FMetasoundFrontendDocument& InDocument, const FString& InPath) const override
			{
				METASOUND_LLM_SCOPE;

				TArray<const IMetasoundUObjectRegistryEntry*> AllInterfaceEntries;

				for (const FMetasoundFrontendVersion& InterfaceVersion : InDocument.Interfaces)
				{
					TArray<const IMetasoundUObjectRegistryEntry*> EntriesForInterface;
					EntriesByInterface.MultiFind(Frontend::GetInterfaceRegistryKey(InterfaceVersion), EntriesForInterface);
					AllInterfaceEntries.Append(MoveTemp(EntriesForInterface));
				}

				auto IsChildClassOfRegisteredClass = [&](const IMetasoundUObjectRegistryEntry* Entry)
				{
					return Entry->IsChildClass(InClass);
				};

				if (const IMetasoundUObjectRegistryEntry* const* EntryForClass = AllInterfaceEntries.FindByPredicate(IsChildClassOfRegisteredClass))
				{
					return NewObject(**EntryForClass, InDocument, InPath);
				}

				return nullptr;
			}

			bool IsRegisteredClass(UObject* InObject) const override
			{
				return (nullptr != GetEntryByUObject(InObject));
			}

			FMetasoundAssetBase* GetObjectAsAssetBase(UObject* InObject) const override
			{
				if (const IMetasoundUObjectRegistryEntry* Entry = GetEntryByUObject(InObject))
				{
					return Entry->Cast(InObject);
				}
				return nullptr;
			}

			const FMetasoundAssetBase* GetObjectAsAssetBase(const UObject* InObject) const override
			{
				if (const IMetasoundUObjectRegistryEntry* Entry = GetEntryByUObject(InObject))
				{
					return Entry->Cast(InObject);
				}
				return nullptr;
			}

			void IterateRegisteredUClasses(TFunctionRef<void(UClass&)> InFunc) const override
			{
				for (const IMetasoundUObjectRegistryEntry* Entry : Entries)
				{
					if (Entry)
					{
						if (UClass* Class = Entry->GetUClass())
						{
							InFunc(*Class);
						}
					}
				}
			}

		private:
			UObject* NewObject(const IMetasoundUObjectRegistryEntry& InEntry, const FMetasoundFrontendDocument& InDocument, const FString& InPath) const
			{
				UPackage* PackageToSaveTo = nullptr;

				if (GIsEditor)
				{
					FText InvalidPathReason;
					bool const bValidPackageName = FPackageName::IsValidLongPackageName(InPath, false, &InvalidPathReason);

					if (!ensureAlwaysMsgf(bValidPackageName, TEXT("Tried to generate a Metasound UObject with an invalid package path/name Falling back to transient package, which means we won't be able to save this asset.")))
					{
						PackageToSaveTo = GetTransientPackage();
					}
					else
					{
						PackageToSaveTo = CreatePackage(*InPath);
					}
				}
				else
				{
					PackageToSaveTo = GetTransientPackage();
				}

				UObject* NewMetasoundObject = InEntry.NewObject(PackageToSaveTo, *InDocument.RootGraph.Metadata.GetClassName().GetFullName().ToString());
				FMetasoundAssetBase* NewAssetBase = InEntry.Cast(NewMetasoundObject);
				if (ensure(nullptr != NewAssetBase))
				{
					NewAssetBase->SetDocument(InDocument);
				}

#if WITH_EDITOR
				AsyncTask(ENamedThreads::GameThread, [NewMetasoundObject]()
				{
					FAssetRegistryModule::AssetCreated(NewMetasoundObject);
					NewMetasoundObject->MarkPackageDirty();
					// todo: how do you get the package for a uobject and save it? I forget
				});
#endif

				return NewMetasoundObject;
			}

			const IMetasoundUObjectRegistryEntry* FindEntryByPredicate(TFunction<bool (const IMetasoundUObjectRegistryEntry*)> InPredicate) const
			{
				const IMetasoundUObjectRegistryEntry* const* Entry = Entries.FindByPredicate(InPredicate);

				if (nullptr == Entry)
				{
					return nullptr;
				}

				return *Entry;
			}

			TArray<const IMetasoundUObjectRegistryEntry*> FindEntriesByPredicate(TFunction<bool (const IMetasoundUObjectRegistryEntry*)> InPredicate) const
			{
				TArray<const IMetasoundUObjectRegistryEntry*> FoundEntries;

				Algo::CopyIf(Entries, FoundEntries, InPredicate);

				return FoundEntries;
			}

			const IMetasoundUObjectRegistryEntry* GetEntryByUObject(const UObject* InObject) const
			{
				auto IsChildClassOfRegisteredClass = [&](const IMetasoundUObjectRegistryEntry* Entry)
				{
					if (nullptr == Entry)
					{
						return false;
					}

					return Entry->IsChildClass(InObject);
				};

				return FindEntryByPredicate(IsChildClassOfRegisteredClass);
			}

			TArray<TUniquePtr<IMetasoundUObjectRegistryEntry>> Storage;
			TMultiMap<Frontend::FInterfaceRegistryKey, const IMetasoundUObjectRegistryEntry*> EntriesByInterface;
			TMultiMap<FName, const IMetasoundUObjectRegistryEntry*> EntriesByName;
			TArray<const IMetasoundUObjectRegistryEntry*> Entries;
	};

	IMetasoundUObjectRegistry& IMetasoundUObjectRegistry::Get()
	{
		static FMetasoundUObjectRegistry Registry;
		return Registry;
	}
}

