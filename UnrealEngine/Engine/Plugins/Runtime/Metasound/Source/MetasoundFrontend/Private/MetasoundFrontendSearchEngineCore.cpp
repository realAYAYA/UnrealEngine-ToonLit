// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendSearchEngineCore.h"

#include "Algo/MaxElement.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistryPrivate.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundTrace.h"

namespace Metasound::Frontend
{
	namespace SearchEngineQuerySteps
	{
		FFrontendQueryKey CreateKeyFromFullClassNameAndMajorVersion(const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion)
		{
			return FFrontendQueryKey(FString::Format(TEXT("{0}_v{1}"), {InClassName.ToString(), InMajorVersion}));
		}

		FFrontendQueryKey CreateKeyFromUClassPathName(const FTopLevelAssetPath& InClassPath)
		{
			return FFrontendQueryKey(InClassPath.ToString());
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

		void FTransformInterfaceToInterfaceVersion::Transform(FFrontendQueryEntry::FValue& InValue) const
		{
			const FMetasoundFrontendInterface* Interface = InValue.TryGet<FMetasoundFrontendInterface>();
			if (ensure(Interface))
			{
				InValue.Set<FMetasoundFrontendVersion>(Interface->Version);
			}
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

		TArray<FFrontendQueryKey> FMapUClassToDefaultInterface::Map(const FFrontendQueryEntry& InEntry) const
		{
			TArray<FFrontendQueryKey> Keys;
			if (const FMetasoundFrontendInterface* Interface = InEntry.Value.TryGet<FMetasoundFrontendInterface>())
			{
				auto IsDefaultInterface = [](const FMetasoundFrontendInterfaceUClassOptions& InOptions)
				{
					return InOptions.bIsDefault;
				};
				auto GetUClassQueryKey = [](const FMetasoundFrontendInterfaceUClassOptions& InOptions)
				{
					return CreateKeyFromUClassPathName(InOptions.ClassPath);
				};
				Algo::TransformIf(Interface->UClassOptions, Keys, IsDefaultInterface, GetUClassQueryKey);
			}

			return Keys;
		}

		bool FRemoveDeprecatedClasses::Filter(const FFrontendQueryEntry& InEntry) const
		{
			if (InEntry.Value.IsType<FMetasoundFrontendClass>())
			{
				return !InEntry.Value.Get<FMetasoundFrontendClass>().Metadata.GetIsDeprecated();
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
	} // namespace SearchEngineQuerySteps


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
		FNodeRegistryKey Key = FNodeRegistryKey::GetInvalid();
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

	FFrontendQuery FFindAllInterfacesQueryPolicy::CreateQuery()
	{
		using namespace SearchEngineQuerySteps;
		FFrontendQuery Query;
		Query.AddStep<FInterfaceRegistryTransactionSource>()
			.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
			.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
			.AddStep<FTransformInterfaceRegistryTransactionToInterface>()
			.AddStep<FMapInterfaceToInterfaceNameAndMajorVersion>()
			.AddStep<FReduceInterfacesToHighestVersion>()
			.AddStep<FMapToNull>();
		return Query;
	}

	TArray<FMetasoundFrontendInterface> FFindAllInterfacesQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
	{
		using namespace SearchEngineQuerySteps;
		return BuildArrayOfInterfacesFromPartition(InPartition);
	}

	FFrontendQuery FFindAllInterfacesIncludingAllVersionsQueryPolicy::CreateQuery()
	{
		using namespace SearchEngineQuerySteps;
		FFrontendQuery Query;
		Query.AddStep<FInterfaceRegistryTransactionSource>()
			.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
			.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
			.AddStep<FTransformInterfaceRegistryTransactionToInterface>()
			.AddStep<FMapToNull>();
		return Query;
	}

	TArray<FMetasoundFrontendInterface> FFindAllInterfacesIncludingAllVersionsQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
	{
		using namespace SearchEngineQuerySteps;
		return BuildArrayOfInterfacesFromPartition(InPartition);
	}

	FFrontendQuery FFindAllDefaultInterfaceVersionsForUClassQueryPolicy::CreateQuery()
	{
		using namespace SearchEngineQuerySteps;
		FFrontendQuery Query;
		Query.AddStep<FInterfaceRegistryTransactionSource>()
			.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
			.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
			.AddStep<FTransformInterfaceRegistryTransactionToInterface>()
			.AddStep<FMapUClassToDefaultInterface>()
			.AddStep<FTransformInterfaceToInterfaceVersion>();
		return Query;
	}

	TArray<FMetasoundFrontendVersion> FFindAllDefaultInterfaceVersionsForUClassQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
	{
		using namespace SearchEngineQuerySteps;
		return BuildArrayOfVersionsFromPartition(InPartition);
	}

	void FSearchEngineCore::Prime()
	{
		METASOUND_LLM_SCOPE;
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::Prime);

		FindClassWithHighestMinorVersionQuery.Prime();
		FindAllRegisteredInterfacesWithNameQuery.Prime();
		FindInterfaceWithHighestVersionQuery.Prime();
		FindAllDefaultInterfaceVersionsForUClassQuery.Prime();
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
		using namespace SearchEngineQuerySteps;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::FindUClassDefaultInterfaces);

		TArray<FMetasoundFrontendInterface> Interfaces;
		TArray<FMetasoundFrontendVersion> InterfaceVersions = FindUClassDefaultInterfaceVersions(FTopLevelAssetPath(InUClassName.ToString()));
		Algo::Transform(InterfaceVersions, Interfaces, [](const FMetasoundFrontendVersion& Version)
		{
			FMetasoundFrontendInterface Interface;
			const FInterfaceRegistryKey Key = GetInterfaceRegistryKey(Version);
			ensure(IInterfaceRegistry::Get().FindInterface(Key, Interface));
			return Interface;
		});

		return Interfaces;
	}

	TArray<FMetasoundFrontendVersion> FSearchEngineCore::FindUClassDefaultInterfaceVersions(const FTopLevelAssetPath& InUClassPath)
	{
		using namespace SearchEngineQuerySteps;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineCore::FindUClassDefaultInterfaceVersions);

		const FFrontendQueryKey Key = CreateKeyFromUClassPathName(InUClassPath);
		return FindAllDefaultInterfaceVersionsForUClassQuery.UpdateAndFindResult(Key);
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
} // namespace Metasound::Frontend
