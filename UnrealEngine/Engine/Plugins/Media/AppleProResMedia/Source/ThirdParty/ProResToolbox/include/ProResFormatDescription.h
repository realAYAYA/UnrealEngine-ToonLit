//
//  ProResFormatDescription.h
//  Copyright (c) 2017 Apple. All rights reserved.
//

#ifndef PRORESFORMATDESCRIPTION_H
#define PRORESFORMATDESCRIPTION_H   1

#include <stdio.h>
#include <stdbool.h>

#include "ProResTime.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#pragma pack(push, 4)
	
enum
{
	kPRFormatDescriptionError_InvalidParameter		= -12710,
	kPRFormatDescriptionError_AllocationFailed		= -12711,
	kPRFormatDescriptionError_ValueNotAvailable		= -12718,
};

/*!
	@typedef	ProResFormatDescriptionRef
	@abstract	A reference to a ProResFormatDescription, a PRType object describing media of a particular type (audio, video, or timecode).
                Retain or release using PRRetain() or PRRelease().
 */
typedef const struct opaqueFormatDescription *ProResFormatDescriptionRef;

/*!
	@function	ProResFormatDescriptionGetMediaType
	@abstract	Returns the media type of a ProResFormatDescription.
	@discussion	For example, returns kPRMediaType_Audio for a description of an audio stream.
	@result		The media type of the ProResFormatDescription.
 */
PR_EXPORT PRMediaType ProResFormatDescriptionGetMediaType(
    ProResFormatDescriptionRef desc);

/*!
	@function	ProResFormatDescriptionGetMediaSubType
	@abstract	Returns the media subtype of a ProResFormatDescription.
	@discussion	The media subtype is defined in a media-specific way.
                For audio streams, the media subtype is the asbd.mFormatID.
                For video streams, the media subtype is the video codec type.
                If a particular type of media stream does not have subtypes, this API may return 0.
	@result		The media subtype of the ProResFormatDescription.
 */
PR_EXPORT uint32_t ProResFormatDescriptionGetMediaSubType(
    ProResFormatDescriptionRef desc);

/*!
	@typedef    ProResAudioFormatDescriptionRef
                Synonym type used for manipulating audio ProResFormatDescriptions
 */
typedef ProResFormatDescriptionRef ProResAudioFormatDescriptionRef;
	
/*!
	@function	ProResAudioFormatDescriptionCreate
	@abstract	Creates a format description for an audio media stream.
	@discussion	The ASBD is required; the channel layout is optional. The caller owns the
				returned ProResFormatDescription, and must release it when done with it. The ASBD and
				channel layout are copied. The caller can deallocate them or re-use them after making this call.
*/
PR_EXPORT PRStatus ProResAudioFormatDescriptionCreate(
	const AudioStreamBasicDescription *asbd,    /*! @param asbd         Audio format description (see ProResTypes.h). This information is required. */
	size_t layoutSize,                          /*! @param layoutSize	Size, in bytes, of audio channel layout. 0 if layout is NULL. */
	const AudioChannelLayout *layout,           /*! @param layout		Audio channel layout (see ProResTypes.h). Can be NULL. */
	ProResAudioFormatDescriptionRef *outDesc);  /*! @param outDesc		Returned newly created audio ProResFormatDescription */

/*!
	@function	ProResAudioFormatDescriptionGetStreamBasicDescription
	@abstract	Returns a read-only pointer to the AudioStreamBasicDescription inside an audio ProResFormatDescription.
	@discussion	See ProResTypes.h for the definition of AudioStreamBasicDescription.
                This API is specific to audio format descriptions, and will return NULL if
                used with a non-audio format descriptions.
 */
PR_EXPORT const AudioStreamBasicDescription * ProResAudioFormatDescriptionGetStreamBasicDescription(
	ProResAudioFormatDescriptionRef desc);      /*! @param desc         ProResFormatDescription being interrogated. */

/*!
	@function	ProResAudioFormatDescriptionGetChannelLayout
	@abstract	Returns a read-only pointer to (and size of) the AudioChannelLayout inside an audio ProResFormatDescription.
	@discussion	See ProResTypes.h for the definition of AudioChannelLayout.
                AudioChannelLayouts are optional; this API will return NULL if
                one does not exist. This API is specific to audio format
                descriptions, and will return NULL if called with a non-audio
                format description.
	@result		A read-only pointer to the AudioChannelLayout inside the audio format description.
 */
