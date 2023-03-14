// Copyright Epic Games, Inc. All Rights Reserved.

#include "IConcertSyncClientModule.h"
#include "IConcertClient.h"
#include "ConcertSyncClient.h"
#include "ConcertSettings.h"
#include "ConcertLogGlobal.h"
#include "ConcertClientPackageBridge.h"
#include "ConcertClientTransactionBridge.h"

#include "Framework/Notifications/NotificationManager.h"

LLM_DEFINE_TAG(Concert_ConcertSyncClient);

/**
 * Implements the Concert Sync module for Event synchronization
 */
class FConcertSyncClientModule : public IConcertSyncClientModule
{
public:
	FConcertSyncClientModule() {}
	virtual ~FConcertSyncClientModule() {}

	virtual void StartupModule() override
	{
		LLM_SCOPE_BYTAG(Concert_ConcertSyncClient);
		PackageBridge = MakeUnique<FConcertClientPackageBridge>();
		TransactionBridge = MakeUnique<FConcertClientTransactionBridge>();
	}

	virtual void ShutdownModule() override
	{
		LLM_SCOPE_BYTAG(Concert_ConcertSyncClient);
		PackageBridge.Reset();
		TransactionBridge.Reset();
	}

	virtual UConcertClientConfig* ParseClientSettings(const TCHAR* CommandLine) override
	{
		LLM_SCOPE_BYTAG(Concert_ConcertSyncClient);

		UConcertClientConfig* ClientConfig = NewObject<UConcertClientConfig>();

		if (ClientConfig)
		{
			// Validates the user input before overwriting the default config value.
			auto OverwriteConfigParamIfValid = [CommandLine](FText (*ValidateFunc)(const FString&), const TCHAR* Param, FString& OutParamValue)
			{
				FString ParsedParamValue;
				if (FParse::Value(CommandLine, Param, ParsedParamValue)) // Parameter was supplied on the command line?
				{
					if (!ParsedParamValue.IsEmpty() && ValidateFunc) // An empty value is always valid -> it clears the default value, no need to validate.
					{
						FText ValidateError = ValidateFunc(ParsedParamValue);
						if (!ValidateError.IsEmpty())
						{
							UE_LOG(LogConcert, Warning, TEXT("Invalid value for '%s' parameter. (Reason: %s). Parameter ignored."), Param, *ValidateError.ToString());
							return;
						}
					}

					OutParamValue = ParsedParamValue; // Overwrite the default config value.
				}
			};

			OverwriteConfigParamIfValid(nullptr, TEXT("-CONCERTSERVER="), ClientConfig->DefaultServerURL);
			OverwriteConfigParamIfValid(&ConcertSettingsUtils::ValidateSessionName, TEXT("-CONCERTSESSION="), ClientConfig->DefaultSessionName);
			OverwriteConfigParamIfValid(&ConcertSettingsUtils::ValidateSessionName, TEXT("-CONCERTSESSIONTORESTORE="), ClientConfig->DefaultSessionToRestore);
			OverwriteConfigParamIfValid(&ConcertSettingsUtils::ValidateSessionName, TEXT("-CONCERTSAVESESSIONAS="), ClientConfig->DefaultSaveSessionAs);
			OverwriteConfigParamIfValid(&ConcertSettingsUtils::ValidateDisplayName, TEXT("-CONCERTDISPLAYNAME="), ClientConfig->ClientSettings.DisplayName);

			ClientConfig->bAutoConnect |= FParse::Param(CommandLine, TEXT("CONCERTAUTOCONNECT"));
			FParse::Bool(CommandLine, TEXT("-CONCERTAUTOCONNECT="), ClientConfig->bAutoConnect);

			ClientConfig->bRetryAutoConnectOnError |= FParse::Param(CommandLine, TEXT("CONCERTRETRYAUTOCONNECTONERROR"));
			FParse::Bool(CommandLine, TEXT("-CONCERTRETRYAUTOCONNECTONERROR="), ClientConfig->bRetryAutoConnectOnError);

			ClientConfig->ClientSettings.bReflectLevelEditorInGame |= FParse::Param(CommandLine, TEXT("CONCERTREFLECTVISIBILITY"));
			FParse::Bool(CommandLine, TEXT("-CONCERTREFLECTVISIBILITY="), ClientConfig->ClientSettings.bReflectLevelEditorInGame);

			ClientConfig->bIsHeadless |= FParse::Param(CommandLine, TEXT("CONCERTISHEADLESS"));
			FParse::Bool(CommandLine, TEXT("-CONCERTISHEADLESS="), ClientConfig->bIsHeadless);
			ClientConfig->bIsHeadless |= !FSlateNotificationManager::Get().AreNotificationsAllowed();

			ClientConfig->EndpointSettings.bEnableLogging |= FParse::Param(CommandLine, TEXT("CONCERTLOGGING"));
			FParse::Bool(CommandLine, TEXT("-CONCERTLOGGING="), ClientConfig->EndpointSettings.bEnableLogging);

			// CONCERTTAGS
			{
				FString CmdTags;
				if (FParse::Value(CommandLine, TEXT("-CONCERTTAGS="), CmdTags))
				{
					ClientConfig->ClientSettings.Tags.Reset();

					TArray<FString> Tags;
					CmdTags.ParseIntoArray(Tags, TEXT("|"), true);
					for (const FString& Tag : Tags)
					{
						ClientConfig->ClientSettings.Tags.Add(FName(*Tag));
					}
				}
			}
		}

		return ClientConfig;
	}

