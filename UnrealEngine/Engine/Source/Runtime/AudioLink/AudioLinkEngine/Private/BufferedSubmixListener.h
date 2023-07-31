// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BufferedListenerBase.h"
#include "ISubmixBufferListener.h"

/** Concrete Submix Buffer Listener  
*/
class AUDIOLINKENGINE_API FBufferedSubmixListener : public ISubmixBufferListener, 
													public FBufferedListenerBase
{
public:
	/** Constructor
	 * @param  InDefaultCircularBufferSize: Size of the circular buffer in samples by default.
	 * @param  bInZeroInputBuffer: The passed in buffer will be set zerod after we've buffered it. Not 
	 */
	FBufferedSubmixListener(int32 InDefaultCircularBufferSize, bool bInZeroInputBuffer);

	virtual ~FBufferedSubmixListener();

	/**
	 * Starts the Submix buffer listener by registering it with the passed in Audio Device.
	 * @param  InDevice Audio Device to register this submix listener with.
	 * @param  InOnFirstBufferCallback Delegate to fire when the format of the buffer is known.
	 *
	 * @return  success true, false otherwise.
	 */
	bool Start(FAudioDevice* InDevice) override;
	void Stop(FAudioDevice* InDevice) override;

private:
	void RegisterWithAudioDevice(FAudioDevice* InDevice);
	void UnregsiterWithAudioDevice(FAudioDevice* InDevice);
	
	//~ Begin ISubmixBufferListener
	void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 InNumSamples, int32 InNumChannels, const int32 InSampleRate, double) override;
	//~ End ISubmixBufferListener

	Audio::FDeviceId DeviceId;
	bool bZeroInputBuffer = false;
};