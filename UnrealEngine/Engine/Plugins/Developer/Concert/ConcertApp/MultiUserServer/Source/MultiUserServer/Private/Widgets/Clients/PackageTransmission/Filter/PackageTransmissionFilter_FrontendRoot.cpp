// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageTransmissionFilter_FrontendRoot.h"

#include "Widgets/Clients/PackageTransmission/Filter/PackageTransmissionFrontendFilter_TextSearch.h"


namespace UE::MultiUserServer
{
	TSharedRef<FPackageTransmissionFilter_FrontendRoot> MakeFilter(TSharedRef<FPackageTransmissionEntryTokenizer> Tokenizer)
	{
		TArray<TSharedRef<TConcertFrontendFilter<const FPackageTransmissionEntry&>>> FrontendFilters;
		return MakeShared<FPackageTransmissionFilter_FrontendRoot>(MoveTemp(Tokenizer), FrontendFilters);
	}
}
