//
//  ProResFileReader.h
//  Copyright © 2017 Apple. All rights reserved.
//

#ifndef PRORESFILEREADER_H
#define PRORESFILEREADER_H  1

#include <stdio.h>
#include <stdbool.h>

#include "ProResFormatDescription.h"
#include "ProResMetadataReader.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#pragma pack(push, 4)

/*!
	@typedef	ProResFileReaderRef
	@abstract	A ProResFileReaderRef provides access to the media samples stored in a QuickTime file whose video format is ProRes.
    @discussion It is a PRType object.
 */
typedef struct opaqueProResFileReader *ProResFileReaderRef;

/*!
	@typedef	ProResTrackReaderRef
	@abstract	A ProResTrackReaderRef provides access to a set of related tracks.
    @discussion Used to get access to the track layout of a particular media source.
                It is a PRType object.
 */
typedef struct opaqueProResTrackReader *ProResTrackReaderRef;

/*!
	@typedef	ProResEditCursorRef
	@abstract	A cursor provided by a ProResTrackReaderRef.
	@discussion A ProResEditCursor always identifies a particular edit segment in the sequence;
                cursors may not be positioned "before the start" or "after the end" of edits.
                If a client attempts to move a cursor outside the sequence of edits,
                the cursor should be set to point to the first or last edit (as appropriate).

                A ProResEditCursor is a PRType and should be PRReleased when the client is done with it.
 */
typedef struct opaqueProResEditCursor *ProResEditCursorRef;

/*!
	@typedef	ProResSampleCursorRef
	@abstract	A cursor provided by a ProResTrackReaderRef.
	@discussion A ProResSampleCursor always identifies a particular sample in the sequence;
                cursors may not be positioned "before the start" or "after the end" of samples.
                If a client attempts to move a cursor outside the sequence of samples,
                the cursor should be set to point to the first or last sample (as appropriate).

                Cursors may be established at various timestamps or ends of the sequence, may be compared,
                may be stepped forward and back, and may be used to obtain information about the samples
                they point at.

                A ProResSampleCursorRef is a PRType object and should be PRReleased when the client is done with it.
 */
typedef struct opaqueProResSampleCursor *ProResSampleCursorRef;

/*!
	@function	ProResFileReaderCreate
	@abstract	Given a valid utf8Path to a QuickTime file whose video format is ProRes, returns a retained ProResFileReaderRef.
                The client must release the ProResFileReaderRef object when done.
 */
PR_EXPORT PRStatus ProResFileReaderCreate(
    const char *utf8Path,
    ProResFileReaderRef *fileReaderOut);

PR_EXPORT PRTime ProResFileReaderGetDuration(
    ProResFileReaderRef fileReader);
    
PR_EXPORT PRTimeScale ProResFileReaderGetTimescale(
    ProResFileReaderRef fileReader);

PR_EXPORT PRMatrix ProResFileReaderGetMovieMatrix(
    ProResFileReaderRef fileReader);
    
/*!
    @function	ProResFileReaderGetCreationTime
    @abstract	The creation time stored in the media header subtracted by the seconds since January 1, 1904.
*/
PR_EXPORT uint64_t ProResFileReaderGetCreationTime(
    ProResFileReaderRef fileReader);
    
PR_EXPORT float ProResFileReaderGetPreferredRate(
    ProResFileReaderRef fileReader);
    
PR_EXPORT float ProResFileReaderGetPreferredVolume(
    ProResFileReaderRef fileReader);
	
/*!
	@function   ProResFileReaderCopyQuickTimeMetadataReader
	@abstract	Provides a retained ProResMetadataReader that represents the movie's 'mdta' atom.
				If no metadata atom exists, returns kProResBaseObjectError_ValueNotAvailable.
	@discussion The client must release the metadata reader when finished.
*/
PR_EXPORT PRStatus ProResFileReaderCopyQuickTimeMetadataReader(
    ProResFileReaderRef fileReader,
    ProResMetadataReaderRef *qtMetadataReaderOut);
	
