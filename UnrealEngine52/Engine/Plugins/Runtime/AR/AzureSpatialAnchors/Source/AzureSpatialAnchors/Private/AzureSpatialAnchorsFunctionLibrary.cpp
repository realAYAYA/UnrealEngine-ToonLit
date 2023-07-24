// Copyright Epic Games, Inc. All Rights Reserved.

#include "AzureSpatialAnchorsFunctionLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "IAzureSpatialAnchors.h"

#include "ARPin.h"
#include "LatentActions.h"

bool UAzureSpatialAnchorsLibrary::CreateSession()
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	return IASA->CreateSession();
}

bool UAzureSpatialAnchorsLibrary::ConfigSession(const FString& AccountId, const FString& AccountKey, const FCoarseLocalizationSettings CoarseLocalizationSettings, EAzureSpatialAnchorsLogVerbosity LogVerbosity)
{
	if (AccountId.Len() == 0)
	{
		UE_LOG(LogAzureSpatialAnchors, Warning, TEXT("ConfigSession called, but AccountId is empty!  This session should not be useable."));
	}

	if (AccountKey.Len() == 0)
	{
		UE_LOG(LogAzureSpatialAnchors, Warning, TEXT("ConfigSession called, but AccountKey is empty!  This session should not be useable."));
	}

	FAzureSpatialAnchorsSessionConfiguration Config;
	Config.AccountId = *AccountId;
	Config.AccountKey = *AccountKey;

	return ConfigSession2(Config, CoarseLocalizationSettings, LogVerbosity);
}

bool UAzureSpatialAnchorsLibrary::ConfigSession2(const FAzureSpatialAnchorsSessionConfiguration& SessionConfiguration, const FCoarseLocalizationSettings CoarseLocalizationSettings, EAzureSpatialAnchorsLogVerbosity LogVerbosity)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	// Authentication can be by AccessToken, AccountId + AccountKey, or AuthenticationToken.
	// So we ought to have one of those.  Probably wrong to have more than one.  Definately wrong to have none.
	int AuthenticationMethodCount = 0;

	if (SessionConfiguration.AccessToken.Len() != 0)
	{
		AuthenticationMethodCount += 1;
	}

	if ((SessionConfiguration.AccountId.Len() != 0) || (SessionConfiguration.AccountKey.Len() != 0))
	{
		if ((SessionConfiguration.AccountId.Len() != 0) && (SessionConfiguration.AccountKey.Len() != 0))
		{
			AuthenticationMethodCount += 1;
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Warning, TEXT("ConfigSession called, but we only have one of AccountID and AccountKey!  This session authentication method should not be useable."));
		}
	}

	if (SessionConfiguration.AuthenticationToken.Len() != 0)
	{
		AuthenticationMethodCount += 1;
	}

	if (AuthenticationMethodCount == 0)
	{
		UE_LOG(LogAzureSpatialAnchors, Warning, TEXT("ConfigSession called, but we found no potentially valid authentication information!  This session should not be useable.  Check that you are supplying the authentication data."));
	}
	else if (AuthenticationMethodCount > 1)
	{
		UE_LOG(LogAzureSpatialAnchors, Warning, TEXT("ConfigSession called, but we found %i potentially valid authentication information sets!  This may work, but you should check that it is working as intended."), AuthenticationMethodCount);
	}

	// Session configuration takes effect on session Start() according to documentation.
	EAzureSpatialAnchorsResult LocResult = IASA->SetLocationProvider(CoarseLocalizationSettings);
	EAzureSpatialAnchorsResult ConfigResult = IASA->SetConfiguration(SessionConfiguration);
	EAzureSpatialAnchorsResult LogResult = IASA->SetLogLevel(LogVerbosity);

	return (ConfigResult == EAzureSpatialAnchorsResult::Success) && (LocResult == EAzureSpatialAnchorsResult::Success) && (LogResult == EAzureSpatialAnchorsResult::Success);
}

bool UAzureSpatialAnchorsLibrary::StartSession()
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	return IASA->StartSession() == EAzureSpatialAnchorsResult::Success;
}

bool UAzureSpatialAnchorsLibrary::StopSession()
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	IASA->StopSession();
	return true;
}

bool UAzureSpatialAnchorsLibrary::DestroySession()
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	IASA->DestroySession();
	return true;
}

namespace AzureSpatialAnchorsLibrary
{
	struct AsyncData
	{
		EAzureSpatialAnchorsResult Result = EAzureSpatialAnchorsResult::NotStarted;
		FString OutError;
		TAtomic<bool> Completed = { false };

		void Complete() { Completed = true; }

		AsyncData() {}
	private:
		AsyncData(const AsyncData&) = delete;
		AsyncData& operator=(const AsyncData&) = delete;
	};

