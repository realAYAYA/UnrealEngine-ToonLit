// Copyright Epic Games, Inc. All Rights Reserved.

#include "AzureSpatialAnchorsForARKit.h"
#include "AzureSpatialAnchorsARKitInterop.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FAzureSpatialAnchorsForARKit, AzureSpatialAnchorsForARKit)

void FAzureSpatialAnchorsForARKit::StartupModule()
{
	FAzureSpatialAnchorsBase::Startup();

	auto AnchorLocatedLambda = std::bind(&FAzureSpatialAnchorsForARKit::AnchorLocatedCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	auto LocateAnchorsCompletedLambda = std::bind(&FAzureSpatialAnchorsForARKit::LocateAnchorsCompletedCallback, this, std::placeholders::_1, std::placeholders::_2);
	auto SessionUpdatedLambda = std::bind(&FAzureSpatialAnchorsForARKit::SessionUpdatedCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

	ARKitInterop = FAzureSpatialAnchorsARKitInterop::Create(AnchorLocatedLambda, LocateAnchorsCompletedLambda, SessionUpdatedLambda);

	IModularFeatures::Get().RegisterModularFeature(IAzureSpatialAnchors::GetModularFeatureName(), this);
	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FAzureSpatialAnchorsForARKit::OnWorldTickStart);
}

void FAzureSpatialAnchorsForARKit::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(IAzureSpatialAnchors::GetModularFeatureName(), this);
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);

	ARKitInterop->StopSession();
	ARKitInterop->DestroySession();
	ARKitInterop = nullptr;

	FAzureSpatialAnchorsBase::Shutdown();
}

bool FAzureSpatialAnchorsForARKit::CreateSession()
{
	return ARKitInterop->CreateSession();
}

void FAzureSpatialAnchorsForARKit::DestroySession()
{
	ARKitInterop->DestroySession();
}

void FAzureSpatialAnchorsForARKit::GetAccessTokenWithAccountKeyAsync(const FString& AccountKey, Callback_Result_String Callback)
{
	ARKitInterop->GetAccessTokenWithAccountKeyAsync(AccountKey, Callback);
}

