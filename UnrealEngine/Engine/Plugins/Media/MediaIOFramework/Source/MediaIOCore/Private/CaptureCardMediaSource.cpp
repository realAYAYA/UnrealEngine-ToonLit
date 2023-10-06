// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureCardMediaSource.h"


UCaptureCardMediaSource::UCaptureCardMediaSource()
{
	Deinterlacer = CreateDefaultSubobject<UBobDeinterlacer>("Deinterlacer");
}

int64 UCaptureCardMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	if (Key == UE::CaptureCardMediaSource::InterlaceFieldOrder)
	{
		return static_cast<int64>(InterlaceFieldOrder);
	}

	if (Key == UE::CaptureCardMediaSource::SourceColorSpace)
	{
		return (int64) OverrideSourceColorSpace;
	}

	if (Key == UE::CaptureCardMediaSource::SourceEncoding)
	{
		return (int64) OverrideSourceEncoding;
	}

	if (Key == UE::CaptureCardMediaSource::EvaluationType)
	{
		return (int64) EvaluationType;
	}

	return UTimeSynchronizableMediaSource::GetMediaOption(Key, DefaultValue);
}

TSharedPtr<IMediaOptions::FDataContainer, ESPMode::ThreadSafe> UCaptureCardMediaSource::GetMediaOption(const FName& Key, const TSharedPtr<FDataContainer, ESPMode::ThreadSafe>& DefaultValue) const
{
	if (Key ==  UE::CaptureCardMediaSource::OpenColorIOSettings)
	{
		FOpenColorIODataContainer Container;
		Container.ColorConversionSettings = ColorConversionSettings;
		return MakeShared<FOpenColorIODataContainer>(MoveTemp(Container));
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

FString UCaptureCardMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == UE::CaptureCardMediaSource::Deinterlacer)
	{
		if (Deinterlacer)
		{
			return Deinterlacer->GetPathName();
		}
	}

	return UTimeSynchronizableMediaSource::GetMediaOption(Key, DefaultValue);
}

bool UCaptureCardMediaSource::GetMediaOption(const FName& Key, bool bDefaultValue) const
{
	if (Key == UE::CaptureCardMediaSource::RenderJIT)
	{
		return bRenderJIT;
	}

	if (Key == UE::CaptureCardMediaSource::Framelock)
	{
		return bFramelock;
	}

	if (Key == UE::CaptureCardMediaSource::OverrideSourceColorSpace)
	{
		return bOverrideSourceColorSpace;
	}

	if (Key == UE::CaptureCardMediaSource::OverrideSourceEncoding)
	{
		return bOverrideSourceEncoding;
	}

	return UTimeSynchronizableMediaSource::GetMediaOption(Key, bDefaultValue);
}

bool UCaptureCardMediaSource::HasMediaOption(const FName& Key) const
{
	if (Key == UE::CaptureCardMediaSource::Deinterlacer
		|| Key == UE::CaptureCardMediaSource::InterlaceFieldOrder
		|| Key == UE::CaptureCardMediaSource::OverrideSourceEncoding
		|| Key == UE::CaptureCardMediaSource::OverrideSourceColorSpace
		|| Key == UE::CaptureCardMediaSource::SourceEncoding
		|| Key == UE::CaptureCardMediaSource::SourceColorSpace
		|| Key == UE::CaptureCardMediaSource::RenderJIT
		|| Key == UE::CaptureCardMediaSource::Framelock
		|| Key == UE::CaptureCardMediaSource::EvaluationType
		|| Key == UE::CaptureCardMediaSource::OpenColorIOSettings)
		{
			return true;
		}

	return UTimeSynchronizableMediaSource::HasMediaOption(Key);
}

#if WITH_EDITOR
bool UCaptureCardMediaSource::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UCaptureCardMediaSource, bFramelock))
	{
		return bRenderJIT && EvaluationType == EMediaIOSampleEvaluationType::Timecode && bUseTimeSynchronization;
	}

	return true;
}

void UCaptureCardMediaSource::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCaptureCardMediaSource, bUseTimeSynchronization))
	{
		if (!bUseTimeSynchronization)
		{
			bFramelock = false;
			EvaluationType = EMediaIOSampleEvaluationType::PlatformTime;
		}
	}

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCaptureCardMediaSource, bRenderJIT))
	{
		if (!bRenderJIT)
		{
			bFramelock = false;
			EvaluationType = (EvaluationType == EMediaIOSampleEvaluationType::Latest ? EMediaIOSampleEvaluationType::PlatformTime : EvaluationType);
		}
	}

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCaptureCardMediaSource, EvaluationType))
	{
		if (EvaluationType != EMediaIOSampleEvaluationType::Timecode)
		{
			bFramelock = false;
		}
	}
}
#endif //WITH_EDITOR
