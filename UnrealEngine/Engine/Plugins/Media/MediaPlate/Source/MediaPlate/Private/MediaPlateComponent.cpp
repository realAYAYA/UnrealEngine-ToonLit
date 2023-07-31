// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlateComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "IMediaClock.h"
#include "IMediaClockSink.h"
#include "IMediaModule.h"
#include "Materials/Material.h"
#include "MediaComponent.h"
#include "MediaPlateModule.h"
#include "MediaPlate.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "MediaSource.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "MediaTextureTracker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaPlateComponent)

#define LOCTEXT_NAMESPACE "MediaPlate"


/**
 * Media clock sink for media textures.
 */
class FMediaComponentClockSink
	: public IMediaClockSink
{
public:

	FMediaComponentClockSink(UMediaPlateComponent* InOwner)
		: Owner(InOwner)
	{ }

	virtual ~FMediaComponentClockSink() { }

	virtual void TickOutput(FTimespan DeltaTime, FTimespan Timecode) override
	{
		if (UMediaPlateComponent* OwnerPtr = Owner.Get())
		{
			Owner->TickOutput();
		}
	}


	/**
	 * Call this when the owner is destroyed.
	 */
	void OwnerDestroyed()
	{
		Owner.Reset();
	}

private:

	TWeakObjectPtr<UMediaPlateComponent> Owner;
};

FLazyName UMediaPlateComponent::MediaComponentName(TEXT("MediaComponent0"));
FLazyName UMediaPlateComponent::MediaPlaylistName(TEXT("MediaPlaylist0"));

UMediaPlateComponent::UMediaPlateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	CacheSettings.bOverride = true;

	// Set up playlist.
	MediaPlaylist = CreateDefaultSubobject<UMediaPlaylist>(MediaPlaylistName);

	// Default to plane since AMediaPlate defaults to SM_MediaPlateScreen
	VisibleMipsTilesCalculations = EMediaTextureVisibleMipsTiles::Plane;
}

void UMediaPlateComponent::OnRegister()
{
	Super::OnRegister();

	// Create media texture if we don't have one.
	if (MediaTexture == nullptr)
	{
		MediaTexture = NewObject<UMediaTexture>(this);
		MediaTexture->NewStyleOutput = true;
	}

	// Create media player if we don't have one.
	if (MediaPlayer == nullptr)
	{
		MediaPlayer = NewObject<UMediaPlayer>(this);
		MediaPlayer->SetLooping(false);
		MediaPlayer->PlayOnOpen = false;
	}
	MediaPlayer->OnMediaOpened.AddUniqueDynamic(this, &UMediaPlateComponent::OnMediaOpened);
	MediaPlayer->OnEndReached.AddUniqueDynamic(this, &UMediaPlateComponent::OnMediaEnd);

	// Set up media texture.
	if (MediaTexture != nullptr)
	{
		// Prevent media texture blackouts by only updating resource and material uniforms on relevant changes.
		bool bApplyTextureMaterialUpdate = false;

		if (FMath::IsNearlyEqual(MediaTexture->GetMipMapBias(), MipMapBias) == false)
		{
			MediaTexture->SetMipMapBias(MipMapBias);
			bApplyTextureMaterialUpdate = true;
		}

		if (MediaTexture->GetMediaPlayer() != MediaPlayer.Get())
		{
			MediaTexture->SetMediaPlayer(MediaPlayer);
			bApplyTextureMaterialUpdate = true;
		}

		if (bApplyTextureMaterialUpdate)
		{
			MediaTexture->UpdateResource();

			if (AMediaPlate* MediaPlate = GetOwner<AMediaPlate>())
			{
				if (UMaterialInterface* Material = MediaPlate->GetCurrentMaterial())
				{
					Material->RecacheUniformExpressions(false);
				}
			}
		}
	}

	// Set up sound component if we have one.
	if (SoundComponent != nullptr)
	{
		if (MediaPlayer != nullptr)
		{
			SoundComponent->SetMediaPlayer(MediaPlayer);
		}
	}

	RegisterWithMediaTextureTracker();
}