/*!
	@function   ProResFileReaderCopyQuickTimeUserDataReader
	@abstract	Provides a retained ProResMetadataReader that represents the movie's 'udta' atom.
				If no user data atom exists, returns kProResBaseObjectError_ValueNotAvailable.
	@discussion The client must release the metadata reader when finished.
*/
PR_EXPORT PRStatus ProResFileReaderCopyQuickTimeUserDataReader(
	ProResFileReaderRef fileReader,
	ProResMetadataReaderRef *qtUserDataReaderOut);

/*!
	@function	ProResFileReaderGetTrackCount
	@abstract	Returns the number of tracks available.
 */
PR_EXPORT PRStatus ProResFileReaderGetTrackCount(
    ProResFileReaderRef fileReader,
    PRIndex *trackCountOut);

/*!
	@function	ProResFileReaderCopyTrackByIndex
	@abstract	Given a zero-based track index, returns a retained ProResTrackReaderRef, mediaType, and ID for the track.
	@discussion	A ProResTrackReaderRef object can be obtained for the specified track. The client must release
                the object when done. Can also return the media type and persistent track ID number for the track.
                Will return an error if the track index is out of bounds.
 */
PR_EXPORT PRStatus ProResFileReaderCopyTrackByIndex(
    ProResFileReaderRef fileReader,
    PRIndex trackIndex,
    ProResTrackReaderRef *retainedTrackOut,  // may be NULL
    PRMediaType *mediaTypeOut,  // may be NULL
    PRPersistentTrackID *persistentTrackIDOut);  // may be NULL

/*!
	@function	ProResFileReaderCopyTrackByID
	@abstract	Given a persistent track ID, returns a retained ProResTrackReaderRef, and mediaType for the track.
	@discussion	A ProResTrackReaderRef object can be obtained for the specified track. The client must release
                the object when done. Can also return the media type for the track.
                Will return an error if the track ID is unknown.
 */
PR_EXPORT PRStatus ProResFileReaderCopyTrackByID(
    ProResFileReaderRef fileReader,
    PRPersistentTrackID persistentTrackID,
    ProResTrackReaderRef *retainedTrackOut,  // may be NULL
    PRMediaType *mediaTypeOut);

/*!
	@function	ProResFileReaderCopyTrackByType
	@abstract	Given a media type and zero-based index, returns a retained ProResTrackReader and/or trackID for the
                indexed track of the given type. In other words, if there are three video tracks and five total tracks,
				trackIndexes of 0, 1, 2 are valid for kPRMediaType_Video and will return the first, second, and third
				video tracks, respectively, regardless of whether they're interspersed with other non-video tracks.
	@discussion	A ProResTrackReaderRef object can be obtained for the specified track. The client must release
                the object when done. Can also return the media type for the track.
                Will return an error if a track of the given type and index is not present.
 */
PR_EXPORT PRStatus ProResFileReaderCopyTrackByType(
    ProResFileReaderRef fileReader,
    PRIndex trackIndex,
    PRMediaType mediaType,
    ProResTrackReaderRef *retainedTrackOut,  // may be NULL
    PRPersistentTrackID *persistentTrackIDOut);

/*!
	@function   ProResTrackReaderCopyQuickTimeMetadataReader
	@abstract	Provides a retained ProResMetadataReader that represents the 'mdta' atom in the track.
				If no metadata atom exists, returns kProResBaseObjectError_ValueNotAvailable.
	@discussion The client must release the metadata reader when finished.
*/
PR_EXPORT PRStatus ProResTrackReaderCopyQuickTimeMetadataReader(
	ProResTrackReaderRef trackReader,
	ProResMetadataReaderRef *qtMetadataReaderOut);

/*!
	@function   ProResTrackReaderCopyQuickTimeUserDataReader
	@abstract	Provides a retained ProResMetadataReader that represents the 'udta' atom in the track.
				If no user data atom exists, returns kProResBaseObjectError_ValueNotAvailable.
	@discussion The client must release the metadata reader when finished.
*/
PR_EXPORT PRStatus ProResTrackReaderCopyQuickTimeUserDataReader(
	ProResTrackReaderRef trackReader,
	ProResMetadataReaderRef *qtUserDataReaderOut);

