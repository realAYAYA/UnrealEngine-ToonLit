// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendSearchEngineCore.h"

#include "Algo/MaxElement.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendInterfaceRegistryPrivate.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundTrace.h"

namespace Metasound
{
	namespace Frontend
	{
		namespace SearchEngineQuerySteps
		{
			FFrontendQueryKey CreateKeyFromFullClassNameAndMajorVersion(const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion)
			{
				return FFrontendQueryKey(FString::Format(TEXT("{0}_v{1}"), {InClassName.ToString(), InMajorVersion}));
			}

			FFrontendQueryKey FMapClassToFullClassNameAndMajorVersion::Map(const FFrontendQueryEntry& InEntry) const
			{
				if (ensure(InEntry.Value.IsType<FMetasoundFrontendClass>()))
				{
					const FMetasoundFrontendClassMetadata& Metadata = InEntry.Value.Get<FMetasoundFrontendClass>().Metadata;
					return CreateKeyFromFullClassNameAndMajorVersion(Metadata.GetClassName(), Metadata.GetVersion().Major);
				}
				return FFrontendQueryKey();
			}

			FInterfaceRegistryTransactionSource::FInterfaceRegistryTransactionSource()
			: TransactionStream(FInterfaceRegistry::Get().CreateTransactionStream())
			{
			}

			void FInterfaceRegistryTransactionSource::Stream(TArray<FFrontendQueryValue>& OutEntries)
			{
				auto AddValue = [&OutEntries](const FInterfaceRegistryTransaction& InTransaction)
				{
					OutEntries.Emplace(TInPlaceType<FInterfaceRegistryTransaction>(), InTransaction);
				};

				if (TransactionStream.IsValid())
				{
					TransactionStream->Stream(AddValue);
				}
			}

			FFrontendQueryKey FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys::Map(const FFrontendQueryEntry& InEntry) const
			{
				FInterfaceRegistryKey RegistryKey;
				if (ensure(InEntry.Value.IsType<FInterfaceRegistryTransaction>()))
				{
					RegistryKey = InEntry.Value.Get<FInterfaceRegistryTransaction>().GetInterfaceRegistryKey();
				}
				return FFrontendQueryKey{RegistryKey};
			}

			FFrontendQueryKey FMapInterfaceRegistryTransactionToInterfaceName::Map(const FFrontendQueryEntry& InEntry) const
			{
				FInterfaceRegistryKey RegistryKey;
				if (ensure(InEntry.Value.IsType<FInterfaceRegistryTransaction>()))
				{
					return FFrontendQueryKey{InEntry.Value.Get<FInterfaceRegistryTransaction>().GetInterfaceVersion().Name};
				}
				return FFrontendQueryKey();
			}

			void FReduceInterfaceRegistryTransactionsToCurrentStatus::Reduce(const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries) const
			{
				// Get most recent transaction
				const FFrontendQueryEntry* FinalEntry = Algo::MaxElementBy(InOutEntries, GetTransactionTimestamp);

				// Check if most recent transaction is valid and not an unregister transaction.
				if (IsValidTransactionOfType(FInterfaceRegistryTransaction::ETransactionType::InterfaceRegistration, FinalEntry))
				{
					FFrontendQueryEntry Entry = *FinalEntry;
					InOutEntries.Reset();
					InOutEntries.Add(Entry);
				}
				else
				{
					InOutEntries.Reset();
				}
			}

			FInterfaceRegistryTransaction::FTimeType FReduceInterfaceRegistryTransactionsToCurrentStatus::GetTransactionTimestamp(const FFrontendQueryEntry& InEntry)
			{
				if (ensure(InEntry.Value.IsType<FInterfaceRegistryTransaction>()))
				{
					return InEntry.Value.Get<FInterfaceRegistryTransaction>().GetTimestamp();
				}
				return 0;
			}

			bool FReduceInterfaceRegistryTransactionsToCurrentStatus::IsValidTransactionOfType(FInterfaceRegistryTransaction::ETransactionType InType, const FFrontendQueryEntry* InEntry)
			{
				if (nullptr != InEntry)
				{
					if (InEntry->Value.IsType<FInterfaceRegistryTransaction>())
					{
						return InEntry->Value.Get<FInterfaceRegistryTransaction>().GetTransactionType() == InType;
					}
				}
				return false;
			}