void UMediaPlateComponent::BeginPlay()
{
	Super::BeginPlay();

	// Start playing?
	if (bAutoPlay)
	{
		Open();
	}
}

void UMediaPlateComponent::BeginDestroy()
{
	if (ClockSink.IsValid())
	{
		// Tell sink we are done.
		ClockSink->OwnerDestroyed();

		IMediaModule* MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
		if (MediaModule != nullptr)
		{
			MediaModule->GetClock().RemoveSink(ClockSink.ToSharedRef());
		}

		ClockSink.Reset();
	}

	Super::BeginDestroy();
}


void UMediaPlateComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MediaPlayer != nullptr)
	{
		if ((CurrentRate != 0.0f) || (bWantsToPlayWhenVisible))
		{
			bool bIsVisible = IsVisible();
			if (bIsVisible)
			{
				ResumeWhenVisible();
			}
			else
			{
				if (MediaPlayer->IsPlaying())
				{
					MediaPlayer->Pause();
					TimeWhenPlaybackPaused = FApp::GetGameTime();
				}
			}
		}
	}
}

void UMediaPlateComponent::OnUnregister()
{
	Super::OnUnregister();

	UnregisterWithMediaTextureTracker();
}

UMediaPlayer* UMediaPlateComponent::GetMediaPlayer()
{
	return MediaPlayer;
}

UMediaTexture* UMediaPlateComponent::GetMediaTexture()
{
	return MediaTexture;
}

void UMediaPlateComponent::Open()
{
	bIsMediaPlatePlaying = true;
	CurrentRate = bPlayOnOpen ? 1.0f : 0.0f;

	if (IsVisible())
	{
		bool bIsPlaying = false;
		if (MediaPlayer != nullptr)
		{
			UMediaSource* MediaSource = nullptr;
			if (MediaPlaylist != nullptr)
			{
				MediaSource = MediaPlaylist->Get(0);
			}
			bIsPlaying = PlayMediaSource(MediaSource);
		}

		// Did anything play?
		if (bIsPlaying == false)
		{
			UE_LOG(LogMediaPlate, Warning, TEXT("Could not play anything."));
		}
	}
	else
	{
		bWantsToPlayWhenVisible = true;
		TimeWhenPlaybackPaused = FApp::GetGameTime();
	}

	UpdateTicking();
}

bool UMediaPlateComponent::Next()
{
	bool bIsSuccessful = false;

	// Do we have a playlist?
	if ((MediaPlaylist != nullptr) && (MediaPlaylist->Num() > 1))
	{
		if (PlaylistIndex < MediaPlaylist->Num() - 1)
		{
			// Get the next media to play.
			UMediaSource* NextSource = MediaPlaylist->GetNext(PlaylistIndex);
			if (NextSource != nullptr)
			{
				bIsSuccessful = PlayMediaSource(NextSource);
			}
		}
	}

	return bIsSuccessful;
}

void UMediaPlateComponent::Play()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->Play();
	}
	CurrentRate = 1.0f;
}

void UMediaPlateComponent::Pause()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->Pause();
	}
	CurrentRate = 0.0f;
}

bool UMediaPlateComponent::Previous()
{
	bool bIsSuccessful = false;

	// Do we have a playlist?
	if ((MediaPlaylist != nullptr) && (MediaPlaylist->Num() > 1))
	{
		// Get the previous media to play.
		if (PlaylistIndex > 0)
		{
			UMediaSource* NextSource = MediaPlaylist->GetPrevious(PlaylistIndex);
			if (NextSource != nullptr)
			{
				bIsSuccessful = PlayMediaSource(NextSource);
			}
		}
	}

	return bIsSuccessful;
}

bool UMediaPlateComponent::Rewind()
{
	return Seek(FTimespan::Zero());
}