	struct SessionStatusAsyncData : public AsyncData
	{
		FAzureSpatialAnchorsSessionStatus Status;
	};
	typedef TSharedPtr<SessionStatusAsyncData, ESPMode::ThreadSafe> SessionStatusAsyncDataPtr;
}

bool UAzureSpatialAnchorsLibrary::GetCachedSessionStatus(FAzureSpatialAnchorsSessionStatus& OutStatus)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	OutStatus = IASA->GetSessionStatus();
	return true;
}

struct FAzureSpatialAnchorsAsyncAction : public FPendingLatentAction
{
public:
	FAzureSpatialAnchorsAsyncAction(const FLatentActionInfo& InLatentInfo, const TCHAR* InDescription, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FPendingLatentAction()
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
		, bStarted(false)
		, OutResult(InOutResult)
		, OutErrorString(InOutErrorString)
		, Description(InDescription)
	{}

	virtual void NotifyObjectDestroyed() override
	{
		Orphan();
	}

	virtual void NotifyActionAborted() override
	{
		Orphan();
	}

	virtual void Orphan() = 0;

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return Description;
	}
#endif

protected:
	// Normally one should return immediately if this returns null.
	IAzureSpatialAnchors* GetIASAOrFailAndFinish(FLatentResponse& Response)
	{
		IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
		if (IASA == nullptr)
		{
			OutResult = EAzureSpatialAnchorsResult::FailSeeErrorString;
			OutErrorString = TEXT("Failed to get IAzureSpatialAnchors.");
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
		}
		return IASA;
	}

	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	bool bStarted;
	EAzureSpatialAnchorsResult& OutResult;
	FString& OutErrorString;

private:
	FString Description;
};

struct FAzureSpatialAnchorsGetSessionStatusAction : public FAzureSpatialAnchorsAsyncAction
{
public:
	FAzureSpatialAnchorsSessionStatus* Status;
	AzureSpatialAnchorsLibrary::SessionStatusAsyncDataPtr Data;

	FAzureSpatialAnchorsGetSessionStatusAction(const FLatentActionInfo& InLatentInfo, FAzureSpatialAnchorsSessionStatus* InOutStatus, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("GetSessionStatus."), InOutResult, InOutErrorString)
		, Status(InOutStatus)
	{		
		check(Status != nullptr);
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bStarted)
		{
			IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
			if (IASA == nullptr)
			{
				return;
			}

			Data = MakeShared<AzureSpatialAnchorsLibrary::SessionStatusAsyncData, ESPMode::ThreadSafe>();

			Data->Result = EAzureSpatialAnchorsResult::Started;
			IAzureSpatialAnchors::Callback_Result_SessionStatus Callback = [Data = Data](EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString, FAzureSpatialAnchorsSessionStatus InStatus)
			{
				Data->Status = InStatus;
				Data->Result = Result;
				Data->OutError = WCHAR_TO_TCHAR(ErrorString);
				Data->Complete();
			};

			IASA->GetSessionStatusAsync(Callback);

			bStarted = true;
		}

		if (Data->Completed)
		{
			OutResult = (EAzureSpatialAnchorsResult)Data->Result;
			OutErrorString = Data->OutError;
			*Status = Data->Status;
			Data.Reset();
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			return;
		}
	}

	virtual void Orphan() override
	{
		Data.Reset();
	}
};

void UAzureSpatialAnchorsLibrary::GetSessionStatus(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, FAzureSpatialAnchorsSessionStatus& OutStatus, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("GetSessionStatus Action. UUID: %d"), LatentInfo.UUID);
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		FAzureSpatialAnchorsGetSessionStatusAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsGetSessionStatusAction*>(
			LatentManager.FindExistingAction<FAzureSpatialAnchorsGetSessionStatusAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
		if (ExistAction == nullptr)
		{
			// does this handle multiple in progress operations?
			FAzureSpatialAnchorsGetSessionStatusAction* NewAction = new FAzureSpatialAnchorsGetSessionStatusAction(LatentInfo, &OutStatus, OutResult, OutErrorString);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping GetSessionStatus latent action."), LatentInfo.UUID);
		}
	}
}

void UAzureSpatialAnchorsLibrary::GetCloudAnchor(UARPin* ARPin, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		OutAzureCloudSpatialAnchor = nullptr;
		return;
	}

	IASA->GetCloudAnchor(ARPin, OutAzureCloudSpatialAnchor);
}

void UAzureSpatialAnchorsLibrary::GetCloudAnchors(TArray<UAzureCloudSpatialAnchor*>& OutCloudAnchors)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		OutCloudAnchors.Empty();
		return;
	}

	IASA->GetCloudAnchors(OutCloudAnchors);
}

namespace AzureSpatialAnchorsLibrary
{
	struct CloudAnchorIDAsyncData : public AsyncData
	{
		IAzureSpatialAnchors::CloudAnchorID CloudAnchorID = IAzureSpatialAnchors::CloudAnchorID_Invalid;
	};
	typedef TSharedPtr<CloudAnchorIDAsyncData, ESPMode::ThreadSafe> CloudAnchorIDAsyncDataPtr;