void FAzureSpatialAnchorsForARKit::GetAccessTokenWithAuthenticationTokenAsync(const FString& AuthenticationToken, Callback_Result_String Callback)
{
	ARKitInterop->GetAccessTokenWithAuthenticationTokenAsync(AuthenticationToken, Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::StartSession()
{
	return ARKitInterop->StartSession();
}

void FAzureSpatialAnchorsForARKit::StopSession()
{
	ARKitInterop->StopSession();
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::ResetSession()
{
	return ARKitInterop->ResetSession();
}

void FAzureSpatialAnchorsForARKit::DisposeSession()
{
	ARKitInterop->DisposeSession();
}

void FAzureSpatialAnchorsForARKit::GetSessionStatusAsync(Callback_Result_SessionStatus Callback)
{
	ARKitInterop->GetSessionStatusAsync(Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::ConstructAnchor(UARPin* InARPin, CloudAnchorID& OutCloudAnchorID)
{
	return ARKitInterop->ConstructAnchor(InARPin, OutCloudAnchorID);
}

void FAzureSpatialAnchorsForARKit::CreateAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	ARKitInterop->CreateAnchorAsync(InCloudAnchorID, Callback);
}

void FAzureSpatialAnchorsForARKit::DeleteAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	ARKitInterop->DeleteAnchorAsync(InCloudAnchorID, Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::CreateWatcher(const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, float InWorldToMetersScale, WatcherID& OutWatcherID, FString& OutErrorString)
{
	return ARKitInterop->CreateWatcher(InLocateCriteria, InWorldToMetersScale, OutWatcherID, OutErrorString);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::GetActiveWatchers(TArray<WatcherID>& OutWatcherIDs)
{
	return ARKitInterop->GetActiveWatchers(OutWatcherIDs);
}

void FAzureSpatialAnchorsForARKit::GetAnchorPropertiesAsync(const FString& InCloudAnchorIdentifier, Callback_Result_CloudAnchorID Callback)
{
	ARKitInterop->GetAnchorPropertiesAsync(InCloudAnchorIdentifier, Callback);
}

void FAzureSpatialAnchorsForARKit::RefreshAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	ARKitInterop->RefreshAnchorPropertiesAsync(InCloudAnchorID, Callback);
}

void FAzureSpatialAnchorsForARKit::UpdateAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback)
{
	ARKitInterop->UpdateAnchorPropertiesAsync(InCloudAnchorID, Callback);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::GetConfiguration(FAzureSpatialAnchorsSessionConfiguration& OutConfig)
{
	return ARKitInterop->GetConfiguration(OutConfig);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::SetConfiguration(const FAzureSpatialAnchorsSessionConfiguration& InConfig)
{
	return ARKitInterop->SetConfiguration(InConfig);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::SetLocationProvider(const FCoarseLocalizationSettings& InConfig)
{
	return ARKitInterop->SetLocationProvider(InConfig);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::GetLogLevel(EAzureSpatialAnchorsLogVerbosity& OutLogVerbosity)
{
	return ARKitInterop->GetLogLevel(OutLogVerbosity);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::SetLogLevel(EAzureSpatialAnchorsLogVerbosity InLogVerbosity)
{
	return ARKitInterop->SetLogLevel(InLogVerbosity);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::GetSessionId(FString& OutSessionID)
{
	return ARKitInterop->GetSessionId(OutSessionID);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::StopWatcher(WatcherID InWatcherIdentifier)
{
	return ARKitInterop->StopWatcher(InWatcherIdentifier);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::GetCloudSpatialAnchorIdentifier(CloudAnchorID InCloudAnchorID, FString& OutCloudAnchorIdentifier)
{
	return ARKitInterop->GetCloudSpatialAnchorIdentifier(InCloudAnchorID, OutCloudAnchorIdentifier);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::SetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float InLifetimeInSeconds)
{
	return ARKitInterop->SetCloudAnchorExpiration(InCloudAnchorID, InLifetimeInSeconds);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::GetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float& OutLifetimeInSeconds)
{
	return ARKitInterop->GetCloudAnchorExpiration(InCloudAnchorID, OutLifetimeInSeconds);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::SetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, const TMap<FString, FString>& InAppProperties)
{
	return ARKitInterop->SetCloudAnchorAppProperties(InCloudAnchorID, InAppProperties);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::GetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, TMap<FString, FString>& OutAppProperties)
{
	return ARKitInterop->GetCloudAnchorAppProperties(InCloudAnchorID, OutAppProperties);
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsForARKit::SetDiagnosticsConfig(FAzureSpatialAnchorsDiagnosticsConfig& InConfig)
{
	return ARKitInterop->SetDiagnosticsConfig(InConfig);
}

void FAzureSpatialAnchorsForARKit::CreateDiagnosticsManifestAsync(const FString& Description, Callback_Result_String Callback)
{
	ARKitInterop->CreateDiagnosticsManifestAsync(Description, Callback);
}

void FAzureSpatialAnchorsForARKit::SubmitDiagnosticsManifestAsync(const FString& ManifestPath, Callback_Result Callback)
{
	ARKitInterop->SubmitDiagnosticsManifestAsync(ManifestPath, Callback);
}

void FAzureSpatialAnchorsForARKit::CreateNamedARPinAroundAnchor(const FString& InLocalAnchorId, UARPin*& OutARPin)
{
	ARKitInterop->CreateNamedARPinAroundAnchor(InLocalAnchorId, OutARPin);
}

bool FAzureSpatialAnchorsForARKit::CreateARPinAroundAzureCloudSpatialAnchor(const FString& PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin)
{
	return ARKitInterop->CreateARPinAroundAzureCloudSpatialAnchor(PinId, InAzureCloudSpatialAnchor, OutARPin);
}

void FAzureSpatialAnchorsForARKit::OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaTime)
{
	ARKitInterop->UpdateSession();
}