PR_EXPORT const AudioChannelLayout * ProResAudioFormatDescriptionGetChannelLayout(
	ProResAudioFormatDescriptionRef desc,       /*! @param desc			ProResFormatDescription being interrogated. */
    size_t *layoutSize);                        /*! @param layoutSize	Pointer to variable that will be written with the size of the layout.
                                                                        Can be NULL. */

/*!
	@typedef ProResVideoFormatDescriptionRef
             Synonym type used for manipulating video ProResFormatDescriptions
 */
typedef ProResFormatDescriptionRef ProResVideoFormatDescriptionRef;

typedef struct {
	int32_t width;
	int32_t height;
} PRVideoDimensions;
	
#define ProResVideoFormatDescriptionGetCodecType(desc)  ProResFormatDescriptionGetMediaSubType(desc)

/*!
	 @function	 ProResVideoFormatDescriptionGetFormatName
	 @abstract	 Returns a pointer to a NULL-terminated string of size formatNameSizeOut (formatNameSizeOut does not include the NULL character).
*/
PR_EXPORT const char * ProResVideoFormatDescriptionGetFormatName(
	ProResVideoFormatDescriptionRef desc,
	size_t *formatNameSizeOut);

PR_EXPORT int32_t ProResVideoFormatDescriptionGetDepth(
	ProResVideoFormatDescriptionRef desc);

/*!
	@function	ProResVideoFormatDescriptionGetDimensions
	@abstract	Returns the dimensions (in encoded pixels)
	@discussion	This does not take into account pixel aspect ratio or clean aperture tags.
 */
PR_EXPORT PRVideoDimensions ProResVideoFormatDescriptionGetDimensions(
    ProResVideoFormatDescriptionRef desc);

/*!
	@function	ProResVideoFormatDescriptionGetCleanAperture
	@abstract	Returns the clean aperture.
	@discussion The clean aperture is a rectangle that defines the portion of the encoded pixel dimensions
                that represents image data valid for display.
 */
PR_EXPORT PRRect ProResVideoFormatDescriptionGetCleanAperture(
	ProResVideoFormatDescriptionRef videoDesc,	/*! @param videoDesc
													FormatDescription being interrogated. */
	bool originIsAtTopLeft						/*! @param originIsAtTopLeft
													Pass true if the PRRect will be used in an environment
													where (0,0) is at the top-left corner of an enclosing rectangle
													and y coordinates increase as you go down.
													Pass false if the PRRect will be used in an environment
													where (0,0) is at the bottom-left corner of an enclosing rectangle
													and y coordinates increase as you go up. */
	);

// used to record the precise numerator and denominator in cases where the number is not an integer.
typedef struct {
	int32_t numerator;
	int32_t denominator;
} PRRational;

typedef struct {
	PRRational width;
	PRRational height;
	PRRational horizontalOffset;  // horizontal offset from center of image buffer
	PRRational verticalOffset;    // vertical offset from center of image buffer
} PRCleanApertureDataRational;
	
/*!
	@function	ProResVideoFormatDescriptionGetCleanApertureData
	@abstract	Returns the raw clean aperture rational data (if available).
*/
PR_EXPORT PRStatus ProResVideoFormatDescriptionGetCleanApertureData(
	ProResVideoFormatDescriptionRef desc,		/*! @param videoDesc
													FormatDescription being interrogated. */
	PRCleanApertureDataRational *dataOut );

/*
	@function	ProResVideoFormatDescriptionGetFieldCount
	@abstract	For progressive material, fieldCount = 1; interlaced material, fieldCount = 2;
*/
PR_EXPORT uint32_t ProResVideoFormatDescriptionGetFieldCount(
	ProResVideoFormatDescriptionRef desc);

enum {
	kProResFormatDescriptionFieldDetail_Unknown					= 0,  /* progressive material */
    kProResFormatDescriptionFieldDetail_SpatialFirstLineEarly	= 9,  /* interlaced material where the _first_ line of the
																	     frame belongs to the earlier temporal field */
	kProResFormatDescriptionFieldDetail_SpatialFirstLineLate	= 14, /* interlaced material where the _second_ line of the
																		 frame belongs to the earlier temporal field */
	
};
typedef uint32_t ProResFormatDescriptionFieldDetail;