	virtual TSharedRef<IConcertSyncClient> CreateClient(const FString& InRole) override
	{
		LLM_SCOPE_BYTAG(Concert_ConcertSyncClient);

		// Remove dead clients.
		Clients.RemoveAll([](TWeakPtr<IConcertSyncClient> WeakClient) { return !WeakClient.IsValid(); });

		TSharedRef<IConcertSyncClient> NewClient = MakeShared<FConcertSyncClient>(InRole, PackageBridge.Get(), TransactionBridge.Get());
		Clients.Add(NewClient);
		OnClientCreated().Broadcast(NewClient);

		return NewClient;
	}

	virtual IConcertClientPackageBridge& GetPackageBridge() override
	{
		return *PackageBridge;
	}

	virtual IConcertClientTransactionBridge& GetTransactionBridge() override
	{
		return *TransactionBridge;
	}

	virtual TArray<TSharedRef<IConcertSyncClient>> GetClients() const override
	{
		TArray<TSharedRef<IConcertSyncClient>> ActiveClients;
		for (TWeakPtr<IConcertSyncClient> WeakClient : Clients)
		{
			if (TSharedPtr<IConcertSyncClient> Client = WeakClient.Pin())
			{
				ActiveClients.Add(Client.ToSharedRef());
			}
		}

		return ActiveClients;
	}

	virtual TSharedPtr<IConcertSyncClient> GetClient(const FString& InRole) const override
	{
		TSharedPtr<IConcertSyncClient> Client;

		for (const TWeakPtr<IConcertSyncClient>& WeakClient : Clients)
		{
			if (TSharedPtr<IConcertSyncClient> ClientPtr = WeakClient.Pin())
			{
				if (ClientPtr->GetConcertClient()->GetRole() == InRole)
				{
					Client = ClientPtr;
				}
			}
		}

		return Client;
	}

	virtual FOnConcertClientCreated& OnClientCreated() override
	{
		return OnClientCreatedDelegate;
	}

private:
	TUniquePtr<FConcertClientPackageBridge> PackageBridge;
	TUniquePtr<FConcertClientTransactionBridge> TransactionBridge;
	TArray<TWeakPtr<IConcertSyncClient>> Clients;
	FOnConcertClientCreated OnClientCreatedDelegate;
};

IMPLEMENT_MODULE(FConcertSyncClientModule, ConcertSyncClient);
