// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PackageTransmissionFrontendFilter_TextSearch.h"
#include "Widgets/Util/Filter/ConcertRootFrontendFilter.h"

class FEndpointToUserNameCache;
class SWidget;

namespace UE::MultiUserServer
{
	class FPackageTransmissionFrontendFilter_TextSearch;
	class FPackageTransmissionEntryTokenizer;

	/** Root filter displayed in package transmission UI */
	class FPackageTransmissionFilter_FrontendRoot : public TConcertFrontendRootFilter<const FPackageTransmissionEntry&, FPackageTransmissionFrontendFilter_TextSearch>
	{
		using Super = TConcertFrontendRootFilter<const FPackageTransmissionEntry&, FPackageTransmissionFrontendFilter_TextSearch>;
	public:

		FPackageTransmissionFilter_FrontendRoot(
			TSharedRef<FPackageTransmissionEntryTokenizer> Tokenizer,
			TArray<TSharedRef<TConcertFrontendFilter<const FPackageTransmissionEntry&>>> FrontendFilters,
			const TArray<TSharedRef<IFilter<const FPackageTransmissionEntry&>>>& NonVisualFilters = {},
			TSharedRef<FFilterCategory> DefaultCategory = MakeShared<FFilterCategory>(NSLOCTEXT("UnrealMultiUserServer.TConcertFrontendRootFilter", "DefaultCategoryLabel", "Default"), FText::GetEmpty())
			)
			: Super(MakeShared<FPackageTransmissionFrontendFilter_TextSearch>(DefaultCategory, MoveTemp(Tokenizer)), MoveTemp(FrontendFilters), NonVisualFilters, DefaultCategory)
		{}
	};

	/** Creates a filter for the global filter log window */
	TSharedRef<FPackageTransmissionFilter_FrontendRoot> MakeFilter(TSharedRef<FPackageTransmissionEntryTokenizer> Tokenizer);
}
