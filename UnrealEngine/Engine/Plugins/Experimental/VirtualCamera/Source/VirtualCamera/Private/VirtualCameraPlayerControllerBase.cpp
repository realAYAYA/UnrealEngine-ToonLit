// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraPlayerControllerBase.h"

#include "ConcertVirtualCamera.h"
#include "CineCameraActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "ILiveLinkClient.h"
#include "IXRTrackingSystem.h"
#include "RemoteSession.h"
#include "Channels/RemoteSessionInputChannel.h"
#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "Channels/RemoteSessionXRTrackingChannel.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "VirtualCamera.h"
#include "VPGameMode.h"
#include "VPRootActor.h"

#if WITH_EDITOR
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#endif


AVirtualCameraPlayerControllerBase::AVirtualCameraPlayerControllerBase(const FObjectInitializer& ObjectInitializer)
{
	// Default tracker input source
	InputSource = ETrackerInputSource::ARKit;

	TargetCameraActorClass = ACineCameraActor::StaticClass();
	TargetCameraActor = nullptr;

	LiveLinkClient = nullptr;
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	}

	// Default autofocus to center of screen
	AutoFocusScreenPosition = FVector2D(.5f, .5f);

	// Default touch input values
	TouchInputState = ETouchInputState::BlueprintDefined;
	PreviousTouchInput = TouchInputState;
	CurrentFocusMethod = EVirtualCameraFocusMethod::Manual;

	// Cached values
	bCachedShouldUpdateTargetCameraTransform = true;
	bCachedIsVirtualCameraControlledByRemoteSession = true;
}

void AVirtualCameraPlayerControllerBase::OnPossess(APawn* InPawn)
{
	bCachedShouldUpdateTargetCameraTransform = ShouldUpdateTargetCameraTransform();
	bCachedIsVirtualCameraControlledByRemoteSession = IsVirtualCameraControlledByRemoteSession();

	Super::OnPossess(InPawn);
}

void AVirtualCameraPlayerControllerBase::BeginPlay()
{
	// Find the Root Actor
	RootActor = nullptr;
	AVPGameMode* VPGameMode = Cast<AVPGameMode>(GetWorld()->GetAuthGameMode());
	if (VPGameMode)
	{
		RootActor = VPGameMode->GetRootActor();
	}
	else
	{
		UE_LOG(LogVirtualCamera, Warning, TEXT("The Game Mode is not a VPGameMode."));
	}

	if (RootActor == nullptr)
	{
		UE_LOG(LogVirtualCamera, Warning, TEXT("There is no VP Root Actor in the scene. A CineCameraActor will be spawned. Multi user functionalities may suffer."));
	}

	// Make a default sequence playback controller
	LevelSequencePlaybackController = NewObject<ULevelSequencePlaybackController>(this);
	if (LevelSequencePlaybackController)
	{
		// Get the Cine Camera actor used by that root actor
		if (RootActor)
		{
			TargetCameraActor = RootActor->GetCineCameraActor();
			if (TargetCameraActor == nullptr)
			{
				UE_LOG(LogVirtualCamera, Warning, TEXT("The Root Actor doesn't have a cinematic camera."));
			}
		}

		if (TargetCameraActor == nullptr)
		{
			// Spawn the target camera that the level sequence will use as a target
			TargetCameraActor = GetWorld()->SpawnActor<ACineCameraActor>(TargetCameraActorClass);
		}

		// bLockToHmd is set to true by default. Remove it to prevent unwanted movement from XR system.
		TargetCameraActor->GetCameraComponent()->bLockToHmd = false;

		// Bind to record enabled state change delegate
		LevelSequencePlaybackController->OnRecordEnabledStateChanged.BindUObject(this, &AVirtualCameraPlayerControllerBase::HandleRecordEnabledStateChange);
	}

	// Is this Controller a RemoteSession controller
	if (bCachedIsVirtualCameraControlledByRemoteSession)
	{
		if (IRemoteSessionModule* RemoteSession = FModuleManager::LoadModulePtr<IRemoteSessionModule>("RemoteSession"))
		{
			//RemoteSession->AddSupportedChannel(FRemoteSessionImageChannel::StaticType(), ERemoteSessionChannelMode::Write);
			//RemoteSession->AddSupportedChannel(FRemoteSessionInputChannel::StaticType(), ERemoteSessionChannelMode::Read);
			//RemoteSession->AddSupportedChannel(FRemoteSessionXRTrackingChannel::StaticType(), ERemoteSessionChannelMode::Read);

			RemoteSession->InitHost();
		}
	}
	else
	{
		SetViewTargetWithBlend(TargetCameraActor);
	}

	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		// Need to make sure we don't let ARKit control camera completely
		CineCamera->bLockToHmd = false;
	}

	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->OnOffsetReset.AddDynamic(this, &AVirtualCameraPlayerControllerBase::BroadcastOffsetReset);
	}

	// Initialize the view of the camera with offsets taken into account
	UpdatePawnWithTrackerData();
	ResetCameraOffsetsToTracker();

	Super::BeginPlay();
}

void AVirtualCameraPlayerControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn();
	if (VCPawn)
	{
		UpdatePawnWithTrackerData();

		if (VCPawn->IsAutoFocusEnabled())
		{
			// Auto focus is just setting screen focus through the Auto Focus position.
			SetFocusDistanceThroughPoint(AutoFocusScreenPosition);
		}
	}

	UVirtualCameraCineCameraComponent* VCCamera = GetVirtualCameraCineCameraComponent();
	if (VCCamera)
	{
		if (VCCamera->GetCurrentFocusMethod() == EVirtualCameraFocusMethod::Auto)
		{
			SetFocusDistanceThroughPoint(AutoFocusScreenPosition);
		}

		VCCamera->UpdateCameraView();
	}

	if (LevelSequencePlaybackController)
	{
		PilotTargetedCamera(VCPawn, VCCamera);
	}
}

void AVirtualCameraPlayerControllerBase::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindTouch(IE_Released, this, &AVirtualCameraPlayerControllerBase::OnTouchInput);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftY, this, &AVirtualCameraPlayerControllerBase::OnMoveForward);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &AVirtualCameraPlayerControllerBase::OnMoveRight);
	InputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &AVirtualCameraPlayerControllerBase::OnMoveUp);
}

void AVirtualCameraPlayerControllerBase::InitializeAutoFocusPoint()
{
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(AutoFocusScreenPosition);
		AutoFocusScreenPosition.X *= 0.5f;
		AutoFocusScreenPosition.Y *= 0.5f;
	}
	UpdateFocusReticle(FVector(AutoFocusScreenPosition, 0.f));
}

ACineCameraActor* AVirtualCameraPlayerControllerBase::GetTargetCamera()
{
	return TargetCameraActor;
}

void AVirtualCameraPlayerControllerBase::SetInputSource(ETrackerInputSource InInputSource)
{
	if (InputSource != InInputSource)
	{
		InputSource = InInputSource;

		bCachedShouldUpdateTargetCameraTransform = ShouldUpdateTargetCameraTransform();
	}
}

FString AVirtualCameraPlayerControllerBase::GetDistanceInDesiredUnits(const float InputDistance, const EUnit ConversionUnit) const
{
	// Checks if the specified conversion unit is a unit of distance, since this function assumes conversion from Unreal Units
	if (!FUnitConversion::IsUnitOfType(ConversionUnit, EUnitType::Distance))
	{
		UE_LOG(LogVirtualCamera, Warning, TEXT("Conversion unit selected is not a unit of distance."))
		return FString();
	}

	// ToDo: Add support for different project settings if default is changed
	float ConvertedDistance = FUnitConversion::Convert<float>(InputDistance, EUnit::Centimeters, ConversionUnit);

	FString ReturnString;

	if (ConversionUnit == EUnit::Feet)
	{
		int32 Feet = FMath::FloorToInt(ConvertedDistance);
		int32 Inches = FMath::RoundToInt(FMath::Frac(ConvertedDistance) * 12.f);

		// Handle when inches rounds to 12
		if (Inches == 12)
		{
			// Feet should increase by one instead of having 12 inches displayed
			Feet++;
			Inches = 0;
		}

		// Use ' and " for feet an inches unit labels
		ReturnString = FString::FromInt(Feet);
		ReturnString += "'";
		
		// Only display inches if there is enough room
		if (Feet < 10000)
		{
			ReturnString += FString::FromInt(Inches);
			ReturnString += "\"";
		}
		
	}
	else
	{
		// Meters
		FNumberFormattingOptions NumberFormat;
		// Only show one degree of precision for decimal values
		NumberFormat.MinimumFractionalDigits = 0;
		NumberFormat.MaximumFractionalDigits = 1;
		ReturnString = FText::AsNumber(ConvertedDistance, &NumberFormat).ToString();
		if (ConvertedDistance >= 1000) 
		{
			// Don't show decimal place if not enough room
			ReturnString = FString::FromInt(FMath::RoundToInt(ConvertedDistance));
		}
		ReturnString += FUnitConversion::GetUnitDisplayString(ConversionUnit);
	}

	return ReturnString;
}

static const FName RemoteSessionTrackingSystemName(TEXT("RemoteSessionXRTrackingProxy"));