	struct LoadByIDAsyncData : public AsyncData
	{
		FString CloudAnchorIdentifier;
		FString LocalAnchorId;
		IAzureSpatialAnchors::WatcherID WatcherID = IAzureSpatialAnchors::WatcherID_Invalid;
		IAzureSpatialAnchors::CloudAnchorID CloudAnchorID = IAzureSpatialAnchors::CloudAnchorID_Invalid;
		UAzureCloudSpatialAnchor* CloudAnchor = nullptr;
	};
	typedef TSharedPtr<LoadByIDAsyncData, ESPMode::ThreadSafe> LoadByIDAsyncDataPtr;

	//struct GetCloudAnchorPropertiesAsyncData : public AsyncData
	//{
	//	FString CloudAnchorIdentifier;
	//	CloudAnchorID CloudAnchorID = CloudAnchorID_Invalid;
	//};
	//typedef TSharedPtr<GetCloudAnchorPropertiesAsyncData, ESPMode::ThreadSafe> GetCloudAnchorPropertiesAsyncDataPtr;
}

struct FAzureSpatialAnchorsSavePinToCloudAction : public FAzureSpatialAnchorsAsyncAction
{
public:
	UARPin* ARPin;
	float Lifetime;
	TMap<FString, FString> AppProperties;
	UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor;
	AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncDataPtr Data;

	FAzureSpatialAnchorsSavePinToCloudAction(const FLatentActionInfo& InLatentInfo, UARPin*& InARPin, float InLifetime, const TMap<FString, FString>& InAppProperties, UAzureCloudSpatialAnchor*& InOutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("SavePinToCloud."), InOutResult, InOutErrorString)
		, ARPin(InARPin)
		, Lifetime(InLifetime)
		, AppProperties(InAppProperties)
		, OutAzureCloudSpatialAnchor(InOutAzureCloudSpatialAnchor)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bStarted)
		{

			IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
			if (IASA == nullptr)
			{
				return;
			}

			// Start the operation
			if (!IASA->ConstructCloudAnchor(ARPin, OutAzureCloudSpatialAnchor))
			{
				OutResult = EAzureSpatialAnchorsResult::FailSeeErrorString;
				OutErrorString = TEXT("ConstructCloudAnchor Failed.");
				Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
				return;
			}

			if (Lifetime > 0)
			{
				OutResult = IASA->SetCloudAnchorExpiration(OutAzureCloudSpatialAnchor->CloudAnchorID, Lifetime);
				if (OutResult  != EAzureSpatialAnchorsResult::Success)
				{
					OutErrorString = TEXT("SetCloudAnchorExpiration Failed.");
					Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
					return;
				}
			}

			if (AppProperties.Num() > 0)
			{
				OutResult = IASA->SetCloudAnchorAppProperties(OutAzureCloudSpatialAnchor->CloudAnchorID, AppProperties);
				if (OutResult != EAzureSpatialAnchorsResult::Success)
				{
					OutErrorString = TEXT("SetCloudAnchorAppProperties Failed.");
					Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
					return;
				}
			}

			Data = MakeShared<AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncData, ESPMode::ThreadSafe>();
			Data->CloudAnchorID = OutAzureCloudSpatialAnchor->CloudAnchorID;

			Data->Result = EAzureSpatialAnchorsResult::Started;
			IAzureSpatialAnchors::Callback_Result Callback = [Data=Data](EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString)
			{
				Data->Result = Result;
				Data->OutError = WCHAR_TO_TCHAR(ErrorString);
				Data->Complete();
			};
			IASA->CreateAnchorAsync(Data->CloudAnchorID, Callback);

			bStarted = true;
		}

		// See if the operation is done.
		if (Data->Completed)
		{
			OutResult = (EAzureSpatialAnchorsResult)Data->Result;
			OutErrorString = Data->OutError;
			Data.Reset();
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			return;
		}
	}

	virtual void Orphan() override
	{
		Data.Reset();
	}
};

void UAzureSpatialAnchorsLibrary::SavePinToCloud(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UARPin* ARPin, float Lifetime, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	TMap<FString, FString> EmptyAppProperties;
	SavePinToCloudWithAppProperties(WorldContextObject, LatentInfo, ARPin, Lifetime, EmptyAppProperties, OutAzureCloudSpatialAnchor, OutResult, OutErrorString);
}

