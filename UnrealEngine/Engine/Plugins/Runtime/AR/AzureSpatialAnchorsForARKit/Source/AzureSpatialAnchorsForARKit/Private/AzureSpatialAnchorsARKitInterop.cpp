// Copyright Epic Games, Inc. All Rights Reserved.

#include "AzureSpatialAnchorsARKitInterop.h"
#include <sstream>

DEFINE_LOG_CATEGORY(LogAzureSpatialAnchorsARKitInterop);

typedef TFunction<void(ASACloudSpatialAnchorSession* session, ASAAnchorLocatedEventArgs* args)> ARKitAnchorLocatedCallback;
typedef TFunction<void(ASACloudSpatialAnchorSession* session, ASALocateAnchorsCompletedEventArgs* args)> ARKitLocateAnchorsCompletedCallback;
typedef TFunction<void(ASACloudSpatialAnchorSession* session, ASASessionUpdatedEventArgs* args)> ARKitSessionUpdatedCallback;
typedef TFunction<void(ASACloudSpatialAnchorSession* session, ASASessionErrorEventArgs* args)> ARKitErrorCallback;
typedef TFunction<void(ASACloudSpatialAnchorSession* session, ASAOnLogDebugEventArgs* args)> ARKitLogDebugEventCallback;

@interface AzureSpatialAnchorsSessionCallbackDelegate : NSObject<ASACloudSpatialAnchorSessionDelegate>
{
    ARKitAnchorLocatedCallback anchorLocatedCallback;
    ARKitLocateAnchorsCompletedCallback locateAnchorsCompletedCallback;
    ARKitSessionUpdatedCallback sessionUpdatedCallback;
    ARKitErrorCallback errorCallback;
    ARKitLogDebugEventCallback logDebugEventCallback;
}

-(void)setCallbacks:(ARKitAnchorLocatedCallback)anchorLocated LocateAnchorsCompleted:(ARKitLocateAnchorsCompletedCallback)locateAnchorsCompleted SessionUpdated:(ARKitSessionUpdatedCallback)sessionUpdated Error:(ARKitErrorCallback)error LogDebugEvent:(ARKitLogDebugEventCallback)logDebugEvent;

@end

@implementation AzureSpatialAnchorsSessionCallbackDelegate

-(void)anchorLocated:(ASACloudSpatialAnchorSession *)cloudSpatialAnchorSession :(ASAAnchorLocatedEventArgs *)args {
    anchorLocatedCallback(cloudSpatialAnchorSession, args);
}
    
-(void)locateAnchorsCompleted:(ASACloudSpatialAnchorSession *)cloudSpatialAnchorSession :(ASALocateAnchorsCompletedEventArgs *)args {
    locateAnchorsCompletedCallback(cloudSpatialAnchorSession, args);
}
    
-(void)sessionUpdated:(ASACloudSpatialAnchorSession *)cloudSpatialAnchorSession :(ASASessionUpdatedEventArgs *)args {
    sessionUpdatedCallback(cloudSpatialAnchorSession, args);
}
    
-(void)error:(ASACloudSpatialAnchorSession *)cloudSpatialAnchorSession :(ASASessionErrorEventArgs *)args {
    errorCallback(cloudSpatialAnchorSession, args);
}
    
-(void)onLogDebug:(ASACloudSpatialAnchorSession *)cloudSpatialAnchorSession :(ASAOnLogDebugEventArgs *)args {
    logDebugEventCallback(cloudSpatialAnchorSession, args);
}

-(void)setCallbacks:(ARKitAnchorLocatedCallback)anchorLocated LocateAnchorsCompleted:(ARKitLocateAnchorsCompletedCallback)locateAnchorsCompleted SessionUpdated:(ARKitSessionUpdatedCallback)sessionUpdated Error:(ARKitErrorCallback)error LogDebugEvent:(ARKitLogDebugEventCallback)logDebugEvent {
    anchorLocatedCallback = anchorLocated;
    locateAnchorsCompletedCallback = locateAnchorsCompleted;
    sessionUpdatedCallback = sessionUpdated;
    errorCallback = error;
    logDebugEventCallback = logDebugEvent;
}
@end

TSharedPtr<FAzureSpatialAnchorsARKitInterop, ESPMode::ThreadSafe> FAzureSpatialAnchorsARKitInterop::Create(
	AnchorLocatedCallbackPtr AnchorLocatedCallback,
	LocateAnchorsCompletedCallbackPtr LocateAnchorsCompletedCallback,
	SessionUpdatedCallbackPtr SessionUpdatedCallback)
{
	struct SharedFAzureSpatialAnchorsARKitInterop : public FAzureSpatialAnchorsARKitInterop {};
	TSharedPtr<FAzureSpatialAnchorsARKitInterop, ESPMode::ThreadSafe> Output = MakeShareable( new SharedFAzureSpatialAnchorsARKitInterop());
	Output->SetWeakThis(Output);
	Output->SetCallbacks(AnchorLocatedCallback, LocateAnchorsCompletedCallback, SessionUpdatedCallback);
        
	return Output;
}

