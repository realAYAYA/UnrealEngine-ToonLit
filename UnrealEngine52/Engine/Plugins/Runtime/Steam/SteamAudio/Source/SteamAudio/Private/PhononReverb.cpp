//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononReverb.h"
#include "SteamAudioModule.h"
#include "SteamAudioSettings.h"
#include "PhononReverbSourceSettings.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundSubmix.h"
#include "DSP/Dsp.h"
#include "Misc/ScopeLock.h"
#include "PhononPluginManager.h"
#include "SteamAudioEnvironment.h"

#include "AudioDevice.h"

namespace SteamAudio
{
	//==============================================================================================================================================
	// FPhononReverb
	//==============================================================================================================================================

	FPhononReverb::FPhononReverb()
		: BinauralRenderer(nullptr)
		, IndirectBinauralEffect(nullptr)
		, IndirectPanningEffect(nullptr)
		, ReverbConvolutionEffect(nullptr)
		, AmbisonicsChannels(0)
		, IndirectOutDeinterleaved(nullptr)
		, bListenerInitialized(false)
		, CachedSpatializationMethod(EIplSpatializationMethod::PANNING)
		, Environment(nullptr)
	{
	}

	FPhononReverb::~FPhononReverb()
	{
		for (auto& ReverbSource : ReverbSources)
		{
			if (ReverbSource.ConvolutionEffect)
			{
				iplDestroyConvolutionEffect(&ReverbSource.ConvolutionEffect);
			}
		}

		if (ReverbConvolutionEffect)
		{
			iplDestroyConvolutionEffect(&ReverbConvolutionEffect);
		}

		if (IndirectBinauralEffect)
		{
			iplDestroyAmbisonicsBinauralEffect(&IndirectBinauralEffect);
		}

		if (IndirectPanningEffect)
		{
			iplDestroyAmbisonicsPanningEffect(&IndirectPanningEffect);
		}

		if (BinauralRenderer)
		{
			iplDestroyBinauralRenderer(&BinauralRenderer);
		}

		if (IndirectOutDeinterleaved)
		{
			for (int32 i = 0; i < AmbisonicsChannels; ++i)
			{
				delete[] IndirectOutDeinterleaved[i];
			}
			delete[] IndirectOutDeinterleaved;
			IndirectOutDeinterleaved = nullptr;
		}
	}

	// Just makes a copy of the init params - actual initialization needs to be deferred until the environment is created.
	// This is because we do not know if we should fall back to Phonon settings from the TAN overrides until the compute
	// device has been created.
	void FPhononReverb::Initialize(const FAudioPluginInitializationParams InitializationParams)
	{
		bListenerInitialized = false;
		AudioPluginInitializationParams = InitializationParams;

		InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		InputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		InputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		InputAudioFormat.numSpeakers = 1;
		InputAudioFormat.speakerDirections = nullptr;
		InputAudioFormat.ambisonicsOrder = -1;
		InputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		InputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		ReverbSources.SetNum(AudioPluginInitializationParams.NumSources);
		for (FReverbSource& ReverbSource : ReverbSources)
		{
			ReverbSource.InBuffer.format = InputAudioFormat;
			ReverbSource.InBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		}
	}