bool AVirtualCameraPlayerControllerBase::GetCurrentTrackerLocationAndRotation(FVector& OutTrackerLocation, FRotator& OutTrackerRotation)
{
	bool bTransformSet = false;

	switch (InputSource)
	{
		case ETrackerInputSource::ARKit:
			if (GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetSystemName() == RemoteSessionTrackingSystemName)
			{
				FQuat ARKitQuaternion;
				GEngine->XRSystem->GetCurrentPose(0, ARKitQuaternion, OutTrackerLocation);
				OutTrackerRotation = ARKitQuaternion.Rotator();
				bTransformSet = true;
			}
			break;

		case ETrackerInputSource::LiveLink:
			if (LiveLinkClient)
			{
				FLiveLinkSubjectFrameData EvaluateData;
				if (LiveLinkClient->EvaluateFrame_AnyThread(LiveLinkTargetName, ULiveLinkTransformRole::StaticClass(), EvaluateData))
				{
					FLiveLinkTransformFrameData* TransformFrameData = EvaluateData.FrameData.Cast<FLiveLinkTransformFrameData>();
					check(TransformFrameData);

					OutTrackerLocation = TransformFrameData->Transform.GetLocation();
					OutTrackerRotation = TransformFrameData->Transform.GetRotation().Rotator();

					bTransformSet = true;
				}
				else if (LiveLinkClient->EvaluateFrame_AnyThread(LiveLinkTargetName, ULiveLinkAnimationRole::StaticClass(), EvaluateData))
				{
					FLiveLinkAnimationFrameData* AnimationFrameData = EvaluateData.FrameData.Cast<FLiveLinkAnimationFrameData>();
					FLiveLinkSkeletonStaticData* AnimationStaticData = EvaluateData.StaticData.Cast<FLiveLinkSkeletonStaticData>();
					check(AnimationFrameData && AnimationStaticData);

					int32 RootIndex = AnimationStaticData->FindRootBone();
					if (AnimationFrameData->Transforms.IsValidIndex(RootIndex))
					{
						
						OutTrackerLocation = AnimationFrameData->Transforms[RootIndex].GetLocation();
						OutTrackerRotation = AnimationFrameData->Transforms[RootIndex].GetRotation().Rotator();

						bTransformSet = true;
					}
				}
			}
			break;

		case ETrackerInputSource::Custom:
			GetCustomTrackerLocationAndRotation(OutTrackerLocation, OutTrackerRotation);
			bTransformSet = true;
			break;

		default:
			UE_LOG(LogVirtualCamera, Warning, TEXT("Selected tracker source is not yet supported"))
			break;
	}

	bTransformSet = bTransformSet && !OutTrackerLocation.ContainsNaN() && !OutTrackerRotation.ContainsNaN();

	// Return failure status if we couldn't find device to track or device isn't supported or the values contains NaN
	return bTransformSet;
}

bool AVirtualCameraPlayerControllerBase::IsTouchInputInFocusMode()
{
	return TouchInputState == ETouchInputState::ActorFocusTargeting || TouchInputState == ETouchInputState::AutoFocusTargeting || TouchInputState == ETouchInputState::ManualTouchFocus;
}

void AVirtualCameraPlayerControllerBase::SetFocusDistanceToActor(const ETouchIndex::Type TouchIndex, const FVector& Location)
{
	AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn();

	// Don't try to execute if not currently possessing a virtual Camera pawn
	if (!VCPawn)
	{
		return;
	}

	FVector TraceDirection;
	FVector CameraWorldLocation;

	if (!DeprojectScreenPositionToWorld(Location.X, Location.Y, CameraWorldLocation, TraceDirection))
	{
		// If projection fails, return now
		return;
	}

	float FocusTraceDist = 1000000.f;

	// Trace to get depth under auto focus position
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(UpdateAutoFocus), true);
	FHitResult Hit;

	FVector const TraceEnd = CameraWorldLocation + TraceDirection * FocusTraceDist;
	bool const bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraWorldLocation, TraceEnd, ECC_Visibility, TraceParams);
	
	if (bHit)
	{
		VCPawn->SetFocusDistance(Hit.Distance);

		// Set this actor as the tracked actor, and move the exact focus tracked point to where the touch occurred
		if (AActor* HitActor = Hit.GetHitObjectHandle().FetchActor())
		{
			FVector TrackingPointOffset = HitActor->GetActorRotation().UnrotateVector(Hit.ImpactPoint - HitActor->GetActorLocation());
			TrackingPointOffset /= HitActor->GetActorScale(); // Adjust for non-standard scales when we rotate the vector

			VCPawn->SetTrackedActorForFocus(HitActor, TrackingPointOffset);

#if WITH_EDITOR
			bool bIsRecording = UTakeRecorderBlueprintLibrary::IsRecording();
#else
			bool bIsRecording = false;
#endif
			if (!bIsRecording)
			{	
				VCPawn->TriggerFocusPlaneTimer();
				VCPawn->HighlightTappedActor(HitActor);
			}
		}
	}
}

AVirtualCameraPawnBase* AVirtualCameraPlayerControllerBase::GetVirtualCameraPawn() const 
{
	return Cast<AVirtualCameraPawnBase>(GetPawn());
}

void AVirtualCameraPlayerControllerBase::HandleRecordEnabledStateChange(const bool bIsRecordEnabled)
{
	OnRecordEnabledStateChanged(bIsRecordEnabled);
}