FAzureSpatialAnchorsARKitInterop::FAzureSpatialAnchorsARKitInterop()
{
    CallbackHelper = [[AzureSpatialAnchorsSessionCallbackDelegate alloc] init];
    
    ARKitAnchorLocatedCallback anchorLocatedCallback = std::bind(&FAzureSpatialAnchorsARKitInterop::OnAnchorLocated, this, std::placeholders::_1, std::placeholders::_2);
    ARKitLocateAnchorsCompletedCallback locateAnchorsCompletedCallback = std::bind(&FAzureSpatialAnchorsARKitInterop::OnLocateAnchorsCompleted, this, std::placeholders::_1, std::placeholders::_2);
    ARKitSessionUpdatedCallback sessionUpdatedCallback = std::bind(&FAzureSpatialAnchorsARKitInterop::OnSessionUpdated, this, std::placeholders::_1, std::placeholders::_2);
    ARKitErrorCallback errorCallback = std::bind(&FAzureSpatialAnchorsARKitInterop::OnError, this, std::placeholders::_1, std::placeholders::_2);
    ARKitLogDebugEventCallback logDebugEventCallback = std::bind(&FAzureSpatialAnchorsARKitInterop::OnLogDebugEvent, this, std::placeholders::_1, std::placeholders::_2);
    
    [CallbackHelper setCallbacks:anchorLocatedCallback LocateAnchorsCompleted:locateAnchorsCompletedCallback SessionUpdated:sessionUpdatedCallback Error:errorCallback LogDebugEvent:logDebugEventCallback];
}

FAzureSpatialAnchorsARKitInterop::~FAzureSpatialAnchorsARKitInterop()
{
    if (CallbackHelper)
    {
        [CallbackHelper release];
        CallbackHelper = nullptr;
    }
}

void FAzureSpatialAnchorsARKitInterop::SetCallbacks(
	AnchorLocatedCallbackPtr AnchorLocated,
	LocateAnchorsCompletedCallbackPtr LocateAnchorsCompleted,
	SessionUpdatedCallbackPtr SessionUpdated)
{
	this->AnchorLocatedCallback = AnchorLocated;
	this->LocateAnchorsCompletedCallback = LocateAnchorsCompleted;
	this->SessionUpdatedCallback = SessionUpdated;
}

bool FAzureSpatialAnchorsARKitInterop::CreateSession()
{
    if (UARBlueprintLibrary::GetARSessionStatus().Status != EARSessionStatus::Running)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Failed to create an Azure Spatial Anchors session, no AR Session was found to be running"));
        return false;
    }
    
    if (Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Warning, TEXT("Creating an Azure Spatial Anchors session when a session already existed"));
        DestroySession();
    }
    
    UE_LOG(LogAzureSpatialAnchorsARKitInterop, Log, TEXT("Creating an Azure Spatial Anchors session"));
    
    auto TempARSystem = TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe>{ StaticCastSharedPtr<FXRTrackingSystemBase>(GEngine->XRSystem)->GetARCompositionComponent() };
    if (!TempARSystem.IsValid())
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Azure Spatial Anchors failed to obtain a valid AR System"));
        return false;
    }
    
    void* SessionHandle = TempARSystem->GetARSessionRawPointer();
    if (!SessionHandle)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Azure Spatial Anchors failed to obtain a valid sesion handle"));
        return false;
    }
    
    ARSystem = TempARSystem;
    Session = [[ASACloudSpatialAnchorSession alloc] init];
    Session.session = static_cast<ARSession*>(SessionHandle);
    Session.delegate = CallbackHelper;
    
    UE_LOG(LogAzureSpatialAnchorsARKitInterop, Log, TEXT("Azure Spatial Anchors session successfully created"));
    
    return true;
}

void FAzureSpatialAnchorsARKitInterop::DestroySession()
{
    if (Session)
    {
        [Session stop];
        [Session dispose];
        Session = nullptr;
    }
}

void FAzureSpatialAnchorsARKitInterop::GetAccessTokenWithAccountKeyAsync(const FString& AccountKey, IAzureSpatialAnchors::Callback_Result_String Callback)
{
    if (!Session)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"", L"");
        return;
    }
    
    [Session getAccessTokenWithAccountKey:FStringToNSString(AccountKey) withCompletionHandler:^(NSString *value, NSError *error) {
        std::wstringstream string;
        if (error != nullptr)
        {
            string << L"Failed to obtain access token from account key: " << *NSStringToFString(error.localizedDescription);
            Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str(), L"");
            return;
        }
        
        string << *NSStringToFString(value);
        Callback(EAzureSpatialAnchorsResult::Success, L"", string.str().c_str());
    }];
}

void FAzureSpatialAnchorsARKitInterop::GetAccessTokenWithAuthenticationTokenAsync(const FString& AuthenticationToken, IAzureSpatialAnchors::Callback_Result_String Callback)
{
    if (!Session)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"", L"");
        return;
    }
    
    [Session getAccessTokenWithAuthenticationToken:FStringToNSString(AuthenticationToken) withCompletionHandler:^(NSString *value, NSError *error) {
        std::wstringstream string;
        if (error != nullptr)
        {
            string << L"Failed to obtain access token from authentication token: " << *NSStringToFString(error.localizedDescription);
            Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str(), L"");
            return;
        }
        
        string << *NSStringToFString(value);
        Callback(EAzureSpatialAnchorsResult::Success, L"", string.str().c_str());
    }];
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::StartSession()
{
    if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Failed to start Azure Spatial Anchors session, session was null"));
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    [Session start];
    bSessionRunning = true;
    UE_LOG(LogAzureSpatialAnchorsARKitInterop, Log, TEXT("Azure Spatial Anchors successfully started a session"));
    return EAzureSpatialAnchorsResult::Success;
}

void FAzureSpatialAnchorsARKitInterop::UpdateSession()
{
    if (Session &&
        ARSystem.IsValid() &&
        bSessionRunning)
    {
        if (UARBlueprintLibrary::GetARSessionStatus().Status != EARSessionStatus::Running)
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Warning, TEXT("AR Session was stopped before azure spatial anchors session was cleaned up"));
            return;
        }
        
        void* frameHandle = ARSystem->GetGameThreadARFrameRawPointer();
        if (frameHandle != nullptr)
        {
            ARFrame* frame = static_cast<ARFrame*>(frameHandle);
            [Session processFrame:frame];
        }
        else
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Azure Spatial Anchors was unable to obtain a frame handle, failed to update the session"));
        }
    }
}