PR_EXPORT ProResFormatDescriptionFieldDetail ProResVideoFormatDescriptionGetFieldDetail(
	ProResVideoFormatDescriptionRef desc);

PR_EXPORT PRStatus ProResVideoFormatDescriptionGetPixelAspectRatio(
	ProResVideoFormatDescriptionRef desc,
	uint32_t *horizontalSpacingOut,
	uint32_t *verticalSpacingOut);

enum {
    kProResFormatDescriptionColorPrimaries_ITU_R_709_2	= 1,
    kProResFormatDescriptionColorPrimaries_EBU_3213		= 5,
    kProResFormatDescriptionColorPrimaries_SMPTE_C		= 6,
	kProResFormatDescriptionColorPrimaries_ITU_R_2020	= 9,
};
typedef uint32_t ProResFormatDescriptionColorPrimaries;

PR_EXPORT PRStatus ProResVideoFormatDescriptionGetColorPrimaries(
	ProResVideoFormatDescriptionRef desc,
	ProResFormatDescriptionColorPrimaries *colorPrimariesOut);

enum {
    kProResFormatDescriptionTransferFunction_ITU_R_709_2		= 1,
	kProResFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ	= 16,
	kProResFormatDescriptionTransferFunction_ARIB_STD_B67_HLG	= 18,
};
typedef uint32_t ProResFormatDescriptionTransferFunction;

PR_EXPORT PRStatus ProResVideoFormatDescriptionGetTransferFunction(
	ProResVideoFormatDescriptionRef desc,
	ProResFormatDescriptionTransferFunction *transferFunctionOut);

enum {
	kProResFormatDescriptionYCbCrMatrix_ITU_R_709_2		= 1,
	kProResFormatDescriptionYCbCrMatrix_ITU_R_601_4		= 6,
	kProResFormatDescriptionYCbCrMatrix_ITU_R_2020		= 9,
};
typedef uint32_t ProResFormatDescriptionYCbCrMatrix;
	
/*!
	@function	ProResVideoFormatDescriptionGetYCbCrMatrix
	@abstract	Describes the color matrix for YCbCr->RGB
 */
PR_EXPORT PRStatus ProResVideoFormatDescriptionGetYCbCrMatrix(
	ProResVideoFormatDescriptionRef desc,
	ProResFormatDescriptionYCbCrMatrix *matrixOut);

/*!
	@function	ProResVideoFormatDescriptionGetGammaLevel
	@abstract	Used in absence of (or ignorance of) a valid ProResFormatDescriptionTransferFunction returned
                by ProResVideoFormatDescriptionGetTransferFunction
 */
PR_EXPORT PRStatus ProResVideoFormatDescriptionGetGammaLevel(
	ProResVideoFormatDescriptionRef desc,
	double *gammaLevelOut);
	