			void FTransformInterfaceRegistryTransactionToInterface::Transform(FFrontendQueryEntry::FValue& InValue) const
			{
				FMetasoundFrontendInterface Interface;
				if (ensure(InValue.IsType<FInterfaceRegistryTransaction>()))
				{
					FInterfaceRegistryKey RegistryKey = InValue.Get<FInterfaceRegistryTransaction>().GetInterfaceRegistryKey();
					IInterfaceRegistry::Get().FindInterface(RegistryKey, Interface);
				}
				InValue.Set<FMetasoundFrontendInterface>(MoveTemp(Interface));
			}

			FFrontendQueryKey FMapInterfaceToInterfaceNameAndMajorVersion::GetKey(const FString& InName, int32 InMajorVersion)
			{
				return FFrontendQueryKey{FString::Format(TEXT("{0}_{1}"), { InName, InMajorVersion })};
			}

			FFrontendQueryKey FMapInterfaceToInterfaceNameAndMajorVersion::Map(const FFrontendQueryEntry& InEntry) const
			{
				FString Key;
				if (ensure(InEntry.Value.IsType<FMetasoundFrontendInterface>()))
				{
					const FMetasoundFrontendInterface& Interface = InEntry.Value.Get<FMetasoundFrontendInterface>();

					return FMapInterfaceToInterfaceNameAndMajorVersion::GetKey(Interface.Version.Name.ToString(), Interface.Version.Number.Major);
				}
				return FFrontendQueryKey{Key};
			}

			void FReduceInterfacesToHighestVersion::Reduce(const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries) const
			{
				FFrontendQueryEntry* HighestVersionEntry = nullptr;
				FMetasoundFrontendVersionNumber HighestVersion = FMetasoundFrontendVersionNumber::GetInvalid();

				for (FFrontendQueryEntry& Entry : InOutEntries)
				{
					if (ensure(Entry.Value.IsType<FMetasoundFrontendInterface>()))
					{
						const FMetasoundFrontendVersionNumber& VersionNumber = Entry.Value.Get<FMetasoundFrontendInterface>().Version.Number;
						if (VersionNumber > HighestVersion)
						{
							HighestVersionEntry = &Entry;
							HighestVersion = VersionNumber;
						}
					}
				}

				if (HighestVersionEntry)
				{
					FFrontendQueryEntry Entry = *HighestVersionEntry;
					InOutEntries.Reset();
					InOutEntries.Add(Entry);
				}
				else
				{
					InOutEntries.Reset();
				}
			}

			bool FRemoveDeprecatedClasses::Filter(const FFrontendQueryEntry& InEntry) const
			{
				if (InEntry.Value.IsType<FMetasoundFrontendClass>())
				{
					return !InEntry.Value.Get<FMetasoundFrontendClass>().Metadata.GetIsDeprecated();
				}
				return false;
			}

			bool FRemoveInterfacesWhichAreNotDefault::Filter(const FFrontendQueryEntry& InEntry) const
			{
				if (InEntry.Value.IsType<FMetasoundFrontendInterface>())
				{
					FInterfaceRegistryKey RegistryKey = GetInterfaceRegistryKey(InEntry.Value.Get<FMetasoundFrontendInterface>());
					const IInterfaceRegistryEntry* RegEntry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(RegistryKey);

					if (nullptr != RegEntry)
					{
						return RegEntry->IsDefault();
					}
				}
				return false;
			}

			class FMapNodeRegistrationEventsToClassAndMajorVersion : public IFrontendQueryMapStep
			{
			public:
				virtual FFrontendQueryKey Map(const FFrontendQueryEntry& InEntry) const override
				{
					if (InEntry.Value.IsType<FNodeRegistryTransaction>())
					{
						const FNodeClassInfo& Info = InEntry.Value.Get<FNodeRegistryTransaction>().GetNodeClassInfo();
						return CreateKeyFromFullClassNameAndMajorVersion(Info.ClassName, Info.Version.Major);
					}
					else
					{
						return FFrontendQueryKey();
					}
				}
			};

			FFrontendQueryKey FMapInterfaceToInterfaceName::Map(const FFrontendQueryEntry& InEntry) const
			{
				FName Key;
				if (ensure(InEntry.Value.IsType<FMetasoundFrontendInterface>()))
				{
					Key = InEntry.Value.Get<FMetasoundFrontendInterface>().Version.Name;
				}
				return FFrontendQueryKey{Key};
			}

			void FTransformInterfaceRegistryTransactionToInterfaceVersion::Transform(FFrontendQueryEntry::FValue& InValue) const
			{
				FMetasoundFrontendVersion Version;
				if (ensure(InValue.IsType<FInterfaceRegistryTransaction>()))
				{
					Version = InValue.Get<FInterfaceRegistryTransaction>().GetInterfaceVersion();
				}
				InValue.Set<FMetasoundFrontendVersion>(Version);
			}