void FAzureSpatialAnchorsARKitInterop::StopSession()
{
    if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Warning, TEXT("Azure Spatial Anchors failed to stop session, session was null"));
        return;
    }
    
    if (CurrentWatcher != nullptr)
    {
        StopWatcher(CurrentWatcher.identifier);
    }
    
    LocalAnchorMap.Empty();
    
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        for (auto AnchorPair : CloudAnchorMap)
        {
            auto NativeAnchor = AnchorPair.Value.localAnchor;
            if (NativeAnchor != nullptr)
            {
                [NativeAnchor release];
            }
            
            [AnchorPair.Value release];
        }
        
        CloudAnchorMap.Empty();
    }
    
    {
        FScopeLock Lock(&SavedAnchorsMutex);
        SavedAnchors.Empty();
    }
    
    [Session stop];
    bSessionRunning = false;
    
    UE_LOG(LogAzureSpatialAnchorsARKitInterop, Log, TEXT("Azure Spatial Anchors stopped a session"));
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::ResetSession()
{
	if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Warning, TEXT("Azure Spatial Anchors failed to reset session, session was null"));
    }
    
    [Session reset];
    return EAzureSpatialAnchorsResult::Success;
}

void FAzureSpatialAnchorsARKitInterop::DisposeSession()
{
    if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Warning, TEXT("Azure Spatial Anchors failed to dispose session, session was null"));
        return;
    }
    
    [Session dispose];
    Session = nullptr;
}

void FAzureSpatialAnchorsARKitInterop::GetSessionStatusAsync(IAzureSpatialAnchors::Callback_Result_SessionStatus Callback)
{
    if (!Session)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"", FAzureSpatialAnchorsSessionStatus{});
        return;
    }
    
    if (!bSessionRunning)
    {
        Callback(EAzureSpatialAnchorsResult::NotStarted, L"", FAzureSpatialAnchorsSessionStatus{});
        return;
    }
    
    [Session getSessionStatusWithCompletionHandler:^(ASASessionStatus * value, NSError *error) {
        FAzureSpatialAnchorsSessionStatus SessionStatus;
        
        if (error != nullptr)
        {
            std::wstringstream string;
            string << L"Failed to obtain session status: " << *NSStringToFString(error.localizedDescription);
            Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str(), SessionStatus);
            return;
        }
        
        SessionStatus.ReadyForCreateProgress = value.readyForCreateProgress;
        SessionStatus.RecommendedForCreateProgress = value.recommendedForCreateProgress;
        SessionStatus.SessionCreateHash = value.sessionCreateHash;
        SessionStatus.SessionLocateHash = value.sessionLocateHash;
        SessionStatus.feedback = static_cast<EAzureSpatialAnchorsSessionUserFeedback>(value.userFeedback);
        
        Callback(EAzureSpatialAnchorsResult::Success, L"", SessionStatus);
    }];
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::ConstructAnchor(UARPin* InARPin, IAzureSpatialAnchors::CloudAnchorID& OutCloudAnchorID)
{
    OutCloudAnchorID = IAzureSpatialAnchors::CloudAnchorID_Invalid;
    if (!Session ||
        !bSessionRunning)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to create an AzureCloudSpatialanchor when no session is running"));
        return EAzureSpatialAnchorsResult::NotStarted;
    }
    
    if (!InARPin)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Provided ARPin was null"));
        return EAzureSpatialAnchorsResult::FailNoARPin;
    }
    
    auto NativeAnchor = static_cast<ARAnchor*>(InARPin->GetNativeResource());
    if (!NativeAnchor)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Provided ARPin did not have a valid associated ARAnchor, Azure Spatial Anchors was unable to create a cloud anchor"));
        return EAzureSpatialAnchorsResult::FailNoLocalAnchor;
    }
    
    UAzureCloudSpatialAnchor* OutCloudAnchor = NewObject<UAzureCloudSpatialAnchor>();
    OutCloudAnchor->ARPin = InARPin;
    OutCloudAnchor->CloudAnchorID = GetNextID();
    LocalAnchorMap.Add(OutCloudAnchor->CloudAnchorID, OutCloudAnchor);
    
    auto CloudAnchor = [[ASACloudSpatialAnchor alloc] init];
    [CloudAnchor retain];
    [NativeAnchor retain];
    CloudAnchor.localAnchor = NativeAnchor;
    
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        CloudAnchorMap.Add(OutCloudAnchor->CloudAnchorID, CloudAnchor);
    }
    
    OutCloudAnchorID = OutCloudAnchor->CloudAnchorID;
	return EAzureSpatialAnchorsResult::Success;
}

void FAzureSpatialAnchorsARKitInterop::CreateAnchorAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback)
{
    if (!Session)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"Azure Spatial Anchors session has not been started/does not exist");
        return;
    }
    
    if (!LocalAnchorMap.Contains(InCloudAnchorID))
    {
        Callback(EAzureSpatialAnchorsResult::FailAnchorDoesNotExist, L"Provided AzureCloudSpatialAnchor was not known to plugin");
        return;
    }
    
    auto LocalAnchor = LocalAnchorMap[InCloudAnchorID];
    if (!LocalAnchor->ARPin)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoAnchor, L"Provided local anchor had null ARPin");
        return;
    }
    
    auto NativeAnchor = static_cast<ARAnchor*>(LocalAnchor->ARPin->GetNativeResource());
    if (!NativeAnchor)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoLocalAnchor, L"Provided ARPin did not have a valid associated ARAnchor, Azure Spatial Anchors was unable to create a cloud anchor");
        return;
    }
    
    ASACloudSpatialAnchor *CloudAnchor = nullptr;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            Callback(EAzureSpatialAnchorsResult::FailAnchorDoesNotExist, L"Provided AzureCloudSpatialAnchor was not known to plugin");
            return;
        }
        
        CloudAnchor = CloudAnchorMap[InCloudAnchorID];
    }
    
    {
        // TODO: decide the best practice for avoiding exceptions due to setting properties after the anchor has been saved
        FScopeLock Lock(&SavedAnchorsMutex);
        SavedAnchors.Add(InCloudAnchorID);
    }
    
    [Session createAnchor:CloudAnchor withCompletionHandler:^(NSError *error) {
        if (error != nullptr)
        {
            std::wstringstream string;
            string << L"Failed to create azure spatial anchor: " << *NSStringToFString(error.localizedDescription) << L", " << *NSStringToFString(error.localizedFailureReason);
            Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str());
            return;
        }
        
        Callback(EAzureSpatialAnchorsResult::Success, L"");
    }];
}

