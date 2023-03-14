// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "StaticMeshResources.h"
#include "NiagaraDataInterface.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "DSP/MultithreadedPatching.h"
#include "NiagaraDataInterfaceAudio.h"
#include "NiagaraDataInterfaceAudioOscilloscope.generated.h"

struct FNiagaraDataInterfaceProxyOscilloscope : public FNiagaraDataInterfaceProxy
{
	FNiagaraDataInterfaceProxyOscilloscope(int32 InResolution, float InScopeInMillseconds);

	~FNiagaraDataInterfaceProxyOscilloscope();

	void OnBeginDestroy();

	/**
	 * Sample vertical displacement from the oscilloscope buffer.
	 * @param NormalizedPositionInBuffer Horizontal position in the Oscilloscope buffer, from 0.0 to 1.0.
	 * @param Channel channel index.
	 * @return Amplitude at this position.
	 */
	float SampleAudio(float NormalizedPositionInBuffer, int32 Channel, int32 NumFramesInBuffer, int32 NumChannelsInBuffer);

	/**
	 * @return the number of channels in the buffer.
	 */
	int32 GetNumChannels();

	// Called when the Submix property changes.
	void OnUpdateSubmix(USoundSubmix* Submix);

	void RegisterToAllAudioDevices();
	void UnregisterFromAllAudioDevices(USoundSubmix* Submix);

	// Called when Resolution or Zoom are changed.
	void OnUpdateResampling(int32 InResolution, float InScopeInMilliseconds);

	// This function enqueues a render thread command to decimate the pop audio off of the SubmixListener, downsample it, and post it to the GPUAudioBuffer.
	void PostAudioToGPU();
	FReadBuffer& ComputeAndPostSRV();

	// This function pops audio and downsamples it to our specified resolution. Returns the number of frames of audio in the downsampled buffer.
	int32 DownsampleAudioToBuffer();

	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}

private:
	void OnNewDeviceCreated(Audio::FDeviceId InID);
	void OnDeviceDestroyed(Audio::FDeviceId InID);

	TMap<Audio::FDeviceId, TUniquePtr<FNiagaraSubmixListener>> SubmixListeners;

	// This mixer is patched into by all instances of FNiagaraSubmixListener in the SubmixListeners map, and is consumed by DownsampleAudioToBuffer().
	Audio::FPatchMixer PatchMixer;

	USoundSubmix* SubmixRegisteredTo;
	bool bIsSubmixListenerRegistered;

	int32 Resolution;
	float ScopeInMilliseconds;

	// The buffer we downsample PoppedBuffer to based on the Resolution property.
	Audio::FAlignedFloatBuffer PopBuffer;
	Audio::FAlignedFloatBuffer DownsampledBuffer;

	// Handle for the SRV used by the generated HLSL.
	FReadBuffer GPUDownsampledBuffer;
	FThreadSafeCounter NumChannelsInDownsampledBuffer;
	
	// Buffer read by VectorVM worker threads. This vector is guaranteed to not be mutated during the VectorVM tasks.
	Audio::FAlignedFloatBuffer VectorVMReadBuffer;

	FDelegateHandle DeviceCreatedHandle;
	FDelegateHandle DeviceDestroyedHandle;

	FCriticalSection DownsampleBufferLock;
};

/** Data Interface allowing sampling of recent audio data. */
UCLASS(EditInlineNew, Category = "Audio", meta = (DisplayName = "Audio Oscilloscope"))
class NIAGARA_API UNiagaraDataInterfaceAudioOscilloscope : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(int,				NumChannels)
		SHADER_PARAMETER_SRV(Buffer<float>,	AudioBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	UPROPERTY(EditAnywhere, Category = "Oscilloscope")
	TObjectPtr<USoundSubmix> Submix;

	static const int32 MaxBufferResolution = 8192;

	// The number of samples of audio to pass to the GPU. Audio will be resampled to fit this resolution.
	// Increasing this number will increase the resolution of the waveform, but will also increase usage of the GPU memory bus,
	// potentially causing issues across the application.
	UPROPERTY(EditAnywhere, Category = "Oscilloscope", AdvancedDisplay, meta = (ClampMin = "64", ClampMax = "8192"))
	int32 Resolution;

	// The number of milliseconds of audio to show.
	UPROPERTY(EditAnywhere, Category = "Oscilloscope", meta = (ClampMin = "5.0", ClampMax = "400.0"))
	float ScopeInMilliseconds;

	//VM function overrides:
	void SampleAudio(FVectorVMExternalFunctionContext& Context);
	void GetNumChannels(FVectorVMExternalFunctionContext& Context);

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override
	{
		return true;
	}

	virtual bool RequiresDistanceFieldData() const override { return false; }

#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
};

