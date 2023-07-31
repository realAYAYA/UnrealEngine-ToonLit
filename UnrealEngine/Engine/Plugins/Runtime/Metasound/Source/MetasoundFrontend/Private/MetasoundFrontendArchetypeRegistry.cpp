// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendInterfaceRegistryPrivate.h"

#include "HAL/PlatformTime.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundTrace.h"

namespace Metasound
{
	namespace Frontend
	{
		FInterfaceRegistry::FInterfaceRegistry()
		: TransactionBuffer(MakeShared<TTransactionBuffer<FInterfaceRegistryTransaction>>())
		{
		}

		void FInterfaceRegistry::RegisterInterface(TUniquePtr<IInterfaceRegistryEntry>&& InEntry)
		{
					METASOUND_LLM_SCOPE;

			FInterfaceRegistryTransaction::FTimeType TransactionTime = FPlatformTime::Cycles64();
			if (InEntry.IsValid())
			{
				FInterfaceRegistryKey Key = GetInterfaceRegistryKey(InEntry->GetInterface());
				if (IsValidInterfaceRegistryKey(Key))
				{
					if (const IInterfaceRegistryEntry* Entry = FindInterfaceRegistryEntry(Key))
					{
						UE_LOG(LogMetaSound, Warning, TEXT("Registration of interface overwriting previously registered interface [RegistryKey: %s]"), *Key);
						
						FInterfaceRegistryTransaction Transaction{FInterfaceRegistryTransaction::ETransactionType::InterfaceUnregistration, Key, Entry->GetInterface().Version, TransactionTime};
						TransactionBuffer->AddTransaction(MoveTemp(Transaction));
					}
					
					FInterfaceRegistryTransaction Transaction{FInterfaceRegistryTransaction::ETransactionType::InterfaceRegistration, Key, InEntry->GetInterface().Version, TransactionTime};
					TransactionBuffer->AddTransaction(MoveTemp(Transaction));

					Entries.Add(Key, MoveTemp(InEntry)).Get();
				}
			}
		}

		const IInterfaceRegistryEntry* FInterfaceRegistry::FindInterfaceRegistryEntry(const FInterfaceRegistryKey& InKey) const
		{
			if (const TUniquePtr<IInterfaceRegistryEntry>* Entry = Entries.Find(InKey))
			{
				return Entry->Get();
			}
			return nullptr;
		}

		bool FInterfaceRegistry::FindInterface(const FInterfaceRegistryKey& InKey, FMetasoundFrontendInterface& OutInterface) const
		{
			if (const IInterfaceRegistryEntry* Entry = FindInterfaceRegistryEntry(InKey))
			{
				OutInterface = Entry->GetInterface();
				return true;
			}

			return false;
		}

		TUniquePtr<FInterfaceTransactionStream> FInterfaceRegistry::CreateTransactionStream()
		{
			return MakeUnique<FInterfaceTransactionStream>(TransactionBuffer);
		}

		void FInterfaceRegistry::ForEachRegistryTransactionSince(FRegistryTransactionID InSince, FRegistryTransactionID* OutCurrentRegistryTransactionID, TFunctionRef<void(const FInterfaceRegistryTransaction&)> InFunc) const
		{
			UE_LOG(LogMetaSound, Error, TEXT("IInterfaceRegistry::ForEachRegistryTransactionSince is no longer supported and should not be called"));
		}


		bool IsValidInterfaceRegistryKey(const FInterfaceRegistryKey& InKey)
		{
			return !InKey.IsEmpty();
		}

		FInterfaceRegistryKey GetInterfaceRegistryKey(const FMetasoundFrontendVersion& InInterfaceVersion)
		{
			return FString::Format(TEXT("{0}_{1}.{2}"), { InInterfaceVersion.Name.ToString(), InInterfaceVersion.Number.Major, InInterfaceVersion.Number.Minor });

		}

		FInterfaceRegistryKey GetInterfaceRegistryKey(const FMetasoundFrontendInterface& InInterface)
		{
			return GetInterfaceRegistryKey(InInterface.Version);
		}

		FInterfaceRegistryTransaction::FInterfaceRegistryTransaction(ETransactionType InType, const FInterfaceRegistryKey& InKey, const FMetasoundFrontendVersion& InInterfaceVersion, FInterfaceRegistryTransaction::FTimeType InTimestamp)
		: Type(InType)
		, Key(InKey)
		, InterfaceVersion(InInterfaceVersion)
		, Timestamp(InTimestamp)
		{
		}

		FInterfaceRegistryTransaction::ETransactionType FInterfaceRegistryTransaction::GetTransactionType() const
		{
			return Type;
		}

		const FMetasoundFrontendVersion& FInterfaceRegistryTransaction::GetInterfaceVersion() const
		{
			return InterfaceVersion;
		}

		const FInterfaceRegistryKey& FInterfaceRegistryTransaction::GetInterfaceRegistryKey() const
		{
			return Key;
		}

		FInterfaceRegistryTransaction::FTimeType FInterfaceRegistryTransaction::GetTimestamp() const
		{
			return Timestamp;
		}

		FInterfaceRegistry& FInterfaceRegistry::Get()
		{
			static FInterfaceRegistry Registry;
			return Registry;
		}

		IInterfaceRegistry& IInterfaceRegistry::Get()
		{
			return FInterfaceRegistry::Get();
		}
	}
}