void FAzureSpatialAnchorsARKitInterop::DeleteAnchorAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback)
{
    if (!Session)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"");
        return;
    }
    
    ASACloudSpatialAnchor* CloudAnchor;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            Callback(EAzureSpatialAnchorsResult::FailAnchorDoesNotExist, L"Provided AzureCloudSpatialAnchor was not known to plugin");
            return;
        }
        
        CloudAnchor = CloudAnchorMap[InCloudAnchorID];
    }
    
    if ([CloudAnchor.identifier length] == 0)
    {
        ClearAnchorData(InCloudAnchorID);
        Callback(EAzureSpatialAnchorsResult::Success, L"");
        return;
    }
    
    [Session deleteAnchor:CloudAnchor withCompletionHandler:^(NSError* error) {
        if (error != nullptr)
        {
           std::wstringstream string;
           string << L"Failed to delete azure spatial anchor: " << *NSStringToFString(error.localizedDescription);
           Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str());
           return;
        }
        
        this->ClearAnchorData(InCloudAnchorID);
        Callback(EAzureSpatialAnchorsResult::Success, L"");
    }];
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::CreateWatcher(const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, float InWorldToMetersScale, IAzureSpatialAnchors::WatcherID& OutWatcherIdentifier, FString& OutErrorString)
{
    if (!Session ||
        !bSessionRunning)
    {
        OutErrorString = TEXT("Attempted to create a watcher when no Azure Spatial Anchors session was running");
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    if (CurrentWatcher != nullptr)
    {
        OutErrorString = TEXT("Attempted to start a watcher when a watcher already exists. Only one watcher is supported at a time");
        return EAzureSpatialAnchorsResult::FailSeeErrorString;
    }
    
    ASACloudSpatialAnchor *NearAnchor = nullptr;
    if (InLocateCriteria.NearAnchor != nullptr)
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InLocateCriteria.NearAnchor->CloudAnchorID))
        {
            OutErrorString = TEXT("Provided near anchor has not been saved/associated with the Azure Spatial Anchor service");
            return EAzureSpatialAnchorsResult::FailBadCloudAnchorIdentifier;
        }
        
        NearAnchor = CloudAnchorMap[InLocateCriteria.NearAnchor->CloudAnchorID];
    }
    
    ASAAnchorLocateCriteria *Criteria = [ASAAnchorLocateCriteria new];
    
    Criteria.bypassCache = InLocateCriteria.bBypassCache;
    Criteria.requestedCategories = static_cast<ASAAnchorDataCategory>(InLocateCriteria.RequestedCategories);
    Criteria.strategy = static_cast<ASALocateStrategy>(InLocateCriteria.Strategy);
    
    NSMutableArray *Identifiers = [[NSMutableArray alloc] init];
    for (FString Identifier : InLocateCriteria.Identifiers)
    {
        [Identifiers addObject:FStringToNSString(Identifier)];
    }
    Criteria.identifiers = Identifiers;
    
    if (NearAnchor != nullptr)
    {
        ASANearAnchorCriteria *NearCriteria = [ASANearAnchorCriteria new];
        NearCriteria.distanceInMeters = InLocateCriteria.NearAnchorDistance / 100.0f; // Convert from cm to m
        NearCriteria.sourceAnchor = NearAnchor;
        NearCriteria.maxResultCount = InLocateCriteria.NearAnchorMaxResultCount;
        Criteria.nearAnchor = NearCriteria;
    }

    if (InLocateCriteria.bSearchNearDevice)
    {
        ASANearDeviceCriteria *DeviceCriteria = [ASANearDeviceCriteria new];
        DeviceCriteria.distanceInMeters = InLocateCriteria.NearDeviceDistance / 100.0f; // Convert from cm to m
        DeviceCriteria.maxResultCount = InLocateCriteria.NearDeviceMaxResultCount;
        Criteria.nearDevice = DeviceCriteria;
    }
    
    CurrentWatcher = [Session createWatcher:Criteria];
    [CurrentWatcher retain];
    OutWatcherIdentifier = CurrentWatcher.identifier;
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::GetActiveWatchers(TArray<IAzureSpatialAnchors::WatcherID>& OutWatcherIDs)
{
    OutWatcherIDs.Empty();
    if (CurrentWatcher)
    {
        OutWatcherIDs.Add(CurrentWatcher.identifier);
    }
    
    return EAzureSpatialAnchorsResult::Success;
}

