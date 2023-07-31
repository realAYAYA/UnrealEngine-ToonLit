//
//  ProResFileWriter.h
//  Copyright © 2017 Apple. All rights reserved.
//

#ifndef PRORESFILEWRITER_H
#define PRORESFILEWRITER_H	1

#include "ProResFormatDescription.h"
#include "ProResMetadataWriter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueProResFileWriter *ProResFileWriterRef;

PRTypeID ProResFileWriterGetTypeID(void);

enum {
	kProResFileWriterError_AllocationFailed						= -12411,
	kProResFileWriterError_DestinationAlreadyExists				= -12412,
	kProResFileWriterError_InvalidDestinationPath				= -12416,
	kProResFileWriterError_InvalidParameter						= -12417,
	kProResFileWriterError_InvalidTime							= -12419,
	kProResFileWriterError_NotPermittedOnceFinished				= -12140,
	kProResFileWriterError_NotPermittedOnceStarted				= -12141,
	kProResFileWriterError_NotYetStarted						= -12142,
	kProResFileWriterError_OutputFileCreateFailed				= -12143,
	kProResFileWriterError_UnknownTrackID						= -12144,
	kProResFileWriterError_InvalidCodec							= -12145
};

/*!
	 @function	ProResFileWriterCreateWithPath
	 @abstract	Creates a ProResFileWriter that writes to the provided path.
	 @discussion
		Call ProResFileWriterAddTrack* to add tracks.
		Call ProResFileWriterBeginSession (optional) to mark the timestamp corresponding to the beginning of the movie.
		Call ProResFileWriterAddSampleBufferToTrack
		Call ProResFileWriterIsTrackQueueAboveHighWaterLevel to determine if your sources should be throttled back.
		Call ProResFileWriterEndSession (optional) to mark the timestamp corresponding to the end of the movie.
		Call ProResFileWriterFinish to finish writing the movie file.
		When you are done, call ProResFileWriterInvalidate to tear down the object
		and PRRelease to release your retain on it.
 */
PR_EXPORT PRStatus ProResFileWriterCreate(
	const char *destUTF8Path,
	ProResFileWriterRef *newAssetWriterOut );

/*!
	 @function	 ProResFileWriterInvalidate
	 @abstract	 Shuts down a file writer.
	 @discussion The file writer will do no more work after being invalidated.
*/
PR_EXPORT PRStatus ProResFileWriterInvalidate(
	ProResFileWriterRef writer );
	
/*!
	 @function   ProResFileWriterSetMovieTimescale
	 @abstract	 Set a track's movie timescale.
	 @discussion This property may only be set before the header is written.
*/
PR_EXPORT PRStatus ProResFileWriterSetMovieTimescale(
	ProResFileWriterRef writer,
	PRTimeScale timescale );
	
PR_EXPORT PRTimeScale ProResFileWriterGetMovieTimescale(
	ProResFileWriterRef writer );

