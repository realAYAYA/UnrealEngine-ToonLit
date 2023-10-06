// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendQuerySteps.h"

#include "Algo/MaxElement.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendNodeRegistryPrivate.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundLog.h"

namespace Metasound
{
	class FNodeClassRegistrationEventsPimpl : public IFrontendQueryStreamStep
	{
	public:
		FNodeClassRegistrationEventsPimpl()
		{
			TransactionStream = Frontend::FRegistryContainerImpl::Get().CreateTransactionStream();
		}

		virtual void Stream(TArray<FFrontendQueryValue>& OutValues) override
		{
			using namespace Frontend;

			auto AddEntry = [&OutValues](const FNodeRegistryTransaction& InTransaction)
			{
				OutValues.Emplace(TInPlaceType<FNodeRegistryTransaction>(), InTransaction);
			};

			if (TransactionStream.IsValid())
			{
				TransactionStream->Stream(AddEntry);
			}
		}

	private:
		TUniquePtr<Frontend::FNodeRegistryTransactionStream> TransactionStream;
	};

	FNodeClassRegistrationEvents::FNodeClassRegistrationEvents()
	: Pimpl(MakePimpl<FNodeClassRegistrationEventsPimpl>())
	{
	}

	void FNodeClassRegistrationEvents::Stream(TArray<FFrontendQueryValue>& OutValues)
	{
		Pimpl->Stream(OutValues);
	}

	FFrontendQueryKey FMapRegistrationEventsToNodeRegistryKeys::Map(const FFrontendQueryEntry& InEntry) const 
	{
		using namespace Frontend;

		FNodeRegistryKey RegistryKey;

		if (ensure(InEntry.Value.IsType<FNodeRegistryTransaction>()))
		{
			RegistryKey = InEntry.Value.Get<FNodeRegistryTransaction>().GetNodeRegistryKey();
		}

		return FFrontendQueryKey(RegistryKey);
	}

