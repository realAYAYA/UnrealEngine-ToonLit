// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerController.h"
#include "CommonInputTypeEnum.h"
#include "Components/PrimitiveComponent.h"
#include "LyraLogChannels.h"
#include "LyraCheatManager.h"
#include "LyraPlayerState.h"
#include "Camera/LyraPlayerCameraManager.h"
#include "UI/LyraHUD.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "EngineUtils.h"
#include "LyraGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemGlobals.h"
#include "CommonInputSubsystem.h"
#include "LyraLocalPlayer.h"
#include "GameModes/LyraGameState.h"
#include "Settings/LyraSettingsLocal.h"
#include "Settings/LyraSettingsShared.h"
#include "Replays/LyraReplaySubsystem.h"
#include "ReplaySubsystem.h"
#include "Development/LyraDeveloperSettings.h"
#include "GameMapsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerController)

namespace Lyra
{
	namespace Input
	{
		static int32 ShouldAlwaysPlayForceFeedback = 0;
		static FAutoConsoleVariableRef CVarShouldAlwaysPlayForceFeedback(TEXT("LyraPC.ShouldAlwaysPlayForceFeedback"),
			ShouldAlwaysPlayForceFeedback,
			TEXT("Should force feedback effects be played, even if the last input device was not a gamepad?"));
	}
}

ALyraPlayerController::ALyraPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCameraManagerClass = ALyraPlayerCameraManager::StaticClass();

#if USING_CHEAT_MANAGER
	CheatClass = ULyraCheatManager::StaticClass();
#endif // #if USING_CHEAT_MANAGER
}

void ALyraPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void ALyraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(false);
}

void ALyraPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ALyraPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Disable replicating the PC target view as it doesn't work well for replays or client-side spectating.
	// The engine TargetViewRotation is only set in APlayerController::TickActor if the server knows ahead of time that 
	// a specific pawn is being spectated and it only replicates down for COND_OwnerOnly.
	// In client-saved replays, COND_OwnerOnly is never true and the target pawn is not always known at the time of recording.
	// To support client-saved replays, the replication of this was moved to ReplicatedViewRotation and updated in PlayerTick.
	DISABLE_REPLICATED_PROPERTY(APlayerController, TargetViewRotation);
}

void ALyraPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
}

void ALyraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// If we are auto running then add some player input
	if (GetIsAutoRunning())
	{
		if (APawn* CurrentPawn = GetPawn())
		{
			const FRotator MovementRotation(0.0f, GetControlRotation().Yaw, 0.0f);
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			CurrentPawn->AddMovementInput(MovementDirection, 1.0f);	
		}
	}

	ALyraPlayerState* LyraPlayerState = GetLyraPlayerState();

	if (PlayerCameraManager && LyraPlayerState)
	{
		APawn* TargetPawn = PlayerCameraManager->GetViewTargetPawn();

		if (TargetPawn)
		{
			// Update view rotation on the server so it replicates
			if (HasAuthority() || TargetPawn->IsLocallyControlled())
			{
				LyraPlayerState->SetReplicatedViewRotation(TargetPawn->GetViewRotation());
			}

			// Update the target view rotation if the pawn isn't locally controlled
			if (!TargetPawn->IsLocallyControlled())
			{
				LyraPlayerState = TargetPawn->GetPlayerState<ALyraPlayerState>();
				if (LyraPlayerState)
				{
					// Get it from the spectated pawn's player state, which may not be the same as the PC's playerstate
					TargetViewRotation = LyraPlayerState->GetReplicatedViewRotation();
				}
			}
		}
	}
}