/*!
	 @function   ProResFileWriterSetTrackMediaTimescale
	 @abstract	 Set a track's media timescale.
	 @discussion
	 Note that sound tracks must match the audio sample rate.
	 This property may only be set before any samples are written for this track.
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackMediaTimescale(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	PRTimeScale timescale );
	
PR_EXPORT PRTimeScale ProResFileWriterGetTrackMediaTimescale(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );
	
/*!
	@function	ProResFileWriterSetTrackPreferredChunkSize
	@abstract	Set the preferred size for each chunk of sample data for a given track.
	@discussion We recommend limiting the size of a video chunk to be smaller than 4 MiB for HD and 2 MiB for SD.
 
				The PRGetCompressedFrameSize() call in ProResEncoder.h can be used to obtain the target compressed 
				frame size in bytes for a given codec type and frame dimensions. For example, this call returns
				917,504 bytes for ProRes 422 HQ at 1920x1080. At 29.97 fps, a 0.5-second chunk would then be 
				15 frames * 917,504 bytes = 13.8 MB. Since this is larger than 4 MiB, a more appropriate chunk size 
				would be floor(4*2^20 / 917504) = 4 frames per chunk.
 
				Audio should be written in chunks of 0.5 second duration or longer.
	
				Both the preferred chunk size and the preferred chunk duration are active at once.
				A chunk ends after either limit is reached or crossed.
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackPreferredChunkSize(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	int32_t preferredChunkSize );

PR_EXPORT int32_t ProResFileWriterGetTrackPreferredChunkSize(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );
	
/*!
	@function	ProResFileWriterSetTrackPreferredChunkDuration
	@abstract	Set the preferred duration for each chunk of sample data for a given track.
	@discussion A chunk contains one or more samples. The total duration of the samples is no greater than
				this preferred chunk duration, or the duration of a single sample if the sample's duration
				is greater than this preferred chunk duration.
				Both the preferred chunk size and the preferred chunk duration are active at once.
				A chunk ends after either limit is reached or crossed.
				
				By default, video tracks are set to ~0.5 seconds - PRTimeMake(501, 1000); all others are set
				to one second - PRTimeMake(1, 1).
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackPreferredChunkDuration(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	PRTime preferredChunkDuration );

PR_EXPORT PRTime ProResFileWriterGetTrackPreferredChunkDuration(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );

/*!
	 @function	ProResFileWriterAddTrack
	 @abstract	Configures the file writer to add an additional track.
	 @discussion
		Returns a PRPersistentTrackID that should be used to identify the track in subsequent calls.
		By default, track visual properties such as width and height are derived from
		the first added sample.
*/
PR_EXPORT PRStatus ProResFileWriterAddTrack(
	ProResFileWriterRef writer,
	PRMediaType mediaType,
	PRPersistentTrackID *writerTrackIDOut );
	
/*!
	@function   ProResFileWriterSetTrackPreferredChunkAlignment
	@abstract	The preferred boundary for chunk alignment in bytes.
				By default, this is set to 4096 bytes for video tracks and 512 bytes for audio tracks.
				All other tracks are set to 0 bytes.
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackPreferredChunkAlignment(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	int32_t preferredChunkAlignment );
	
PR_EXPORT int32_t ProResFileWriterGetTrackPreferredChunkAlignment(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );
	
/*!
	@function   ProResFileWriterSetTrackDimensions
	@abstract	Identifies the track dimensions of a track header.
	@discussion This property can only be set before the header is written.
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackDimensions(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	PRSize dimensions );
	
PR_EXPORT PRStatus ProResFileWriterGetTrackDimensions(
	ProResFileWriterRef writer,
    PRPersistentTrackID writerTrackID,
    PRSize *trackDimensionsOut );

/*!
	@function   ProResFileWriterSetTrackInterleavingAdvance
	@abstract	An additional duration that this track should be written ahead of other tracks, when interleaving.
	@discussion By default, this is 1.0 seconds for audio tracks and zero for other tracks.
				This property may only be set before any samples are written.
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackInterleavingAdvance(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	PRTime interleavingAdvance );
	
PR_EXPORT PRTime ProResFileWriterGetTrackInterleavingAdvance(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );
	
/*!
	@function   ProResFileWriterSetTrackShouldInterleave
	@abstract	Specifies whether the track should be interleaved.
	@discussion This is true by default for all tracks except timecode tracks.
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackShouldInterleave(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	bool interleaveTrack );
	
PR_EXPORT bool ProResFileWriterGetTrackShouldInterleave(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );
	
/*!
	@function	ProResFileWriterSetTrackReferences
	@abstract	Specify a video trackID to reference a timecode trackID.
	@discussion	Each array alternates referencing PRPersistentTrackIDs and referenced PRPersistentTrackIDs.
				For example, an array [A,B,C,D] indicates that track A references track B and track C references track D.
				If there are no track references, trackIDs may be NULL.
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackReferences(
	ProResFileWriterRef writer,
	PRPersistentTrackID *trackIDs,
	PRIndex numTrackIDs );

/*!
	@function	ProResFileWriterGetTrackReferences
	@discussion	First call ProResFileWriterGetTrackReferences(writer, NULL, &numTrackIDs), then use numTrackIDs for
				trackIDs = malloc(numTrackIDs * sizeof(PRPersistentTrackID), then call
				ProResFileWriterGetTrackReferences(writer, trackIDs, NULL) a second time to get the trackIDs array.
*/
PR_EXPORT PRStatus ProResFileWriterGetTrackReferences(ProResFileWriterRef writer,
	PRPersistentTrackID *trackIDsOut,
	PRIndex *numTrackIDsOut );
	