bool UMediaPlateComponent::Seek(const FTimespan& Time)
{
	if (MediaPlayer != nullptr)
	{
		return MediaPlayer->Seek(Time);
	}

	return false;
}

void UMediaPlateComponent::Close()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->Close();
	}

	StopClockSink();
	bIsMediaPlatePlaying = false;
	bWantsToPlayWhenVisible = false;
	bResumeWhenOpened = false;
	PlaylistIndex = 0;
	UpdateTicking();
}

bool UMediaPlateComponent::GetLoop()
{
	return bLoop;
}

void UMediaPlateComponent::SetLoop(bool bInLoop)
{
	if (bLoop != bInLoop)
	{
		bLoop = bInLoop;
		if (MediaPlayer != nullptr)
		{
			MediaPlayer->SetLooping(bLoop);
		}
	}
}

void UMediaPlateComponent::SetMeshRange(FVector2D InMeshRange)
{
	MeshRange = InMeshRange;

	if (MediaTextureTrackerObject != nullptr)
	{
		MediaTextureTrackerObject->MeshRange = MeshRange;
	}
}

void UMediaPlateComponent::SetPlayOnlyWhenVisible(bool bInPlayOnlyWhenVisible)
{
	bPlayOnlyWhenVisible = bInPlayOnlyWhenVisible;
	PlayOnlyWhenVisibleChanged();
}

void UMediaPlateComponent::PlayOnlyWhenVisibleChanged()
{
	// If we are turning off PlayOnlyWhenVisible then make sure we are playing.
	if (bPlayOnlyWhenVisible == false)
	{
		ResumeWhenVisible();
	}
}

void UMediaPlateComponent::RegisterWithMediaTextureTracker()
{
	UnregisterWithMediaTextureTracker();

	// Set up object.
	if (MediaTextureTrackerObject == nullptr)
	{
		MediaTextureTrackerObject = MakeShared<FMediaTextureTrackerObject, ESPMode::ThreadSafe>();
	}

	MediaTextureTrackerObject->Object = GetOwner();
	MediaTextureTrackerObject->MipMapLODBias = MipMapBias;
	MediaTextureTrackerObject->VisibleMipsTilesCalculations = VisibleMipsTilesCalculations;
	MediaTextureTrackerObject->MeshRange = MeshRange;

	// Add our texture.
	if (MediaTexture != nullptr)
	{
		FMediaTextureTracker& MediaTextureTracker = FMediaTextureTracker::Get();
		MediaTextureTracker.RegisterTexture(MediaTextureTrackerObject, MediaTexture);
	}
}

void UMediaPlateComponent::UnregisterWithMediaTextureTracker()
{
	// Remove out texture.
	if (MediaTextureTrackerObject != nullptr)
	{
		FMediaTextureTracker& MediaTextureTracker = FMediaTextureTracker::Get();
		MediaTextureTracker.UnregisterTexture(MediaTextureTrackerObject, MediaTexture);
	}
}

bool UMediaPlateComponent::PlayMediaSource(UMediaSource* InMediaSource)
{
	bool bIsPlaying = false;

	if (InMediaSource != nullptr)
	{
		// Set cache settings.
		InMediaSource->SetCacheSettings(CacheSettings);
		
		// Set media options.
		if (MediaPlayer != nullptr)
		{
			// Play the source.
			FMediaPlayerOptions Options;
			Options.SeekTime = FTimespan::FromSeconds(StartTime);
			Options.PlayOnOpen = bPlayOnOpen ? EMediaPlayerOptionBooleanOverride::Enabled :
				EMediaPlayerOptionBooleanOverride::Disabled;
			Options.Loop = bLoop ? EMediaPlayerOptionBooleanOverride::Enabled :
				EMediaPlayerOptionBooleanOverride::Disabled;
			bIsPlaying = MediaPlayer->OpenSourceWithOptions(InMediaSource, Options);

			// Did we play anything?
			if (bIsPlaying)
			{
				// Are we using automatic aspect ratio?
				if ((bIsAspectRatioAuto) &&
					(VisibleMipsTilesCalculations == EMediaTextureVisibleMipsTiles::Plane))
				{
					// Start the clock sink so we can tick.
					IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");
					if (MediaModule != nullptr)
					{
						if (ClockSink.IsValid() == false)
						{
							ClockSink = MakeShared<FMediaComponentClockSink, ESPMode::ThreadSafe>(this);
						}
						MediaModule->GetClock().AddSink(ClockSink.ToSharedRef());
					}
				}
			}
		}
	}

	return bIsPlaying;
}