void FAzureSpatialAnchorsARKitInterop::GetAnchorPropertiesAsync(const FString& InCloudAnchorIdentifier, IAzureSpatialAnchors::Callback_Result_CloudAnchorID Callback)
{
    if (!Session ||
        !bSessionRunning)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"Attempted to get AzureCloudSpatialAnchor properties when no session was running", IAzureSpatialAnchors::CloudAnchorID_Invalid);
        return;
    }
    
    if (InCloudAnchorIdentifier.IsEmpty())
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"Provided CloudIdentifier was invalid", IAzureSpatialAnchors::CloudAnchorID_Invalid);
        return;
    }
    
    NSString *Identifier = FStringToNSString(InCloudAnchorIdentifier);
    IAzureSpatialAnchors::CloudAnchorID CloudAnchorID = IAzureSpatialAnchors::CloudAnchorID_Invalid;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        for (auto pair : CloudAnchorMap)
        {
            if (pair.Value.identifier == Identifier)
            {
                CloudAnchorID = pair.Key;
                break;
            }
        }
    }
    
    if (CloudAnchorID == IAzureSpatialAnchors::CloudAnchorID_Invalid)
    {
        Callback(EAzureSpatialAnchorsResult::FailBadCloudAnchorIdentifier, L"Provided CloudIdentifier was not known to the plugin", IAzureSpatialAnchors::CloudAnchorID_Invalid);
        return;
    }
    
    [Session getAnchorProperties:Identifier withCompletionHandler: ^(ASACloudSpatialAnchor *value, NSError *error) {
        if (error != nullptr)
        {
            std::wstringstream string;
            string << L"Failed to get azure spatial anchor properties: " << *NSStringToFString(error.localizedDescription);
            Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str(), IAzureSpatialAnchors::CloudAnchorID_Invalid);
            return;
        }
        
        Callback(EAzureSpatialAnchorsResult::Success, L"", CloudAnchorID);
    }];
}

void FAzureSpatialAnchorsARKitInterop::RefreshAnchorPropertiesAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback)
{
	if (!Session ||
        !bSessionRunning)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"Attempted to refresh AzureCloudSpatialAnchor properties when no session was running");
        return;
    }
    
    ASACloudSpatialAnchor *CloudAnchor = nullptr;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            Callback(EAzureSpatialAnchorsResult::FailAnchorDoesNotExist, L"Provided AzureCloudSpatialAnchor does not have an associated cloud anchor. This anchor was not saved/declared to the Azure Spatial Anchors service");
            return;
        }
        
        CloudAnchor = CloudAnchorMap[InCloudAnchorID];
    }
    
    if ([CloudAnchor.identifier length] == 0)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoCloudAnchor, L"Provided AzureCloudSpatialAnchor has not been saved to the service");
        return;
    }
    
    [Session refreshAnchorProperties:CloudAnchor withCompletionHandler:^(NSError *error) {
        if (error != nullptr)
        {
           std::wstringstream string;
           string << L"Failed to refresh azure spatial anchor properties: " <<  *NSStringToFString(error.localizedDescription);
           Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str());
           return;
        }
        
        Callback(EAzureSpatialAnchorsResult::Success, L"");
    }];
}

