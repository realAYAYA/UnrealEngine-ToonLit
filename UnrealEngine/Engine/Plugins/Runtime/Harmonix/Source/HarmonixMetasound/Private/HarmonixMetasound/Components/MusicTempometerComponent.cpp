// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/Components/MusicTempometerComponent.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MusicTempometerComponent)

UMusicTempometerComponent::UMusicTempometerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMusicTempometerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MaterialParameterCollection)
	{
		UpdateCachedSongPosIfNeeded();
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

#if WITH_EDITOR
void UMusicTempometerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMusicTempometerComponent, MaterialParameterCollection))
	{
		SetComponentTickEnabled(MaterialParameterCollection != nullptr);
	}
}
#endif // WITH_EDITOR

void UMusicTempometerComponent::UpdateCachedSongPos() const
{
	LastFrameCounter = GFrameCounter;

	// Cache or clear various FSongPos
	if (UMusicClockComponent* Clock = GetMutableClockNoMutex())
	{
		SongPos = Clock->GetCurrentVideoRenderSongPos();
	}
	else
	{
		SongPos.Reset();
	}

	// Find a MaterialParameterCollectionInstance to update
	if (!IsValid(MaterialParameterCollectionInstance))
	{
		if (MaterialParameterCollection)
		{
			if (UWorld* World = GetWorld())
			{
				MaterialParameterCollectionInstance = World->GetParameterCollectionInstance(MaterialParameterCollection);
			}
		}

		if (!IsValid(MaterialParameterCollectionInstance))
		{
			return;
		}
	}

	MaterialParameterCollectionInstance->SetScalarParameterValue(SecondsIncludingCountInParameterName, SongPos.SecondsIncludingCountIn);
	MaterialParameterCollectionInstance->SetScalarParameterValue(BarsIncludingCountInParameterName, SongPos.BarsIncludingCountIn);
	MaterialParameterCollectionInstance->SetScalarParameterValue(BeatsIncludingCountInParameterName, SongPos.BeatsIncludingCountIn);

	MaterialParameterCollectionInstance->SetScalarParameterValue(SecondsFromBarOneParameterName, SongPos.SecondsFromBarOne);
	MaterialParameterCollectionInstance->SetScalarParameterValue(TimestampBarParameterName, (float)SongPos.Timestamp.Bar);
	MaterialParameterCollectionInstance->SetScalarParameterValue(TimestampBeatInBarParameterName, SongPos.Timestamp.Beat);

	MaterialParameterCollectionInstance->SetScalarParameterValue(BarProgressParameterName, FMath::Fractional(SongPos.BarsIncludingCountIn));
	MaterialParameterCollectionInstance->SetScalarParameterValue(BeatProgressParameterName, FMath::Fractional(SongPos.BeatsIncludingCountIn));

	MaterialParameterCollectionInstance->SetScalarParameterValue(TimeSignatureNumeratorParameterName, SongPos.TimeSigNumerator);
	MaterialParameterCollectionInstance->SetScalarParameterValue(TimeSignatureDenominatorParameterName, SongPos.TimeSigDenominator);
	MaterialParameterCollectionInstance->SetScalarParameterValue(TempoParameterName, SongPos.Tempo);
}

UMusicClockComponent* UMusicTempometerComponent::FindClock(AActor* Actor) const
{
	UMusicClockComponent* FoundClock = nullptr;
	for (UActorComponent* Component : Actor->GetComponents())
	{
		FoundClock = Cast<UMusicClockComponent>(Component);
		if (FoundClock)
		{
			break;
		}
	}
	return FoundClock;
}

void UMusicTempometerComponent::SetOwnerClock() const
{
	MusicClock = FindClock(GetOwner());
}

