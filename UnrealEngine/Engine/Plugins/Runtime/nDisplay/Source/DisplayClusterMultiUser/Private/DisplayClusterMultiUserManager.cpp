// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterMultiUserManager.h"
#include "DisplayClusterMultiUserLog.h"

#include "DisplayClusterConfigurationTypes_Base.h"

#include "IConcertClientTransactionBridge.h"
#include "IDisplayClusterConfiguration.h"

#include "IConcertSyncClient.h"
#include "IConcertSyncClientModule.h"

#define NDISPLAY_MULTIUSER_TRANSACTION_FILTER TEXT("DisplayClusterMultiUser")

FDisplayClusterMultiUserManager::FDisplayClusterMultiUserManager()
{
	const TSharedPtr<IConcertSyncClient> ConcertSyncClient = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
	if (ConcertSyncClient.IsValid())
	{
		IConcertClientTransactionBridge* Bridge = ConcertSyncClient->GetTransactionBridge();
		check(Bridge != nullptr);

		Bridge->RegisterTransactionFilter(NDISPLAY_MULTIUSER_TRANSACTION_FILTER,
			FTransactionFilterDelegate::CreateRaw(this, &FDisplayClusterMultiUserManager::ShouldObjectBeTransacted));
		Bridge->OnApplyTransaction().AddRaw(this, &FDisplayClusterMultiUserManager::OnApplyRemoteTransaction);
	}
}

FDisplayClusterMultiUserManager::~FDisplayClusterMultiUserManager()
{
	const TSharedPtr<IConcertSyncClient> ConcertSyncClient = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
	if (ConcertSyncClient.IsValid())
	{
		IConcertClientTransactionBridge* Bridge = ConcertSyncClient->GetTransactionBridge();
		check(Bridge != nullptr);

		Bridge->UnregisterTransactionFilter(NDISPLAY_MULTIUSER_TRANSACTION_FILTER);
		Bridge->OnApplyTransaction().RemoveAll(this);
	}
}

void FDisplayClusterMultiUserManager::OnApplyRemoteTransaction(ETransactionNotification Notification, const bool bIsSnapshot)
{
	IDisplayClusterConfiguration& Config = IDisplayClusterConfiguration::Get();
	if (Notification == ETransactionNotification::Begin && bIsSnapshot)
	{
		Config.SetIsSnapshotTransacting(true);
	}
	else if (bIsSnapshot)
	{
		Config.SetIsSnapshotTransacting(false);
	}
}

ETransactionFilterResult FDisplayClusterMultiUserManager::ShouldObjectBeTransacted(UObject* InObject, UPackage* InPackage)
{
	if (InObject && ((InObject->IsA<UDisplayClusterConfigurationData_Base>() &&
		!InObject->IsTemplate() && !InObject->HasAnyFlags(RF_Transient) && InPackage != GetTransientPackage()) ||
		(InObject->GetClass()->HasMetaData(TEXT("DisplayClusterMultiUserInclude")))))
	{
		UE_LOG(LogDisplayClusterMultiUser, Log, TEXT("FDisplayClusterMultiUser transaction for object: %s"), *InObject->GetName());
		return ETransactionFilterResult::IncludeObject;
	}

	return ETransactionFilterResult::UseDefault;
}