/*!
    @function   ProResTrackReaderCopyFormatDescription
    @result     The format description for the track.
	@discussion	The client must release the format description when done.
 */
PR_EXPORT ProResFormatDescriptionRef ProResTrackReaderCopyFormatDescription(
	ProResTrackReaderRef trackReader);

PR_EXPORT PRTimeScale ProResTrackReaderGetTimescale(
    ProResTrackReaderRef trackReader);

/*!
	@function   ProResTrackReaderGetUneditedDuration
	@result     The duration of the track, sans edits.
 */
PR_EXPORT PRTime ProResTrackReaderGetUneditedDuration(
    ProResTrackReaderRef trackReader);

/*!
	@function   ProResTrackReaderGetUneditedNumDataBytes
	@result     The total number of sample bytes of the track, sans edits.
 */
PR_EXPORT int64_t ProResTrackReaderGetUneditedNumDataBytes(
    ProResTrackReaderRef trackReader);

/*!
    @function   ProResTrackReaderGetUneditedSampleCount
    @result     The total number of samples in the track, sans edits.
 */
PR_EXPORT int32_t ProResTrackReaderGetUneditedSampleCount(
    ProResTrackReaderRef trackReader);

/*!
	@function   ProResTrackReaderIsTrackEnabled
	@result     Indicates whether the track is enabled in the file.
 */
PR_EXPORT bool ProResTrackReaderIsTrackEnabled(
    ProResTrackReaderRef trackReader);

/*!
	@function   ProResTrackReaderGetLayer
	@result     The visual layer of the track.
 */
PR_EXPORT int16_t	ProResTrackReaderGetLayer(
    ProResTrackReaderRef trackReader);

/*!
	@function   ProResTrackReaderGetAlternateGroupID
	@result     The ID of the alternate track group to which this track belongs.
                A value of zero indicates the track is not a member of any alternate group.
 */
PR_EXPORT int16_t ProResTrackReaderGetAlternateGroupID(
    ProResTrackReaderRef trackReader);

/*!
	@function   ProResTrackReaderTimecodeMustBeShown
	@result     Indicates whether timecode is set to be displayed.
 */
PR_EXPORT bool ProResTrackReaderTimecodeMustBeShown(
    ProResTrackReaderRef trackReader);
    
PR_EXPORT float ProResTrackReaderGetVolume(
    ProResTrackReaderRef trackReader);

PR_EXPORT PRSize ProResTrackReaderGetDimensions(
    ProResTrackReaderRef trackReader);

PR_EXPORT bool ProResTrackReaderHasCleanApertureDimensions(
    ProResTrackReaderRef trackReader);
PR_EXPORT PRSize ProResTrackReaderGetCleanApertureDimensions(
    ProResTrackReaderRef trackReader);

PR_EXPORT bool ProResTrackReaderHasProductionApertureDimensions(
    ProResTrackReaderRef trackReader);
PR_EXPORT PRSize ProResTrackReaderGetProductionApertureDimensions(
    ProResTrackReaderRef trackReader);

PR_EXPORT bool ProResTrackReaderHasEncodedPixelsDimensions(
    ProResTrackReaderRef trackReader);
PR_EXPORT PRSize ProResTrackReaderGetEncodedPixelsDimensions(
    ProResTrackReaderRef trackReader);

PR_EXPORT PRMatrix ProResTrackReaderGetMatrix(
    ProResTrackReaderRef trackReader);

/*!
	@function	ProResTrackReaderGetTrackInfo
	@abstract	Returns information about a media track.
	@discussion	Track ID is used to uniquely identify the track within a ProResFileReader.
	
	@param trackReader			The track reader.
	@param persistentTrackIDOut	Return param for ID of the track.
	@param mediaTypeOut			Return param for mediaType of the track.
 */
PR_EXPORT PRStatus ProResTrackReaderGetTrackInfo(
    ProResTrackReaderRef trackReader,
    PRPersistentTrackID *persistentTrackIDOut,
    PRMediaType* mediaTypeOut);