			TArray<FMetasoundFrontendClass> BuildArrayOfClassesFromPartition(const FFrontendQueryPartition& InPartition)
			{
				TArray<FMetasoundFrontendClass> Result;
				for (const FFrontendQueryEntry& Entry : InPartition)
				{
					check(Entry.Value.IsType<FMetasoundFrontendClass>());
					Result.Add(Entry.Value.Get<FMetasoundFrontendClass>());
				}
				return Result;
			}

			FMetasoundFrontendClass BuildSingleClassFromPartition(const FFrontendQueryPartition& InPartition)
			{
				FMetasoundFrontendClass MetaSoundClass;
				if (ensureMsgf(InPartition.Num() > 0, TEXT("Cannot retrieve class from empty partition")))
				{
					MetaSoundClass = InPartition.CreateConstIterator()->Value.Get<FMetasoundFrontendClass>();
				}
				return MetaSoundClass;
			}

			TArray<FMetasoundFrontendInterface> BuildArrayOfInterfacesFromPartition(const FFrontendQueryPartition& InPartition)
			{
				TArray<FMetasoundFrontendInterface> Result;
				for (const FFrontendQueryEntry& Entry : InPartition)
				{
					check(Entry.Value.IsType<FMetasoundFrontendInterface>());
					Result.Add(Entry.Value.Get<FMetasoundFrontendInterface>());
				}
				return Result;
			}

			FMetasoundFrontendInterface BuildSingleInterfaceFromPartition(const FFrontendQueryPartition& InPartition)
			{
				FMetasoundFrontendInterface MetaSoundInterface;
				if (ensureMsgf(InPartition.Num() > 0, TEXT("Cannot retrieve class from empty partition")))
				{
					MetaSoundInterface = InPartition.CreateConstIterator()->Value.Get<FMetasoundFrontendInterface>();
				}
				return MetaSoundInterface;
			}

			TArray<FMetasoundFrontendVersion> BuildArrayOfVersionsFromPartition(const FFrontendQueryPartition& InPartition)
			{
				TArray<FMetasoundFrontendVersion> Result;
				for (const FFrontendQueryEntry& Entry : InPartition)
				{
					check(Entry.Value.IsType<FMetasoundFrontendVersion>());
					Result.Add(Entry.Value.Get<FMetasoundFrontendVersion>());
				}
				return Result;
			}


		}


		FFrontendQuery FFindClassWithHighestMinorVersionQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FNodeClassRegistrationEvents>()
				.AddStep<FMapRegistrationEventsToNodeRegistryKeys>()
				.AddStep<FReduceRegistrationEventsToCurrentStatus>()
				.AddStep<FMapNodeRegistrationEventsToClassAndMajorVersion>();
			return Query;
		}

		FNodeRegistryKey FFindClassWithHighestMinorVersionQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;

			FMetasoundFrontendVersionNumber HighestVersionNumber{-1, -1};
			FNodeRegistryKey Key = NodeRegistryKey::GetInvalid();
			for (const FFrontendQueryEntry& InEntry : InPartition)
			{
				if (InEntry.Value.IsType<FNodeRegistryTransaction>())
				{
					const FNodeRegistryTransaction& Transaction = InEntry.Value.Get<FNodeRegistryTransaction>();
					if (FNodeRegistryTransaction::ETransactionType::NodeRegistration == Transaction.GetTransactionType())
					{
						const FNodeClassInfo& NodeClassInfo = Transaction.GetNodeClassInfo();
						if (NodeClassInfo.Version > HighestVersionNumber)
						{
							HighestVersionNumber = NodeClassInfo.Version;
							Key = Transaction.GetNodeRegistryKey();
						}
					}
				}
			}