void AVirtualCameraPlayerControllerBase::SetFocusDistanceThroughPoint(const FVector2D ScreenPosition)
{
	AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn();

	FVector TraceDirection;
	FVector CameraWorldLocation;

	if (!DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, CameraWorldLocation, TraceDirection))
	{
		// If projection fails, return now
		return;
	}

	float FocusTraceDist = 1000000.f;

	// Trace to get depth under auto focus position
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(UpdateAutoFocus), true);
	FHitResult Hit;

	FVector const TraceEnd = CameraWorldLocation + TraceDirection * FocusTraceDist;
	bool const bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraWorldLocation, TraceEnd, ECC_Visibility, TraceParams);

	if (bHit && Hit.GetHitObjectHandle().IsValid())
	{
		VCPawn->SetFocusDistance(Hit.Distance);
	}	
}

UVirtualCameraCineCameraComponent* AVirtualCameraPlayerControllerBase::GetVirtualCameraCineCameraComponent() const 
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->CineCamera;
	}

	return nullptr;
}

UVirtualCameraMovementComponent* AVirtualCameraPlayerControllerBase::GetVirtualCameraMovementComponent() const 
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->MovementComponent;
	}

	return nullptr;
}

void AVirtualCameraPlayerControllerBase::PilotTargetedCamera(AVirtualCameraPawnBase* PawnToFollow, UVirtualCameraCineCameraComponent* CameraToFollow)
{
	if (TargetCameraActor == nullptr || PawnToFollow == nullptr || CameraToFollow == nullptr)
	{
		return;
	}

	UCineCameraComponent* TargetCameraComponent = TargetCameraActor->GetCineCameraComponent();
	if (TargetCameraComponent == nullptr)
	{
		return;
	}

	bool bAssignValues = false;
	FConcertVirtualCameraControllerEvent CameraEvent;
	if (bCachedIsVirtualCameraControlledByRemoteSession)
	{
		CameraEvent.InputSource = InputSource;
		CameraEvent.CameraData = FConcertVirtualCameraCameraData(PawnToFollow, CameraToFollow);

		IVirtualCameraModule::Get().GetConcertVirtualCameraManager()->SendControllerCameraEvent(CameraEvent);
		bAssignValues = true;
	}
	else
	{
		bAssignValues = IVirtualCameraModule::Get().GetConcertVirtualCameraManager()->GetLatestControllerCameraEvent(CameraEvent);
		if (bAssignValues)
		{
			CameraEvent.CameraData.ApplyTo(PawnToFollow, CameraToFollow, bCachedShouldUpdateTargetCameraTransform);

			if (CameraEvent.InputSource != InputSource)
			{
				SetInputSource(CameraEvent.InputSource);
			}
		}
	}

	// Copy the info to the camera target camera
	if (bAssignValues)
	{
		TargetCameraActor->SetActorLocationAndRotation(CameraEvent.CameraData.CameraActorLocation, CameraEvent.CameraData.CameraActorRotation);
		TargetCameraComponent->CurrentAperture = CameraToFollow->CurrentAperture;
		TargetCameraComponent->CurrentFocalLength = CameraToFollow->CurrentFocalLength;
		TargetCameraComponent->FocusSettings = CameraToFollow->FocusSettings;
		TargetCameraComponent->LensSettings = CameraToFollow->LensSettings;
		TargetCameraComponent->Filmback = CameraToFollow->Filmback;
	}
	TargetCameraComponent->SetRelativeLocationAndRotation(CameraToFollow->GetRelativeLocation(), CameraToFollow->GetRelativeRotation());
}

bool AVirtualCameraPlayerControllerBase::InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D & TouchLocation, float Force, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	// Don't allow for input touch events if not within matte
	if (!IsLocationWithinMatte(FVector(TouchLocation, 0.0f)))
	{
		return Super::InputTouch(Handle, Type, TouchLocation, Force, DeviceTimestamp, TouchpadIndex);;
	}

	// Allow touch and drag of the auto focus reticle when in auto focus targeting mode
	if (Type == ETouchType::Moved && TouchInputState == ETouchInputState::AutoFocusTargeting)
	{
		UpdateScreenFocus(ETouchIndex::Touch1, FVector(TouchLocation, 0.f));
	}

	return Super::InputTouch(Handle, Type, TouchLocation, Force, DeviceTimestamp, TouchpadIndex);
}

void AVirtualCameraPlayerControllerBase::OnTouchInput(const ETouchIndex::Type TouchIndex, const FVector Location)
{
	if (!IsLocationWithinMatte(Location))
	{
		return;
	}

	switch (TouchInputState)
	{
		// Mode for attaching focus to an actor.
		case ETouchInputState::ActorFocusTargeting:
			SetFocusDistanceToActor(TouchIndex, Location);
			UpdateScreenFocus(TouchIndex, Location);
			break;

		// Mode for changing the point on the screen used for auto focus targeting.
		case ETouchInputState::AutoFocusTargeting:
			UpdateScreenFocus(TouchIndex, Location);
			ShowFocusPlaneFromTouch();
			break;

		case ETouchInputState::ManualTouchFocus:
			SetFocusDistanceThroughPoint(FVector2D(Location.X, Location.Y));
			UpdateScreenFocus(TouchIndex, Location);
			ShowFocusPlaneFromTouch();
			break;

		// Allows for user defined behavior in blueprint
		case ETouchInputState::BlueprintDefined:
			break;

		default:
			break;
	}
	
}

