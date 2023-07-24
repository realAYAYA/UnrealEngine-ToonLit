// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetasoundFrontendRegistryTransaction.h"

namespace Metasound
{
	namespace Frontend
	{
		int32 MetaSoundFrontendDiscardStreamedRegistryTransactionsCVar = 1;

		FAutoConsoleVariableRef CVarMetaSoundFrontendDiscardStreamedRegistryTransactions(
			TEXT("au.MetaSound.Frontend.DiscardStreamedRegistryTransactions"),
			MetaSoundFrontendDiscardStreamedRegistryTransactionsCVar,
			TEXT("If enabled, MetaSound registry transactions are discarded after they have been streamed.\n")
			TEXT("0: Disabled, !0: Enabled (default)"),
			ECVF_Default);
	}
}