/*!
	@function	 ProResTrackReaderCreateEditCursorAtTrackTime
    @abstract	 Creates a cursor object pointing to the edit segment at a given track time.
    @discussion  The new cursor should point to the last edit segment with trackStart less than or equal to
                 trackTime, or, if there are no such edit segments, the first edit segment.
                 If there are no edit segments at all, it shall return kPREditCursorNoEditsErr.
                 The caller can detect the "beforeStart" and "afterEnd" cases by examining the edit segment
                 the returned cursor is positioned at:

                 if (trackTime < editSegment.trackStart)
                 beforeStart = true;
                 else if (trackTime >= (editSegment.trackStart + editSegment.trackDuration))
                 afterEnd = true;

                 Callers can use trackTimes kPRTimePositiveInfinity and kPRTimeNegativeInfinity to create cursors
                 at the first and last edit segments respectively.
    @param	newCursorOut
                 Points to a ProResEditCursor to receive the new cursor.
                 The client is responsible for releasing the cursor by calling PRRelease when
                 it is done with it.
    @result      Returns noErr in case of success.  Returns kPREditCursorNoEditsErr if there are no edit segments.
 */
PR_EXPORT PRStatus ProResTrackReaderCreateEditCursorAtTrackTime(
    ProResTrackReaderRef trackReader,
    PRTime trackTime,
    ProResEditCursorRef *newCursorOut);

/*!
	@function	ProResEditCursorCopy
    @abstract	Creates a cursor object identical to an existing cursor object.
    @discussion Further manipulation of the new cursor shall not have any effect on the original cursor.
    @param      cursor
                The cursor to copy.
    @param      newCursorOut
                Points to a ProResEditCursor to receive the new cursor.
                The client is responsible for releasing the new cursor by calling PRRelease when
                it is done with it.
 */
PR_EXPORT PRStatus ProResEditCursorCopy(
    ProResEditCursorRef cursor,
    ProResEditCursorRef *newCursorOut);

/*!
	@function	ProResEditCursorStep
    @abstract	Moves the cursor a given number of edit segments.
    @discussion If the request would advance the cursor past the last edit segment or before the first edit seegment,
                the cursor shall be set to point to that limiting edit segment and the method shall
                return kPREditCursorHitEndErr.
    @param      stepCount
                The number of edit segments to move.
                If positive, step forward this many edit segments.
                If negative, step backward (-stepCount) edit segments.
 */
PR_EXPORT PRStatus ProResEditCursorStep(
    ProResEditCursorRef cursor,
    int32_t stepCount);

/*!
    @function	ProResEditCursorGetEditSegment
    @abstract	Retrieves the edit segment pointed at by the cursor.
    @discussion The end track time of one edit segment must exactly match the start track time of the next one;
                there must not be gaps between edit segments on the track timeline.
 */
PR_EXPORT PRStatus ProResEditCursorGetEditSegment(
    ProResEditCursorRef cursor,
    PRTimeMapping *editSegmentOut);

/*!
	@function	ProResTrackReaderCreateSampleCursorAtTimeStamp
    @abstract	Creates a cursor object pointing to the sample at a given timeStamp.
    @discussion The new cursor should point to the last sample with time less than or equal to
                timeStamp, or, if there are no such samples, the first sample.
    @param  newCursorOut
                Points to a ProResSampleCursor to receive the new cursor.
                The client is responsible for releasing the cursor by calling PRRelease when
                it is done with it.
    @param  beforeStartOut
                Points to a bool that will be set to true if timeStamp is before the
                first sample, false otherwise.
                The client should pass NULL if it is not interested in this information.
    @param  afterEndOut
                Points to a bool that will be set to true if timeStamp is after the
                last sample, false otherwise.
                The client should pass NULL if it is not interested in this information.
    @result     Returns 0 in case of success.
 */
PR_EXPORT PRStatus ProResTrackReaderCreateSampleCursorAtTimeStamp(
    ProResTrackReaderRef trackReader,
    PRTime timeStamp,
    ProResSampleCursorRef *newCursorOut,
    bool *beforeStartOut, // may be NULL
    bool *afterEndOut);	// may be NULL