void AVirtualCameraPlayerControllerBase::UpdateScreenFocus(const ETouchIndex::Type TouchIndex, const FVector Location)
{
	// Set New Screen Location for auto focus to trace through
	AutoFocusScreenPosition.X = Location.X;
	AutoFocusScreenPosition.Y = Location.Y;
	UpdateFocusReticle(Location);
}

void AVirtualCameraPlayerControllerBase::OnMoveForward(const float InValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->OnMoveForward(InValue);
	}
}

void AVirtualCameraPlayerControllerBase::OnMoveRight(const float InValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->OnMoveRight(InValue);
	}
}

void AVirtualCameraPlayerControllerBase::OnMoveUp(const float InValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		// ToDo: Figure out why this needs to be negated; maybe inverted look?
		MovementComponent->OnMoveUp(-InValue);
	}
}

void AVirtualCameraPlayerControllerBase::ShowFocusPlaneFromTouch()
{
	AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn();

	if (!VCPawn)
	{
		return;
	}

#if WITH_EDITOR
	bool bIsRecording = UTakeRecorderBlueprintLibrary::IsRecording();
#else
	bool bIsRecording = false;
#endif
	if (!bIsRecording)
	{
		VCPawn->TriggerFocusPlaneTimer();
	}
}

void AVirtualCameraPlayerControllerBase::UpdatePawnWithTrackerData()
{
	// Initialize the virtual camera view
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		FVector TrackerLocation = FVector::ZeroVector;
		FRotator TrackerRotation = FRotator::ZeroRotator;
		if (GetCurrentTrackerLocationAndRotation(TrackerLocation, TrackerRotation))
		{
			// Apply tracker offset to tracker; convert everything to transforms to make sure motions are calculated in the right order
			FTransform TrackerRaw = FTransform(TrackerRotation, TrackerLocation);
			FTransform AdjustedTracker = TrackerPostOffset.AsTransform() * TrackerRaw * TrackerPreOffset.AsTransform();
			TrackerRotation = AdjustedTracker.Rotator();
			TrackerLocation = AdjustedTracker.GetLocation();

			VCPawn->ProcessMovementInput(TrackerLocation, TrackerRotation);
		}
	}
}

bool AVirtualCameraPlayerControllerBase::IsLocationWithinMatte(const FVector Location) const 
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		FVector2D ViewportSize; 
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		FVector2D MatteResolution;
		FVector2D LowerBound;
		FVector2D UpperBound;
		
		// Calculate the matte resolution based on viewport and ratio
		MatteResolution.X = ViewportSize.X * CineCamera->ViewSizeRatio.X;
		MatteResolution.Y = ViewportSize.Y * CineCamera->ViewSizeRatio.Y;

		// Lower bound is half the difference between the two resolutions
		LowerBound.X = (ViewportSize.X - MatteResolution.X) / 2;
		LowerBound.Y = (ViewportSize.Y - MatteResolution.Y) / 2;

		// Upper bound is the viewport size minus the lower bound
		UpperBound.X = ViewportSize.X - LowerBound.X;
		UpperBound.Y = ViewportSize.Y - LowerBound.Y;

		// The touch location needs to be within the bounds
		if (Location.X >= LowerBound.X && Location.X <= UpperBound.X && Location.Y >= LowerBound.Y && Location.Y <= UpperBound.Y)
		{
			return true;
		}

	}
	
	return false;
}

bool AVirtualCameraPlayerControllerBase::IsVirtualCameraControlledByRemoteSession_Implementation() const
{
	return true;
}

bool AVirtualCameraPlayerControllerBase::ShouldUpdateTargetCameraTransform_Implementation() const
{
	return InputSource == ETrackerInputSource::ARKit || IsVirtualCameraControlledByRemoteSession();
}

/***** UI Interface Functions *****/

void AVirtualCameraPlayerControllerBase::AddBlendableToCamera(TScriptInterface<IBlendableInterface> BlendableToAdd, float Weight)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->AddBlendableToCamera(BlendableToAdd, Weight);
	}
}

float AVirtualCameraPlayerControllerBase::ChangeAperturePreset(const bool bShiftUp)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->ChangeAperturePreset(bShiftUp);
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::ChangeFocalLengthPreset(const bool bShiftUp)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->ChangeFocalLengthPreset(bShiftUp);
	}

	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::ClearActiveLevelSequence()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->ClearActiveLevelSequence();
	}
}

int32 AVirtualCameraPlayerControllerBase::DeletePreset(FString PresetName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->DeletePreset(PresetName);
	}
	return -1;
}