ALyraPlayerState* ALyraPlayerController::GetLyraPlayerState() const
{
	return CastChecked<ALyraPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

ULyraAbilitySystemComponent* ALyraPlayerController::GetLyraAbilitySystemComponent() const
{
	const ALyraPlayerState* LyraPS = GetLyraPlayerState();
	return (LyraPS ? LyraPS->GetLyraAbilitySystemComponent() : nullptr);
}

ALyraHUD* ALyraPlayerController::GetLyraHUD() const
{
	return CastChecked<ALyraHUD>(GetHUD(), ECastCheckedType::NullAllowed);
}

bool ALyraPlayerController::TryToRecordClientReplay()
{
	// See if we should record a replay
	if (ShouldRecordClientReplay())
	{
		if (ULyraReplaySubsystem* ReplaySubsystem = GetGameInstance()->GetSubsystem<ULyraReplaySubsystem>())
		{
			APlayerController* FirstLocalPlayerController = GetGameInstance()->GetFirstLocalPlayerController();
			if (FirstLocalPlayerController == this)
			{
				// If this is the first player, update the spectator player for local replays and then record
				if (ALyraGameState* GameState = Cast<ALyraGameState>(GetWorld()->GetGameState()))
				{
					GameState->SetRecorderPlayerState(PlayerState);

					ReplaySubsystem->RecordClientReplay(this);
					return true;
				}
			}
		}
	}
	return false;
}

bool ALyraPlayerController::ShouldRecordClientReplay()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance != nullptr &&
		World != nullptr &&
		!World->IsPlayingReplay() &&
		!World->IsRecordingClientReplay() &&
		NM_DedicatedServer != GetNetMode() &&
		IsLocalPlayerController())
	{
		FString DefaultMap = UGameMapsSettings::GetGameDefaultMap();
		FString CurrentMap = World->URL.Map;

#if WITH_EDITOR
		CurrentMap = UWorld::StripPIEPrefixFromPackageName(CurrentMap, World->StreamingLevelsPrefix);
#endif
		if (CurrentMap == DefaultMap)
		{
			// Never record demos on the default frontend map, this could be replaced with a better check for being in the main menu
			return false;
		}

		if (UReplaySubsystem* ReplaySubsystem = GameInstance->GetSubsystem<UReplaySubsystem>())
		{
			if (ReplaySubsystem->IsRecording() || ReplaySubsystem->IsPlaying())
			{
				// Only one at a time
				return false;
			}
		}

		// If this is possible, now check the settings
		if (const ULyraLocalPlayer* LyraLocalPlayer = Cast<ULyraLocalPlayer>(GetLocalPlayer()))
		{
			if (LyraLocalPlayer->GetLocalSettings()->ShouldAutoRecordReplays())
			{
				return true;
			}
		}
	}
	return false;
}

void ALyraPlayerController::OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ConditionalBroadcastTeamChanged(this, IntegerToGenericTeamId(OldTeam), IntegerToGenericTeamId(NewTeam));
}

void ALyraPlayerController::OnPlayerStateChanged()
{
	// Empty, place for derived classes to implement without having to hook all the other events
}

void ALyraPlayerController::BroadcastOnPlayerStateChanged()
{
	OnPlayerStateChanged();

	// Unbind from the old player state, if any
	FGenericTeamId OldTeamID = FGenericTeamId::NoTeam;
	if (LastSeenPlayerState != nullptr)
	{
		if (ILyraTeamAgentInterface* PlayerStateTeamInterface = Cast<ILyraTeamAgentInterface>(LastSeenPlayerState))
		{
			OldTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
		}
	}

	// Bind to the new player state, if any
	FGenericTeamId NewTeamID = FGenericTeamId::NoTeam;
	if (PlayerState != nullptr)
	{
		if (ILyraTeamAgentInterface* PlayerStateTeamInterface = Cast<ILyraTeamAgentInterface>(PlayerState))
		{
			NewTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnPlayerStateChangedTeam);
		}
	}

	// Broadcast the team change (if it really has)
	ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);

	LastSeenPlayerState = PlayerState;
}

void ALyraPlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	BroadcastOnPlayerStateChanged();
}

void ALyraPlayerController::CleanupPlayerState()
{
	Super::CleanupPlayerState();
	BroadcastOnPlayerStateChanged();
}

void ALyraPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BroadcastOnPlayerStateChanged();
}

void ALyraPlayerController::SetPlayer(UPlayer* InPlayer)
{
	Super::SetPlayer(InPlayer);

	if (const ULyraLocalPlayer* LyraLocalPlayer = Cast<ULyraLocalPlayer>(InPlayer))
	{
		ULyraSettingsShared* UserSettings = LyraLocalPlayer->GetSharedSettings();
		UserSettings->OnSettingChanged.AddUObject(this, &ThisClass::OnSettingsChanged);

		OnSettingsChanged(UserSettings);
	}
}

void ALyraPlayerController::OnSettingsChanged(ULyraSettingsShared* InSettings)
{
	bForceFeedbackEnabled = InSettings->GetForceFeedbackEnabled();
}

void ALyraPlayerController::AddCheats(bool bForce)
{
#if USING_CHEAT_MANAGER
	Super::AddCheats(true);
#else //#if USING_CHEAT_MANAGER
	Super::AddCheats(bForce);
#endif // #else //#if USING_CHEAT_MANAGER
}