void FAzureSpatialAnchorsARKitInterop::UpdateAnchorPropertiesAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback)
{
    if (!Session ||
        !bSessionRunning)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"Attempted to update AzureCloudSpatialAnchor properties when no session was running");
        return;
    }
    
    ASACloudSpatialAnchor *CloudAnchor = nullptr;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            Callback(EAzureSpatialAnchorsResult::FailAnchorDoesNotExist, L"Provided AzureCloudSpatialAnchor does not have an associated cloud anchor. This anchor was not saved/declared to the Azure Spatial Anchors service");
            return;
        }
        
        CloudAnchor = CloudAnchorMap[InCloudAnchorID];
    }
    
    [Session updateAnchorProperties:CloudAnchor withCompletionHandler:^(NSError *error) {
        if (error != nullptr)
        {
           std::wstringstream string;
           string << L"Failed to update azure spatial anchor properties: " <<  *NSStringToFString(error.localizedDescription);
           Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str());
           return;
        }
        
        Callback(EAzureSpatialAnchorsResult::Success, L"");
    }];
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::GetConfiguration(FAzureSpatialAnchorsSessionConfiguration& OutConfig)
{
    if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Failed to configure Azure Spatial Anchors session, session was null"));
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    OutConfig.AccessToken = NSStringToFString(Session.configuration.accessToken);
    OutConfig.AccountId = NSStringToFString(Session.configuration.accountId);
    OutConfig.AccountKey = NSStringToFString(Session.configuration.accountKey);
    OutConfig.AccountDomain = NSStringToFString(Session.configuration.accountDomain);
    OutConfig.AuthenticationToken = NSStringToFString(Session.configuration.authenticationToken);
    
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::SetConfiguration(const FAzureSpatialAnchorsSessionConfiguration& InConfig)
{
    if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Failed to configure Azure Spatial Anchors session, session was null"));
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
   
    if (!InConfig.AccessToken.IsEmpty())
    {
        Session.configuration.accessToken = FStringToNSString(InConfig.AccessToken);
    }
    
    if (!InConfig.AccountId.IsEmpty())
    {
        Session.configuration.accountId = FStringToNSString(InConfig.AccountId);
    }
    
    if (!InConfig.AccountKey.IsEmpty())
    {
        Session.configuration.accountKey = FStringToNSString(InConfig.AccountKey);
    }
    
    if (!InConfig.AccountDomain.IsEmpty())
    {
        Session.configuration.accountDomain = FStringToNSString(InConfig.AccountDomain);
    }
    
    if (!InConfig.AuthenticationToken.IsEmpty())
    {
        Session.configuration.accessToken = FStringToNSString(InConfig.AuthenticationToken);
    }
    
	return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::SetLocationProvider(const FCoarseLocalizationSettings& InConfig)
{
    if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Azure Spatial Anchors failed to set location provider for session, session was null"));
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    if (InConfig.bEnable)
    {
        auto LocationProvider = [[ASAPlatformLocationProvider alloc] init];
        LocationProvider.sensors.geoLocationEnabled = InConfig.bEnableGPS;
        LocationProvider.sensors.wifiEnabled = InConfig.bEnableWifi;
        
        bool bBluetoothEnabled = InConfig.BLEBeaconUUIDs.Num() > 0;
        LocationProvider.sensors.bluetoothEnabled = bBluetoothEnabled;
        if (bBluetoothEnabled)
        {
            NSMutableArray *beacons = [NSMutableArray array];
            for (int i = 0; i < InConfig.BLEBeaconUUIDs.Num(); i++)
            {
                [beacons addObject:FStringToNSString(InConfig.BLEBeaconUUIDs[i])];
            }
            
            LocationProvider.sensors.knownBeaconProximityUuids = beacons;
        }
        
        Session.locationProvider = LocationProvider;
    }
    else
    {
        // TODO: jeff.fisher - uncomment out this bottom line when the service crash is fixed
        // Session.locationProvider = nullptr;
    }
    
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::GetLogLevel(EAzureSpatialAnchorsLogVerbosity& OutLogVerbosity)
{
    if (!Session)
    {
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    OutLogVerbosity = static_cast<EAzureSpatialAnchorsLogVerbosity>(Session.logLevel);
    return EAzureSpatialAnchorsResult::Success;
}
	
EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::SetLogLevel(EAzureSpatialAnchorsLogVerbosity InLogVerbosity)
{
    if (!Session)
    {
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    Session.logLevel = static_cast<ASASessionLogLevel>(InLogVerbosity);
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::GetSessionId(FString& OutSessionID)
{
    if (!Session)
    {
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    OutSessionID = NSStringToFString(Session.sessionId);
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::StopWatcher(IAzureSpatialAnchors::WatcherID InWatcherIdentifier)
{
    if (!Session ||
        !bSessionRunning)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to stop a watcher when no Azure Spatial Anchor session was running"));
        return EAzureSpatialAnchorsResult::NotStarted;
    }
    
    if (CurrentWatcher == nullptr ||
        CurrentWatcher.identifier != InWatcherIdentifier)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to stop a watcher that was not known to the current Azure Spatial Anchor"));
        return EAzureSpatialAnchorsResult::FailNoWatcher;
    }
    
    [CurrentWatcher stop];
    [CurrentWatcher release];
    CurrentWatcher = nullptr;
	return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::GetCloudSpatialAnchorIdentifier(IAzureSpatialAnchors::CloudAnchorID CloudAnchorID, FString& OutCloudAnchorIdentifier)
{
    OutCloudAnchorIdentifier = FString{};
    
    if (!Session)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to get a cloud anchor identifier when no Azure Spatial Anchor session is running"));
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    ASACloudSpatialAnchor *CloudAnchor = nullptr;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(CloudAnchorID))
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to get cloud anchor identifier for an anchor not known to the plugin"));
            return EAzureSpatialAnchorsResult::FailNoCloudAnchor;
        }
        
        CloudAnchor = CloudAnchorMap[CloudAnchorID];
        if (CloudAnchor == nil)
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to get cloud anchor identifier for an anchor released by the plugin"));
            CloudAnchorMap.Remove(CloudAnchorID);
            return EAzureSpatialAnchorsResult::FailNoCloudAnchor;
        }
    }
    
    OutCloudAnchorIdentifier = NSStringToFString(CloudAnchor.identifier);
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::SetCloudAnchorExpiration(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, float InLifetimeInSeconds)
{
    if (!Session ||
        !bSessionRunning)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to set cloud anchor expiration when no Azure Spatial Anchors session is running"));
        return EAzureSpatialAnchorsResult::NotStarted;
    }
    
    ASACloudSpatialAnchor *CloudAnchor = nullptr;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to set cloud anchor expiration for an anchor not known to the plugin"));
            return EAzureSpatialAnchorsResult::FailNoCloudAnchor;
        }
        
        CloudAnchor = CloudAnchorMap[InCloudAnchorID];
    }
    
    bool bAnchorSaved = false;
    {
        FScopeLock Lock(&SavedAnchorsMutex);
        bAnchorSaved = SavedAnchors.Contains(InCloudAnchorID);
    }
    
    if (bAnchorSaved)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to set cloud anchor expiration for an anchor already saved to the Azure Spatial Anchors session. This is not supported."));
        return EAzureSpatialAnchorsResult::FailAnchorAlreadyTracked;
    }
    
    NSDate *Expiration = [[NSDate alloc] initWithTimeIntervalSinceNow: static_cast<NSTimeInterval>(InLifetimeInSeconds)];
    CloudAnchor.expiration = Expiration;
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::GetCloudAnchorExpiration(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, float& OutLifetimeInSeconds)
{
    if (!Session ||
        !bSessionRunning)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to get cloud anchor expiration when no Azure Spatial Anchors session is running"));
        return EAzureSpatialAnchorsResult::NotStarted;
    }
    
    ASACloudSpatialAnchor *CloudAnchor = nullptr;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to get cloud anchor expiration for an anchor not known to the plugin"));
            return EAzureSpatialAnchorsResult::FailNoCloudAnchor;
        }
        
        CloudAnchor = CloudAnchorMap[InCloudAnchorID];
    }
    
    OutLifetimeInSeconds = CloudAnchor.expiration.timeIntervalSinceNow;
    return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::SetCloudAnchorAppProperties(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, const TMap<FString, FString>& InAppProperties)
{
    if (!Session ||
        !bSessionRunning)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to set cloud anchor app properties when no Azure Spatial anchors session is running"));
        return EAzureSpatialAnchorsResult::NotStarted;
    }
    
    ASACloudSpatialAnchor *CloudAnchor = nullptr;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to set cloud anchor app properties for an AzureCloudSpatialAnchor not known to the plugin"));
            return EAzureSpatialAnchorsResult::FailNoCloudAnchor;
        }
        
        CloudAnchor = CloudAnchorMap[InCloudAnchorID];
    }
    
    if ([CloudAnchor.identifier length] == 0)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to set cloud anchor app properties for an AzureCloudSpatialAnchor not known to the service"));
        return EAzureSpatialAnchorsResult::FailNoCloudAnchor;
    }
    
    NSMutableDictionary *AppProperties = [[NSMutableDictionary alloc] init];
    for (auto Property : InAppProperties)
    {
        AppProperties[FStringToNSString(Property.Key)] = FStringToNSString(Property.Value);
    }
    
    CloudAnchor.appProperties = AppProperties;
	return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::GetCloudAnchorAppProperties(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, TMap<FString, FString>& OutAppProperties)
{
    OutAppProperties.Empty();
    if (!Session ||
        !bSessionRunning)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to get cloud anchor app properties when no Azure Spatial Anchors session is running"));
        return EAzureSpatialAnchorsResult::NotStarted;
    }
    
    NSDictionary *Properties;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InCloudAnchorID))
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to get cloud anchor properties for an AzureCloudSpatialAnchor not known to the plugin"));
            return EAzureSpatialAnchorsResult::FailNoCloudAnchor;
        }
        
        Properties = CloudAnchorMap[InCloudAnchorID].appProperties;
    }
    
    for (NSString *Key in Properties)
    {
        OutAppProperties[NSStringToFString(Key)] = NSStringToFString([Properties objectForKey:Key]);
    }
    
	return EAzureSpatialAnchorsResult::Success;
}