int32 AVirtualCameraPlayerControllerBase::DeleteScreenshot(FString ScreenshotName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->DeleteScreenshot(ScreenshotName);
	}
	return -1;
}

int32 AVirtualCameraPlayerControllerBase::DeleteWaypoint(FString WaypointName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->DeleteWaypoint(WaypointName);
	}
	return -1;
}

FString AVirtualCameraPlayerControllerBase::GetActiveLevelSequenceName()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetActiveLevelSequenceName();
	}

	return FString();
}

ULevelSequence* AVirtualCameraPlayerControllerBase::GetActiveLevelSequence()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetActiveLevelSequence();
	}

	return nullptr;
}

float AVirtualCameraPlayerControllerBase::GetAxisStabilizationScale(EVirtualCameraAxis AxisToRetrieve)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->GetAxisStabilizationScale(AxisToRetrieve);
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetAxisMovementScale(EVirtualCameraAxis AxisToRetrieve)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->GetAxisMovementScale(AxisToRetrieve);
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentAperture()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentAperture();
	}

	return 0.0f;
}

FString AVirtualCameraPlayerControllerBase::GetCurrentFilmbackName()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentFilmbackName();
	}

	return FString();
}

float AVirtualCameraPlayerControllerBase::GetCurrentFocalLength()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentFocalLength();
	}

	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetCurrentFocusDistance()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetCurrentFocusDistance();
	}

	return 0.0f;
}

FFrameNumber AVirtualCameraPlayerControllerBase::GetCurrentSequencePlaybackEnd()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentSequencePlaybackEnd();
	}

	return FFrameNumber();
}

FFrameNumber AVirtualCameraPlayerControllerBase::GetCurrentSequencePlaybackStart()
{

	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentSequencePlaybackStart();
	}

	return FFrameNumber();
}

bool AVirtualCameraPlayerControllerBase::IsSequencerLockedToCameraCut()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->IsSequencerLockedToCameraCut();
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::SetSequencerLockedToCameraCut(bool bLockView)
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->SetSequencerLockedToCameraCut(bLockView);
	}
}

FFrameRate AVirtualCameraPlayerControllerBase::GetCurrentSequenceFrameRate()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentSequenceFrameRate();
	}

	return FFrameRate();
}

EUnit AVirtualCameraPlayerControllerBase::GetDesiredDistanceUnits()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->GetDesiredDistanceUnits();
	}
	return EUnit();
}



FColor AVirtualCameraPlayerControllerBase::GetFocusPlaneColor()
{
#if WITH_EDITOR
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->FocusSettings.DebugFocusPlaneColor;
	}
#endif
	return FColor();
}

bool AVirtualCameraPlayerControllerBase::GetFilmbackPresetOptions(TArray<FString>& OutFilmbackPresetsArray)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetFilmbackPresetOptions(OutFilmbackPresetsArray);
	}

	return false;
}

FFrameNumber AVirtualCameraPlayerControllerBase::GetLevelSequenceLength()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentSequenceDuration();
	}

	return FFrameNumber();
}

void AVirtualCameraPlayerControllerBase::GetLevelSequences(TArray<FLevelSequenceData>& OutLevelSequenceNames)
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->GetLevelSequences(OutLevelSequenceNames);
	}
}

float AVirtualCameraPlayerControllerBase::GetMatteAspectRatio()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->GetMatteAspectRatio();
	}
	return 0.0f;
}

float AVirtualCameraPlayerControllerBase::GetMatteOpacity()
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->MatteOpacity;
	}
	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::GetMatteValues(TArray<float>& OutMatteValues)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->GetMatteValues(OutMatteValues);
		OutMatteValues.Sort();
	}
}

FFrameTime AVirtualCameraPlayerControllerBase::GetPlaybackPosition()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentSequencePlaybackPosition();
	}

	return FFrameTime();
}

FTimecode AVirtualCameraPlayerControllerBase::GetPlaybackTimecode()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->GetCurrentSequencePlaybackTimecode();
	}

	return FTimecode();
}

void AVirtualCameraPlayerControllerBase::GetScreenshotInfo(FString ScreenshotName, FVirtualCameraScreenshot &OutScreenshotInfo)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetScreenshotInfo(ScreenshotName, OutScreenshotInfo);
	}
}

void AVirtualCameraPlayerControllerBase::GetScreenshotNames(TArray<FString>& OutArray)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetScreenshotNames(OutArray);
		// Sort OutArray reversed by name
		OutArray.Sort([](const FString A, const FString B) {
			return A > B;
		});
	}
}

TMap<FString, FVirtualCameraSettingsPreset> AVirtualCameraPlayerControllerBase::GetSettingsPresets()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->GetSettingsPresets();
	}

	return TMap<FString, FVirtualCameraSettingsPreset>();
}

void AVirtualCameraPlayerControllerBase::GetWaypointInfo(FString WaypointName, FVirtualCameraWaypoint &OutWaypointInfo)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetWaypointInfo(WaypointName, OutWaypointInfo);
	}
}