/*!
	@function   ProResFileWriterCopyQuickTimeMetadataWriter
	@abstract	Metadata writer for adding QuickTime metadata using ProResMetadataWriterRef's routines.
	@discussion	Metadata items added to the writer will be written to the resulting QuickTime movie at the movie level.
 */
PR_EXPORT PRStatus ProResFileWriterCopyQuickTimeMetadataWriter(ProResFileWriterRef writer,
	ProResMetadataWriterRef *qtMetadataWriterOut );
	
/*!
	@function   ProResFileWriterCopyQuickTimeUserDataWriter
	@abstract	Metadata writer for adding QuickTime User Data using ProResMetadataWriterRef's routines.
	@discussion	Metadata items added to the writer will be written to the resulting QuickTime movie at the movie level.
*/
PR_EXPORT PRStatus ProResFileWriterCopyQuickTimeUserDataWriter(ProResFileWriterRef writer,
	ProResMetadataWriterRef *qtUserDataWriterOut );
	
/*!
	@function   ProResFileWriterCopyTrackQuickTimeMetadataWriter
	@abstract	Metadata writer for adding QuickTime metadata to a particular track using ProResMetadataWriterRef's
				routines.
	@discussion	Metadata items added to the writer will be written to the resulting QuickTime movie at the track level.
*/
PR_EXPORT PRStatus ProResFileWriterCopyTrackQuickTimeMetadataWriter(ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	ProResMetadataWriterRef *qtMetadataWriterOut );
	
/*!
	@function   ProResFileWriterCopyTrackQuickTimeUserDataWriter
	@abstract	Metadata writer for adding QuickTime User Data to a particular track using ProResMetadataWriterRef's
				routines.
	@discussion	Metadata items added to the writer will be written to the resulting QuickTime movie at the track level.
*/
PR_EXPORT PRStatus ProResFileWriterCopyTrackQuickTimeUserDataWriter(ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	ProResMetadataWriterRef *qtUserDataWriterOut );

/*!
	@function	ProResFileWriterBeginSession
	@abstract	Begins a sample-adding session.
	@discussion
		Sequences of samples added via ProResFileWriterAddSampleBufferToTrack are considered
		to fall within "sample-adding sessions".  You may call ProResFileWriterBeginSession
		to begin one of these sessions.
		
		Each session has a start time which defines the mapping from the timeline of source 
		samples onto the file's timeline. The first session begins at movie time 0,
		so a sample added with timestamp T will be played at movie time (T-sessionStartTime).
		Samples with timestamps before sessionStartTime will still be added to the media but will
		be edited out of the movie. If the earliest sample buffer in a track is later than
		sessionStartTime, an empty edit will be inserted to preserve synchronization between
		tracks.
 
		It is mandatory to call ProResFileWriterBeginSession;
		if ProResFileWriterAddSampleBufferToTrack is called without a prior call to
		ProResFileWriterBeginSession, it will be an error.
 
		It is an error to call ProResFileWriterBeginSession twice in a row without calling
		ProResFileWriterEndSession in between.
*/
PR_EXPORT PRStatus ProResFileWriterBeginSession(
	ProResFileWriterRef writer,
	PRTime sessionStartTime );
	
typedef struct {
	uint32_t version;
	void (*FreeMemory)(void* refCon, void* doomedMemory, size_t sizeInBytes);
	void *refCon;
} PRSampleBufferDeallocator;

