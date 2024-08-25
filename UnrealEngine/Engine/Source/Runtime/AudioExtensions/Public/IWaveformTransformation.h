// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "DSP/BufferVectorOperations.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "IWaveformTransformation.generated.h"

namespace Audio
{
	// information about the current state of the wave file we are transforming
	struct FWaveformTransformationWaveInfo
	{
		float SampleRate = 0.f;
		int32 NumChannels = 0;
		Audio::FAlignedFloatBuffer* Audio = nullptr;
		uint32 StartFrameOffset = 0;
		uint32 NumEditedSamples = 0;
	};

	/*
	 * Base class for the object that processes waveform data
	 * Pass tweakable variables from its paired settings UObject in the constructor in UWaveformTransformationBase::CreateTransformation
	 *
	 * note: WaveTransformation vs WaveformTransformation is to prevent UHT class name conflicts without having to namespace everything - remember this in derived classes!
	 */
	class IWaveTransformation
	{
	public:

		// Applies the transformation to the waveform and modifies WaveInfo with the resulting changes
		virtual void ProcessAudio(FWaveformTransformationWaveInfo& InOutWaveInfo) const {};
		
		virtual bool SupportsRealtimePreview() const { return false; }
		virtual bool CanChangeFileLength() const { return false; }
		virtual bool CanChangeChannelCount() const { return false; }

		virtual ~IWaveTransformation() {};
	};

	using FTransformationPtr = TUniquePtr<Audio::IWaveTransformation>;
}

// Information about the the wave file we are transforming for Transformation UObjects
struct FWaveTransformUObjectConfiguration
{
	int32 NumChannels = 0;
	float SampleRate = 0;
	float StartTime = 0.f; 
	float EndTime = -1.f; 
};

// Base class to hold editor configurable properties for an arbitrary transformation of audio waveform data
UCLASS(Abstract, EditInlineNew, MinimalAPI)
class UWaveformTransformationBase : public UObject
{
	GENERATED_BODY()

public:
	virtual Audio::FTransformationPtr CreateTransformation() const { return nullptr; }
	virtual void UpdateConfiguration(FWaveTransformUObjectConfiguration& InOutConfiguration) {};
	
	virtual bool IsEditorOnly() const override { return true; }
};

// Object that holds an ordered list of transformations to perform on a sound wave
UCLASS(EditInlineNew, MinimalAPI)
class UWaveformTransformationChain : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Instanced, Category = "Transformations")
	TArray<TObjectPtr<UWaveformTransformationBase>> Transformations;

	virtual bool IsEditorOnly() const override { return true; }
	
	AUDIOEXTENSIONS_API TArray<Audio::FTransformationPtr> CreateTransformations() const;
};