void UAzureSpatialAnchorsLibrary::SavePinToCloudWithAppProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UARPin* ARPin, float Lifetime, const TMap<FString, FString>& InAppProperties, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("SavePinToCloud Action. UUID: %d"), LatentInfo.UUID);
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		FAzureSpatialAnchorsSavePinToCloudAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsSavePinToCloudAction*>(
			LatentManager.FindExistingAction<FAzureSpatialAnchorsSavePinToCloudAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
		if (ExistAction == nullptr || ExistAction->ARPin != ARPin)
		{
			// does this handle multiple in progress operations?
			FAzureSpatialAnchorsSavePinToCloudAction* NewAction = new FAzureSpatialAnchorsSavePinToCloudAction(LatentInfo, ARPin, Lifetime, InAppProperties, OutAzureCloudSpatialAnchor, OutResult, OutErrorString);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping SavePinToCloud latent action."), LatentInfo.UUID);
		}
	}
}


struct FAzureSpatialAnchorsDeleteCloudAnchorAction : public FAzureSpatialAnchorsAsyncAction
{
public:
	UAzureCloudSpatialAnchor* CloudSpatialAnchor;
	AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncDataPtr Data;

	FAzureSpatialAnchorsDeleteCloudAnchorAction(const FLatentActionInfo& InLatentInfo, UAzureCloudSpatialAnchor* InCloudSpatialAnchor, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("DeleteCloudAnchor."), InOutResult, InOutErrorString)
		, CloudSpatialAnchor(InCloudSpatialAnchor)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bStarted)
		{
			if (CloudSpatialAnchor == nullptr)
			{
				OutResult = EAzureSpatialAnchorsResult::FailNoCloudAnchor;
				Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
				return;
			}

			IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
			if (IASA == nullptr)
			{
				return;
			}

			Data = MakeShared<AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncData, ESPMode::ThreadSafe>();
			Data->CloudAnchorID = CloudSpatialAnchor->CloudAnchorID;

			Data->Result = EAzureSpatialAnchorsResult::Started;
			IAzureSpatialAnchors::Callback_Result Callback = [Data = Data](EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString)
			{
				Data->Result = Result;
				Data->OutError = WCHAR_TO_TCHAR(ErrorString);
				Data->Complete();
			};

			IASA->DeleteAnchorAsync(Data->CloudAnchorID, Callback);

			bStarted = true;
		}

		if (Data->Completed)
		{
			OutResult = (EAzureSpatialAnchorsResult)Data->Result;
			OutErrorString = Data->OutError;
			Data.Reset();
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			return;
		}
	}

	virtual void Orphan() override
	{
		Data.Reset();
	}
};

void UAzureSpatialAnchorsLibrary::DeleteCloudAnchor(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* InCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("DeleteCloudAnchor Action. UUID: %d"), LatentInfo.UUID);
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		FAzureSpatialAnchorsDeleteCloudAnchorAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsDeleteCloudAnchorAction*>(
			LatentManager.FindExistingAction<FAzureSpatialAnchorsDeleteCloudAnchorAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
		if (ExistAction == nullptr || ExistAction->CloudSpatialAnchor != InCloudSpatialAnchor)
		{
			// does this handle multiple in progress operations?
			FAzureSpatialAnchorsDeleteCloudAnchorAction* NewAction = new FAzureSpatialAnchorsDeleteCloudAnchorAction(LatentInfo, InCloudSpatialAnchor, OutResult, OutErrorString);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping DeleteCloudAnchor latent action."), LatentInfo.UUID);
		}
	}
}

struct FAzureSpatialAnchorsLoadCloudAnchorAction : public FAzureSpatialAnchorsAsyncAction
{
public:
	FString PinId;
	FString CloudId;
	UARPin*& OutARPin;
	UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor;
	AzureSpatialAnchorsLibrary::LoadByIDAsyncDataPtr Data;
	FDelegateHandle LocatedDelegateHandle;
	FDelegateHandle CompletedDelegateHandle;