	void FPhononReverb::SetEnvironment(FEnvironment* InEnvironment)
	{
		if (!InEnvironment)
		{
			return;
		}

		Environment = InEnvironment;

		const int32 IndirectImpulseResponseOrder = InEnvironment->GetSimulationSettings().ambisonicsOrder;
		AmbisonicsChannels = (IndirectImpulseResponseOrder + 1) * (IndirectImpulseResponseOrder + 1);

		ReverbInputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		ReverbInputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		ReverbInputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		ReverbInputAudioFormat.numSpeakers = 2;
		ReverbInputAudioFormat.speakerDirections = nullptr;
		ReverbInputAudioFormat.ambisonicsOrder = -1;
		ReverbInputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		ReverbInputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		IndirectOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		IndirectOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS;
		IndirectOutputAudioFormat.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;
		IndirectOutputAudioFormat.numSpeakers = (IndirectImpulseResponseOrder + 1) * (IndirectImpulseResponseOrder + 1);
		IndirectOutputAudioFormat.speakerDirections = nullptr;
		IndirectOutputAudioFormat.ambisonicsOrder = IndirectImpulseResponseOrder;
		IndirectOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		IndirectOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		// Assume stereo output - if wrong, will be dynamically changed in the mixer processing
		BinauralOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		BinauralOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		BinauralOutputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		BinauralOutputAudioFormat.numSpeakers = 2;
		BinauralOutputAudioFormat.speakerDirections = nullptr;
		BinauralOutputAudioFormat.ambisonicsOrder = -1;
		BinauralOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		BinauralOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		IPLHrtfParams HrtfParams;
		HrtfParams.hrtfData = nullptr;
		HrtfParams.sofaFileName = IPLstring("");
		HrtfParams.type = IPL_HRTFDATABASETYPE_DEFAULT;

		// The binaural renderer always uses Phonon convolution even if TAN is available.
		IPLRenderingSettings BinauralRenderingSettings = Environment->GetRenderingSettings();
		BinauralRenderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;

		iplCreateBinauralRenderer(SteamAudio::GlobalContext, BinauralRenderingSettings, HrtfParams, &BinauralRenderer);

		CachedSpatializationMethod = GetDefault<USteamAudioSettings>()->IndirectSpatializationMethod;

		switch (CachedSpatializationMethod)
		{
		case EIplSpatializationMethod::HRTF:
			iplCreateAmbisonicsBinauralEffect(BinauralRenderer, IndirectOutputAudioFormat, BinauralOutputAudioFormat, &IndirectBinauralEffect);
			break;
		case EIplSpatializationMethod::PANNING:
			iplCreateAmbisonicsPanningEffect(BinauralRenderer, IndirectOutputAudioFormat, BinauralOutputAudioFormat, &IndirectPanningEffect);
			break;
		}

		IndirectOutDeinterleaved = new IPLfloat32*[AmbisonicsChannels];
		for (int32 i = 0; i < AmbisonicsChannels; ++i)
		{
			IndirectOutDeinterleaved[i] = new IPLfloat32[AudioPluginInitializationParams.BufferLength];
		}

		IndirectIntermediateBuffer.format = IndirectOutputAudioFormat;
		IndirectIntermediateBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		IndirectIntermediateBuffer.interleavedBuffer = nullptr;
		IndirectIntermediateBuffer.deinterleavedBuffer = IndirectOutDeinterleaved;

		DryBuffer.format = ReverbInputAudioFormat;
		DryBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		DryBuffer.interleavedBuffer = nullptr;
		DryBuffer.deinterleavedBuffer = nullptr;

		IndirectOutArray.SetNumZeroed(AudioPluginInitializationParams.BufferLength * BinauralOutputAudioFormat.numSpeakers);
		IndirectOutBuffer.format = BinauralOutputAudioFormat;
		IndirectOutBuffer.numSamples = AudioPluginInitializationParams.BufferLength;
		IndirectOutBuffer.interleavedBuffer = IndirectOutArray.GetData();
		IndirectOutBuffer.deinterleavedBuffer = nullptr;
	}