void AVirtualCameraPlayerControllerBase::GetWaypointNames(TArray<FString>& OutArray)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->GetWaypointNames(OutArray);
		// Sort OutArray reversed by name
		OutArray.Sort([] (const FString A, const FString B) {
			return A > B;
		});
	}
}

bool AVirtualCameraPlayerControllerBase::IsAxisLocked(EVirtualCameraAxis AxisToCheck)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->IsAxisLocked(AxisToCheck);
	}
	return false;
}

bool AVirtualCameraPlayerControllerBase::IsFocusVisualizationAllowed()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->IsFocusVisualizationAllowed();
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::IsPlaying()
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->IsSequencePlaybackActive();
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::IsUsingGlobalBoom()
{
	if (UVirtualCameraMovementComponent* VCMovementComponent = GetVirtualCameraMovementComponent())
	{
		return VCMovementComponent->IsUsingGlobalBoom();
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::JumpToLevelSequenceEnd()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->JumpToPlaybackPosition(LevelSequencePlaybackController->GetCurrentSequencePlaybackEnd());
		LevelSequencePlaybackController->PauseLevelSequence();
	}
}

void AVirtualCameraPlayerControllerBase::JumpToLevelSequenceStart()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->JumpToPlaybackPosition(LevelSequencePlaybackController->GetCurrentSequencePlaybackStart());
		LevelSequencePlaybackController->PauseLevelSequence();
	}
}

void AVirtualCameraPlayerControllerBase::JumpToPlaybackPosition(const FFrameNumber& InFrameNumber)
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->JumpToPlaybackPosition(InFrameNumber);
	}
}

bool AVirtualCameraPlayerControllerBase::LoadPreset(FString PresetName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->LoadPreset(PresetName);
	}
	return false;
}

bool AVirtualCameraPlayerControllerBase::LoadScreenshotView(const FString& ScreenshotName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->LoadScreenshotView(ScreenshotName);
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::PauseLevelSequence()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->PauseLevelSequence();
	}
}

void AVirtualCameraPlayerControllerBase::PlayLevelSequence()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->PlayLevelSequence();
	}
}

void AVirtualCameraPlayerControllerBase::PlayLevelSequenceInReverse()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->PlayLevelSequenceReverse();
	}
}

void AVirtualCameraPlayerControllerBase::ResetCameraOffsetsToTracker()
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->ResetCameraOffsetsToTracker();
	}
}

void AVirtualCameraPlayerControllerBase::ResumeLevelSequencePlay()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->PlayLevelSequence();
	}
}

void AVirtualCameraPlayerControllerBase::SaveHomeWaypoint(const FString& NewHomeWaypointName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SaveHomeWaypoint(NewHomeWaypointName);
	}
}

FString AVirtualCameraPlayerControllerBase::SavePreset(bool bSaveCameraSettings, bool bSaveStabilization, bool bSaveAxisLocking, bool bSaveMotionScale)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->SavePreset(bSaveCameraSettings, bSaveStabilization, bSaveAxisLocking, bSaveMotionScale);
	}
	return FString();
}

FString AVirtualCameraPlayerControllerBase::SaveWaypoint()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->SaveWaypoint();
	}

	return FString();
}

bool AVirtualCameraPlayerControllerBase::SetActiveLevelSequence(ULevelSequence* InNewLevelSequence)
{
	if (LevelSequencePlaybackController)
	{
		return LevelSequencePlaybackController->SetActiveLevelSequence(InNewLevelSequence);
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::SetAllowFocusPlaneVisualization(bool bShouldAllowFocusVisualization)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetAllowFocusPlaneVisualization(bShouldAllowFocusVisualization);
	}
}

float AVirtualCameraPlayerControllerBase::SetAxisStabilizationScale(EVirtualCameraAxis AxisToAdjust, float NewStabilizationAmount)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->SetAxisStabilizationScale(AxisToAdjust, NewStabilizationAmount);
	}

	return 0.0f;
}

void AVirtualCameraPlayerControllerBase::SetCurrentAperture(float NewAperture)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->SetCurrentAperture(NewAperture);
	}
}

void AVirtualCameraPlayerControllerBase::SetCurrentFocalLength(const float NewFocalLength)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->SetCurrentFocalLength(NewFocalLength);
	}
}

void AVirtualCameraPlayerControllerBase::SetCurrentFocusDistance(const float NewFocusDistance)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->SetCurrentFocusDistance(NewFocusDistance);
	}
}

void AVirtualCameraPlayerControllerBase::SetDesiredDistanceUnits(const EUnit DesiredUnits)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetDesiredDistanceUnits(DesiredUnits);
	}
}

bool AVirtualCameraPlayerControllerBase::SetFilmbackPresetOption(const FString & NewFilmbackPreset)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->SetFilmbackPresetOption(NewFilmbackPreset);
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::SetFocusMethod(const EVirtualCameraFocusMethod NewFocusMethod)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CurrentFocusMethod = NewFocusMethod;
		FocusMethodChanged(NewFocusMethod);
		return CineCamera->SetFocusMethod(NewFocusMethod);
	}
}

