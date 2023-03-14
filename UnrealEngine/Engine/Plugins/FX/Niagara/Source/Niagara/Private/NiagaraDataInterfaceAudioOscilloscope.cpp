// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceAudioOscilloscope.h"
#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "AudioResampler.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
#include "ClearQuad.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceAudioOscilloscope)

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceGridAudioOscilloscope"


// Global VM function names, also used by the shaders code generation methods.
static const FName SampleAudioBufferFunctionName("SampleAudioBuffer");
static const FName GetAudioBufferNumChannelsFunctionName("GetAudioBufferNumChannels");

// Global variable prefixes, used in HLSL parameter declarations.
static const FString AudioBufferName(TEXT("_AudioBuffer"));
static const FString NumChannelsName(TEXT("_NumChannels"));

FNiagaraDataInterfaceProxyOscilloscope::FNiagaraDataInterfaceProxyOscilloscope(int32 InResolution, float InScopeInMillseconds)
	: PatchMixer()
	, SubmixRegisteredTo(nullptr)
	, bIsSubmixListenerRegistered(false)
	, Resolution(InResolution)
	, ScopeInMilliseconds(InScopeInMillseconds)
	, NumChannelsInDownsampledBuffer(0)
{
	DeviceCreatedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddRaw(this, &FNiagaraDataInterfaceProxyOscilloscope::OnNewDeviceCreated);
	DeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddRaw(this, &FNiagaraDataInterfaceProxyOscilloscope::OnDeviceDestroyed);

	VectorVMReadBuffer.Reset();
	VectorVMReadBuffer.AddZeroed(Resolution * AUDIO_MIXER_MAX_OUTPUT_CHANNELS);
}

FNiagaraDataInterfaceProxyOscilloscope::~FNiagaraDataInterfaceProxyOscilloscope()
{
	check(IsInRenderingThread());
	GPUDownsampledBuffer.Release();

	FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
	FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(DeviceDestroyedHandle);
}

void FNiagaraDataInterfaceProxyOscilloscope::RegisterToAllAudioDevices()
{
	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		// Register a new submix listener for every audio device that currently exists.
		DeviceManager->IterateOverAllDevices([&](Audio::FDeviceId DeviceId, FAudioDevice* InDevice)
		{
			check(!SubmixListeners.Contains(DeviceId));
			const int32 NumSamplesToPop = Align(FMath::FloorToInt(ScopeInMilliseconds / 1000.0f * InDevice->GetSampleRate()) * AUDIO_MIXER_MAX_OUTPUT_CHANNELS, 4);
			SubmixListeners.Emplace(DeviceId, new FNiagaraSubmixListener(PatchMixer, NumSamplesToPop, DeviceId, SubmixRegisteredTo));
			SubmixListeners[DeviceId]->RegisterToSubmix();
		});
	}

	bIsSubmixListenerRegistered = true;
}

void FNiagaraDataInterfaceProxyOscilloscope::UnregisterFromAllAudioDevices(USoundSubmix* Submix)
{
	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		// Register a new submix listener for every audio device that currently exists.
		DeviceManager->IterateOverAllDevices([&](Audio::FDeviceId DeviceId, FAudioDevice* InDevice)
		{
			check(SubmixListeners.Contains(DeviceId));
			SubmixListeners.Remove(DeviceId);
		});
	}

	ensure(SubmixListeners.Num() == 0);
	SubmixListeners.Reset();
}

void FNiagaraDataInterfaceProxyOscilloscope::OnUpdateSubmix(USoundSubmix* Submix)
{
	if (bIsSubmixListenerRegistered)
	{
		UnregisterFromAllAudioDevices(Submix);
	}

	SubmixRegisteredTo = Submix;

	RegisterToAllAudioDevices();
	
}

void FNiagaraDataInterfaceProxyOscilloscope::OnNewDeviceCreated(Audio::FDeviceId InID)
{
	if (bIsSubmixListenerRegistered)
	{
		check(!SubmixListeners.Contains(InID));

		FAudioDevice * DeviceHandle = FAudioDeviceManager::Get()->GetAudioDeviceRaw(InID);
		if (ensure(DeviceHandle))
		{
			const int32 NumSamplesToPop = Align(FMath::FloorToInt(ScopeInMilliseconds / 1000.0f * DeviceHandle->GetSampleRate()) * AUDIO_MIXER_MAX_OUTPUT_CHANNELS, 4);
			SubmixListeners.Emplace(InID, new FNiagaraSubmixListener(PatchMixer, NumSamplesToPop, InID, SubmixRegisteredTo));
			SubmixListeners[InID]->RegisterToSubmix();
		}
	}
}