/*!
	@function	ProResTrackReaderCreateCursorAtFirstSample
    @abstract	Creates a cursor object pointing to the first sample.
    @param	newCursorOut
                Points to a ProResSampleCursor to receive the new cursor.
                The client is responsible for releasing the cursor by calling PRRelease when
                it is done with it.
    @result     Returns 0 in case of success.
 */
PR_EXPORT PRStatus ProResTrackReaderCreateCursorAtFirstSample(
    ProResTrackReaderRef trackReader,
    ProResSampleCursorRef *newCursorOut);

/*!
	@function	ProResTrackReaderCreateCursorAtLastSample
    @abstract   Creates a cursor object pointing to the last sample.
    @param	newCursorOut
                Points to a ProResSampleCursor to receive the new cursor.
                The client is responsible for releasing the cursor by calling PRRelease when
                it is done with it.
    @result
                Returns 0 in case of success.
 */
PR_EXPORT PRStatus ProResTrackReaderCreateCursorAtLastSample(
    ProResTrackReaderRef trackReader,
    ProResSampleCursorRef *newCursorOut);

/*!
	@function	ProResSampleCursorCopy
    @abstract	Creates a cursor object identical to an existing cursor object.
    @discussion Further manipulation of the new cursor shall not have any effect on the original cursor.
    @param	cursor
                The cursor to copy.
    @param	newCursorOut
                Points to a ProResSampleCursor to receive the new cursor.
                The client is responsible for releasing the new cursor by calling PRRelease when
                it is done with it.
 */
PR_EXPORT PRStatus ProResSampleCursorCopy(
    ProResSampleCursorRef cursor,
    ProResSampleCursorRef *newCursorOut);

/*!
	@function	ProResSampleCursorStepAndReportStepsTaken
    @abstract	Moves the cursor a given number of samples.
    @discussion If the request would advance the cursor past the last sample or before the first sample,
                the cursor shall be set to point to that limiting sample and, if stepsTakenOut is not NULL,
                *stepsTakenOut shall be set to the number of samples the cursor was able to move.
    @param	stepCount
                The number of samples to move.
                If positive, step forward this many samples.
                If negative, step backward (-stepCount) samples.
    @param stepsTakenOut
                A pointer to an int32_t to receive the count of steps taken. May be NULL.
 */
PR_EXPORT PRStatus ProResSampleCursorStepAndReportStepsTaken(
    ProResSampleCursorRef cursor,
    int64_t stepCount,
    int64_t *stepsTakenOut);

/*!
	@function	ProResSampleCursorStepByTime
    @abstract	Moves the cursor by a given deltaTime.
    @discussion If the request would advance the cursor past the end of the last sample or before the first sample,
                the cursor shall be set to point to that limiting sample and the method shall
                return kProResSampleCursorHitEndErr.
    @param	deltaTime
                The cursor is moved to the sample at the current sample's time + deltaTime.
 */
PR_EXPORT PRStatus ProResSampleCursorStepByTime(
    ProResSampleCursorRef cursor,
    PRTime deltaTime);

/*!
	@function	ProResSampleCursorCompare
    @abstract	Compares the relative positions of two cursor objects.
    @discussion The two cursors must have been created by the same ProResTrackReader.
    @result     kPRCompareLessThan, kPRCompareEqualTo or kPRCompareGreaterThan, depending whether
                cursor1 points at a sample before, the same as, or after the sample pointed to by cursor2.
 */
PR_EXPORT PRComparisonResult ProResSampleCursorCompare(
    ProResSampleCursorRef cursor1,
    ProResSampleCursorRef cursor2);

/*!
	@function	ProResSampleCursorGetTimeStamp
    @abstract	Retrieves the time of the sample pointed at by the cursor.
 */
PR_EXPORT PRStatus ProResSampleCursorGetTimeStamp(
    ProResSampleCursorRef cursor,
    PRTime *timeStampOut);

/*!
	@function	ProResSampleCursorGetDuration
    @abstract	Retrieves the duration of the sample pointed at by the cursor.
 */