float UMediaPlateComponent::GetAspectRatio()
{
	if (StaticMeshComponent != nullptr)
	{
		// Calculate aspect ratio from the scale.
		FVector Scale = StaticMeshComponent->GetRelativeScale3D();
		float AspectRatio = 0.0f;
		if (Scale.Z != 0.0f)
		{
			AspectRatio = Scale.Y / Scale.Z;
		}
		return AspectRatio;
	}

	return 0.0f;
}

void UMediaPlateComponent::SetAspectRatio(float AspectRatio)
{
	// Get the static mesh.
	if (StaticMeshComponent != nullptr)
	{
		// Update the scale.
		float Height = 1.0f;
		if (AspectRatio != 0.0f)
		{
			Height = 1.0f / AspectRatio;
		}
		FVector Scale(1.0f, 1.0f, Height);
#if WITH_EDITOR
		StaticMeshComponent->Modify();
#endif // WITH_EDITOR
		StaticMeshComponent->SetRelativeScale3D(Scale);

		UpdateLetterboxes();
	}
}

void UMediaPlateComponent::SetLetterboxAspectRatio(float AspectRatio)
{
	LetterboxAspectRatio = FMath::Max(0.0f, AspectRatio);
	if (LetterboxAspectRatio == 0.0f)
	{
		RemoveLetterboxes();
	}
	else
	{
		AddLetterboxes();
	}

	UpdateLetterboxes();
}

void UMediaPlateComponent::TickOutput()
{
	if (MediaPlayer != nullptr)
	{
		// Is the player ready?
		if (MediaPlayer->IsPreparing() == false)
		{
			FIntPoint VideoDim = MediaPlayer->GetVideoTrackDimensions(INDEX_NONE, INDEX_NONE);
			if (VideoDim.Y != 0)
			{ 
				// Set aspect ratio.
				float AspectRatio = (float)VideoDim.X / (float)VideoDim.Y;
				SetAspectRatio(AspectRatio);
					
				// No need to tick anymore.
				StopClockSink();
			}
		}
	}
}

float UMediaPlateComponent::GetProxyRate() const
{
	return CurrentRate;
}

bool UMediaPlateComponent::SetProxyRate(float Rate)
{
	CurrentRate = Rate;

	bool bSuccess = true;
	if (MediaPlayer != nullptr)
	{
		bSuccess = MediaPlayer->SetRate(Rate);
	}

	return bSuccess;
}

bool UMediaPlateComponent::IsExternalControlAllowed()
{
	// Allow control if we are visible.
	return IsVisible();
}

const FMediaSourceCacheSettings& UMediaPlateComponent::GetCacheSettings() const
{
	return CacheSettings;
}

void UMediaPlateComponent::RestartPlayer()
{
	if (MediaPlayer != nullptr)
	{
		if (MediaPlayer->IsPlaying())
		{
			MediaPlayer->Close();
			Open();
		}
	}
}

void UMediaPlateComponent::StopClockSink()
{
	if (ClockSink.IsValid())
	{
		IMediaModule* MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
		if (MediaModule != nullptr)
		{
			MediaModule->GetClock().RemoveSink(ClockSink.ToSharedRef());
		}
	}
}

bool UMediaPlateComponent::IsVisible()
{
	bool bIsVisible = ((StaticMeshComponent != nullptr) && (StaticMeshComponent->ShouldRender()));

	if (bIsVisible && bPlayOnlyWhenVisible)
	{
		bIsVisible = GetOwner()->WasRecentlyRendered();
	}

	return bIsVisible;
}