void FNiagaraDataInterfaceProxyOscilloscope::OnDeviceDestroyed(Audio::FDeviceId InID)
{
	if (bIsSubmixListenerRegistered)
	{
		if (SubmixListeners.Contains(InID))
		{
			SubmixListeners.Remove(InID);
		}
	}
}

UNiagaraDataInterfaceAudioOscilloscope::UNiagaraDataInterfaceAudioOscilloscope(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Submix(nullptr)
	, Resolution(512)
	, ScopeInMilliseconds(20.0f)
	
{
	Proxy = TUniquePtr<FNiagaraDataInterfaceProxyOscilloscope>(new FNiagaraDataInterfaceProxyOscilloscope(Resolution, ScopeInMilliseconds));
}

float FNiagaraDataInterfaceProxyOscilloscope::SampleAudio(float NormalizedPositionInBuffer, int32 ChannelIndex, int32 NumSamplesInBuffer, int32 NumChannelsInBuffer)
{
	if (NumSamplesInBuffer == 0 || NumChannelsInBuffer == 0)
	{
		return 0.0f;
	}

	NormalizedPositionInBuffer = FMath::Clamp(NormalizedPositionInBuffer, 0.0f, 1.0f - SMALL_NUMBER);
	ChannelIndex = FMath::Clamp(ChannelIndex, 0, NumChannelsInBuffer);

	float FrameIndex = NormalizedPositionInBuffer * (NumSamplesInBuffer / NumChannelsInBuffer);
	int32 LowerFrameIndex = FMath::FloorToInt(FrameIndex);
	float LowerFrameAmplitude = VectorVMReadBuffer[LowerFrameIndex * NumChannelsInBuffer + ChannelIndex];
	int32 HigherFrameIndex = LowerFrameIndex < NumSamplesInBuffer ? LowerFrameIndex + 1 : LowerFrameIndex;
	float HigherFrameAmplitude = VectorVMReadBuffer[HigherFrameIndex * NumChannelsInBuffer + ChannelIndex];
	float Fraction = HigherFrameIndex - FrameIndex;
	return FMath::Lerp<float>(LowerFrameAmplitude, HigherFrameAmplitude, Fraction);
}

void UNiagaraDataInterfaceAudioOscilloscope::SampleAudio(FVectorVMExternalFunctionContext& Context)
{
	const int32 NumSamplesInDownsampledBuffer = GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->DownsampleAudioToBuffer();

	VectorVM::FExternalFuncInputHandler<float> InNormalizedPos(Context);
	VectorVM::FExternalFuncInputHandler<int32> InChannel(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutAmplitude(Context);

	const int32 NumChannelsInDownsampledBuffer = GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->GetNumChannels();

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		float Position = InNormalizedPos.Get();
		int32 Channel = InChannel.Get();
		*OutAmplitude.GetDest() = GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->SampleAudio(Position, Channel, NumSamplesInDownsampledBuffer, NumChannelsInDownsampledBuffer);

		InNormalizedPos.Advance();
		InChannel.Advance();
		OutAmplitude.Advance();
	}
}

int32 FNiagaraDataInterfaceProxyOscilloscope::GetNumChannels()
{
	return NumChannelsInDownsampledBuffer.GetValue();
}

void UNiagaraDataInterfaceAudioOscilloscope::GetNumChannels(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutChannel(Context);

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		*OutChannel.GetDestAndAdvance() = GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->GetNumChannels();
	}
}

void UNiagaraDataInterfaceAudioOscilloscope::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	Super::GetFunctions(OutFunctions);

	{
		FNiagaraFunctionSignature SampleAudioBufferSignature;
		SampleAudioBufferSignature.Name = SampleAudioBufferFunctionName;
		SampleAudioBufferSignature.Inputs.Add(FNiagaraVariable(GetClass(), TEXT("Oscilloscope")));
		SampleAudioBufferSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("NormalizedPositionInBuffer")));
		SampleAudioBufferSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ChannelIndex")));
		SampleAudioBufferSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Amplitude")));

		SampleAudioBufferSignature.bMemberFunction = true;
		SampleAudioBufferSignature.bRequiresContext = false;
		OutFunctions.Add(SampleAudioBufferSignature);
	}

	{
		FNiagaraFunctionSignature NumChannelsSignature;
		NumChannelsSignature.Name = GetAudioBufferNumChannelsFunctionName;
		NumChannelsSignature.Inputs.Add(FNiagaraVariable(GetClass(), TEXT("Oscilloscope")));
		NumChannelsSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumChannels")));

		NumChannelsSignature.bMemberFunction = true;
		NumChannelsSignature.bRequiresContext = false;
		OutFunctions.Add(NumChannelsSignature);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceAudioOscilloscope, SampleAudio);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceAudioOscilloscope, GetNumChannels);

void UNiagaraDataInterfaceAudioOscilloscope::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	if (BindingInfo.Name == SampleAudioBufferFunctionName)
	{
		//TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceAudioOscilloscope, SampleAudio)>::Bind(this, BindingInfo, InstanceData, OutFunc);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceAudioOscilloscope, SampleAudio)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetAudioBufferNumChannelsFunctionName)
	{
		//TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceAudioOscilloscope, GetNumChannels)>::Bind(this, BindingInfo, InstanceData, OutFunc);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceAudioOscilloscope, GetNumChannels)::Bind(this, OutFunc);
	}
	else
	{
		ensureMsgf(false, TEXT("Error! Function defined for this class but not bound."));
	}
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceAudioOscilloscope::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	bSuccess &= InVisitor->UpdateShaderParameters<FShaderParameters>();
	return bSuccess;
}

bool UNiagaraDataInterfaceAudioOscilloscope::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	bool ParentRet = Super::GetFunctionHLSL(ParamInfo, FunctionInfo, FunctionInstanceIndex, OutHLSL);
	if (ParentRet)
	{
		return true;
	}
	else if (FunctionInfo.DefinitionName == SampleAudioBufferFunctionName)
	{
		// See SampleBuffer(float InNormalizedPosition, int32 InChannel)
		/**
		 * float FrameIndex = NormalizedPositionInBuffer * DownsampledBuffer.Num();
		 * int32 LowerFrameIndex = FMath::FloorToInt(FrameIndex);
		 * float LowerFrameAmplitude = DownsampledBuffer[LowerFrameIndex * NumChannelsInDownsampledBuffer + ChannelIndex];
		 * int32 HigherFrameIndex = FMath::CeilToInt(FrameIndex);
		 * float HigherFrameAmplitude = DownsampledBuffer[HigherFrameIndex * NumChannelsInDownsampledBuffer + ChannelIndex];
		 * float Fraction = HigherFrameIndex - FrameIndex;
		 * return FMath::Lerp<float>(LowerFrameAmplitude, HigherFrameAmplitude, Fraction);
		 */
		static const TCHAR *FormatBounds = TEXT(R"(
			void {FunctionName}(float In_NormalizedPosition, int In_ChannelIndex, out float Out_Val)
			{
				float FrameIndex = In_NormalizedPosition * {AudioBufferNumSamples} / {ChannelCount};
				int LowerIndex = floor(FrameIndex);
				int UpperIndex =  LowerIndex < {AudioBufferNumSamples} ? LowerIndex + 1.0 : LowerIndex;
				float Fraction = FrameIndex - LowerIndex;
				float LowerValue = {AudioBuffer}.Load(LowerIndex * {ChannelCount} + In_ChannelIndex);
				float UpperValue = {AudioBuffer}.Load(UpperIndex * {ChannelCount} + In_ChannelIndex);
				Out_Val = lerp(LowerValue, UpperValue, Fraction);
			}
		)");
		TMap<FString, FStringFormatArg> ArgsBounds = {
			{TEXT("FunctionName"), FStringFormatArg(FunctionInfo.InstanceName)},
			{TEXT("ChannelCount"), FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + NumChannelsName)},
			{TEXT("AudioBufferNumSamples"), FStringFormatArg(Resolution)},
			{TEXT("AudioBuffer"), FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + AudioBufferName)},
		};
		OutHLSL += FString::Format(FormatBounds, ArgsBounds);
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetAudioBufferNumChannelsFunctionName)
	{
		// See GetNumChannels()
		static const TCHAR *FormatBounds = TEXT(R"(
			void {FunctionName}(out int Out_Val)
			{
				Out_Val = {ChannelCount};
			}
		)");
		TMap<FString, FStringFormatArg> ArgsBounds = {
			{TEXT("FunctionName"), FStringFormatArg(FunctionInfo.InstanceName)},
			{TEXT("ChannelCount"), FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + NumChannelsName)},
		};
		OutHLSL += FString::Format(FormatBounds, ArgsBounds);
		return true;
	}
	else
	{
		return false;
	}
}

void UNiagaraDataInterfaceAudioOscilloscope::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	Super::GetParameterDefinitionHLSL(ParamInfo, OutHLSL);

	OutHLSL.Appendf(TEXT("int %s%s;\n"), *ParamInfo.DataInterfaceHLSLSymbol, *NumChannelsName);
	OutHLSL.Appendf(TEXT("Buffer<float> %s%s;\n"), *ParamInfo.DataInterfaceHLSLSymbol, *AudioBufferName);

}
#endif