/*!
	@constant kPRSampleBufferDeallocatorFree
	Predefined PRSampleBufferDeallocator structure appropriate for use when the data buffer 
	was allocated using malloc functions.
	The data buffer will be deallocated using free().
 */
PR_EXPORT const PRSampleBufferDeallocator kPRSampleBufferDeallocatorFree;
	
/*!
	 @constant kPRSampleBufferDeallocatorAlignedFree
	 Predefined PRSampleBufferDeallocator structure appropriate for use when the data buffer
	 was allocated using _aligned_malloc functions.
	 The data buffer will be deallocated using _aligned_free().
 */
PR_EXPORT const PRSampleBufferDeallocator kPRSampleBufferDeallocatorAlignedFree;

/*!
	 @typedef	PRSampleTimingInfo
	 @abstract	Collection of timing info for a sample buffer. A single PRSampleTimingInfo struct can
				describe every individual sample in a sample buffer, if the samples all have the same duration.
*/
typedef struct
{
	PRTime duration;	/*! @field duration
							The duration of the sample. If a single struct applies to
							each of the samples, they all will have this duration. */
	PRTime timeStamp;	/*! @field timeStamp
							The time at which the sample will be presented. If a single
							struct applies to each of the samples, this is the time of the
							first sample. The time of subsequent samples will be derived by
							repeatedly adding the sample duration. */
} PRSampleTimingInfo;

/*!
	 @function	ProResFileWriterAddSampleBufferToTrack
	 @abstract	Appends one or many samples to a track.
	 @discussion
		The file writer may retain the sample data and write data at a later time
		in order to improve chunking or interleaving.
		If one track gets too far ahead of the others, the file writer may flush that
		track's samples to the output file regardless of the interleaving period.

		Where applicable, sample descriptions are saved based on sample buffer format descriptions.
	 
		Samples will be stored in the order that they are provided to
		ProResFileWriterAddSampleBufferToTrack.
 
		Calls to add sample buffers must be preceded by a call to ProResFileWriterBeginSession().
*/
PR_EXPORT PRStatus ProResFileWriterAddSampleBufferToTrack(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
    void *dataBuffer,								/*! @param dataBuffer
														buffer already containing the media data. Must not be NULL.
														If the deallocator is NULL, the data buffer will be copied and disposed using standard 
														malloc/free routines.
														If the deallocator is non-NULL, the deallocator will be used for freeing the data buffer. */
	size_t dataBufferLength,						/*! @param dataBufferLength
														length of the dataBuffer. Must not be 0. */
	const PRSampleBufferDeallocator *deallocator,	/*! @param deallocator
														If non-NULL, it will be used for freeing the data buffer. It will be called once the
														sample buffer is disposed.
														If NULL, the dataBuffer will be copied and disposed using standard malloc/free routines. */
	ProResFormatDescriptionRef formatDescription,	/*! @param formatDescription
														A description of the media data's format. Cannot be NULL. The format description will be retained. */
	int64_t numSamples,								/*! @param numSamples
														Number of samples in the data buffer. Must not be 0. */
	int64_t numSampleTimingEntries,					/*! @param numSampleTimingEntries
														Number of entries in sampleTimingArray. Must be 0, 1, or numSamples. */
	const PRSampleTimingInfo * sampleTimingArray,	/*! @param sampleTimingArray
														Array of PRSampleTimingInfo structs, one struct per sample.
														If all samples have the same duration, you can pass a single PRSampleTimingInfo struct with
														duration set to the duration of one sample and timeStamp set to the time of the numerically
														earliest sample. Behaviour is undefined if multiple samples have the same timeStamp. Cannot be NULL. */
	int64_t numSampleSizeEntries,					/*! @param numSampleSizeEntries
														Number of entries in sampleSizeArray. Must be 1, or numSamples. */
	const size_t *sampleSizeArray );				/*! @param sampleSizeArray
														Array of size entries, one entry per sample. If all samples have the
														same size, you can pass a single size entry containing the size of one sample. Must be
														NULL if the samples are non-contiguous in the buffer (eg. non-interleaved audio, where the channel
														values for a single sample are scattered through the buffer). */