void UMediaPlateComponent::ResumeWhenVisible()
{
	if (MediaPlayer != nullptr)
	{
		if (MediaPlayer->IsPaused())
		{
			// Should we be playing?
			if (CurrentRate != 0.0f)
			{
				FTimespan PlayTime = GetResumeTime();
				MediaPlayer->Seek(PlayTime);
				MediaPlayer->Play();
			}
		}
		else if (bWantsToPlayWhenVisible)
		{
			if ((bResumeWhenOpened == false) &&
				(MediaPlayer->IsPreparing() == false) &&
				(MediaPlayer->IsPlaying() == false))
			{
				bResumeWhenOpened = true;
				bWantsToPlayWhenVisible = false;
				Open();
			}
		}
	}
}

FTimespan UMediaPlateComponent::GetResumeTime()
{
	FTimespan PlayerTime;
	if (MediaPlayer != nullptr)
	{
		PlayerTime = MediaPlayer->GetTime();
		float CurrentTime = FApp::GetGameTime();
		float ElapsedTime = CurrentTime - TimeWhenPlaybackPaused;
		PlayerTime += FTimespan::FromSeconds(ElapsedTime);
		
		// Are we over the length of the media?
		FTimespan MediaDuration = MediaPlayer->GetDuration();
		if (PlayerTime > MediaDuration)
		{
			bool bIsPlaylist = (MediaPlaylist != nullptr) && (MediaPlaylist->Num() > 1);
			if ((bLoop) && (bIsPlaylist == false))
			{
				PlayerTime %= MediaDuration;
			}
			else
			{
				// It wont play if we seek to the very end, so go back a little bit.
				PlayerTime = MediaDuration - FTimespan::FromSeconds(0.001f);
			}
		}
	}

	return PlayerTime;
}

void UMediaPlateComponent::UpdateTicking()
{
	bool bEnableTick = bIsMediaPlatePlaying;
	PrimaryComponentTick.SetTickFunctionEnable(bEnableTick);
}


void UMediaPlateComponent::UpdateLetterboxes()
{
	float AspectRatio = GetAspectRatio();
	if ((AspectRatio <= LetterboxAspectRatio) || (LetterboxAspectRatio <= 0.0f))
	{
		for (TObjectPtr<UStaticMeshComponent> Letterbox : Letterboxes)
		{
			if (Letterbox != nullptr)
			{
				Letterbox->Modify();

				Letterbox->SetVisibility(false);
			}
		}
	}
	else if (AspectRatio > 0.0f)
	{
		float DefaultHeight = 50.0f;
		float VideoHeight = DefaultHeight / AspectRatio;
		float MaxHeight = DefaultHeight / LetterboxAspectRatio;

		float LetterboxHeight = (MaxHeight - VideoHeight) * 0.5f;
		LetterboxHeight = FMath::Max(LetterboxHeight, 0.0f);
		FVector Scale(1.0f, 1.0f, LetterboxHeight / DefaultHeight);

		FVector Location(0.0f, 0.0f, VideoHeight + LetterboxHeight);

		for (TObjectPtr<UStaticMeshComponent> Letterbox : Letterboxes)
		{
			if (Letterbox != nullptr)
			{
				Letterbox->Modify();
				Letterbox->SetVisibility(true);
				Letterbox->SetRelativeScale3D(Scale);
				Letterbox->SetRelativeLocation(Location);
				Location.Z = -Location.Z;
			}
		}
	}
}


