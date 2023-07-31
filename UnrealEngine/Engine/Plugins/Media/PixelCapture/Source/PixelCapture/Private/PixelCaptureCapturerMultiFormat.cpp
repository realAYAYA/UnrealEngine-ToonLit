// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerMultiFormat.h"
#include "PixelCaptureCapturer.h"
#include "PixelCapturePrivate.h"
#include "Misc/ScopeLock.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

TSharedPtr<FPixelCaptureCapturerMultiFormat> FPixelCaptureCapturerMultiFormat::Create(IPixelCaptureCapturerSource* InCapturerSource, TArray<float> LayerScales)
{
	return TSharedPtr<FPixelCaptureCapturerMultiFormat>(new FPixelCaptureCapturerMultiFormat(InCapturerSource, LayerScales));
}

FPixelCaptureCapturerMultiFormat::FPixelCaptureCapturerMultiFormat(IPixelCaptureCapturerSource* InCapturerSource, TArray<float> InLayerScales)
	: CapturerSource(InCapturerSource)
	, LayerScales(InLayerScales)
{
}

FPixelCaptureCapturerMultiFormat::~FPixelCaptureCapturerMultiFormat()
{
	FlushWaitingEvents();
}

int32 FPixelCaptureCapturerMultiFormat::GetWidth(int LayerIndex) const
{
	return LayerIndex < LayerSizes.Num() ? LayerSizes[LayerIndex].X : 0;
}

int32 FPixelCaptureCapturerMultiFormat::GetHeight(int LayerIndex) const
{
	return LayerIndex < LayerSizes.Num() ? LayerSizes[LayerIndex].Y : 0;
}

void FPixelCaptureCapturerMultiFormat::Capture(const IPixelCaptureInputFrame& SourceFrame)
{
	if (PendingFormats > 0)
	{
		// still capturing previous frame. drop new one.
		return;
	}

	// cache each layer size
	if (LayerSizes.IsEmpty())
	{
		const int Width = SourceFrame.GetWidth();
		const int Height = SourceFrame.GetHeight();
		LayerSizes.SetNum(LayerScales.Num());
		for (int i = 0; i < LayerScales.Num(); ++i)
		{
			LayerSizes[i].X = StaticCast<int>(Width * LayerScales[i]);
			LayerSizes[i].Y = StaticCast<int>(Height * LayerScales[i]);
		}
	}

	if (FormatCapturers.Num() > 0)
	{
		// UE-173694: Need to acquire the FormatGuard before FPixelCaptureCapturerLayered acquires its LayersGuard
		// to avoid a deadlock with the EncoderQueue
		FScopeLock FormatLock(&FormatGuard);
		// iterate a temp copy so modifications to the map in Capture
		// (and their callback events) is ok.
		// Note: When considering whether the lifetime of this lock can be shorter consider the deadlock issue we had in UE-173694

		TArray<TSharedPtr<FPixelCaptureCapturerLayered>> TempCopy;
		for (auto& FormatCapturer : FormatCapturers)
		{
			TempCopy.Add(FormatCapturer.Value);
		}
		// start all the format captures
		PendingFormats = FormatCapturers.Num();
		for (auto& Capturer : TempCopy)
		{
			Capturer->Capture(SourceFrame);
		}
	}
	else
	{
		// if we have no capturers just signal this capture as complete
		if (PendingFormats == 0)
		{
			OnComplete.Broadcast();
		}
	}
}

void FPixelCaptureCapturerMultiFormat::AddOutputFormat(int32 Format)
{
	FScopeLock FormatLock(&FormatGuard);

	if (!FormatCapturers.Contains(Format))
	{
		TSharedPtr<FPixelCaptureCapturerLayered> NewCapturer = FPixelCaptureCapturerLayered::Create(CapturerSource, Format, LayerScales);
		NewCapturer->OnComplete.AddSP(this, &FPixelCaptureCapturerMultiFormat::OnCaptureFormatComplete, Format);
		FormatCapturers.Add(Format, NewCapturer);
	}
}

TSharedPtr<IPixelCaptureOutputFrame> FPixelCaptureCapturerMultiFormat::RequestFormat(int32 Format, int32 LayerIndex)
{
	{
		FScopeLock FormatLock(&FormatGuard);

		if (TSharedPtr<FPixelCaptureCapturerLayered>* FormatCapturerPtr = FormatCapturers.Find(Format))
		{
			return (*FormatCapturerPtr)->ReadOutput((LayerIndex != -1 ? LayerIndex : LayerScales.Num() - 1));
		}
	}

	// if we reached here then we dont have a pipeline for the given format.
	// add it and return null
	AddOutputFormat(Format);
	return nullptr;
}

TSharedPtr<IPixelCaptureOutputFrame> FPixelCaptureCapturerMultiFormat::WaitForFormat(int32 Format, int32 LayerIndex, uint32 MaxWaitTime)
{
	if (TSharedPtr<IPixelCaptureOutputFrame> Frame = RequestFormat(Format, LayerIndex))
	{
		return Frame;
	}

	// No current output.
	// Wait for an event to signify the format capture completed
	if (FEvent* Event = GetEventForFormat(Format))
	{
		Event->Wait(MaxWaitTime);
		FreeEvent(Format, Event);
		return RequestFormat(Format, LayerIndex);
	}
	return nullptr;
}

void FPixelCaptureCapturerMultiFormat::OnDisconnected()
{
	FlushWaitingEvents();
	bDisconnected = true;
}

void FPixelCaptureCapturerMultiFormat::OnCaptureFormatComplete(int32 Format)
{
	--PendingFormats;
	CheckFormatEvent(Format); // checks if anything is waiting on this format
	if (PendingFormats == 0)
	{
		OnComplete.Broadcast();
	}
}

FEvent* FPixelCaptureCapturerMultiFormat::GetEventForFormat(int32 Format)
{
	FScopeLock Lock(&EventMutex);
	if (!bDisconnected)
	{
		FEvent* Event = FPlatformProcess::GetSynchEventFromPool();
		FormatEvents.Add(Format, Event);
		return Event;
	}
	return nullptr;
}

void FPixelCaptureCapturerMultiFormat::CheckFormatEvent(int32 Format)
{
	FScopeLock Lock(&EventMutex);
	if (FormatEvents.Contains(Format))
	{
		FEvent* Event = FormatEvents[Format];
		FormatEvents.Remove(Format);
		Event->Trigger();
	}
}

void FPixelCaptureCapturerMultiFormat::FreeEvent(int32 Format, FEvent* Event)
{
	FScopeLock Lock(&EventMutex);
	FormatEvents.Remove(Format);
	FPlatformProcess::ReturnSynchEventToPool(Event);
}

void FPixelCaptureCapturerMultiFormat::FlushWaitingEvents()
{
	FScopeLock Lock(&EventMutex);
	for (auto& KeyValue : FormatEvents)
	{
		KeyValue.Value->Trigger();
	}
	FormatEvents.Empty();
}