	void FPhononReverb::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UReverbPluginSourceSettingsBase* InSettings)
	{
		if (!Environment || !Environment->GetEnvironmentalRenderer())
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to find environmental renderer for reverb. Reverb will not be applied. Make sure to export the scene."));
			return;
		}

		FString SourceString = AudioComponentUserId.ToString().ToLower();
		IPLBakedDataIdentifier SourceIdentifier;
		SourceIdentifier.type = IPL_BAKEDDATATYPE_STATICSOURCE;
		SourceIdentifier.identifier = Environment->GetBakedIdentifierMap().Get(SourceString);

		UE_LOG(LogSteamAudio, Log, TEXT("Creating reverb effect for %s"), *SourceString);

		UPhononReverbSourceSettings* Settings = static_cast<UPhononReverbSourceSettings*>(InSettings);
		FReverbSource& ReverbSource = ReverbSources[SourceId];

		ReverbSource.IndirectContribution = Settings->SourceReverbContribution;
		ReverbSource.DipolePower = 0.0f;
		ReverbSource.DipoleWeight = 0.0f;

		InputAudioFormat.numSpeakers = NumChannels;
		switch (NumChannels)
		{
			case 1: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO; break;
			case 2: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO; break;
			case 4: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC; break;
			case 6: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE; break;
			case 8: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE; break;
		}

		ReverbSource.InBuffer.format = InputAudioFormat;

		switch (Settings->SourceReverbSimulationType)
		{
		case EIplSimulationType::BAKED:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), SourceIdentifier, IPL_SIMTYPE_BAKED, InputAudioFormat, IndirectOutputAudioFormat,
				&ReverbSource.ConvolutionEffect);
			break;
		case EIplSimulationType::REALTIME:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), SourceIdentifier, IPL_SIMTYPE_REALTIME, InputAudioFormat, IndirectOutputAudioFormat,
				&ReverbSource.ConvolutionEffect);
			break;
		case EIplSimulationType::DISABLED:
		default:
			break;
		}
	}

	void FPhononReverb::OnReleaseSource(const uint32 SourceId)
	{
		UE_LOG(LogSteamAudio, Log, TEXT("Destroying reverb effect."));

		check((int32)SourceId < ReverbSources.Num());

		if (ReverbSources[SourceId].ConvolutionEffect)
		{
			iplDestroyConvolutionEffect(&ReverbSources[SourceId].ConvolutionEffect);
		}
	}

	void FPhononReverb::ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
		if (!Environment || !Environment->GetEnvironmentalRenderer() || !Environment->GetEnvironmentCriticalSectionHandle())
		{
			return;
		}

		FScopeLock EnvironmentLock(Environment->GetEnvironmentCriticalSectionHandle());

		FReverbSource& ReverbSource = ReverbSources[InputData.SourceId];

		IPLSource SourceData;
		SourceData.position = SteamAudio::UnrealToPhononIPLVector3(InputData.SpatializationParams->EmitterWorldPosition);
		SourceData.ahead = SteamAudio::UnrealToPhononIPLVector3(InputData.SpatializationParams->EmitterWorldRotation * FVector::ForwardVector);
		SourceData.right = SteamAudio::UnrealToPhononIPLVector3(InputData.SpatializationParams->EmitterWorldRotation * FVector::RightVector);
		SourceData.up = SteamAudio::UnrealToPhononIPLVector3(InputData.SpatializationParams->EmitterWorldRotation * FVector::UpVector);
		SourceData.directivity = IPLDirectivity{ ReverbSource.DipolePower, ReverbSource.DipoleWeight, nullptr, nullptr };

		if (ReverbSource.ConvolutionEffect)
		{
			ReverbSource.IndirectInArray.SetNumUninitialized(InputData.AudioBuffer->Num());
			for (int32 i = 0; i < InputData.AudioBuffer->Num(); ++i)
			{
				ReverbSource.IndirectInArray[i] = (*InputData.AudioBuffer)[i] * ReverbSource.IndirectContribution;
			}
			ReverbSource.InBuffer.interleavedBuffer = ReverbSource.IndirectInArray.GetData();

			iplSetDryAudioForConvolutionEffect(ReverbSource.ConvolutionEffect, SourceData, ReverbSource.InBuffer);
		}
	}

	void FPhononReverb::ProcessMixedAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
	{
		if (!Environment || !Environment->GetEnvironmentalRenderer() || !Environment->GetEnvironmentCriticalSectionHandle() || !bListenerInitialized)
		{
			return;
		}

		FScopeLock EnvironmentLock(Environment->GetEnvironmentCriticalSectionHandle());

		if (IndirectOutBuffer.format.numSpeakers != OutData.NumChannels)
		{
			if (IndirectBinauralEffect)
			{
				iplDestroyAmbisonicsBinauralEffect(&IndirectBinauralEffect);
			}

			if (IndirectPanningEffect)
			{
				iplDestroyAmbisonicsPanningEffect(&IndirectPanningEffect);
			}

			IndirectOutBuffer.format.numSpeakers = OutData.NumChannels;
			switch (OutData.NumChannels)
			{
				case 1: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_MONO; break;
				case 2: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_STEREO; break;
				case 4: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC; break;
				case 6: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE; break;
				case 8: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE; break;
			}

			IndirectOutArray.SetNumZeroed(OutData.AudioBuffer->Num());
			IndirectOutBuffer.interleavedBuffer = IndirectOutArray.GetData();

			switch (CachedSpatializationMethod)
			{
			case EIplSpatializationMethod::HRTF:
				iplCreateAmbisonicsBinauralEffect(BinauralRenderer, IndirectOutputAudioFormat, IndirectOutBuffer.format, &IndirectBinauralEffect);
				break;
			case EIplSpatializationMethod::PANNING:
				iplCreateAmbisonicsPanningEffect(BinauralRenderer, IndirectOutputAudioFormat, IndirectOutBuffer.format, &IndirectPanningEffect);
				break;
			}
		}

		if (ReverbConvolutionEffect)
		{
			float ListenerReverbContribution = GetDefault<USteamAudioSettings>()->ListenerReverbContribution;
			ReverbIndirectInArray.SetNumUninitialized(InData.AudioBuffer->Num());
			for (int32 i = 0; i < InData.AudioBuffer->Num(); ++i)
			{
				ReverbIndirectInArray[i] = (*InData.AudioBuffer)[i] * ListenerReverbContribution;
			}

			IPLSource ReverbSource = {};
			ReverbSource.position = ListenerPosition;
			ReverbSource.ahead = ListenerForward;
			ReverbSource.up = ListenerUp;
			ReverbSource.directivity = IPLDirectivity{ 0.0f, 0.0f, nullptr, nullptr };

			DryBuffer.interleavedBuffer = ReverbIndirectInArray.GetData();
			iplSetDryAudioForConvolutionEffect(ReverbConvolutionEffect, ReverbSource, DryBuffer);
		}

		iplGetMixedEnvironmentalAudio(Environment->GetEnvironmentalRenderer(), ListenerPosition, ListenerForward, ListenerUp, IndirectIntermediateBuffer);

		switch (CachedSpatializationMethod)
		{
		case EIplSpatializationMethod::HRTF:
			iplApplyAmbisonicsBinauralEffect(IndirectBinauralEffect, BinauralRenderer, IndirectIntermediateBuffer, IndirectOutBuffer);
			break;
		case EIplSpatializationMethod::PANNING:
			iplApplyAmbisonicsPanningEffect(IndirectPanningEffect, BinauralRenderer, IndirectIntermediateBuffer, IndirectOutBuffer);
			break;
		}

		FMemory::Memcpy(OutData.AudioBuffer->GetData(), IndirectOutArray.GetData(), sizeof(float) * IndirectOutArray.Num());
	}

	void FPhononReverb::CreateReverbEffect()
	{
		check(Environment && Environment->GetEnvironmentalRenderer() && Environment->GetEnvironmentCriticalSectionHandle());

		IPLBakedDataIdentifier ReverbIdentifier;
		ReverbIdentifier.type = IPL_BAKEDDATATYPE_REVERB;
		ReverbIdentifier.identifier = 0;

		switch (GetDefault<USteamAudioSettings>()->ListenerReverbSimulationType)
		{
		case EIplSimulationType::BAKED:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), ReverbIdentifier, IPL_SIMTYPE_BAKED, ReverbInputAudioFormat, IndirectOutputAudioFormat,
				&ReverbConvolutionEffect);
			break;
		case EIplSimulationType::REALTIME:
			iplCreateConvolutionEffect(Environment->GetEnvironmentalRenderer(), ReverbIdentifier, IPL_SIMTYPE_REALTIME, ReverbInputAudioFormat, IndirectOutputAudioFormat,
				&ReverbConvolutionEffect);
			break;
		case EIplSimulationType::DISABLED:
		default:
			break;
		}
	}

	void FPhononReverb::UpdateListener(const FVector& Position, const FVector& Forward, const FVector& Up, const FVector& Right)
	{
		ListenerPosition = SteamAudio::UnrealToPhononIPLVector3(Position);
		ListenerForward = SteamAudio::UnrealToPhononIPLVector3(Forward, false);
		ListenerUp = SteamAudio::UnrealToPhononIPLVector3(Up, false);
		ListenerRight = SteamAudio::UnrealToPhononIPLVector3(Right, false);
		bListenerInitialized = true;
	}

	FSoundEffectSubmixPtr FPhononReverb::GetEffectSubmix()
	{
		if (!SubmixEffect.IsValid())
		{
			USubmixEffectReverbPluginPreset* ReverbPluginPreset = nullptr;
			USoundSubmix* Submix = GetSubmix();
			if (Submix->SubmixEffectChain.Num() > 0)
			{
				if (USubmixEffectReverbPluginPreset* CurrentPreset = Cast<USubmixEffectReverbPluginPreset>(Submix->SubmixEffectChain[0]))
				{
					ReverbPluginPreset = CurrentPreset;
				}
			}

			if (!ReverbPluginPreset)
			{
				ReverbPluginPreset = NewObject<USubmixEffectReverbPluginPreset>(Submix, TEXT("Reverb Plugin Effect Preset"));
			}

			SubmixEffect = USoundEffectPreset::CreateInstance<FSoundEffectSubmixInitData, FSoundEffectSubmix>(FSoundEffectSubmixInitData(), *ReverbPluginPreset);
			StaticCastSharedPtr<FSubmixEffectReverbPlugin, FSoundEffectSubmix, ESPMode::ThreadSafe>(SubmixEffect)->SetPhononReverbPlugin(this);
			SubmixEffect->SetEnabled(true);
		}

		return SubmixEffect;
	}

	USoundSubmix* FPhononReverb::GetSubmix()
	{
		const USteamAudioSettings* Settings = GetDefault<USteamAudioSettings>();
		check(Settings);
		
		if (!ReverbSubmix.IsValid())
		{
			ReverbSubmix = Cast<USoundSubmix>(Settings->OutputSubmix.TryLoad());
		}

		if (!ReverbSubmix.IsValid())
		{
			static const FString DefaultSubmixName = TEXT("Phonon Reverb Submix");
			UE_LOG(LogSteamAudio, Error, TEXT("Failed to load Phonon Reverb Submix from object path '%s' in PhononSettings. Creating '%s' as stub."),
				*Settings->OutputSubmix.GetAssetPathString(),
				*DefaultSubmixName);

			ReverbSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), *DefaultSubmixName);
			ReverbSubmix->bMuteWhenBackgrounded = true;
		}
		ReverbSubmix->bAutoDisable = false;

		bool bFoundPreset = false;
		for (USoundEffectSubmixPreset* Preset : ReverbSubmix->SubmixEffectChain)
		{
			if (USubmixEffectReverbPluginPreset* PluginPreset = Cast<USubmixEffectReverbPluginPreset>(Preset))
			{
				bFoundPreset = true;
				break;
			}
		}

		if (!bFoundPreset)
		{
			static const FString DefaultPresetName = TEXT("PhononReverbDefault_0");
			UE_LOG(LogSteamAudio, Error, TEXT("Failed to find Phonon USubmixEffectReverbPluginPreset on default Phonon Reverb Submix. Creating stub '%s'."),
				*Settings->OutputSubmix.GetAssetPathString(),
				*DefaultPresetName);
			ReverbSubmix->SubmixEffectChain.Add(NewObject<USubmixEffectReverbPluginPreset>(USubmixEffectReverbPluginPreset::StaticClass(), *DefaultPresetName));
		}

		return ReverbSubmix.Get();
	}

	//==============================================================================================================================================
	// FReverbSource
	//==============================================================================================================================================

	FReverbSource::FReverbSource()
		: ConvolutionEffect(nullptr)
		, IndirectContribution(1.0f)
		, DipolePower(0.0f)
		, DipoleWeight(0.0f)
	{
	}

}

//==================================================================================================================================================
// FSubmixEffectReverbPlugin
//==================================================================================================================================================

FSubmixEffectReverbPlugin::FSubmixEffectReverbPlugin()
	: PhononReverbPlugin(nullptr)
{}

void FSubmixEffectReverbPlugin::Init(const FSoundEffectSubmixInitData& InInitData)
{
}

void FSubmixEffectReverbPlugin::OnPresetChanged()
{
}

uint32 FSubmixEffectReverbPlugin::GetDesiredInputChannelCountOverride() const
{
	return 2;
}

void FSubmixEffectReverbPlugin::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	if (PhononReverbPlugin)
	{
		PhononReverbPlugin->ProcessMixedAudio(InData, OutData);
	}
}

void FSubmixEffectReverbPlugin::SetPhononReverbPlugin(SteamAudio::FPhononReverb* InPhononReverbPlugin)
{
	PhononReverbPlugin = InPhononReverbPlugin;
}