	FAzureSpatialAnchorsLoadCloudAnchorAction(const FLatentActionInfo& InLatentInfo, FString InCloudId, FString InPinId, UARPin*& InOutARPin, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("LoadCloudAnchor."), InOutResult, InOutErrorString)
		, PinId(InPinId)
		, CloudId(InCloudId)
		, OutARPin(InOutARPin)
		, OutAzureCloudSpatialAnchor(OutAzureCloudSpatialAnchor)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bStarted)
		{
			// Start the operation
			if (CloudId.IsEmpty())
			{
				OutResult = EAzureSpatialAnchorsResult::FailBadCloudAnchorIdentifier;
				OutErrorString = TEXT("InCloudId is empty.  No load attempted.");
				Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
				return;
			}

			IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
			if (IASA == nullptr)
			{
				return;
			}

			Data = MakeShared<AzureSpatialAnchorsLibrary::LoadByIDAsyncData, ESPMode::ThreadSafe>();
			Data->CloudAnchorIdentifier = *CloudId;
			Data->LocalAnchorId = *PinId;

			FAzureSpatialAnchorsLocateCriteria Criteria;
			Criteria.Identifiers.Add(*CloudId);
			IAzureSpatialAnchors::WatcherID OutWatcherID = IAzureSpatialAnchors::WatcherID_Invalid;
			Data->Result = EAzureSpatialAnchorsResult::Started;
			// We don't use distance in the criteria here, so the worldscale parameter does not actually matter.
			const float WorldScale = 100.0f;
			EAzureSpatialAnchorsResult Result = IASA->CreateWatcher(Criteria, WorldScale, OutWatcherID, OutErrorString);
			if (Result == EAzureSpatialAnchorsResult::Success)
			{
				Data->WatcherID = OutWatcherID;
				UE_LOG(LogAzureSpatialAnchors, Log, TEXT("LoadCloudAnchorByID created watcher %i for %s"), OutWatcherID, *CloudId);
			}
			else
			{
				Data->Result = Result;
				Data->OutError = OutErrorString;
				Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
				return;
			}
			LocatedDelegateHandle = IAzureSpatialAnchors::ASAAnchorLocatedDelegate.AddLambda(
				[Data=Data](int32 InWatcherIdentifier, EAzureSpatialAnchorsLocateAnchorStatus InLocateAnchorStatus, UAzureCloudSpatialAnchor* InCloudAnchor)
				{
					if (InWatcherIdentifier == Data->WatcherID)
					{
						switch (InLocateAnchorStatus)
						{
						case EAzureSpatialAnchorsLocateAnchorStatus::Located:
						{
							// This is a single identifier search so only one anchor located event can be properly handled.
							check(Data->CloudAnchor == nullptr);
							
							UE_LOG(LogAzureSpatialAnchors, Log, TEXT("LoadCloudAnchorByID Located status Located CloudAnchor %i %s"), InCloudAnchor->CloudAnchorID, *Data->CloudAnchorIdentifier);
							Data->CloudAnchor = InCloudAnchor;

							Data->Result = EAzureSpatialAnchorsResult::Success;
						}
						break;
						case EAzureSpatialAnchorsLocateAnchorStatus::AlreadyTracked:
						{
							UE_LOG(LogAzureSpatialAnchors, Log, TEXT("LoadCloudAnchorByID Located status AlreadyTracked"));

							Data->Result = EAzureSpatialAnchorsResult::FailAnchorAlreadyTracked;
						}
						break;
						case EAzureSpatialAnchorsLocateAnchorStatus::NotLocated:
						{
							UE_LOG(LogAzureSpatialAnchors, Log, TEXT("LoadCloudAnchorByID Located status NotLocated"));

							// This gets called repeatedly for a while until something else happens.
							Data->Result = EAzureSpatialAnchorsResult::NotLocated;
						}
						break;
						case EAzureSpatialAnchorsLocateAnchorStatus::NotLocatedAnchorDoesNotExist:
						{
							UE_LOG(LogAzureSpatialAnchors, Log, TEXT("LoadCloudAnchorByID Located status NotLocatedAnchorDoesNotExist"));

							Data->Result = EAzureSpatialAnchorsResult::FailAnchorDoesNotExist;
						}
						break;
						default:
							check(false);
						}
					}
				});

			CompletedDelegateHandle = IAzureSpatialAnchors::ASALocateAnchorsCompletedDelegate.AddLambda(
				[Data = Data](int32 InWatcherIdentifier, bool InWasCanceled)
				{
					if (InWatcherIdentifier == Data->WatcherID)
					{
						UE_LOG(LogAzureSpatialAnchors, Log, TEXT("LoadCloudAnchorByID watcher % has completed"), InWatcherIdentifier);

						if (InWasCanceled)
						{
							Data->Result = EAzureSpatialAnchorsResult::Canceled;
						}
						// Do not set Data->Result here because it was set in LoadCloudAnchorByID_Located.
						Data->Complete();
					}
				});

			bStarted = true;
		}

		if (Data->Completed)
		{		
			LocatedDelegateHandle.Reset();
			CompletedDelegateHandle.Reset();

			OutResult = (EAzureSpatialAnchorsResult)Data->Result;
			OutErrorString = Data->OutError;

			if (OutResult == EAzureSpatialAnchorsResult::Success)
			{
				IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
				if (IASA == nullptr)
				{
					return;
				}

				// Create an ARPin around the cloud anchor so we can return them both.
				IASA->CreateARPinAroundAzureCloudSpatialAnchor(Data->LocalAnchorId, Data->CloudAnchor, OutARPin);
				if (OutARPin == nullptr)
				{
					OutResult = EAzureSpatialAnchorsResult::FailSeeErrorString;
					OutErrorString = TEXT("LoadCloudAnchorByID failed to create ARPin after loading anchor.  This can happen if blueprint, or something else, is listening for AnchorLocated events and creating pins for the CloudAnchors that are being loaded here.");
				}
			}

			OutAzureCloudSpatialAnchor = Data->CloudAnchor;
			Data.Reset();
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			return;
		}
	}

	virtual void Orphan() override
	{
		LocatedDelegateHandle.Reset();
		CompletedDelegateHandle.Reset();
		Data.Reset();
	}
};