void UNiagaraDataInterfaceAudioOscilloscope::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UNiagaraDataInterfaceAudioOscilloscope::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNiagaraDataInterfaceProxyOscilloscope& DIProxy = Context.GetProxy<FNiagaraDataInterfaceProxyOscilloscope>();
	FReadBuffer& AudioBufferSRV = DIProxy.ComputeAndPostSRV();

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();
	ShaderParameters->NumChannels = DIProxy.GetNumChannels();
	ShaderParameters->AudioBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(AudioBufferSRV.SRV);
}

void FNiagaraDataInterfaceProxyOscilloscope::OnUpdateResampling(int32 InResolution, float InScopeInMilliseconds)
{
	Resolution = InResolution;
	ScopeInMilliseconds = InScopeInMilliseconds;

	const int32 NumSamplesInBuffer = Resolution * NumChannelsInDownsampledBuffer.GetValue();
	if (NumSamplesInBuffer)
	{
		ENQUEUE_RENDER_COMMAND(FUpdateDIAudioBuffer)(
			[&, NumSamplesInBuffer](FRHICommandListImmediate& RHICmdList)
		{
			if (GPUDownsampledBuffer.NumBytes > 0)
			{
				GPUDownsampledBuffer.Release();
			}

			GPUDownsampledBuffer.Initialize(TEXT("FNiagaraDataInterfaceProxyOscilloscope_GPUDownsampledBuffer"), sizeof(float), NumSamplesInBuffer, EPixelFormat::PF_R32_FLOAT, BUF_Static);
		});

		VectorVMReadBuffer.SetNumZeroed(Resolution * AUDIO_MIXER_MAX_OUTPUT_CHANNELS);
	}
}

FReadBuffer& FNiagaraDataInterfaceProxyOscilloscope::ComputeAndPostSRV()
{
	// Copy to GPUDownsampledBuffer:
	PostAudioToGPU();
	return GPUDownsampledBuffer;
}

#if WITH_EDITOR
void UNiagaraDataInterfaceAudioOscilloscope::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	static FName SubmixFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioOscilloscope, Submix);
	static FName ResolutionFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioOscilloscope, Resolution);
	static FName ScopeInMillisecondsFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioOscilloscope, ScopeInMilliseconds);

	// Regenerate on save any compressed sound formats or if analysis needs to be re-done
	if (FProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		const FName& Name = PropertyThatChanged->GetFName();
		if (Name == SubmixFName)
		{
			GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateSubmix(Submix);
		}
		else if (Name == ResolutionFName || Name == ScopeInMillisecondsFName)
		{
			GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateResampling(Resolution, ScopeInMilliseconds);
		}
	}
}
#endif //WITH_EDITOR

void UNiagaraDataInterfaceAudioOscilloscope::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
	else if (!HasAnyFlags(RF_NeedLoad) && !HasAnyFlags(RF_Transactional))
	{
		GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateResampling(Resolution, ScopeInMilliseconds);
		GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateSubmix(Submix);
	}
}

void FNiagaraDataInterfaceProxyOscilloscope::OnBeginDestroy()
{
	if (bIsSubmixListenerRegistered)
	{
		UnregisterFromAllAudioDevices(SubmixRegisteredTo);
	}
}

void UNiagaraDataInterfaceAudioOscilloscope::BeginDestroy()
{
	GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnBeginDestroy();

	Super::BeginDestroy();
}

void UNiagaraDataInterfaceAudioOscilloscope::PostLoad()
{
	Super::PostLoad();
	GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateResampling(Resolution, ScopeInMilliseconds);
	GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateSubmix(Submix);
}

bool UNiagaraDataInterfaceAudioOscilloscope::Equals(const UNiagaraDataInterface* Other) const
{
	const UNiagaraDataInterfaceAudioOscilloscope* CastedOther = Cast<const UNiagaraDataInterfaceAudioOscilloscope>(Other);
	return Super::Equals(Other)
		&& (CastedOther->Submix == Submix)
		&& (CastedOther->Resolution == Resolution)
		&& FMath::IsNearlyEqual(CastedOther->ScopeInMilliseconds, ScopeInMilliseconds);
}