/*!
    @function	ProResVideoFormatDescriptionCreate
    @abstract	Creates a format description for a video media stream.
    @discussion	The caller owns the returned ProResFormatDescription, and must release it when done with it.
				All input parameters are copied. The caller can deallocate them or re-use them after making this call.
*/
PR_EXPORT PRStatus ProResVideoFormatDescriptionCreate(
    PRVideoCodecType codecType,									/*! @param codecType
																	kPRVideoCodecType_AppleProRes* */
    PRVideoDimensions dimensions,								/*! @param dimensions     
																	Should _not_ take into account pixel aspect ratio or clean aperture tags. */
    int32_t depth,												/*! @param depth      
																	- 32 for ProRes 4444 if at least one frame contains non-opaque alpha.
																	The "allOpaqueAlpha" argument returned by the ProResEncoder's PREncodeFrame()
																	will be true if any non-opaque alpha values were encoded.
																	- 24 in all other cases (<http://developer.apple.com/mac/library/qa/qa2001/qa1183.html>),
																	including ProRes 4444 with an entirely opaque alpha channel. */
    uint32_t fieldCount,										/*! @param fieldCount
																	For progressive material, fieldCount = 1; interlaced material, fieldCount = 2; */
    ProResFormatDescriptionFieldDetail fieldDetail,				/*! @param fieldDetail      
																	- For progressive material, fieldDetail = kProResFormatDescriptionFieldDetail_Unknown.
																	- For interlaced material where the _first_ line of the frame belongs to the earlier
																	temporal field = kProResFormatDescriptionFieldDetail_SpatialFirstLineEarly.
																	- For interlaced material where the _second_ line of the frame belongs to the earlier
																	temporal field = kProResFormatDescriptionFieldDetail_SpatialFirstLineLate. */
    ProResFormatDescriptionColorPrimaries colorPrimaries,		/*! @param colorPrimaries
																	- SD Rec. 601 525/60Hz-system = kProResFormatDescriptionColorPrimaries_SMPTE_C
																	- SD Rec. 601 625/50Hz-system = kProResFormatDescriptionColorPrimaries_EBU_3213
																	- HD Rec. 709 = kProResFormatDescriptionColorPrimaries_ITU_R_709_2
																	- Rec. 2020 = kProResFormatDescriptionColorPrimaries_ITU_R_2020 */
    ProResFormatDescriptionTransferFunction transferFunction,	/*! @param transferFunction
																	- SD Rec. 601 525/60Hz-system = kProResFormatDescriptionTransferFunction_ITU_R_709_2
																	- SD Rec. 601 625/50Hz-system = kProResFormatDescriptionTransferFunction_ITU_R_709_2
																	- HD Rec. 709 = kProResFormatDescriptionTransferFunction_ITU_R_709_2
																	- Rec. 2020 = kProResFormatDescriptionTransferFunction_ITU_R_709_2
																	- HDR with SMPTE ST 2084 (PQ) transfer function and Rec. 2020 primaries =
																		kProResFormatDescriptionTransferFunction_SMPTE_ST_2084
																	- HDR with HLG transfer function and Rec. 2020 primaries =
																		kProResFormatDescriptionTransferFunction_HLG */
    ProResFormatDescriptionYCbCrMatrix matrix,					/*! @param matrix
																	- SD Rec. 601 525/60Hz-system = kProResFormatDescriptionYCbCrMatrix_ITU_R_601_4
																	- SD Rec. 601 625/50Hz-system = kProResFormatDescriptionYCbCrMatrix_ITU_R_601_4
																	- HD Rec. 709 = kProResFormatDescriptionYCbCrMatrix_ITU_R_709_2
																	- Rec. 2020 = kProResFormatDescriptionYCbCrMatrix_ITU_R_2020
																	- HDR with SMPTE ST 2084 (PQ) transfer function and Rec. 2020 primaries =
																		kProResFormatDescriptionYCbCrMatrix_ITU_R_2020
																	- HDR with HLG transfer function and Rec. 2020 primaries =
																		kProResFormatDescriptionYCbCrMatrix_ITU_R_2020 */
    uint32_t paspHorizontalSpacing,								/*! @param paspHorizontalSpacing
																	- For NTSC 4:3 material use 10
																	- For NTSC 16:9 material use 40
																	- For PAL 4:3 material use 59
																	- For PAL 16:9 material use 118
																	- For HD material encoded at 1280x1080 use 3
																	- For HD material encoded at 960x720 or 1440x1080 use 4
																	- For all square-pixel material (e.g., HD at 1280x720 or 1920x1080) use 1 */
	uint32_t paspVerticalSpacing,								/*! @param paspVerticalSpacing
																	- For NTSC 4:3 material use 11
																	- For NTSC 16:9 material use 33
																	- For PAL 4:3 material use 54
																	- For PAL 16:9 material use 81
																	- For HD material encoded at 1280x1080 use 2
																	- For HD material encoded at 960x720 or 1440x1080 use 3
																	- For all square-pixel material (e.g., HD at 1280x720 or 1920x1080) use 1 */
	const PRCleanApertureDataRational *clapData,				/*! @param clapData
																	- For NTSC use { 704, 1, 480, 1, 0, 1, 0, 1 }
																	- For PAL use { 768*54, 59, 576, 1, 0, 1, 0, 1 }
																	- For HD, ‘clap’ may be omitted */
	bool hasGammaLevel,											/*! @param hasGammaLevel If gammaLevel is a valid value, pass true. */
    double gammaLevel,											/*! @param gammaLevel    The gamma level. */
    ProResVideoFormatDescriptionRef *outDesc);

