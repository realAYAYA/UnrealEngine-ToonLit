// Copyright Epic Games, Inc. All Rights Reserved.

#include "SelectionSystem/DataprepBoolFilter.h"

#include "DataprepCoreLogCategory.h"
#include "SelectionSystem/DataprepBoolFetcher.h"
#include "SelectionSystem/DataprepSelectionSystemStructs.h"
#include "SelectionSystem/DataprepSelectionSystemUtils.h"

#include "Containers/ArrayView.h"

bool UDataprepBoolFilter::Filter(const bool bResult) const
{
	// Simply exist to reuse the templated function in FilterObjects
	return bResult;
}

TArray<UObject*> UDataprepBoolFilter::FilterObjects(const TArrayView<UObject*>& Objects) const
{
	if ( BoolFetcher )
	{
		return DataprepSelectionSystemUtils::FilterObjects< UDataprepBoolFilter, UDataprepBoolFetcher, bool >( *this, * BoolFetcher, Objects );
	}

	UE_LOG( LogDataprepCore, Error, TEXT("UDataprepBoolFilter::FilterObjects: There was no Fetcher") );
	return {};
}

void UDataprepBoolFilter::FilterAndGatherInfo(const TArrayView<UObject*>& InObjects, const TArrayView<FDataprepSelectionInfo>& OutFilterResults) const
{
	if ( BoolFetcher )
	{
		return DataprepSelectionSystemUtils::FilterAndGatherInfo< UDataprepBoolFilter, UDataprepBoolFetcher, bool >( *this, *BoolFetcher, InObjects, OutFilterResults );
	}

	UE_LOG( LogDataprepCore, Error, TEXT("UDataprepBoolFilter::FilterAndGatherInfo: There was no Fetcher") );
}

void UDataprepBoolFilter::FilterAndStoreInArrayView(const TArrayView<UObject *>& InObjects, const TArrayView<bool>& OutFilterResults) const
{
	if ( BoolFetcher )
	{
		return DataprepSelectionSystemUtils::FilterAndStoreInArrayView< UDataprepBoolFilter, UDataprepBoolFetcher, bool >( *this, *BoolFetcher, InObjects, OutFilterResults );
	}

	UE_LOG( LogDataprepCore, Error, TEXT("UDataprepBoolFilter::FilterAndStoreInArrayView: There was no Fetcher") );
}

FText UDataprepBoolFilter::GetFilterCategoryText() const
{
	return NSLOCTEXT("DataprepBoolFilter", "BoolFilterCategory", "Condition");
}

TSubclassOf<UDataprepFetcher> UDataprepBoolFilter::GetAcceptedFetcherClass() const
{
	return UDataprepBoolFetcher::StaticClass();
}

void UDataprepBoolFilter::SetFetcher(const TSubclassOf<UDataprepFetcher>& FetcherClass)
{
		UClass* NewFetcherClass = FetcherClass;

	if ( NewFetcherClass && NewFetcherClass->IsChildOf( GetAcceptedFetcherClass() ) )
	{
		UClass* OldFetcherClass = BoolFetcher ? BoolFetcher->GetClass() : nullptr;
		if ( NewFetcherClass != OldFetcherClass )
		{
			Modify();
			BoolFetcher = NewObject< UDataprepBoolFetcher >( this, NewFetcherClass, NAME_None, RF_Transactional );
		}
	}
	else
	{
		ensure( false );
		UE_LOG( LogDataprepCore, Error, TEXT("UDataprepBoolFilter::SetFetcher: The Fetcher Class is not compatible") );
	}
}

const UDataprepFetcher* UDataprepBoolFilter::GetFetcherImplementation() const
{
	return BoolFetcher;
}