EAzureSpatialAnchorsResult FAzureSpatialAnchorsARKitInterop::SetDiagnosticsConfig(FAzureSpatialAnchorsDiagnosticsConfig& InConfig)
{
    if (!Session)
    {
        return EAzureSpatialAnchorsResult::FailNoSession;
    }
    
    Session.diagnostics.imagesEnabled = InConfig.bImagesEnabled;
    Session.diagnostics.logDirectory = FStringToNSString(InConfig.LogDirectory);
    Session.diagnostics.logLevel = static_cast<ASASessionLogLevel>(InConfig.LogLevel);
    Session.diagnostics.maxDiskSizeInMB = InConfig.MaxDiskSizeInMB;
    
    return EAzureSpatialAnchorsResult::Success;
}

void FAzureSpatialAnchorsARKitInterop::CreateDiagnosticsManifestAsync(const FString& Description, IAzureSpatialAnchors::Callback_Result_String Callback)
{
    if (!Session)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"", L"");
    }
    
    NSString *Desc = FStringToNSString(Description);
    [Session.diagnostics createManifest:Desc withCompletionHandler:^(NSString *value, NSError *error) {
        std::wstringstream string;
        if (error != nullptr)
        {
           string << L"Failed to create diagnostics manifest: " <<  *NSStringToFString(error.localizedDescription);
           Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str(), L"");
           return;
        }
        
        string << *NSStringToFString(value);
        Callback(EAzureSpatialAnchorsResult::Success, L"", string.str().c_str());
    }];
}

void FAzureSpatialAnchorsARKitInterop::SubmitDiagnosticsManifestAsync(const FString& ManifestPath, IAzureSpatialAnchors::Callback_Result Callback)
{
    if (!Session)
    {
        Callback(EAzureSpatialAnchorsResult::FailNoSession, L"");
    }
    
    NSString *Path = FStringToNSString(ManifestPath);
    [Session.diagnostics submitManifest:Path withCompletionHandler:^(NSError *error) {
        if (error != nullptr)
        {
           std::wstringstream string;
           string << L"Failed to submit diagnostics manifest: " <<  *NSStringToFString(error.localizedDescription);
           Callback(EAzureSpatialAnchorsResult::FailSeeErrorString, string.str().c_str());
           return;
        }
        
        Callback(EAzureSpatialAnchorsResult::Success, L"");
    }];
}

void FAzureSpatialAnchorsARKitInterop::CreateNamedARPinAroundAnchor(const FString& InLocalAnchorId, UARPin*& OutARPin)
{
    OutARPin = nullptr;
    // TODO: jeff.fisher
}

bool FAzureSpatialAnchorsARKitInterop::CreateARPinAroundAzureCloudSpatialAnchor(const FString& PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin)
{
    OutARPin = nullptr;
    if (!ARSystem.IsValid())
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to create an ARPin for an AzureCloudSpatialAnchor when no valid AR System was found"));
    }
    
    if (!Session ||
        !bSessionRunning)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to create an ARPin for an AzureCloudSpatialAnchor when no Azure Spatial Anchor session was running"));
        return false;
    }
    
    if (!InAzureCloudSpatialAnchor)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to create an ARPin for a null AzureCloudSpatialAnchor"));
        return false;
    }
    
    if (InAzureCloudSpatialAnchor->ARPin)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Warning, TEXT("Attempted to create an ARPin for an AzureCloudSpatialAnchor that already had an ARPin"));
        return true;
    }
    
    ASACloudSpatialAnchor *Anchor;
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (!CloudAnchorMap.Contains(InAzureCloudSpatialAnchor->CloudAnchorID))
        {
            UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to create an ARPin for an invalid AzureCloudSpatialAnchor"));
            return false;
        }
        
        Anchor = CloudAnchorMap[InAzureCloudSpatialAnchor->CloudAnchorID];
    }
    
    ARAnchor* LocalAnchor = Anchor.localAnchor;
    if (!LocalAnchor)
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Attempted to create an ARPin for an AzureCloudSpatialAnchor without a valid ARAnchor"));
        return false;
    }
    
    UARPin* ARPin = nullptr;
    if(!ARSystem->TryGetOrCreatePinForNativeResource(static_cast<void*>(LocalAnchor), PinId, ARPin))
    {
        UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("ARKit failed to create an ARPin for the provided ARAnchor"));
        return false;
    }

    InAzureCloudSpatialAnchor->ARPin = ARPin;
    OutARPin = ARPin;
	return true;
}