PR_EXPORT PRStatus ProResSampleCursorGetDuration(
    ProResSampleCursorRef cursor,
    PRTime *durationOut);

/*!
	@function	ProResSampleCursorGetSampleLocation
    @abstract	Returns the location of the sample indicated by the cursor.
    @param cursor
                Indicates the sample in question.
    @param sampleOffsetOut
                Returns the starting file offset of the sample.  Pass NULL if you do not want this information.
    @param sampleSizeOut
                Returns the size of the sample.  Pass NULL if you do not want this information.
 */
PR_EXPORT PRStatus ProResSampleCursorGetSampleLocation(
    ProResSampleCursorRef cursor,
    int64_t *sampleOffsetOut,
    size_t *sampleSizeOut);

/*!
	@function	ProResSampleCursorGetChunkDetails
    @abstract	Returns information about the chunk holding the sample indicated by the cursor.
    @param cursor
                Indicates the sample in question.
    @param chunkOffsetOut
                Returns the offset of the sample's chunk within the file. May be NULL.
    @param chunkSizeOut
                Returns the byte length of the chunk. May be NULL. Will return 0 if there is no chunk associated with the sample.
    @param sampleCountOut
                Returns the number of samples in the chunk. May be NULL. Will return 0 if there is no chunk associated with the sample.
    @param sampleIndexOut
                Returns the offset in samples of cursor within the chunk. You could step back this many samples to position
                the cursor at the start of the chunk. Subtract from *sampleCountOut to obtain number of samples to the end
                of the chunk. May be NULL.
    @param allSameSizeOut
                Returns true if all samples in the chunk have same byte length. May be NULL.
    @param allSameDurationOut
                Returns true if all samples in the chunk have same duration. May be NULL.
 */
PR_EXPORT PRStatus ProResSampleCursorGetChunkDetails(
    ProResSampleCursorRef cursor,
    int64_t *chunkOffsetOut,
    size_t *chunkSizeOut,
    int64_t *sampleCountOut,
    int64_t *sampleIndexOut,
    bool *allSameSizeOut,
    bool *allSameDurationOut);

/*!
	@function	ProResSampleCursorCopyFormatDescription
    @abstract	Returns the format description for the sample indicated by the cursor.
    @param cursor
                Indicates the sample in question.
    @param formatDescriptionOut
                Points to a ProResFormatDescription to receive the format description.
                This method retains the ProResFormatDescription on behalf of the client; the client should call PRRelease 
                when it is done with the ProResFormatDescription.
 */
PR_EXPORT PRStatus ProResSampleCursorCopyFormatDescription(
    ProResSampleCursorRef cursor,
    ProResFormatDescriptionRef *formatDescriptionOut);
	
/*!
	@function	ProResSampleCursorReadSample
	@abstract	Reads a single sample indicated by the cursor.
	@param cursor
				Indicates the sample in question.
	@param dataBuffer
				Sample data will be copied into the dataBuffer provided it is allocated memory the size of a single sample.
	@param dataBufferLength
				dataBufferLength equal to the third parameter of the call to ProResSampleCursorGetSampleLocation() - sampleSizeOut.
 */
PR_EXPORT PRStatus ProResSampleCursorReadSample(
	ProResSampleCursorRef cursor,
	void *dataBuffer,
	size_t dataBufferLength);
	
/*!
	 @function	ProResSampleCursorReadChunk
	 @abstract	Reads the chunk indicated by the cursor.
	 @param cursor
				Indicates the sample in question.
	 @param dataBuffer
				Chunk data will be copied into the dataBuffer provided it is allocated memory the size of the chunk.
	 @param dataBufferLength
				dataBufferLength equal to the third parameter of the call to ProResSampleCursorGetChunkDetails() - chunkSizeOut.
*/
PR_EXPORT PRStatus ProResSampleCursorReadChunk(
	ProResSampleCursorRef cursor,
	void *dataBuffer,
	size_t dataBufferLength);
	
#pragma pack(pop)
    
#ifdef __cplusplus
}
#endif

#endif // PRORESFILEREADER_H