void AVirtualCameraPlayerControllerBase::SetFocusPlaneColor(const FColor NewFocusPlaneColor)
{
#if WITH_EDITOR
	if (GetVirtualCameraCineCameraComponent())
	{
		GetVirtualCameraCineCameraComponent()->FocusSettings.DebugFocusPlaneColor = NewFocusPlaneColor;
	}
#endif
}

void AVirtualCameraPlayerControllerBase::SetFocusVisualization(bool bShowFocusVisualization)
{
	if (GetVirtualCameraCineCameraComponent())
	{
		GetVirtualCameraCineCameraComponent()->SetFocusVisualization(bShowFocusVisualization);
	}
}

bool AVirtualCameraPlayerControllerBase::IsFocusVisualizationActivated() const
{
	if (GetVirtualCameraCineCameraComponent())
	{
		return GetVirtualCameraCineCameraComponent()->IsFocusVisualizationActivated();
	}
	return false;
}

bool AVirtualCameraPlayerControllerBase::SetMatteAspectRatio(const float NewMatteAspectRatio)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		return CineCamera->SetMatteAspectRatio(NewMatteAspectRatio);
	}
	return false;
}

void AVirtualCameraPlayerControllerBase::SetMatteOpacity(const float NewMatteOpacity)
{
	if (UVirtualCameraCineCameraComponent* CineCamera = GetVirtualCameraCineCameraComponent())
	{
		CineCamera->MatteOpacity = NewMatteOpacity;
	}
}

void AVirtualCameraPlayerControllerBase::SetMovementScale(const EVirtualCameraAxis AxisToAdjust, const float NewMovementScale)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->SetMovementScale(AxisToAdjust, NewMovementScale);
	}
}

void AVirtualCameraPlayerControllerBase::SetPresetFavoriteStatus(const FString& PresetName, const bool bIsFavorite)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetPresetFavoriteStatus(PresetName, bIsFavorite);
	}
}

void AVirtualCameraPlayerControllerBase::SetSaveSettingsWhenClosing(bool bShouldSettingsSave)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetSaveSettingsWhenClosing(bShouldSettingsSave);
	}
}

void AVirtualCameraPlayerControllerBase::SetScreenshotFavoriteStatus(const FString& ScreenshotName, const bool bIsFavorite)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetScreenshotFavoriteStatus(ScreenshotName, bIsFavorite);
	}
}

void AVirtualCameraPlayerControllerBase::SetUseGlobalBoom(bool bShouldUseGlobalBoom)
{
	if (UVirtualCameraMovementComponent* VCMovementComponent = GetVirtualCameraMovementComponent())
	{
		VCMovementComponent->SetUseGlobalBoom(bShouldUseGlobalBoom);
	}
}

void AVirtualCameraPlayerControllerBase::SetWaypointFavoriteStatus(const FString& WaypointName, const bool bIsFavorite)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		VCPawn->SetWaypointFavoriteStatus(WaypointName, bIsFavorite);
	}
}

void AVirtualCameraPlayerControllerBase::SetZeroDutchOnLock(const bool bInValue)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		MovementComponent->SetZeroDutchOnLock(bInValue);
	}
}

bool AVirtualCameraPlayerControllerBase::ShouldSaveSettingsWhenClosing()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->GetSaveSettingsWhenClosing();
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::StopLevelSequencePlay()
{
	if (LevelSequencePlaybackController)
	{
		LevelSequencePlaybackController->StopLevelSequencePlay();
	}
}

FString AVirtualCameraPlayerControllerBase::TakeScreenshot()
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->TakeScreenshot();
	}

	return FString();
}

void AVirtualCameraPlayerControllerBase::TeleportToHomeWaypoint()
{
	if (GetVirtualCameraPawn() && GetVirtualCameraPawn()->TeleportToHomeWaypoint())
	{
		// Do nothing since we teleport in the above call
	}
	else 
	{
		ResetCameraOffsetsToTracker();
	}
}

bool AVirtualCameraPlayerControllerBase::TeleportToWaypoint(const FString& WaypointName)
{
	if (AVirtualCameraPawnBase* VCPawn = GetVirtualCameraPawn())
	{
		return VCPawn->TeleportToWaypoint(WaypointName);
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::ToggleAxisFreeze(const EVirtualCameraAxis AxisToToggle)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->ToggleAxisFreeze(AxisToToggle);
	}

	return false;
}

bool AVirtualCameraPlayerControllerBase::ToggleAxisLock(const EVirtualCameraAxis AxisToToggle)
{
	if (UVirtualCameraMovementComponent* MovementComponent = GetVirtualCameraMovementComponent())
	{
		return MovementComponent->ToggleAxisLock(AxisToToggle);
	}

	return false;
}

void AVirtualCameraPlayerControllerBase::ActivateGameViewport()
{
	FSlateApplication::Get().ActivateGameViewport();
}
