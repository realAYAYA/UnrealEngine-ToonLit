// Copyright Epic Games, Inc. All Rights Reserved.

#include "AzureSpatialAnchorsForARCore.h"
#include "AzureSpatialAnchorsAndroidInterop.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FAzureSpatialAnchorsForARCore, AzureSpatialAnchorsForARCore)

void FAzureSpatialAnchorsForARCore::StartupModule()
{
	FAzureSpatialAnchorsBase::Startup();

	auto AnchorLocatedLambda = std::bind(&FAzureSpatialAnchorsForARCore::AnchorLocatedCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	auto LocateAnchorsCompletedLambda = std::bind(&FAzureSpatialAnchorsForARCore::LocateAnchorsCompletedCallback, this, std::placeholders::_1, std::placeholders::_2);
	auto SessionUpdatedLambda = std::bind(&FAzureSpatialAnchorsForARCore::SessionUpdatedCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

	AndroidInterop = FAzureSpatialAnchorsAndroidInterop::Create(AnchorLocatedLambda, LocateAnchorsCompletedLambda, SessionUpdatedLambda);

	IModularFeatures::Get().RegisterModularFeature(IAzureSpatialAnchors::GetModularFeatureName(), this);
	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FAzureSpatialAnchorsForARCore::OnWorldTickStart);
}

void FAzureSpatialAnchorsForARCore::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(IAzureSpatialAnchors::GetModularFeatureName(), this);
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);

	AndroidInterop->StopSession();
	AndroidInterop->DestroySession();
	AndroidInterop = nullptr;

	FAzureSpatialAnchorsBase::Shutdown();
}

bool FAzureSpatialAnchorsForARCore::CreateSession()
{
	return AndroidInterop->CreateSession();
}

void FAzureSpatialAnchorsForARCore::DestroySession()
{
	AndroidInterop->DestroySession();
}

void FAzureSpatialAnchorsForARCore::GetAccessTokenWithAccountKeyAsync(const FString& AccountKey, Callback_Result_String Callback)
{
	AndroidInterop->GetAccessTokenWithAccountKeyAsync(AccountKey, Callback);
}