/*!
	 @function	ProResFileWriterAddAudioSampleBufferToTrack
	 @abstract	Appends one or many audio samples to a track.
	 @discussion
		The file writer may retain the sample data and write data at a later time
		in order to improve chunking or interleaving.
		If one track gets too far ahead of the others, the file writer may flush that
		track's samples to the output file regardless of the interleaving period.
	 
		Where applicable, sample descriptions are saved based on sample buffer format descriptions.
	 
		Samples will be stored in the order that they are provided to
		ProResFileWriterAddAudioSampleBufferToTrack.
*/
PR_EXPORT PRStatus ProResFileWriterAddAudioSampleBufferToTrack(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID,
	void *dataBuffer,								/*! @param dataBuffer
														buffer already containing the media data. Must not be NULL.
														If the deallocator is NULL, the data buffer will be copied and disposed using standard
														malloc/free routines.
														If the deallocator is non-NULL, the deallocator will be used for freeing the data buffer. */
	size_t dataBufferLength,						/*! @param dataBufferLength
														length of the dataBuffer. Must not be 0. */
    const PRSampleBufferDeallocator *deallocator,	/*! @param callback
														If non-NULL, it will be used for freeing the data buffer. It will be called once the
														sample buffer is disposed.
														If NULL, the dataBuffer will be copied and disposed using standard malloc/free routines. */
    ProResFormatDescriptionRef formatDescription,	/*! @param formatDescription
														A description of the media data's format. Cannot be NULL. Will be retained. */
	int64_t numSamples,								/*! @param numSamples
														Number of samples in the CMSampleBuffer. Must not be 0. */
	PRTime timeStamp );								/*! @param pts
														Timestamp of the first sample in the buffer. Must be a numeric PRTime. */

/*!
	@function   ProResFileWriterIsTrackQueueAboveHighWaterLevel
	@abstract   A client may use this call to govern sample buffer creation (ie, if the queue is greater
			    than the high water level, stop until the queue is below a low water level).
				High water level is set to ~ 2 seconds of data.
 
				This is generally used in conjunction with ProResFileWriterTrackQueueNowBelowLowWaterLevelCallback.
 
	@discussion It's OK if the client doesn't pay attention to water levels; it just means interleaving isn't guaranteed.
				Note that this method's return value may change from true to false asynchronously as I/O catches up.
 */
PR_EXPORT bool ProResFileWriterIsTrackQueueAboveHighWaterLevel(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );

/*! 
	@function	ProResFileWriterMarkEndOfDataForTrack
	@abstract   Can't add more sample buffers to a track after calling this.
	@discussion Once called, writer will no longer wait for this track in interleaving loop.
*/
PR_EXPORT PRStatus ProResFileWriterMarkEndOfDataForTrack(
	ProResFileWriterRef writer,
	PRPersistentTrackID writerTrackID );

/*!
	@function	ProResFileWriterEndSession
	@abstract	Concludes a sample-adding session.
	@discussion
		You may call ProResFileWriterEndSession to complete a session you began by
		calling ProResFileWriterBeginSession.
		
		The sessionEndTime defines the moment on the timeline of source samples at which
		the session ends.
 
		Each sessionStartTime...sessionEndTime pair corresponds to a period of movie time 
		into which the session's samples are inserted. Samples with later timestamps will 
		be still be added to the media but will be edited out of the movie.
 
		So if the first session has duration D1 = sessionEndTime - sessionStartTime,
		it will be inserted into the movie at movie time 0 through D1; the second session
		would be inserted into the movie at movie time D1 through D1+D2, etc.
 
		It is legal to have a session with no samples; this will cause creation of an
		empty edit of the prescribed duration.
		
		It is an error to call ProResFileWriterEndSession without a matching prior call to
		ProResFileWriterBeginSession.
		Following a call to ProResFileWriterEndSession, it is an error to call
		ProResFileWriterAddSampleBufferToTrack without first calling ProResFileWriterBeginSession.
*/
PR_EXPORT PRStatus ProResFileWriterEndSession(
	ProResFileWriterRef writer,
	PRTime sessionEndTime );

