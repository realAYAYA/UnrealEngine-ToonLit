// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDevice.h"
#include "BufferedListenerBase.h"

/**
	FBufferedSourceListener: Concrete implementation of both a buffer listener and buffered output.
	Contains a circular buffer.
*/
class AUDIOLINKENGINE_API FBufferedSourceListener : public ISourceBufferListener, public FBufferedListenerBase
{
public:
	FBufferedSourceListener(int32 InDefaultCircularBufferSize);
	virtual ~FBufferedSourceListener();

	/**
	 * @param  InDevice Audio Device to register this listener with.
	 * @param  InOnFirstBufferCallback Delegate to fire when the format of the buffer is known.
	 *
	 * @return  success true, false otherwise.
	 */
	bool Start(FAudioDevice* InDevice) override;

	/** Stop the buffer by unregistering the buffer listener */
	void Stop(FAudioDevice* InDevice) override;
	
private:
	//~ Begin ISourceBufferListener
	void OnNewBuffer(const ISourceBufferListener::FOnNewBufferParams& InParams) override;
	void OnSourceReleased(const int32 SourceId) override;
	//~ End ISourceBufferListener

	//~ Begin IBufferedAudioOutput
	void SetBufferStreamEndDelegate(FOnBufferStreamEnd InBufferStreamEndDelegate) override;
	//~ End IBufferedAudioOutput

	std::atomic<int32> CurrentSourceId	= INDEX_NONE;		// r/w AudioMixer thread.	
	FOnBufferStreamEnd OnBufferStreamEndDelegate;
};