IAzureSpatialAnchors::CloudAnchorID FAzureSpatialAnchorsARKitInterop::GetNextID()
{
	FScopeLock Lock(&CurrentIDMutex);
	int Id = CurrentID;
	CurrentID++;
	return Id;
}

void FAzureSpatialAnchorsARKitInterop::ClearAnchorData(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID)
{
    if (LocalAnchorMap.Contains(InCloudAnchorID))
    {
        LocalAnchorMap.Remove(InCloudAnchorID);
    }
    
    {
        FScopeLock Lock(&CloudAnchorMapMutex);
        if (CloudAnchorMap.Contains(InCloudAnchorID))
        {
            auto CloudAnchor = CloudAnchorMap[InCloudAnchorID].localAnchor;
            [CloudAnchor release];
            CloudAnchorMap.Remove(InCloudAnchorID);
        }
    }
}

void FAzureSpatialAnchorsARKitInterop::OnAnchorLocated(ASACloudSpatialAnchorSession* session, ASAAnchorLocatedEventArgs* args)
{
    if (!Session ||
        !bSessionRunning)
    {
        return;
    }
    
    if (args.anchor != nullptr)
    {
        IAzureSpatialAnchors::CloudAnchorID CloudAnchorID = IAzureSpatialAnchors::CloudAnchorID_Invalid;
        {
            FScopeLock Lock(&CloudAnchorMapMutex);
            for (auto Pair : CloudAnchorMap)
            {
                if ([Pair.Value.identifier length] > 0 &&
                    [Pair.Value.identifier isEqualToString:args.anchor.identifier])
                {
                    CloudAnchorID = Pair.Key;
                    break;
                }
            }
        }
        
        auto CloudAnchor = args.anchor;
        if (CloudAnchorID == IAzureSpatialAnchors::CloudAnchorID_Invalid)
        {
            UAzureCloudSpatialAnchor* Anchor = NewObject<UAzureCloudSpatialAnchor>();
            Anchor->ARPin = nullptr;
            Anchor->CloudAnchorID = GetNextID();
            CloudAnchorID = Anchor->CloudAnchorID;
            
            LocalAnchorMap.Add(Anchor->CloudAnchorID, Anchor);
            
            {
                FScopeLock Lock(&CloudAnchorMapMutex);
                [CloudAnchor retain];
                CloudAnchorMap.Add(Anchor->CloudAnchorID, CloudAnchor);
            }
        }
        
        AnchorLocatedCallback(args.watcher.identifier, static_cast<int>(args.status), static_cast<int>(CloudAnchorID));
    }
}

void FAzureSpatialAnchorsARKitInterop::OnLocateAnchorsCompleted(ASACloudSpatialAnchorSession* session, ASALocateAnchorsCompletedEventArgs* args)
{
    if (!Session ||
        Session != session)
    {
        return;
    }
    
    int WatcherIdentifier = args.watcher.identifier;
    bool Cancelled = args.cancelled;
    LocateAnchorsCompletedCallback(WatcherIdentifier, Cancelled);
}

void FAzureSpatialAnchorsARKitInterop::OnSessionUpdated(ASACloudSpatialAnchorSession* session, ASASessionUpdatedEventArgs* args)
{
    if (!Session ||
        Session != session)
    {
        return;
    }
    
    float ReadyForCreateProgress = args.status.readyForCreateProgress;
    float RecommendedForCreateProgress = args.status.recommendedForCreateProgress;
    int SessionCreateHash = args.status.sessionCreateHash;
    int SessionLocateHash = args.status.sessionLocateHash;
    int UserFeedback = static_cast<int>(args.status.userFeedback);
    SessionUpdatedCallback(ReadyForCreateProgress, RecommendedForCreateProgress, SessionCreateHash, SessionLocateHash, UserFeedback);
}

void FAzureSpatialAnchorsARKitInterop::OnError(ASACloudSpatialAnchorSession* session, ASASessionErrorEventArgs* args)
{
    if (!Session ||
        Session != session)
    {
        return;
    }
    
    int ErrorCode = static_cast<int>(args.errorCode);
    const FString ErrorMessage = NSStringToFString(args.errorMessage);
    UE_LOG(LogAzureSpatialAnchorsARKitInterop, Error, TEXT("Azure Spatial Anchors encountered an error, ErrorCode: %d, ErrorMessage: %s"), ErrorCode, *ErrorMessage);
}

void FAzureSpatialAnchorsARKitInterop::OnLogDebugEvent(ASACloudSpatialAnchorSession* session, ASAOnLogDebugEventArgs* args)
{
    const FString Message = NSStringToFString(args.message);
    UE_LOG(LogAzureSpatialAnchorsARKitInterop, Log, TEXT("Azure Spatial Anchors: %s"), *Message);
}

NSString* FAzureSpatialAnchorsARKitInterop::FStringToNSString(const FString& fstring)
{
    return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*fstring)];
}

FString FAzureSpatialAnchorsARKitInterop::NSStringToFString(const NSString* nsstring)
{
    if (nsstring != nullptr &&
        [nsstring length] > 0)
    {
        return UTF8_TO_TCHAR(std::string(nsstring.UTF8String).c_str());
    }
    
    return FString{};
}

