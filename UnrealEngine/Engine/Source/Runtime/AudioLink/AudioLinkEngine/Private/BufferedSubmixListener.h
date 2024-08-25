// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BufferedListenerBase.h"
#include "ISubmixBufferListener.h"

/** Concrete Submix Buffer Listener  
*/
class FBufferedSubmixListener : public ISubmixBufferListener, public FBufferedListenerBase
{
public:
	/** Constructor
	 * @param  InDefaultCircularBufferSize: Size of the circular buffer in samples by default.
	 * @param  bInZeroInputBuffer: The passed in buffer will be set zeroed after we've buffered it.
	 * @param InName(Optional): Optional name to track listener lifetime with. 
	 */
	AUDIOLINKENGINE_API FBufferedSubmixListener(int32 InDefaultCircularBufferSize, bool bInZeroInputBuffer, const FString* InName);

	AUDIOLINKENGINE_API virtual ~FBufferedSubmixListener();

	/**
	 * Starts the Submix buffer listener by registering it with the passed in Audio Device.
	 * @param  InDevice Audio Device to register this submix listener with.
	 * @param  InOnFirstBufferCallback Delegate to fire when the format of the buffer is known.
	 *
	 * @return  success true, false otherwise.
	 */
	AUDIOLINKENGINE_API bool Start(FAudioDevice* InDevice) override;
	AUDIOLINKENGINE_API void Stop(FAudioDevice* InDevice) override;

private:
	void RegisterWithAudioDevice(FAudioDevice* InDevice);
	void UnregsiterWithAudioDevice(FAudioDevice* InDevice);
	
	//~ Begin ISubmixBufferListener
	AUDIOLINKENGINE_API void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 InNumSamples, int32 InNumChannels, const int32 InSampleRate, double) override;
	AUDIOLINKENGINE_API const FString& GetListenerName() const override;
	//~ End ISubmixBufferListener

	Audio::FDeviceId DeviceId;
	bool bZeroInputBuffer = false;
	FString Name;
};