bool UNiagaraDataInterfaceAudioOscilloscope::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	Super::CopyToInternal(Destination);

	UNiagaraDataInterfaceAudioOscilloscope* CastedDestination = Cast<UNiagaraDataInterfaceAudioOscilloscope>(Destination);

	if (CastedDestination)
	{
		CastedDestination->Submix = Submix;
		CastedDestination->Resolution = Resolution;
		CastedDestination->ScopeInMilliseconds = ScopeInMilliseconds;

		CastedDestination->GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateResampling(Resolution, ScopeInMilliseconds);
		CastedDestination->GetProxyAs<FNiagaraDataInterfaceProxyOscilloscope>()->OnUpdateSubmix(Submix);
	}
	
	return true;
}

void FNiagaraDataInterfaceProxyOscilloscope::PostAudioToGPU()
{
	ENQUEUE_RENDER_COMMAND(FUpdateDIAudioBuffer)(
		[&](FRHICommandListImmediate& RHICmdList)
	{
		DownsampleAudioToBuffer();
		size_t BufferSize = DownsampledBuffer.Num() * sizeof(float);
		if (BufferSize != 0 && !GPUDownsampledBuffer.NumBytes)
		{
			GPUDownsampledBuffer.Initialize(TEXT("GPUDownsampledBuffer"), sizeof(float), Resolution * NumChannelsInDownsampledBuffer.GetValue(), EPixelFormat::PF_R32_FLOAT, BUF_Static);
		}

		if (GPUDownsampledBuffer.NumBytes > 0)
		{
			float *BufferData = static_cast<float*>(RHILockBuffer(GPUDownsampledBuffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
			FScopeLock ScopeLock(&DownsampleBufferLock);
			FPlatformMemory::Memcpy(BufferData, DownsampledBuffer.GetData(), BufferSize);
			RHIUnlockBuffer(GPUDownsampledBuffer.Buffer);
		}
	});
}

int32 FNiagaraDataInterfaceProxyOscilloscope::DownsampleAudioToBuffer()
{
	FScopeLock ScopeLock(&DownsampleBufferLock);

	// Get the number of channels from the first listener we find.
	// If the num channels hasn't been set, either SubmixListeners is empty
	// or we haven't pushed audio to any of them yet.
	float SourceSampleRate = 0.0f;
	int32 NumChannels = 0;

	for (auto& Listener : SubmixListeners)
	{
		NumChannels = Listener.Value->GetNumChannels();
		SourceSampleRate = Listener.Value->GetSampleRate();

		if (NumChannels != 0)
		{
			break;
		}
	}

	if (NumChannels == 0 || FMath::IsNearlyZero(SourceSampleRate))
	{
		return 0;
	}

	check(NumChannels > 0);
	NumChannelsInDownsampledBuffer.Set(NumChannels);

	// Get the number of frames of audio in the original sample rate to show.
	int32 NumFramesToPop = Align(FMath::FloorToInt(SourceSampleRate * (ScopeInMilliseconds / 1000.0f)), 4);

	// If we have enough frames buffered to pop, update the PopBuffer. Otherwise, use the previous frames buffer.
	int32 NumSamplesToPop = NumFramesToPop * NumChannels;

	if (PopBuffer.Num() != NumSamplesToPop)
	{
		PopBuffer.Reset();
		PopBuffer.AddZeroed(NumSamplesToPop);
	}

	if (PatchMixer.MaxNumberOfSamplesThatCanBePopped() >= NumSamplesToPop)
	{
		PatchMixer.PopAudio(PopBuffer.GetData(), NumSamplesToPop, true);
	}

	// Downsample buffer in place.
	float SampleRateRatio = ((float)Resolution) / ((float)NumFramesToPop);
	float DestinationSampleRate = SourceSampleRate * SampleRateRatio;

	Audio::FResamplingParameters ResampleParameters = {
		Audio::EResamplingMethod::Linear,
		NumChannels,
		SourceSampleRate,
		DestinationSampleRate,
		PopBuffer
	};

	int32 DownsampleBufferSize = Audio::GetOutputBufferSize(ResampleParameters);
	DownsampledBuffer.Reset();
	DownsampledBuffer.AddZeroed(DownsampleBufferSize);


	Audio::FResamplerResults ResampleResults;
	ResampleResults.OutBuffer = &DownsampledBuffer;

	bool bResampleResult = Audio::Resample(ResampleParameters, ResampleResults);
	check(bResampleResult);

	// here we may have an extra sample or two due to roundoff.
	DownsampledBuffer.SetNumZeroed(Resolution * NumChannels);

	check(DownsampledBuffer.Num() <= VectorVMReadBuffer.Num());
	FMemory::Memcpy(VectorVMReadBuffer.GetData(), DownsampledBuffer.GetData(), VectorVMReadBuffer.Num());

	return Resolution;
}

#undef LOCTEXT_NAMESPACE