void UMediaPlateComponent::AddLetterboxes()
{
	if (Letterboxes.Num() == 0)
	{
		AActor* Owner = GetOwner();
		if (Owner != nullptr)
		{
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(NULL, TEXT("/MediaPlate/SM_MediaPlateScreen"), NULL, LOAD_None, NULL);
			UMaterial* Material = LoadObject<UMaterial>(NULL, TEXT("/MediaPlate/M_MediaPlateLetterbox"), NULL, LOAD_None, NULL);
			if ((Mesh != nullptr) && (Material != nullptr))
			{
				for (int32 Index = 0; Index < 2; ++Index)
				{
					UStaticMeshComponent* Letterbox = NewObject<UStaticMeshComponent>(Owner);
					if (Letterbox != nullptr)
					{
						Letterboxes.Add(Letterbox);
						Owner->Modify();
						Owner->AddInstanceComponent(Letterbox);
						Letterbox->OnComponentCreated();
						Letterbox->AttachToComponent(Owner->GetRootComponent(),
							FAttachmentTransformRules::KeepRelativeTransform);
						Letterbox->RegisterComponent();
						Letterbox->SetStaticMesh(Mesh);
						Letterbox->SetMaterial(0, Material);
						Letterbox->bCastStaticShadow = false;
						Letterbox->bCastDynamicShadow = false;
						Letterbox->SetVisibility(true);
					}
				}
			}
		}
	}
}

void UMediaPlateComponent::RemoveLetterboxes()
{
	for (TObjectPtr<UStaticMeshComponent> Letterbox : Letterboxes)
	{
		if (Letterbox != nullptr)
		{	
			Letterbox->DestroyComponent();
		}
	}

	Letterboxes.Empty();
}

void UMediaPlateComponent::OnMediaOpened(FString DeviceUrl)
{
	if (bResumeWhenOpened)
	{
		bResumeWhenOpened = false;
		bIsMediaPlatePlaying = true;
		if (MediaPlayer != nullptr)
		{
			FTimespan PlayTime = GetResumeTime();
			MediaPlayer->Seek(PlayTime);
		}
	}
}

void UMediaPlateComponent::OnMediaEnd()
{
	StopClockSink();

	Next();
}

#if WITH_EDITOR

void UMediaPlateComponent::SetVisibleMipsTilesCalculations(EMediaTextureVisibleMipsTiles InVisibleMipsTilesCalculations)
{
	VisibleMipsTilesCalculations = InVisibleMipsTilesCalculations;

	if (MediaTextureTrackerObject != nullptr)
	{
		MediaTextureTrackerObject->VisibleMipsTilesCalculations = VisibleMipsTilesCalculations;

		RestartPlayer();
	}
}

void UMediaPlateComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Has bEnableAudiio changed?
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, bEnableAudio))
	{
		// Are we turning on audio?
		if (bEnableAudio)
		{
			// Get the media player.
			if (MediaPlayer != nullptr)
			{
				// Create a sound component.
				SoundComponent = NewObject<UMediaSoundComponent>(this, NAME_None);
				if (SoundComponent != nullptr)
				{
					SoundComponent->bIsUISound = true;
					SoundComponent->SetMediaPlayer(MediaPlayer);
					SoundComponent->Initialize();
					SoundComponent->RegisterComponent();
				}
			}
		}
		else
		{
			// Remove this sound component.
			if (SoundComponent != nullptr)
			{
				SoundComponent->UnregisterComponent();
				SoundComponent->SetMediaPlayer(nullptr);
				SoundComponent->UpdatePlayer();
				SoundComponent->DestroyComponent();
				SoundComponent = nullptr;
			}
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, bPlayOnlyWhenVisible))
	{
		PlayOnlyWhenVisibleChanged();
	}
	else if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, CacheSettings))
	{
		RestartPlayer();
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, VisibleMipsTilesCalculations))
	{
		if (MediaTextureTrackerObject != nullptr)
		{
			MediaTextureTrackerObject->VisibleMipsTilesCalculations = VisibleMipsTilesCalculations;
			
			RestartPlayer();
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, MipMapBias))
	{
		if (MediaTextureTrackerObject != nullptr)
		{
			MediaTextureTrackerObject->MipMapLODBias = MipMapBias;

			// Note: Media texture bias and material sampler automatically updated by UMediaPlateComponent::OnRegister().
		}
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE

