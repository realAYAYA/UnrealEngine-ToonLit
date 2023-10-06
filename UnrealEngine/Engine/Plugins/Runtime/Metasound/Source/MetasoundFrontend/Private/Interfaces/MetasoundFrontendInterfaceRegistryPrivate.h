// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "UObject/NoExportTypes.h"
#include "UObject/TopLevelAssetPath.h"


namespace Metasound::Frontend
{
	using FInterfaceTransactionStream = TTransactionStream<FInterfaceRegistryTransaction>;

	class FInterfaceRegistry : public IInterfaceRegistry
	{
	public:
		static FInterfaceRegistry& Get();

		FInterfaceRegistry();

		virtual ~FInterfaceRegistry() = default;

		virtual bool RegisterInterface(TUniquePtr<IInterfaceRegistryEntry>&& InEntry) override;
		virtual const IInterfaceRegistryEntry* FindInterfaceRegistryEntry(const FInterfaceRegistryKey& InKey) const override;
		virtual bool FindInterface(const FInterfaceRegistryKey& InKey, FMetasoundFrontendInterface& OutInterface) const override;

		TUniquePtr<FInterfaceTransactionStream> CreateTransactionStream();

	private:
		using FInterfaceTransactionBuffer = TTransactionBuffer<FInterfaceRegistryTransaction>;

		TMap<FInterfaceRegistryKey, TUniquePtr<IInterfaceRegistryEntry>> Entries;

		TSharedRef<FInterfaceTransactionBuffer> TransactionBuffer;
	};
} // namespace Metasound::Frontend