void UAzureSpatialAnchorsLibrary::LoadCloudAnchor(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, FString CloudId, FString PinId, UARPin*& OutARPin, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("LoadCloudAnchor Action. UUID: %d"), LatentInfo.UUID);
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		FAzureSpatialAnchorsLoadCloudAnchorAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsLoadCloudAnchorAction*>(
			LatentManager.FindExistingAction<FAzureSpatialAnchorsLoadCloudAnchorAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
		if (ExistAction == nullptr || ExistAction->CloudId != CloudId)
		{
			// does this handle multiple in progress operations?
			FAzureSpatialAnchorsLoadCloudAnchorAction* NewAction = new FAzureSpatialAnchorsLoadCloudAnchorAction(LatentInfo, CloudId, PinId, OutARPin, OutAzureCloudSpatialAnchor, OutResult, OutErrorString);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping LoadCloudAnchor latent action."), LatentInfo.UUID);
		}
	}
}

void UAzureSpatialAnchorsLibrary::ConstructCloudAnchor(UARPin* ARPin, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return;
	}

	if (IASA->ConstructCloudAnchor(ARPin, OutAzureCloudSpatialAnchor))
	{
		OutResult = EAzureSpatialAnchorsResult::Success;
	}
	else
	{
		OutResult = EAzureSpatialAnchorsResult::FailSeeErrorString;
		OutErrorString = TEXT("ConstructCloudAnchor Failed.");
	}
}

struct FAzureSpatialAnchorsSaveCloudAnchorAction : public FAzureSpatialAnchorsAsyncAction
{
public:
	UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor;
	AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncDataPtr Data;

	FAzureSpatialAnchorsSaveCloudAnchorAction(const FLatentActionInfo& InLatentInfo, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("SaveCloudAnchor."), InOutResult, InOutErrorString)
		, InAzureCloudSpatialAnchor(InAzureCloudSpatialAnchor)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bStarted)
		{
			IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
			if (IASA == nullptr)
			{
				return;
			}

			Data = MakeShared<AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncData, ESPMode::ThreadSafe>();
			Data->CloudAnchorID = InAzureCloudSpatialAnchor->CloudAnchorID;

			Data->Result = EAzureSpatialAnchorsResult::Started;
			IAzureSpatialAnchors::Callback_Result Callback = [Data = Data](EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString)
			{
				Data->Result = Result;
				Data->OutError = WCHAR_TO_TCHAR(ErrorString);
				Data->Complete();
			};
			IASA->CreateAnchorAsync(Data->CloudAnchorID, Callback);

			bStarted = true;
		}

		// See if the operation is done.
		if (Data->Completed)
		{
			OutResult = (EAzureSpatialAnchorsResult)Data->Result;
			OutErrorString = Data->OutError;
			Data.Reset();
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			return;
		}
	}

	virtual void Orphan() override
	{
		Data.Reset();
	}
};

void UAzureSpatialAnchorsLibrary::SaveCloudAnchor(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("SaveCloudAnchor Action. UUID: %d"), LatentInfo.UUID);
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		FAzureSpatialAnchorsSaveCloudAnchorAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsSaveCloudAnchorAction*>(
			LatentManager.FindExistingAction<FAzureSpatialAnchorsSaveCloudAnchorAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
		if (ExistAction == nullptr || ExistAction->InAzureCloudSpatialAnchor != InAzureCloudSpatialAnchor)
		{
			// does this handle multiple in progress operations?
			FAzureSpatialAnchorsSaveCloudAnchorAction* NewAction = new FAzureSpatialAnchorsSaveCloudAnchorAction(LatentInfo, InAzureCloudSpatialAnchor, OutResult, OutErrorString);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping SaveCloudAnchor latent action."), LatentInfo.UUID);
		}
	}
}

struct FAzureSpatialAnchorsUpdateCloudAnchorPropertiesAction : public FAzureSpatialAnchorsAsyncAction
{
public:
	UAzureCloudSpatialAnchor*& InAzureCloudSpatialAnchor;
	AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncDataPtr Data;

	FAzureSpatialAnchorsUpdateCloudAnchorPropertiesAction(const FLatentActionInfo& InLatentInfo, UAzureCloudSpatialAnchor*& InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("UpdateCloudAnchorProperties."), InOutResult, InOutErrorString)
		, InAzureCloudSpatialAnchor(InAzureCloudSpatialAnchor)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bStarted)
		{
			if (InAzureCloudSpatialAnchor == nullptr)
			{
				OutResult = EAzureSpatialAnchorsResult::FailNoCloudAnchor;
				Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
				return;
			}

			IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
			if (IASA == nullptr)
			{
				return;
			}

			Data = MakeShared<AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncData, ESPMode::ThreadSafe>();
			Data->CloudAnchorID = InAzureCloudSpatialAnchor->CloudAnchorID;

			Data->Result = EAzureSpatialAnchorsResult::Started;
			IAzureSpatialAnchors::Callback_Result Callback = [Data = Data](EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString)
			{
				Data->Result = Result;
				Data->OutError = WCHAR_TO_TCHAR(ErrorString);
				Data->Complete();
			};
			IASA->UpdateAnchorPropertiesAsync(Data->CloudAnchorID, Callback);

			bStarted = true;
		}

		if (Data->Completed)
		{
			OutResult = (EAzureSpatialAnchorsResult)Data->Result;
			OutErrorString = Data->OutError;
			Data.Reset();
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			return;
		}
	}

	virtual void Orphan() override
	{
		Data.Reset();
	}
};