void ALyraPlayerController::ServerCheat_Implementation(const FString& Msg)
{
#if USING_CHEAT_MANAGER
	if (CheatManager)
	{
		UE_LOG(LogLyra, Warning, TEXT("ServerCheat: %s"), *Msg);
		ClientMessage(ConsoleCommand(Msg));
	}
#endif // #if USING_CHEAT_MANAGER
}

bool ALyraPlayerController::ServerCheat_Validate(const FString& Msg)
{
	return true;
}

void ALyraPlayerController::ServerCheatAll_Implementation(const FString& Msg)
{
#if USING_CHEAT_MANAGER
	if (CheatManager)
	{
		UE_LOG(LogLyra, Warning, TEXT("ServerCheatAll: %s"), *Msg);
		for (TActorIterator<ALyraPlayerController> It(GetWorld()); It; ++It)
		{
			ALyraPlayerController* LyraPC = (*It);
			if (LyraPC)
			{
				LyraPC->ClientMessage(LyraPC->ConsoleCommand(Msg));
			}
		}
	}
#endif // #if USING_CHEAT_MANAGER
}

bool ALyraPlayerController::ServerCheatAll_Validate(const FString& Msg)
{
	return true;
}

void ALyraPlayerController::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PreProcessInput(DeltaTime, bGamePaused);
}

void ALyraPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void ALyraPlayerController::OnCameraPenetratingTarget()
{
	bHideViewTargetPawnNextFrame = true;
}

void ALyraPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

#if WITH_SERVER_CODE && WITH_EDITOR
	if (GIsEditor && (InPawn != nullptr) && (GetPawn() == InPawn))
	{
		for (const FLyraCheatToRun& CheatRow : GetDefault<ULyraDeveloperSettings>()->CheatsToRun)
		{
			if (CheatRow.Phase == ECheatExecutionTime::OnPlayerPawnPossession)
			{
				ConsoleCommand(CheatRow.Cheat, /*bWriteToLog=*/ true);
			}
		}
	}
#endif

	SetIsAutoRunning(false);
}

void ALyraPlayerController::SetIsAutoRunning(const bool bEnabled)
{
	const bool bIsAutoRunning = GetIsAutoRunning();
	if (bEnabled != bIsAutoRunning)
	{
		if (!bEnabled)
		{
			OnEndAutoRun();
		}
		else
		{
			OnStartAutoRun();
		}
	}
}

bool ALyraPlayerController::GetIsAutoRunning() const
{
	bool bIsAutoRunning = false;
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		bIsAutoRunning = LyraASC->GetTagCount(LyraGameplayTags::Status_AutoRunning) > 0;
	}
	return bIsAutoRunning;
}

void ALyraPlayerController::OnStartAutoRun()
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_AutoRunning, 1);
		K2_OnStartAutoRun();
	}	
}

void ALyraPlayerController::OnEndAutoRun()
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_AutoRunning, 0);
		K2_OnEndAutoRun();
	}
}

void ALyraPlayerController::UpdateForceFeedback(IInputInterface* InputInterface, const int32 ControllerId)
{
	if (bForceFeedbackEnabled)
	{
		if (const UCommonInputSubsystem* CommonInputSubsystem = UCommonInputSubsystem::Get(GetLocalPlayer()))
		{
			const ECommonInputType CurrentInputType = CommonInputSubsystem->GetCurrentInputType();
			if (Lyra::Input::ShouldAlwaysPlayForceFeedback || CurrentInputType == ECommonInputType::Gamepad || CurrentInputType == ECommonInputType::Touch)
			{
				InputInterface->SetForceFeedbackChannelValues(ControllerId, ForceFeedbackValues);
				return;
			}
		}
	}
	
	InputInterface->SetForceFeedbackChannelValues(ControllerId, FForceFeedbackValues());
}

void ALyraPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& OutHiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, OutHiddenComponents);

	if (bHideViewTargetPawnNextFrame)
	{
		AActor* const ViewTargetPawn = PlayerCameraManager ? Cast<AActor>(PlayerCameraManager->GetViewTarget()) : nullptr;
		if (ViewTargetPawn)
		{
			// internal helper func to hide all the components
			auto AddToHiddenComponents = [&OutHiddenComponents](const TInlineComponentArray<UPrimitiveComponent*>& InComponents)
			{
				// add every component and all attached children
				for (UPrimitiveComponent* Comp : InComponents)
				{
					if (Comp->IsRegistered())
					{
						OutHiddenComponents.Add(Comp->GetPrimitiveSceneId());

						for (USceneComponent* AttachedChild : Comp->GetAttachChildren())
						{
							static FName NAME_NoParentAutoHide(TEXT("NoParentAutoHide"));
							UPrimitiveComponent* AttachChildPC = Cast<UPrimitiveComponent>(AttachedChild);
							if (AttachChildPC && AttachChildPC->IsRegistered() && !AttachChildPC->ComponentTags.Contains(NAME_NoParentAutoHide))
							{
								OutHiddenComponents.Add(AttachChildPC->GetPrimitiveSceneId());
							}
						}
					}
				}
			};

			//TODO Solve with an interface.  Gather hidden components or something.
			//TODO Hiding isn't awesome, sometimes you want the effect of a fade out over a proximity, needs to bubble up to designers.

			// hide pawn's components
			TInlineComponentArray<UPrimitiveComponent*> PawnComponents;
			ViewTargetPawn->GetComponents(PawnComponents);
			AddToHiddenComponents(PawnComponents);

			//// hide weapon too
			//if (ViewTargetPawn->CurrentWeapon)
			//{
			//	TInlineComponentArray<UPrimitiveComponent*> WeaponComponents;
			//	ViewTargetPawn->CurrentWeapon->GetComponents(WeaponComponents);
			//	AddToHiddenComponents(WeaponComponents);
			//}
		}

		// we consumed it, reset for next frame
		bHideViewTargetPawnNextFrame = false;
	}
}

void ALyraPlayerController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogLyraTeams, Error, TEXT("You can't set the team ID on a player controller (%s); it's driven by the associated player state"), *GetPathNameSafe(this));
}

FGenericTeamId ALyraPlayerController::GetGenericTeamId() const
{
	if (const ILyraTeamAgentInterface* PSWithTeamInterface = Cast<ILyraTeamAgentInterface>(PlayerState))
	{
		return PSWithTeamInterface->GetGenericTeamId();
	}
	return FGenericTeamId::NoTeam;
}

FOnLyraTeamIndexChangedDelegate* ALyraPlayerController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void ALyraPlayerController::OnUnPossess()
{
	// Make sure the pawn that is being unpossessed doesn't remain our ASC's avatar actor
	if (APawn* PawnBeingUnpossessed = GetPawn())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
		{
			if (ASC->GetAvatarActor() == PawnBeingUnpossessed)
			{
				ASC->SetAvatarActor(nullptr);
			}
		}
	}

	Super::OnUnPossess();
}

//////////////////////////////////////////////////////////////////////
// ALyraReplayPlayerController

void ALyraReplayPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The state may go invalid at any time due to scrubbing during a replay
	if (!IsValid(FollowedPlayerState))
	{
		UWorld* World = GetWorld();

		// Listen for changes for both recording and playback
		if (ALyraGameState* GameState = Cast<ALyraGameState>(World->GetGameState()))
		{
			if (!GameState->OnRecorderPlayerStateChangedEvent.IsBoundToObject(this))
			{
				GameState->OnRecorderPlayerStateChangedEvent.AddUObject(this, &ThisClass::RecorderPlayerStateUpdated);
			}
			if (APlayerState* RecorderState = GameState->GetRecorderPlayerState())
			{
				RecorderPlayerStateUpdated(RecorderState);
			}
		}
	}
}

void ALyraReplayPlayerController::SmoothTargetViewRotation(APawn* TargetPawn, float DeltaSeconds)
{
	// Default behavior is to interpolate to TargetViewRotation which is set from APlayerController::TickActor but it's not very smooth

	Super::SmoothTargetViewRotation(TargetPawn, DeltaSeconds);
}

bool ALyraReplayPlayerController::ShouldRecordClientReplay()
{
	return false;
}

void ALyraReplayPlayerController::RecorderPlayerStateUpdated(APlayerState* NewRecorderPlayerState)
{
	if (NewRecorderPlayerState)
	{
		FollowedPlayerState = NewRecorderPlayerState;

		// Bind to when pawn changes and call now
		NewRecorderPlayerState->OnPawnSet.AddUniqueDynamic(this, &ALyraReplayPlayerController::OnPlayerStatePawnSet);
		OnPlayerStatePawnSet(NewRecorderPlayerState, NewRecorderPlayerState->GetPawn(), nullptr);
	}
}

void ALyraReplayPlayerController::OnPlayerStatePawnSet(APlayerState* ChangedPlayerState, APawn* NewPlayerPawn, APawn* OldPlayerPawn)
{
	if (ChangedPlayerState == FollowedPlayerState)
	{
		SetViewTarget(NewPlayerPawn);
	}
}

