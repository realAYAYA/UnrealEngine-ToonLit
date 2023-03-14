// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMultiUserModule.h"

#include "IConcertClientTransactionBridge.h"
#include "IConcertSyncClientModule.h"
#include "LiveLinkControllerBase.h"


namespace LiveLinkMultiUserUtils
{
	ETransactionFilterResult HandleTransactionFiltering(UObject* ObjectToFilter, UPackage* ObjectsPackage)
	{
		//Always allow LiveLink controllers base class
		if(Cast<ULiveLinkControllerBase>(ObjectToFilter) != nullptr)
		{
			return ETransactionFilterResult::IncludeObject;
		}

		return ETransactionFilterResult::UseDefault;
	}
}

void FLiveLinkMultiUserModule::StartupModule()
{
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertClientTransactionBridge& TransactionBridge = IConcertSyncClientModule::Get().GetTransactionBridge();
		TransactionBridge.RegisterTransactionFilter( TEXT("LiveLinkTransactionFilter"), FTransactionFilterDelegate::CreateStatic(&LiveLinkMultiUserUtils::HandleTransactionFiltering));
	}
}

void FLiveLinkMultiUserModule::ShutdownModule()
{
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertClientTransactionBridge& TransactionBridge = IConcertSyncClientModule::Get().GetTransactionBridge();
		TransactionBridge.UnregisterTransactionFilter( TEXT("LiveLinkTransactionFilter"));
	}	
}

IMPLEMENT_MODULE(FLiveLinkMultiUserModule, LiveLinkMultiUser);