void UAzureSpatialAnchorsLibrary::UpdateCloudAnchorProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("UpdateCloudAnchorProperties Action. UUID: %d"), LatentInfo.UUID);
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		FAzureSpatialAnchorsUpdateCloudAnchorPropertiesAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsUpdateCloudAnchorPropertiesAction*>(
			LatentManager.FindExistingAction<FAzureSpatialAnchorsUpdateCloudAnchorPropertiesAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
		if (ExistAction == nullptr || ExistAction->InAzureCloudSpatialAnchor != InAzureCloudSpatialAnchor)
		{
			// does this handle multiple in progress operations?
			FAzureSpatialAnchorsUpdateCloudAnchorPropertiesAction* NewAction = new FAzureSpatialAnchorsUpdateCloudAnchorPropertiesAction(LatentInfo, InAzureCloudSpatialAnchor, OutResult, OutErrorString);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping UpdateCloudAnchorProperties latent action."), LatentInfo.UUID);
		}
	}
}

struct FAzureSpatialAnchorsRefreshCloudAnchorPropertiesAction : public FAzureSpatialAnchorsAsyncAction
{
public:
	UAzureCloudSpatialAnchor*& InAzureCloudSpatialAnchor;
	AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncDataPtr Data;

	FAzureSpatialAnchorsRefreshCloudAnchorPropertiesAction(const FLatentActionInfo& InLatentInfo, UAzureCloudSpatialAnchor*& InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("RefreshCloudAnchorProperties."), InOutResult, InOutErrorString)
		, InAzureCloudSpatialAnchor(InAzureCloudSpatialAnchor)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bStarted)
		{
			if (InAzureCloudSpatialAnchor == nullptr)
			{
				OutResult = EAzureSpatialAnchorsResult::FailNoCloudAnchor;
				Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
				return;
			}

			IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
			if (IASA == nullptr)
			{
				return;
			}

			Data = MakeShared<AzureSpatialAnchorsLibrary::CloudAnchorIDAsyncData, ESPMode::ThreadSafe>();
			Data->CloudAnchorID = InAzureCloudSpatialAnchor->CloudAnchorID;

			Data->Result = EAzureSpatialAnchorsResult::Started;
			IAzureSpatialAnchors::Callback_Result Callback = [Data = Data](EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString)
			{
				Data->Result = Result;
				Data->OutError = WCHAR_TO_TCHAR(ErrorString);
				Data->Complete();
			};
			IASA->RefreshAnchorPropertiesAsync(Data->CloudAnchorID, Callback);

			bStarted = true;
		}

		if (Data->Completed)
		{
			OutResult = (EAzureSpatialAnchorsResult)Data->Result;
			OutErrorString = Data->OutError;
			Data.Reset();
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			return;
		}
	}

	virtual void Orphan() override
	{
		Data.Reset();
	}
};

void UAzureSpatialAnchorsLibrary::RefreshCloudAnchorProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("RefreshCloudAnchorProperties Action. UUID: %d"), LatentInfo.UUID);
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		FAzureSpatialAnchorsRefreshCloudAnchorPropertiesAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsRefreshCloudAnchorPropertiesAction*>(
			LatentManager.FindExistingAction<FAzureSpatialAnchorsRefreshCloudAnchorPropertiesAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
		if (ExistAction == nullptr || ExistAction->InAzureCloudSpatialAnchor != InAzureCloudSpatialAnchor)
		{
			// does this handle multiple in progress operations?
			FAzureSpatialAnchorsRefreshCloudAnchorPropertiesAction* NewAction = new FAzureSpatialAnchorsRefreshCloudAnchorPropertiesAction(LatentInfo, InAzureCloudSpatialAnchor, OutResult, OutErrorString);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping RefreshCloudAnchorProperties latent action."), LatentInfo.UUID);
		}
	}
}