/*!
	@function	 ProResFileWriterFinish
	@abstract	 Finishes writing.
	@discussion  After calling ProResFileWriterFinish, watch for ProResFileWriterCompletedCallback 
				 and ProResFileWriterFailedCallback to know when the writing is done. Only one will be dispatched.
				 If this function returns an error, no callback will be invoked.
 */
PR_EXPORT PRStatus ProResFileWriterFinish(
	ProResFileWriterRef writer );

typedef void (*ProResFileWriterFailedCallback)(
	ProResFileWriterRef fileWriter,
	const void *refcon,
	PRStatus code );

/*!
	@function	ProResFileWriterSetWriterFailedCallback
	@abstract	This callback will be invoked if an error occurs during the file writing process.
*/
PR_EXPORT PRStatus ProResFileWriterSetWriterFailedCallback(
    ProResFileWriterRef fileWriter,
    ProResFileWriterFailedCallback callback,
    const void *refcon );

typedef void (*ProResFileWriterCompletedCallback)(
	ProResFileWriterRef fileWriter,
	const void *refcon);

/*!
	@function	ProResFileWriterSetWriterCompletedCallback
	@abstract	This callback will be invoked if the file writing completed successfully.
*/
PR_EXPORT PRStatus ProResFileWriterSetWriterCompletedCallback(
    ProResFileWriterRef fileWriter,
    ProResFileWriterCompletedCallback callback,
    const void *refcon );
	
typedef void (*ProResFileWriterTrackQueueNowBelowLowWaterLevelCallback)(
	ProResFileWriterRef fileWriter,
	const void *refcon,
	PRPersistentTrackID writerTrackID );

/*!
	@function	ProResFileWriterSetTrackQueueNowBelowLowWaterLevelCallback
	@abstract	This callback will be invoked if any of the file writer's tracks fall below the low water level. 
				High water level for each track's sample buffer queue is ~ 2 seconds; low water is ~ 1 second.
	@discussion A common way to use this when adding sample buffers from other threads:
				- Create a semaphore - ProResSemaphoreCreate(0, 1) and use it as the refcon for the callback.
				- After each call to add a video sample to ProResFileWriterAddSampleBufferToTrack(), call
				ProResFileWriterIsTrackQueueAboveHighWaterLevel() on the video track. If the video track is above 
				the high water level, check to see if all audio tracks are also above the high water level.
				- If all tracks are above high water, use the semaphore to wait - ProResSemaphoreWaitRelative(semaphore, 120 * 1000 * 1000).
				- When this callback is invoked, signal the semaphore - ProResSemaphoreSignal(semaphore) - in order
				to wake up and continue adding samples.
				- Destroy the semaphore when writing has finished - ProResSemaphoreDestroy(semaphore)
*/
PR_EXPORT PRStatus ProResFileWriterSetTrackQueueNowBelowLowWaterLevelCallback(
    ProResFileWriterRef fileWriter,
    ProResFileWriterTrackQueueNowBelowLowWaterLevelCallback callback,
    const void *refcon );
	
typedef struct OpaqueProResSemaphore *ProResSemaphoreRef;
	
PR_EXPORT ProResSemaphoreRef ProResSemaphoreCreate(int32_t initialValue, int32_t maxValue);
PR_EXPORT PRStatus ProResSemaphoreDestroy(ProResSemaphoreRef inSemaphore);
PR_EXPORT PRStatus ProResSemaphoreSignal(ProResSemaphoreRef inSemaphore);
PR_EXPORT PRStatus ProResSemaphoreWaitRelative(ProResSemaphoreRef inSemaphore, int64_t timeoutInNanoseconds);
	
#ifdef __cplusplus
}
#endif

#endif // PRORESFILEWRITER_H
