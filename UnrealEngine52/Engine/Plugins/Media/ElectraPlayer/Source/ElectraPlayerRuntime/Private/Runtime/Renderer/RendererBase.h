// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "PlayerTime.h"
#include "ParameterDictionary.h"

#include "MediaDecoderOutput.h"

namespace Electra
{

	class IMediaRenderClock
	{
	public:
		virtual ~IMediaRenderClock() = default;

		enum class ERendererType
		{
			Video,
			Audio,
			Subtitles,
		};

		/**
		 * Called by the renderer to set the time of the most sample that has been output most recently.
		 *
		 * @param ForRenderer
		 *               Identifies the type of renderer from which the time is set.
		 * @param CurrentRenderTime
		 *               The time of the sample most recently output.
		 */
		virtual void SetCurrentTime(ERendererType ForRenderer, const FTimeValue& CurrentRenderTime) = 0;

		/**
		 * Gets the current _interpolated_ sample output time from the last
		 * time SetCurrentTime() was called plus the elapsed time since then.
		 *
		 * @param FromRenderer
		 *               Identifies the type of renderer for which to get the time.
		 *
		 * @return Interpolated render time
		 */
		virtual FTimeValue GetInterpolatedRenderTime(ERendererType FromRenderer) = 0;
	};





	class IMediaRenderer : public IDecoderOutputOwner
	{
	public:
		virtual ~IMediaRenderer() = default;

		/**
		 * Buffer interface
		 */
		class IBuffer
		{
		public:
			virtual ~IBuffer() = default;

			/**
			 * Returns the properties of the buffer.
			 *
			 * @return Const reference to the dictionary containing buffer properties.
			 */
			virtual const FParamDict& GetBufferProperties() const = 0;
			
			// Same, but returns the writable dictionary. Use with caution.
			virtual FParamDict& GetMutableBufferProperties() = 0;
		};


		//=================================================================================================================
		// Methods called from both a decoder and the player from their respective threads.
		//

		/**
		 * Returns the properties of the buffer pool. Those properties should not change
		 * during the lifetime of the pool.
		 * A mandatory property is the maximum number of buffers that can be obtained from
		 * the pool ("max_buffers":int64).
		 *
		 * @return Const reference to the dictionary containing buffer pool properties.
		 */
		virtual const FParamDict& GetBufferPoolProperties() const = 0;



		//=================================================================================================================
		// Methods called from a decoder (and from within the decoder thread)
		//

		/**
		 * Create a buffer pool from where a decoder can get the block of memory to decode into.
		 *
		 * @param Parameters Dictionary with create options depending on the type of
		 *                   render buffers to create (video or audio)
		 *
		 * @return One of UEMEDIA_ERROR_OK, UEMEDIA_ERROR_BAD_ARGUMENTS or UEMEDIA_ERROR_OOM (or others if warranted).
		 */
		virtual UEMediaError CreateBufferPool(const FParamDict& Parameters) = 0;

		/**
		 * Asks for a sample buffer from the buffer pool created previously through CreateBufferPool().
		 *
		 * @param OutBuffer Receives the address of the returned IBuffer
		 * @param TimeoutInMicroseconds
		 *                  Maximum time to wait for a buffer to become available before giving up.
		 *                  If 0 a "peek" if a buffer is available is to be performed and a buffer
		 *                  returned if one is available.
		 *                  No value indicates an infinite wait time!
		 * @param InParameters
		 *                  Optional parameters for buffer properties if different from those to CreateBufferPool().
		 *
		 * @return One of UEMEDIA_ERROR_OK, UEMEDIA_ERROR_BAD_ARGUMENTS or UEMEDIA_ERROR_INSUFFICIENT_DATA.
		 *         Other values if warranted but those will cause a playback error.
		 */
		virtual UEMediaError AcquireBuffer(IBuffer*& OutBuffer, int32 TimeoutInMicroseconds, const FParamDict& InParameters) = 0;

		/**
		 * Releases the buffer for rendering and subsequent return to the buffer pool.
		 *
		 * @param Buffer  The buffer being returned to the renderer for rendering.
		 * @param bRender true to render the contents of the buffer, false to discard them.
		 * @param InSampleProperties
		 *                Properties of the decoded sample (eg. width and height for video, sample rate for audio)
		 *
		 * @return Should be UEMEDIA_ERROR_OK only. Anything else will cause a playback error (like UEMEDIA_ERROR_BAD_ARGUMENTS for bad properties)
		 */
		virtual UEMediaError ReturnBuffer(IBuffer* Buffer, bool bRender, const FParamDict& InSampleProperties) = 0;

		/**
		 * Informs that the decoder is done with this pool. This only indicates no calls to
		 * GetBuffer() and ReleaseBuffer() will be made by this decoder instance any more.
		 * It is up to the renderer to decide whether or not to destroy the pool or keep it
		 * around for re-use with a new decoder instance (the next CreateBufferPool() call).
		 *
		 * @return Should be UEMEDIA_ERROR_OK only. Anything else will cause a playback error.
		 */
		virtual UEMediaError ReleaseBufferPool() = 0;


		/**
		 * Returns if the renderer's output queues are ready to receive any output from a decode
		 *
		 * @return True if ready, false if decode should be delayed.
		 */
		virtual bool CanReceiveOutputFrames(uint64 NumFrames) const = 0;

		//=================================================================================================================
		// Methods called from the player (and from within the player thread)
		//

		/**
		 * Sets the render clock which this renderer needs to update with the presentation time
		 * of the most recently rendered sample.
		 *
		 * @param RenderClock
		 *               Render clock to update with the most recent output sample time.
		 */
		virtual void SetRenderClock(TSharedPtr<IMediaRenderClock, ESPMode::ThreadSafe> RenderClock) = 0;


		/**
		 * Called if this renderer is being wrapped by another renderer.
		 */
		virtual void SetParentRenderer(TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> ParentRenderer) = 0;

		/**
		 * Sets the next expected sample's approximate presentation time stamp.
		 * This is called as data buffering begins at the indicated play start
		 * position. The actual timestamp of the first sample will vary.
		 * The renderer can ignore this value and rely only on the timestamp
		 * of the decoded sample.
		 *
		 * @param NextApproxPTS
		 *               Approximate timestamp at which playback is about to commence.
		 */
		virtual void SetNextApproximatePresentationTime(const FTimeValue& NextApproxPTS) = 0;

		/**
		 * Flushes all pending buffers not yet rendered.
		 * There should be no outstanding buffers the decoder has not yet released.
		 * Options may indicate to hold the current frame on-screen (for video) or
		 * to tear it down.
		 *
		 * @param InOptions Options indicating flush operation, like keeping the current video frame on-screen.
		 *
		 * @return Should be UEMEDIA_ERROR_OK only.
		 *         If there are still frames a decoder has not returned yet UEMEDIA_ERROR_INTERNAL should be returned.
		 *         Any failure will cause a playback error.
		 */
		virtual UEMediaError Flush(const FParamDict& InOptions) = 0;

		/**
		 * Begins rendering of the first sample buffer.
		 *
		 * @param InOptions Reserved for future use.
		 */
		virtual void StartRendering(const FParamDict& InOptions) = 0;

		/**
		 * Stops rendering of sample buffers.
		 *
		 * @param InOptions Reserved for future use.
		 */
		virtual void StopRendering(const FParamDict& InOptions) = 0;

		/**
		 * Work any regular tasks on the output buffer pool
		 *
		 * @note Expected to be called as needed from (normally) the decoder worker thread
		 */
		virtual void TickOutputBufferPool() {};
	};

} // namespace Electra