//struct FAzureSpatialAnchorsGetCloudAnchorPropertiesAction : public FAzureSpatialAnchorsAsyncAction
//{
//public:
//	FString CloudIdentifier;
//	UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor;
//	GetCloudAnchorPropertiesAsyncDataPtr Data;
//
//	FAzureSpatialAnchorsGetCloudAnchorPropertiesAction(const FLatentActionInfo& InLatentInfo, FString CloudIdentifier, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& InOutResult, FString& InOutErrorString)
//		: FAzureSpatialAnchorsAsyncAction(InLatentInfo, TEXT("GetCloudAnchorProperties."), InOutResult, InOutErrorString)
//		, CloudIdentifier(CloudIdentifier)
//		, OutAzureCloudSpatialAnchor(OutAzureCloudSpatialAnchor)
//	{}
//
//	virtual void UpdateOperation(FLatentResponse& Response) override
//	{
		//if (!bStarted)
		//{
		//	if (InAzureCloudSpatialAnchor == nullptr)
		//	{
		//		OutResult = EAzureSpatialAnchorsResult::FailNoCloudAnchor;
		//		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
		//		return;
		//	}

		//IAzureSpatialAnchors* IASA = GetIASAOrFailAndFinish(Response);
		//if (IASA == nullptr)
		//{
		//	return;
		//}
		//	Data = MakeShared<AzureSpatialAnchorsLibrary::GetCloudAnchorPropertiesAsyncData, ESPMode::ThreadSafe>();
		//	Data->CloudAnchorIdentifier = CloudIdentifier;

		//	Data->Result = EAzureSpatialAnchorsResult::Started;
		//	IAzureSpatialAnchors::Callback_Result Callback = [Data = Data](EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString)
		//	{
		//		Data->Result = Result;
		//		Data->OutError = WCHAR_TO_TCHAR(ErrorString);
		//		Data->Complete();
		//	};
		//	IASA->GetAnchorPropertiesAsync(Data->CloudAnchorIdentifier, Data->CloudAnchorID, Callback);

		//	bStarted = true;
		//}

		//if (Data->Completed)
		//{, 
		//	OutResult = (EAzureSpatialAnchorsResult)Data->Result;
		//	OutErrorString = Data->OutError;
//OutAzureCloudSpatialAnchor = 
		//	Data.Reset();
		//	Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
		//	return;
		//}
//	}
//
//	virtual void Orphan() override
//	{
//		Data.Reset();
//	}
//};
//
//void UAzureSpatialAnchorsLibrary::GetCloudAnchorProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, FString CloudIdentifier, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
//{
//	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
//	{
//		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("GetCloudAnchorProperties Action. UUID: %d"), LatentInfo.UUID);
//		FLatentActionManager& LatentManager = World->GetLatentActionManager();
//
//		FAzureSpatialAnchorsGetCloudAnchorPropertiesAction* ExistAction = reinterpret_cast<FAzureSpatialAnchorsGetCloudAnchorPropertiesAction*>(
//			LatentManager.FindExistingAction<FAzureSpatialAnchorsGetCloudAnchorPropertiesAction>(LatentInfo.CallbackTarget, LatentInfo.UUID));
//		if (ExistAction == nullptr || ExistAction->CloudIdentifier != CloudIdentifier)
//		{
//			// does this handle multiple in progress operations?
//			FAzureSpatialAnchorsGetCloudAnchorPropertiesAction* NewAction = new FAzureSpatialAnchorsGetCloudAnchorPropertiesAction(LatentInfo, CloudIdentifier, OutAzureCloudSpatialAnchor, OutResult, OutErrorString);
//			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
//		}
//		else
//		{
//			UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("Skipping GetCloudAnchorProperties latent action."), LatentInfo.UUID);
//		}
//	}
//}

void UAzureSpatialAnchorsLibrary::CreateWatcher(UObject* WorldContextObject, const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, int32& OutWatcherIdentifier, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UE_LOG(LogAzureSpatialAnchors, Verbose, TEXT("CreateWatcher"));

		IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
		if (IASA == nullptr)
		{
			OutResult = EAzureSpatialAnchorsResult::FailSeeErrorString;
			OutErrorString = TEXT("Failed to get IAzureSpatialAnchors.");
			return;
		}

		const float WorldToMetersScale = WorldContextObject->GetWorld()->GetWorldSettings()->WorldToMeters;
		OutResult = IASA->CreateWatcher(InLocateCriteria, WorldToMetersScale, OutWatcherIdentifier, OutErrorString);
	}
	else
	{
		OutResult = EAzureSpatialAnchorsResult::FailSeeErrorString;
		OutErrorString = TEXT("Failed to get World from WorldContextObject.");
	}
}

bool UAzureSpatialAnchorsLibrary::StopWatcher(int32 InWatcherIdentifier)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	return IASA->StopWatcher(InWatcherIdentifier) == EAzureSpatialAnchorsResult::Success;
}

bool UAzureSpatialAnchorsLibrary::CreateARPinAroundAzureCloudSpatialAnchor(FString PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return false;
	}

	return IASA->CreateARPinAroundAzureCloudSpatialAnchor(PinId, InAzureCloudSpatialAnchor, OutARPin);
}