			return Key;
		}

		FFrontendQuery FFindAllDefaultInterfacesQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FInterfaceRegistryTransactionSource>()
				.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
				.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
				.AddStep<FTransformInterfaceRegistryTransactionToInterface>()
				.AddStep<FMapInterfaceToInterfaceNameAndMajorVersion>()
				.AddStep<FReduceInterfacesToHighestVersion>()
				.AddStep<FRemoveInterfacesWhichAreNotDefault>()
				.AddStep<FMapToNull>();
			return Query;
		}

		TArray<FMetasoundFrontendInterface> FFindAllDefaultInterfacesQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfInterfacesFromPartition(InPartition);
		}

		FFrontendQuery FFindAllRegisteredInterfacesWithNameQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FInterfaceRegistryTransactionSource>()
				.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
				.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
				.AddStep<FMapInterfaceRegistryTransactionToInterfaceName>()
				.AddStep<FTransformInterfaceRegistryTransactionToInterfaceVersion>();
			return Query;
		}

		TArray<FMetasoundFrontendVersion> FFindAllRegisteredInterfacesWithNameQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfVersionsFromPartition(InPartition);
		}

		FFrontendQuery FFindInterfaceWithHighestVersionQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FInterfaceRegistryTransactionSource>()
				.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
				.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
				.AddStep<FTransformInterfaceRegistryTransactionToInterface>()
				.AddStep<FMapInterfaceToInterfaceName>()
				.AddStep<FReduceInterfacesToHighestVersion>();
			return Query;
		}

		FMetasoundFrontendInterface FFindInterfaceWithHighestVersionQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildSingleInterfaceFromPartition(InPartition);
		}

		void FSearchEngineCore::Prime()
		{
			METASOUND_LLM_SCOPE;
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::Prime);

			FindClassWithHighestMinorVersionQuery.Prime();
			FindAllDefaultInterfacesQuery.Prime();
			FindAllRegisteredInterfacesWithNameQuery.Prime();
			FindInterfaceWithHighestVersionQuery.Prime();
		}

		bool FSearchEngineCore::FindClassWithHighestMinorVersion(const FMetasoundFrontendClassName& InName, int32 InMajorVersion, FMetasoundFrontendClass& OutClass)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::FindClassWithHighestMinorVersion);
			using namespace SearchEngineQuerySteps;

			const FFrontendQueryKey QueryKey = CreateKeyFromFullClassNameAndMajorVersion(InName, InMajorVersion);
			FNodeRegistryKey NodeRegistryKey; 
			if (FindClassWithHighestMinorVersionQuery.UpdateAndFindResult(QueryKey, NodeRegistryKey))
			{
				if (FMetasoundFrontendRegistryContainer* NodeRegistry = FMetasoundFrontendRegistryContainer::Get())
				{
					return NodeRegistry->FindFrontendClassFromRegistered(NodeRegistryKey, OutClass);
				}
			}
			return false;
		}

		TArray<FMetasoundFrontendInterface> FSearchEngineCore::FindUClassDefaultInterfaces(FName InUClassName)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::FindUClassDefaultInterfaces);

			const FFrontendQueryKey NullKey;
			TArray<FMetasoundFrontendInterface> DefaultInterfaces = FindAllDefaultInterfacesQuery.UpdateAndFindResult(NullKey);

			// All default interfaces are filtered here because a UClass name cannot effectively 
			// be used as a "Key" for this query.
			//
			// We cannot know all the UClasses associated with each default interface given only a UClassName
			// because we need to inspect UClass inheritance to handle derived UClasses
			// which should be compatible with their parent's default interfaces. 
			//
			// This module does not have access to the UClass inheritance structure.
			// Instead, this information is accessed in the IInterfaceRegistryEntry
			// concrete implementation. 
			auto DoesNotSupportUClass = [&InUClassName](const FMetasoundFrontendInterface& InInterface)
			{
				FInterfaceRegistryKey RegistryKey = GetInterfaceRegistryKey(InInterface);
				const IInterfaceRegistryEntry* RegEntry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(RegistryKey);
				if (ensure(RegEntry))
				{
					return !RegEntry->UClassIsSupported(InUClassName);
				}

				return true;
			};

			DefaultInterfaces.RemoveAllSwap(DoesNotSupportUClass);

			return DefaultInterfaces;
		}

		TArray<FMetasoundFrontendVersion> FSearchEngineCore::FindAllRegisteredInterfacesWithName(FName InInterfaceName)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::FindAllRegisteredInterfacesWithName);

			const FFrontendQueryKey Key{InInterfaceName};
			return FindAllRegisteredInterfacesWithNameQuery.UpdateAndFindResult(Key);
		}

		bool FSearchEngineCore::FindInterfaceWithHighestVersion(FName InInterfaceName, FMetasoundFrontendInterface& OutInterface)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::FindInterfaceWithHighestVersion);

			const FFrontendQueryKey Key{InInterfaceName};
			return FindInterfaceWithHighestVersionQuery.UpdateAndFindResult(Key, OutInterface);
		}
	}
}