/*!
	@typedef ProResTimecodeFormatDescriptionRef
             Synonym type used for manipulating Timecode media ProResFormatDescriptions
 */
typedef ProResFormatDescriptionRef ProResTimecodeFormatDescriptionRef;

/*!
	@enum Timecode Flags
	@discussion Flags describing the content of ProResTimecodeFormatDescription samples.
	@constant	kProResTimecodeFlag_DropFrame	Timecodes are to be rendered in drop-frame format.
	@constant	kProResTimecodeFlag_24HourMax	Timecode rolls over every 24 hours.
	@constant	kProResTimecodeFlag_NegTimesOK	Track may contain negative timecodes.
 */
enum {
	kProResTimecodeFlag_DropFrame	= 1 << 0,
	kProResTimecodeFlag_24HourMax	= 1 << 1,
	kProResTimecodeFlag_NegTimesOK	= 1 << 2,
};

/*!
	@function	ProResTimecodeFormatDescriptionGetFrameDuration
	@abstract	Returns the duration of each frame (eg. 100/2997)
 */
PR_EXPORT PRTime ProResTimecodeFormatDescriptionGetFrameDuration(
	ProResTimecodeFormatDescriptionRef desc);

/*!
	@function	ProResTimecodeFormatDescriptionGetFrameQuanta
	@abstract	Returns the frames/sec for timecode (eg. 30) OR frames/tick for counter mode
 */
PR_EXPORT uint32_t ProResTimecodeFormatDescriptionGetFrameQuanta(
	ProResTimecodeFormatDescriptionRef desc);

/*!
	@function	ProResTimecodeFormatDescriptionGetTimecodeFlags
	@abstract	Returns the flags for kProResTimecodeFlag_*
 */
PR_EXPORT uint32_t ProResTimecodeFormatDescriptionGetTimecodeFlags(
	ProResTimecodeFormatDescriptionRef desc);

/*!
	 @function	ProResTimecodeFormatDescriptionGetSourceReferenceName
	 @abstract	Returns a pointer to a NULL-terminated string of size sourceReferenceNameSizeOut
				(sourceReferenceNameSizeOut does not include the NULL character).
				This string corresponds to the 'name' entry in the timecode description's user data atom.
 */
PR_EXPORT const char * ProResTimecodeFormatDescriptionGetSourceReferenceName(
	ProResTimecodeFormatDescriptionRef desc,
	size_t *sourceReferenceNameSizeOut,
	int16_t *languageCodeOut); // may be NULL
	
/*!
	@function	ProResTimecodeFormatDescriptionCreate
	@abstract	Creates a format description for a timecode media.
	@discussion	The caller owns the returned ProResFormatDescription, and must release it when done with it. All input parameters
				are copied (the extensions are deep-copied).  The caller can deallocate them or re-use them after making this call.
 */
PR_EXPORT PRStatus ProResTimecodeFormatDescriptionCreate(
	 PRTime frameDuration,							 /*! @param frameDuration			Duration of each frame (eg. 100/2997) */
	 uint32_t frameQuanta,                           /*! @param frameQuanta				Frames/sec for timecode (eg. 30) OR frames/tick for counter mode */
	 uint32_t tcFlags,                               /*! @param tcFlags					kProResTimecodeFlag_* */
	 const char *sourceReferenceName,				 /*! @param sourceReferenceName		This string is stored in the 'name' field of the user data atom containing
																						information about the source tape. May be NULL. */
	 size_t sourceReferenceNameSize,				 /*! @param sourceReferenceNameSize The size of sourceReferenceName string. */
	 int16_t languageCode,                           /*! @param languageCode			The sourceReferenceName's language code; stored in the 'name' field of the
																						user data atom containing information about the source tape. */
	 ProResTimecodeFormatDescriptionRef *outDesc);   /*! @param descOut					Receives the newly-created ProResFormatDescription. */
	
#pragma pack(pop)
    
#ifdef __cplusplus
}
#endif

#endif // PRORESFORMATDESCRIPTION_H