	void FReduceRegistrationEventsToCurrentStatus::Reduce(const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries) const
	{
		using namespace Frontend;
		if (InOutEntries.IsEmpty())
		{
			return;
		}

		const FFrontendQueryEntry* FinalEntry = Algo::MaxElementBy(InOutEntries, GetTransactionTimestamp);
		// If last entry is registration, keep the last entry 
		// since that's the most relevant up to date status
		if (IsValidTransactionOfType(FNodeRegistryTransaction::ETransactionType::NodeRegistration, FinalEntry))
		{
			FFrontendQueryEntry Entry = *FinalEntry;
			InOutEntries.Reset();
			InOutEntries.Add(Entry);
		} 
		else if (IsValidTransactionOfType(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, FinalEntry))
		{
			// Check that pairs of entries are Registration - Unregistration
			auto CheckRegistrationTransactionPairs = [](FFrontendQueryPartition& InOutEntries, int32 EndIndexInclusive)
			{
				InOutEntries.Sort([](const FFrontendQueryEntry& A, const FFrontendQueryEntry& B)
				{
					return GetTransactionTimestamp(A) < GetTransactionTimestamp(B);
				});
				for (int32 PairIndex = 0; PairIndex < EndIndexInclusive; PairIndex += 2)
				{
					const FNodeRegistryTransaction& FirstEntry = InOutEntries[PairIndex].Value.Get<FNodeRegistryTransaction>();
					const FNodeRegistryTransaction& SecondEntry = InOutEntries[PairIndex + 1].Value.Get<FNodeRegistryTransaction>();
					const FNodeRegistryTransaction::ETransactionType FirstEntryTransactionType = FirstEntry.GetTransactionType();
					const FNodeRegistryTransaction::ETransactionType SecondEntryTransactionType = SecondEntry.GetTransactionType();

					if (!(FirstEntryTransactionType == FNodeRegistryTransaction::ETransactionType::NodeRegistration &&
						SecondEntryTransactionType == FNodeRegistryTransaction::ETransactionType::NodeUnregistration))
					{
						UE_LOG(LogMetaSound, Warning, TEXT("Mismatched transaction entries with keys '%s' and '%s' and transaction types '%s' and '%s' in a search engine query.\
							Transactions should be registration and unregistration pairs except for the last entry."),
							*FirstEntry.GetNodeRegistryKey(),
							*SecondEntry.GetNodeRegistryKey(),
							*FNodeRegistryTransaction::LexToString(FirstEntryTransactionType),
							*FNodeRegistryTransaction::LexToString(SecondEntryTransactionType));
					}
				}
			};
			// If odd number of entries, check previous pairs and keep the final entry
			if (InOutEntries.Num() % 2 == 1)
			{
				FFrontendQueryEntry Entry = *FinalEntry;
				CheckRegistrationTransactionPairs(InOutEntries, FMath::Max(0, InOutEntries.Num() - 2));
				InOutEntries.Reset();
				InOutEntries.Add(Entry);
			}
			// If even, check all pairs  
			// and clear them all, since the current status is as if those hadn't happened
			else
			{
				CheckRegistrationTransactionPairs(InOutEntries, InOutEntries.Num() - 1);
				InOutEntries.Reset();
			}
		}
		else
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Missing FNodeRegistryTransaction::ETransactionType case in search engine FReduceRegistrationEventsToCurrentStatus::Reduce."))
		}

	}

	FReduceRegistrationEventsToCurrentStatus::FTimeType FReduceRegistrationEventsToCurrentStatus::GetTransactionTimestamp(const FFrontendQueryEntry& InEntry)
	{
		using namespace Frontend;

		if (ensure(InEntry.Value.IsType<FNodeRegistryTransaction>()))
		{
			return InEntry.Value.Get<FNodeRegistryTransaction>().GetTimestamp();
		}
		return 0;
	}

	bool FReduceRegistrationEventsToCurrentStatus::IsValidTransactionOfType(Frontend::FNodeRegistryTransaction::ETransactionType InType, const FFrontendQueryEntry* InEntry)
	{
		using namespace Frontend;

		if (nullptr != InEntry)
		{
			if (InEntry->Value.IsType<FNodeRegistryTransaction>())
			{
				return InEntry->Value.Get<FNodeRegistryTransaction>().GetTransactionType() == InType;
			}
		}
		return false;
	}

	void FTransformRegistrationEventsToClasses::Transform(FFrontendQueryEntry::FValue& InValue) const
	{
		using namespace Frontend;

		FMetasoundFrontendClass FrontendClass;

		if (ensure(InValue.IsType<FNodeRegistryTransaction>()))
		{
			const FNodeRegistryTransaction& Transaction = InValue.Get<FNodeRegistryTransaction>();
			
			if (Transaction.GetTransactionType() == Frontend::FNodeRegistryTransaction::ETransactionType::NodeRegistration)
			{
				// It's possible that the node is no longer registered (we're processing removals) 
				// but that's okay because the returned default FrontendClass will be processed out later
				FMetasoundFrontendRegistryContainer::Get()->FindFrontendClassFromRegistered(Transaction.GetNodeRegistryKey(), FrontendClass);
			}
		}
		InValue.Set<FMetasoundFrontendClass>(MoveTemp(FrontendClass));
	}

	FFilterClassesByInputVertexDataType::FFilterClassesByInputVertexDataType(const FName& InTypeName)
	:	InputVertexTypeName(InTypeName)
	{
	}

	bool FFilterClassesByInputVertexDataType::Filter(const FFrontendQueryEntry& InEntry) const
	{
		check(InEntry.Value.IsType<FMetasoundFrontendClass>());

		return InEntry.Value.Get<FMetasoundFrontendClass>().Interface.Inputs.ContainsByPredicate(
			[this](const FMetasoundFrontendClassInput& InDesc)
			{
				return InDesc.TypeName == InputVertexTypeName;
			}
		);
	}

	FFilterClassesByOutputVertexDataType::FFilterClassesByOutputVertexDataType(const FName& InTypeName)
	:	OutputVertexTypeName(InTypeName)
	{
	}

	bool FFilterClassesByOutputVertexDataType::Filter(const FFrontendQueryEntry& InEntry) const
	{
		return InEntry.Value.Get<FMetasoundFrontendClass>().Interface.Outputs.ContainsByPredicate(
			[this](const FMetasoundFrontendClassOutput& InDesc)
			{
				return InDesc.TypeName == OutputVertexTypeName;
			}
		);
	}

	FFrontendQueryKey FMapClassesToClassName::Map(const FFrontendQueryEntry& InEntry) const 
	{
		return FFrontendQueryKey(InEntry.Value.Get<FMetasoundFrontendClass>().Metadata.GetClassName().GetFullName());
	}

	FFilterClassesByClassID::FFilterClassesByClassID(const FGuid InClassID)
		: ClassID(InClassID)
	{
	}

	bool FFilterClassesByClassID::Filter(const FFrontendQueryEntry& InEntry) const
	{
		return InEntry.Value.Get<FMetasoundFrontendClass>().ID == ClassID;
	}

	FFrontendQueryKey FMapToFullClassName::Map(const FFrontendQueryEntry& InEntry) const
	{
		const FMetasoundFrontendClass& FrontendClass = InEntry.Value.Get<FMetasoundFrontendClass>();
		return FFrontendQueryKey(FrontendClass.Metadata.GetClassName().GetFullName());
	}

	void FReduceClassesToHighestVersion::Reduce(const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries) const
	{
		FFrontendQueryEntry* HighestVersionEntry = nullptr;
		FMetasoundFrontendVersionNumber HighestVersion;

		for (FFrontendQueryEntry& Entry : InOutEntries)
		{
			const FMetasoundFrontendVersionNumber& Version = Entry.Value.Get<FMetasoundFrontendClass>().Metadata.GetVersion();

			if (!HighestVersionEntry || HighestVersion < Version)
			{
				HighestVersionEntry = &Entry;
				HighestVersion = Version;
			}
		}

		if (HighestVersionEntry)
		{
			FFrontendQueryEntry Entry = *HighestVersionEntry;
			InOutEntries.Reset();
			InOutEntries.Add(Entry);
		}
	}

	bool FSortClassesByVersion::Sort(const FFrontendQueryEntry& InEntryLHS, const FFrontendQueryEntry& InEntryRHS) const
	{
		const FMetasoundFrontendVersionNumber& VersionLHS = InEntryLHS.Value.Get<FMetasoundFrontendClass>().Metadata.GetVersion();
		const FMetasoundFrontendVersionNumber& VersionRHS = InEntryRHS.Value.Get<FMetasoundFrontendClass>().Metadata.GetVersion();
		return VersionLHS > VersionRHS;
	}
}