void FAzureSpatialAnchorsForARCore::GetAccessTokenWithAuthenticationTokenAsync(const FString& AuthenticationToken, Callback_Result_String Callback)
{
	AndroidInterop->GetAccessTokenWithAuthenticationTokenAsync(AuthenticationToken, Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::StartSession()
{
	return AndroidInterop->StartSession();
}

void FAzureSpatialAnchorsForARCore::StopSession()
{
	AndroidInterop->StopSession();
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::ResetSession()
{
	return AndroidInterop->ResetSession();
}

void FAzureSpatialAnchorsForARCore::DisposeSession()
{
	AndroidInterop->DisposeSession();
}

void FAzureSpatialAnchorsForARCore::GetSessionStatusAsync(Callback_Result_SessionStatus Callback)
{
	AndroidInterop->GetSessionStatusAsync(Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::ConstructAnchor(UARPin* InARPin, CloudAnchorID& OutCloudAnchorID)
{
	return AndroidInterop->ConstructAnchor(InARPin, OutCloudAnchorID);
}

void FAzureSpatialAnchorsForARCore::CreateAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	AndroidInterop->CreateAnchorAsync(InCloudAnchorID, Callback);
}

void FAzureSpatialAnchorsForARCore::DeleteAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	AndroidInterop->DeleteAnchorAsync(InCloudAnchorID, Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::CreateWatcher(const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, float InWorldToMetersScale, WatcherID& OutWatcherID, FString& OutErrorString)
{
	return AndroidInterop->CreateWatcher(InLocateCriteria, InWorldToMetersScale, OutWatcherID, OutErrorString);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::GetActiveWatchers(TArray<WatcherID>& OutWatcherIDs)
{
	return AndroidInterop->GetActiveWatchers(OutWatcherIDs);
}

void FAzureSpatialAnchorsForARCore::GetAnchorPropertiesAsync(const FString& InCloudAnchorIdentifier, Callback_Result_CloudAnchorID Callback)
{
	AndroidInterop->GetAnchorPropertiesAsync(InCloudAnchorIdentifier, Callback);
}

void FAzureSpatialAnchorsForARCore::RefreshAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	AndroidInterop->RefreshAnchorPropertiesAsync(InCloudAnchorID, Callback);
}

void FAzureSpatialAnchorsForARCore::UpdateAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	AndroidInterop->UpdateAnchorPropertiesAsync(InCloudAnchorID, Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::GetConfiguration(FAzureSpatialAnchorsSessionConfiguration& OutConfig)
{
	return AndroidInterop->GetConfiguration(OutConfig);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::SetConfiguration(const FAzureSpatialAnchorsSessionConfiguration& InConfig)
{
	return AndroidInterop->SetConfiguration(InConfig);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::SetLocationProvider(const FCoarseLocalizationSettings& InConfig)
{
	return AndroidInterop->SetLocationProvider(InConfig);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::GetLogLevel(EAzureSpatialAnchorsLogVerbosity& OutLogVerbosity)
{
	return AndroidInterop->GetLogLevel(OutLogVerbosity);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::SetLogLevel(EAzureSpatialAnchorsLogVerbosity InLogVerbosity)
{
	return AndroidInterop->SetLogLevel(InLogVerbosity);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::GetSessionId(FString& OutSessionID)
{
	return AndroidInterop->GetSessionId(OutSessionID);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::StopWatcher(WatcherID InWatcherIdentifier)
{
	return AndroidInterop->StopWatcher(InWatcherIdentifier);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::GetCloudSpatialAnchorIdentifier(CloudAnchorID InCloudAnchorID, FString& OutCloudAnchorIdentifier)
{
	return AndroidInterop->GetCloudSpatialAnchorIdentifier(InCloudAnchorID, OutCloudAnchorIdentifier);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::SetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float InLifetimeInSeconds)
{
	return AndroidInterop->SetCloudAnchorExpiration(InCloudAnchorID, InLifetimeInSeconds);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::GetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float& OutLifetimeInSeconds)
{
	return AndroidInterop->GetCloudAnchorExpiration(InCloudAnchorID, OutLifetimeInSeconds);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::SetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, const TMap<FString, FString>& InAppProperties)
{
	return AndroidInterop->SetCloudAnchorAppProperties(InCloudAnchorID, InAppProperties);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::GetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, TMap<FString, FString>& OutAppProperties) 
{
	return AndroidInterop->GetCloudAnchorAppProperties(InCloudAnchorID, OutAppProperties);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARCore::SetDiagnosticsConfig(FAzureSpatialAnchorsDiagnosticsConfig& InConfig)
{
	return AndroidInterop->SetDiagnosticsConfig(InConfig);
}

void FAzureSpatialAnchorsForARCore::CreateDiagnosticsManifestAsync(const FString& Description, Callback_Result_String Callback)
{
	AndroidInterop->CreateDiagnosticsManifestAsync(Description, Callback);
}

void FAzureSpatialAnchorsForARCore::SubmitDiagnosticsManifestAsync(const FString& ManifestPath, Callback_Result Callback)
{
	AndroidInterop->SubmitDiagnosticsManifestAsync(ManifestPath, Callback);
}

void FAzureSpatialAnchorsForARCore::CreateNamedARPinAroundAnchor(const FString& InLocalAnchorId, UARPin*& OutARPin)
{
	AndroidInterop->CreateNamedARPinAroundAnchor(InLocalAnchorId, OutARPin);
}

bool FAzureSpatialAnchorsForARCore::CreateARPinAroundAzureCloudSpatialAnchor(const FString& PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin)
{
	return AndroidInterop->CreateARPinAroundAzureCloudSpatialAnchor(PinId, InAzureCloudSpatialAnchor, OutARPin);
}

void FAzureSpatialAnchorsForARCore::OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaTime)
{
	AndroidInterop->UpdateSession();
}