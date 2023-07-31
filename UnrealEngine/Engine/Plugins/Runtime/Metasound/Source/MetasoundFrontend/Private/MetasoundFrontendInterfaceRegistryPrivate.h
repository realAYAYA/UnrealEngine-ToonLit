// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "UObject/NoExportTypes.h"

namespace Metasound
{
	namespace Frontend
	{
		using FInterfaceTransactionStream = TTransactionStream<FInterfaceRegistryTransaction>;

		class FInterfaceRegistry : public IInterfaceRegistry
		{
		public:

			static FInterfaceRegistry& Get();

			FInterfaceRegistry();

			virtual ~FInterfaceRegistry() = default;

			virtual void RegisterInterface(TUniquePtr<IInterfaceRegistryEntry>&& InEntry) override;

			virtual const IInterfaceRegistryEntry* FindInterfaceRegistryEntry(const FInterfaceRegistryKey& InKey) const override;

			virtual bool FindInterface(const FInterfaceRegistryKey& InKey, FMetasoundFrontendInterface& OutInterface) const override;

			TUniquePtr<FInterfaceTransactionStream> CreateTransactionStream();

			virtual void ForEachRegistryTransactionSince(FRegistryTransactionID InSince, FRegistryTransactionID* OutCurrentRegistryTransactionID, TFunctionRef<void(const FInterfaceRegistryTransaction&)> InFunc) const override;
		private:
			using FInterfaceTransactionBuffer = TTransactionBuffer<FInterfaceRegistryTransaction>;

			TMap<FInterfaceRegistryKey, TUniquePtr<IInterfaceRegistryEntry>> Entries;
			TSharedRef<FInterfaceTransactionBuffer> TransactionBuffer;
		};
	}
}

