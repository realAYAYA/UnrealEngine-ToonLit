//
//  ProResTypes.h
//  Copyright © 2016 Apple. All rights reserved.
//

#ifndef PRORESTYPES_H
#define PRORESTYPES_H   1

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 4)
	
#if _WIN32
#ifdef PRORESTOOLBOX_EXPORTS
	#define PR_EXPORT __declspec(dllexport) extern
#else
	#define PR_EXPORT __declspec(dllimport) extern
#endif
#else
	#define PR_EXPORT extern
#endif
	
#if __LLP64__
typedef unsigned long long PRTypeID;
typedef signed long long PRIndex;
#else
typedef unsigned long PRTypeID;
typedef signed long PRIndex;
#endif
	
/*!
	 @abstract   An untyped "generic" reference to any PRType object.
	 @discussion The PRTypeRef type is the absolute base type of all ProRes* objects.
				 It is a generic object reference that acts as a placeholder for other true ProRes objects.
 */
typedef const void * PRTypeRef;

/*!
	 @function	 PRRetain
	 @abstract	 Retains a PRType object.
	 @param      type	The PRType object to retain. This value must not be NULL.
	 @result     The input value, type.
	 @discussion You should retain a PRType object when you receive it from elsewhere (that is, you did not create or copy it)
				 and you want it to persist. If you retain a PRType object you are responsible for releasing it.
 */
PR_EXPORT PRTypeRef PRRetain(PRTypeRef type);

/*!
	 @function	 PRGetRetainCount
	 @abstract	 Returns the reference count of a PRType object.
	 @param      type	The PRType object to examine.
	 @result     A number representing the reference count of type.
	 @discussion You increment the reference count using the PRRetain function, and decrement the reference count using the
				 PRRelease function. This function may useful for debugging memory leaks. You normally do not use this function otherwise.
 */
PR_EXPORT PRIndex PRGetRetainCount(PRTypeRef type);

/*!
	 @function	 PRRelease
	 @abstract	 Releases a PRType object.
	 @param      type	A PRType object to release. This value must not be NULL.
	 @discussion If the retain count of type becomes zero the memory allocated to the object is deallocated and the object is 
				 destroyed. If you create, copy, or explicitly retain (see the PRRetain function) a PRType object, you are 
				 responsible for releasing it when you no longer need it.
 */
PR_EXPORT void PRRelease(PRTypeRef type);
	
// convenience macro: if a PRTypeRef isn't NULL, release it and set its value to NULL.
// usage:
//      PRTypeRef type;
//      PRReleaseAndClear(&type);
#define PRReleaseAndClear(type)		\
	do {							\
		if (*(type) != NULL) {      \
			PRRelease(*(type));		\
			*(type) = NULL;			\
		}                           \
	} while (0)						\

enum {
	kProResBaseObjectError_ParamErr				= -12780,
	kProResBaseObjectError_UnsupportedOperation	= -12782,
	kProResBaseObjectError_ValueNotAvailable	= -12783,
	kProResBaseObjectError_Invalidated			= -12785,
	kProResBaseObjectError_AllocationFailed		= -12786,
	kProResBaseObjectError_InvalidCodec			= -12787,
	
	kProResTrackError_TrackNotFound				= -12843,
	
	kProResSampleCursor_HitEndErr				= -12840,
	
	kProResEditCursor_HitEndErr					= -12520,
	kProResEditCursor_NoEditsErr				= -12521,
};

enum {
	kPRMediaType_Video							= 'vide',
	kPRMediaType_Audio							= 'soun',
	kPRMediaType_Timecode						= 'tmcd',
};
typedef uint32_t PRMediaType;

enum {
	kPRVideoCodecType_AppleProRes4444XQ			= 'ap4x',
	kPRVideoCodecType_AppleProRes4444			= 'ap4h',
	kPRVideoCodecType_AppleProRes422HQ			= 'apch',
	kPRVideoCodecType_AppleProRes422			= 'apcn',
	kPRVideoCodecType_AppleProRes422LT			= 'apcs',
	kPRVideoCodecType_AppleProRes422Proxy		= 'apco',
};
typedef uint32_t PRVideoCodecType;

typedef struct {
	float matrix[3][3];
} PRMatrix;
	
#if defined(__LP64__) && __LP64__
	typedef double PRFloat;
#else
	typedef float PRFloat;
#endif

typedef struct {
	PRFloat width;
	PRFloat height;
} PRSize;

typedef struct {
	PRFloat x;
	PRFloat y;
} PRPoint;

typedef struct {
	PRPoint origin;
	PRSize size;
} PRRect;

PR_EXPORT PRRect PRRectMake(PRFloat x, PRFloat y, PRFloat width, PRFloat height);

enum {
	kPRCompareLessThan = -1L,
	kPRCompareEqualTo = 0,
	kPRCompareGreaterThan = 1
};
typedef int32_t PRComparisonResult;

/*!
	@typedef	PRPersistentTrackID
	@abstract	A PRPersistentTrackID is used to specify a track to a ProResFileReader
                to obtain a ProResTrackReader.
 */
typedef int32_t PRPersistentTrackID;

typedef int32_t PRStatus;
	
enum {
	kProResMetadataKeyFormatShort	= 'itsk', // four character code, e.g. 'mdta', usually used with QuickTimeUserData.
	kProResMetadataKeyFormatLong	= 'itlk', // string in reverse DNS format, e.g. "com.apple.finalcutstudio.media.uuid"
											  // usually used with QuickTimeMetadata.
};
typedef uint32_t ProResMetadataKeyFormat;

enum {
	kProResMetadataFormat_QuickTimeMetadata = 1,
	kProResMetadataFormat_QuickTimeUserData = 2,
};
typedef uint32_t ProResMetadataFormat;

// predefined common QuickTime metadata keys

PR_EXPORT const char* kProResQuickTimeMetadataKey_Author;
PR_EXPORT const char* kProResQuickTimeMetadataKey_Comment;
PR_EXPORT const char* kProResQuickTimeMetadataKey_Copyright;
PR_EXPORT const char* kProResQuickTimeMetadataKey_CreationDate;
PR_EXPORT const char* kProResQuickTimeMetadataKey_Director;
PR_EXPORT const char* kProResQuickTimeMetadataKey_DisplayName;
PR_EXPORT const char* kProResQuickTimeMetadataKey_Information;
PR_EXPORT const char* kProResQuickTimeMetadataKey_ContentIdentifier;
PR_EXPORT const char* kProResQuickTimeMetadataKey_Keywords;
PR_EXPORT const char* kProResQuickTimeMetadataKey_Make;
PR_EXPORT const char* kProResQuickTimeMetadataKey_Model;

// predefined common QuickTime user data keys

enum {
	kProResUserDataShortKey_Copyright = 0xA9637079 /* ©cpy */,
	kProResUserDataShortKey_CreationDate = 0xA9646179 /* ©day */,
	kProResUserDataShortKey_Description = 0xA9646573 /* ©des */,
	kProResUserDataShortKey_Director = 0xA9646972 /* ©dir */,
	kProResUserDataShortKey_Information = 0xA9696E66 /* ©inf */,
	kProResUserDataShortKey_Keywords = 0xA96B6579 /* ©key */,
	kProResUserDataShortKey_Make = 0xA96D616B /* ©mak */,
	kProResUserDataShortKey_Track = 0xA974726B /* ©trk */,
	kProResUserDataShortKey_Name = 0x6e616d65 /* name */,
};

// data types for QuickTime metdata and user data when dealing with ProResMetadataDataType
enum {
	kProResQuickTimeMetadataTypeBinary					= 0,
	kProResQuickTimeMetadataTypeUTF8					= 1,
	kProResQuickTimeMetadataTypeUTF16BE					= 2,
	kProResQuickTimeMetadataTypeMacEncodedText			= 3,
	kProResQuickTimeMetadataTypeJPEGImage				= 13,
	kProResQuickTimeMetadataTypePNGImage				= 14,
	kProResQuickTimeMetadataTypeSignedInteger			= 21,	// The size of the integer is defined by the value size
	kProResQuickTimeMetadataTypeUnsignedInteger			= 22,	// The size of the integer is defined by the value size
	kProResQuickTimeMetadataTypeFloat32					= 23,
	kProResQuickTimeMetadataTypeFloat64					= 24,
	kProResQuickTimeMetadataTypeBMPImage				= 27,
	kProResQuickTimeMetadataType8BitSignedInteger		= 65,
	kProResQuickTimeMetadataType16BitSignedInteger		= 66,
	kProResQuickTimeMetadataType32BitSignedInteger		= 67,
	kProResQuickTimeMetadataType64BitSignedInteger		= 74,
	kProResQuickTimeMetadataType8BitUnsignedInteger		= 75,
	kProResQuickTimeMetadataType16BitUnsignedInteger	= 76,
	kProResQuickTimeMetadataType32BitUnsignedInteger	= 77,
	kProResQuickTimeMetadataType64BitUnsignedInteger	= 78,
};
enum {
	kProResQuickTimeUserDataTypeBinary						= 0,
	kProResQuickTimeUserDataTypeTextEncodedPerMovieLangCode	= 1,
	kProResQuickTimeUserDataTypeUTF8						= 2,
	kProResQuickTimeUserDataTypeUTF16						= 3
};
typedef uint32_t ProResMetadataDataType;
	
/*!
	@typedef    AudioFormatID
	@abstract   A four char code indicating the general kind of data in the stream.
*/
typedef uint32_t  AudioFormatID;
	
enum {
	kAudioFormatLinearPCM = 'lpcm',
};

/*!
 @enum           Standard AudioFormatFlags Values for AudioStreamBasicDescription
 @abstract       These are the standard AudioFormatFlags for use in the mFormatFlags field of the
                 AudioStreamBasicDescription structure.
 @discussion     Typically, when an ASBD is being used, the fields describe the complete layout
                 of the sample data in the buffers that are represented by this description -
                 where typically those buffers are represented by an AudioBuffer that is
                 contained in an AudioBufferList.
 
                 However, when an ASBD has the kAudioFormatFlagIsNonInterleaved flag, the
                 AudioBufferList has a different structure and semantic. In this case, the ASBD
                 fields will describe the format of ONE of the AudioBuffers that are contained in
                 the list, AND each AudioBuffer in the list is determined to have a single (mono)
                 channel of audio data. Then, the ASBD's mChannelsPerFrame will indicate the
                 total number of AudioBuffers that are contained within the AudioBufferList -
                 where each buffer contains one channel. This is used primarily with the
                 AudioUnit (and AudioConverter) representation of this list - and won't be found
                 in the AudioHardware usage of this structure.
 @constant       kAudioFormatFlagIsFloat
                 Set for floating point, clear for integer.
 @constant       kAudioFormatFlagIsBigEndian
                 Set for big endian, clear for little endian.
 @constant       kAudioFormatFlagIsSignedInteger
                 Set for signed integer, clear for unsigned integer. This is only valid if
                 kAudioFormatFlagIsFloat is clear.
 @constant       kAudioFormatFlagIsPacked
                 Set if the sample bits occupy the entire available bits for the channel,
                 clear if they are high or low aligned within the channel. Note that even if
                 this flag is clear, it is implied that this flag is set if the
                 AudioStreamBasicDescription is filled out such that the fields have the
                 following relationship:
                 ((mBitsPerSample / 8) * mChannelsPerFrame) == mBytesPerFrame
 @constant       kAudioFormatFlagIsAlignedHigh
                 Set if the sample bits are placed into the high bits of the channel, clear
                 for low bit placement. This is only valid if kAudioFormatFlagIsPacked is
                 clear.
 @constant       kAudioFormatFlagIsNonInterleaved
                 Set if the samples for each channel are located contiguously and the
                 channels are layed out end to end, clear if the samples for each frame are
                 layed out contiguously and the frames layed out end to end.
 @constant       kAudioFormatFlagIsNonMixable
                 Set to indicate when a format is non-mixable. Note that this flag is only
                 used when interacting with the HAL's stream format information. It is not a
                 valid flag for any other uses.
 @constant       kAudioFormatFlagsAreAllClear
                 Set if all the flags would be clear in order to preserve 0 as the wild card
                 value.
 @constant       kLinearPCMFormatFlagIsFloat
                 Synonym for kAudioFormatFlagIsFloat.
 @constant       kLinearPCMFormatFlagIsBigEndian
                 Synonym for kAudioFormatFlagIsBigEndian.
 @constant       kLinearPCMFormatFlagIsSignedInteger
                 Synonym for kAudioFormatFlagIsSignedInteger.
 @constant       kLinearPCMFormatFlagIsPacked
                 Synonym for kAudioFormatFlagIsPacked.
 @constant       kLinearPCMFormatFlagIsAlignedHigh
                 Synonym for kAudioFormatFlagIsAlignedHigh.
 @constant       kLinearPCMFormatFlagIsNonInterleaved
                 Synonym for kAudioFormatFlagIsNonInterleaved.
 @constant       kLinearPCMFormatFlagIsNonMixable
                 Synonym for kAudioFormatFlagIsNonMixable.
 @constant       kLinearPCMFormatFlagsSampleFractionShift
                 The linear PCM flags contain a 6-bit bitfield indicating that an integer
                 format is to be interpreted as fixed point. The value indicates the number
                 of bits are used to represent the fractional portion of each sample value.
                 This constant indicates the bit position (counting from the right) of the
                 bitfield in mFormatFlags.
 @constant       kLinearPCMFormatFlagsSampleFractionMask
                 number_fractional_bits = (mFormatFlags &
                 kLinearPCMFormatFlagsSampleFractionMask) >>
                 kLinearPCMFormatFlagsSampleFractionShift
 @constant       kLinearPCMFormatFlagsAreAllClear
                 Synonym for kAudioFormatFlagsAreAllClear.
 */
enum {
	kAudioFormatFlagIsFloat                     = (1U << 0),     // 0x1
	kAudioFormatFlagIsBigEndian                 = (1U << 1),     // 0x2
	kAudioFormatFlagIsSignedInteger             = (1U << 2),     // 0x4
	kAudioFormatFlagIsPacked                    = (1U << 3),     // 0x8
	kAudioFormatFlagIsAlignedHigh               = (1U << 4),     // 0x10
	kAudioFormatFlagIsNonInterleaved            = (1U << 5),     // 0x20
	kAudioFormatFlagIsNonMixable                = (1U << 6),     // 0x40
	kAudioFormatFlagsAreAllClear                = 0x80000000,
	
	kLinearPCMFormatFlagIsFloat                 = kAudioFormatFlagIsFloat,
	kLinearPCMFormatFlagIsBigEndian             = kAudioFormatFlagIsBigEndian,
	kLinearPCMFormatFlagIsSignedInteger         = kAudioFormatFlagIsSignedInteger,
	kLinearPCMFormatFlagIsPacked                = kAudioFormatFlagIsPacked,
	kLinearPCMFormatFlagIsAlignedHigh           = kAudioFormatFlagIsAlignedHigh,
	kLinearPCMFormatFlagIsNonInterleaved        = kAudioFormatFlagIsNonInterleaved,
	kLinearPCMFormatFlagIsNonMixable            = kAudioFormatFlagIsNonMixable,
	kLinearPCMFormatFlagsSampleFractionShift    = 7,
	kLinearPCMFormatFlagsSampleFractionMask     = (0x3F << kLinearPCMFormatFlagsSampleFractionShift),
	kLinearPCMFormatFlagsAreAllClear            = kAudioFormatFlagsAreAllClear,
};

/*!
	@typedef        AudioFormatFlags
	@abstract       Flags that are specific to each format.
 */
typedef uint32_t  AudioFormatFlags;

/*!
 @struct         AudioStreamBasicDescription
 @abstract       This structure encapsulates all the information for describing the basic
                 format properties of a stream of audio data.
 @discussion     This structure is sufficient to describe any constant bit rate format that  has
                 channels that are the same size. Extensions are required for variable bit rate
                 data and for constant bit rate data where the channels have unequal sizes.
                 However, where applicable, the appropriate fields will be filled out correctly
                 for these kinds of formats (the extra data is provided via separate properties).
                 In all fields, a value of 0 indicates that the field is either unknown, not
                 applicable or otherwise is inapproprate for the format and should be ignored.
                 Note that 0 is still a valid value for most formats in the mFormatFlags field.
                 
                 In audio data a frame is one sample across all channels. In non-interleaved
                 audio, the per frame fields identify one channel. In interleaved audio, the per
                 frame fields identify the set of n channels. In uncompressed audio, a Packet is
                 one frame, (mFramesPerPacket == 1). In compressed audio, a Packet is an
                 indivisible chunk of compressed data, for example an AAC packet will contain
                 1024 sample frames.
                 
 @field          mSampleRate
                 The number of sample frames per second of the data in the stream.
 @field          mFormatID
                 The four cc (e.g., 'lpcm') indicating the general kind of data in the stream.
 @field          mFormatFlags
                 The AudioFormatFlags for the format indicated by mFormatID.
 @field          mBytesPerPacket
                 The number of bytes in a packet of data.
 @field          mFramesPerPacket
                 The number of sample frames in each packet of data.
 @field          mBytesPerFrame
                 The number of bytes in a single sample frame of data.
 @field          mChannelsPerFrame
                 The number of channels in each frame of data.
 @field          mBitsPerChannel
                 The number of bits of sample data for each channel in a frame of data.
 @field          mReserved
                 Pads the structure out to force an even 8 byte alignment.
 */
struct AudioStreamBasicDescription
{
	double				mSampleRate;
	AudioFormatID       mFormatID;
	AudioFormatFlags    mFormatFlags;
	uint32_t			mBytesPerPacket;
	uint32_t			mFramesPerPacket;
	uint32_t			mBytesPerFrame;
	uint32_t			mChannelsPerFrame;
	uint32_t			mBitsPerChannel;
	uint32_t			mReserved;
};
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;

/*!
 @enum           AudioChannelLabel Constants
 @abstract       These constants are for use in the mChannelLabel field of an
                 AudioChannelDescription structure.
 @discussion     These channel labels attempt to list all labels in common use. Due to the
                 ambiguities in channel labeling by various groups, there may be some overlap or
                 duplication in the labels below. Use the label which most clearly describes what
                 you mean.
                 
                 WAVE files seem to follow the USB spec for the channel flags. A channel map lets
                 you put these channels in any order, however a WAVE file only supports labels
                 1-18 and if present, they must be in the order given below. The integer values
                 for the labels below match the bit position of the label in the USB bitmap and
                 thus also the WAVE file bitmap.
 */
enum
{
    kAudioChannelLabel_Unknown                  = 0xFFFFFFFF,   // unknown or unspecified other use
    kAudioChannelLabel_Unused                   = 0,            // channel is present, but has no intended use or destination
    kAudioChannelLabel_UseCoordinates           = 100,          // channel is described by the mCoordinates fields.
    
    kAudioChannelLabel_Left                     = 1,
    kAudioChannelLabel_Right                    = 2,
    kAudioChannelLabel_Center                   = 3,
    kAudioChannelLabel_LFEScreen                = 4,
    kAudioChannelLabel_LeftSurround             = 5,            // WAVE: "Back Left"
    kAudioChannelLabel_RightSurround            = 6,            // WAVE: "Back Right"
    kAudioChannelLabel_LeftCenter               = 7,
    kAudioChannelLabel_RightCenter              = 8,
    kAudioChannelLabel_CenterSurround           = 9,            // WAVE: "Back Center" or plain "Rear Surround"
    kAudioChannelLabel_LeftSurroundDirect       = 10,           // WAVE: "Side Left"
    kAudioChannelLabel_RightSurroundDirect      = 11,           // WAVE: "Side Right"
    kAudioChannelLabel_TopCenterSurround        = 12,
    kAudioChannelLabel_VerticalHeightLeft       = 13,           // WAVE: "Top Front Left"
    kAudioChannelLabel_VerticalHeightCenter     = 14,           // WAVE: "Top Front Center"
    kAudioChannelLabel_VerticalHeightRight      = 15,           // WAVE: "Top Front Right"
    
    kAudioChannelLabel_TopBackLeft              = 16,
    kAudioChannelLabel_TopBackCenter            = 17,
    kAudioChannelLabel_TopBackRight             = 18,
    
    kAudioChannelLabel_RearSurroundLeft         = 33,
    kAudioChannelLabel_RearSurroundRight        = 34,
    kAudioChannelLabel_LeftWide                 = 35,
    kAudioChannelLabel_RightWide                = 36,
    kAudioChannelLabel_LFE2                     = 37,
    kAudioChannelLabel_LeftTotal                = 38,           // matrix encoded 4 channels
    kAudioChannelLabel_RightTotal               = 39,           // matrix encoded 4 channels
    kAudioChannelLabel_HearingImpaired          = 40,
    kAudioChannelLabel_Narration                = 41,
    kAudioChannelLabel_Mono                     = 42,
    kAudioChannelLabel_DialogCentricMix         = 43,
    
    kAudioChannelLabel_CenterSurroundDirect     = 44,           // back center, non diffuse
    
    kAudioChannelLabel_Haptic                   = 45,
    
    // first order ambisonic channels
    kAudioChannelLabel_Ambisonic_W              = 200,
    kAudioChannelLabel_Ambisonic_X              = 201,
    kAudioChannelLabel_Ambisonic_Y              = 202,
    kAudioChannelLabel_Ambisonic_Z              = 203,
    
    // Mid/Side Recording
    kAudioChannelLabel_MS_Mid                   = 204,
    kAudioChannelLabel_MS_Side                  = 205,
    
    // X-Y Recording
    kAudioChannelLabel_XY_X                     = 206,
    kAudioChannelLabel_XY_Y                     = 207,
    
    // other
    kAudioChannelLabel_HeadphonesLeft           = 301,
    kAudioChannelLabel_HeadphonesRight          = 302,
    kAudioChannelLabel_ClickTrack               = 304,
    kAudioChannelLabel_ForeignLanguage          = 305,
    
    // generic discrete channel
    kAudioChannelLabel_Discrete                 = 400,
    
    // numbered discrete channel
    kAudioChannelLabel_Discrete_0               = (1U<<16) | 0,
    kAudioChannelLabel_Discrete_1               = (1U<<16) | 1,
    kAudioChannelLabel_Discrete_2               = (1U<<16) | 2,
    kAudioChannelLabel_Discrete_3               = (1U<<16) | 3,
    kAudioChannelLabel_Discrete_4               = (1U<<16) | 4,
    kAudioChannelLabel_Discrete_5               = (1U<<16) | 5,
    kAudioChannelLabel_Discrete_6               = (1U<<16) | 6,
    kAudioChannelLabel_Discrete_7               = (1U<<16) | 7,
    kAudioChannelLabel_Discrete_8               = (1U<<16) | 8,
    kAudioChannelLabel_Discrete_9               = (1U<<16) | 9,
    kAudioChannelLabel_Discrete_10              = (1U<<16) | 10,
    kAudioChannelLabel_Discrete_11              = (1U<<16) | 11,
    kAudioChannelLabel_Discrete_12              = (1U<<16) | 12,
    kAudioChannelLabel_Discrete_13              = (1U<<16) | 13,
    kAudioChannelLabel_Discrete_14              = (1U<<16) | 14,
    kAudioChannelLabel_Discrete_15              = (1U<<16) | 15,
    kAudioChannelLabel_Discrete_65535           = (1U<<16) | 65535
};
typedef uint32_t AudioChannelLabel;

/*!
 @enum           Channel Bitmap Constants
 @abstract       These constants are for use in the mChannelBitmap field of an
                 AudioChannelLayout structure.
 */
enum
{
    kAudioChannelBit_Left                       = (1U<<0),
    kAudioChannelBit_Right                      = (1U<<1),
    kAudioChannelBit_Center                     = (1U<<2),
    kAudioChannelBit_LFEScreen                  = (1U<<3),
    kAudioChannelBit_LeftSurround               = (1U<<4),      // WAVE: "Back Left"
    kAudioChannelBit_RightSurround              = (1U<<5),      // WAVE: "Back Right"
    kAudioChannelBit_LeftCenter                 = (1U<<6),
    kAudioChannelBit_RightCenter                = (1U<<7),
    kAudioChannelBit_CenterSurround             = (1U<<8),      // WAVE: "Back Center"
    kAudioChannelBit_LeftSurroundDirect         = (1U<<9),      // WAVE: "Side Left"
    kAudioChannelBit_RightSurroundDirect        = (1U<<10),     // WAVE: "Side Right"
    kAudioChannelBit_TopCenterSurround          = (1U<<11),
    kAudioChannelBit_VerticalHeightLeft         = (1U<<12),     // WAVE: "Top Front Left"
    kAudioChannelBit_VerticalHeightCenter       = (1U<<13),     // WAVE: "Top Front Center"
    kAudioChannelBit_VerticalHeightRight        = (1U<<14),     // WAVE: "Top Front Right"
    kAudioChannelBit_TopBackLeft                = (1U<<15),
    kAudioChannelBit_TopBackCenter              = (1U<<16),
    kAudioChannelBit_TopBackRight               = (1U<<17)
};
typedef uint32_t AudioChannelBitmap;

/*!
 @enum           Channel Coordinate Flags
 @abstract       These constants are used in the mChannelFlags field of an
                 AudioChannelDescription structure.
 @constant       kAudioChannelFlags_RectangularCoordinates
                 The channel is specified by the cartesioan coordinates of the speaker
                 position. This flag is mutally exclusive with
                 kAudioChannelFlags_SphericalCoordinates.
 @constant       kAudioChannelFlags_SphericalCoordinates
                 The channel is specified by the spherical coordinates of the speaker
                 position. This flag is mutally exclusive with
                 kAudioChannelFlags_RectangularCoordinates.
 @constant       kAudioChannelFlags_Meters
                 Set to indicate the units are in meters, clear to indicate the units are
                 relative to the unit cube or unit sphere.
 */
enum
{
    kAudioChannelFlags_AllOff                   = 0,
    kAudioChannelFlags_RectangularCoordinates   = (1U<<0),
    kAudioChannelFlags_SphericalCoordinates     = (1U<<1),
    kAudioChannelFlags_Meters                   = (1U<<2)
};
typedef uint32_t AudioChannelFlags;

// these are indices for acessing the mCoordinates array in struct AudioChannelDescription
/*!
 @enum           Channel Coordinate Index Constants
 @abstract       Constants for indexing the mCoordinates array in an AudioChannelDescription
                 structure.
 @constant       kAudioChannelCoordinates_LeftRight
                 For rectangulare coordinates, negative is left and positive is right.
 @constant       kAudioChannelCoordinates_BackFront
                 For rectangulare coordinates, negative is back and positive is front.
 @constant       kAudioChannelCoordinates_DownUp
                 For rectangulare coordinates, negative is below ground level, 0 is ground
                 level, and positive is above ground level.
 @constant       kAudioChannelCoordinates_Azimuth
                 For spherical coordinates, 0 is front center, positive is right, negative is
                 left. This is measured in degrees.
 @constant       kAudioChannelCoordinates_Elevation
                 For spherical coordinates, +90 is zenith, 0 is horizontal, -90 is nadir.
                 This is measured in degrees.
 @constant       kAudioChannelCoordinates_Distance
                 For spherical coordinates, the units are described by flags.
 */
enum
{
    kAudioChannelCoordinates_LeftRight  = 0,
    kAudioChannelCoordinates_BackFront  = 1,
    kAudioChannelCoordinates_DownUp     = 2,
    kAudioChannelCoordinates_Azimuth    = 0,
    kAudioChannelCoordinates_Elevation  = 1,
    kAudioChannelCoordinates_Distance   = 2
};
typedef uint32_t AudioChannelCoordinateIndex;

/*!
 @enum           AudioChannelLayoutTag Constants
 @abstract       These constants are used in the mChannelLayoutTag field of an AudioChannelLayout
                 structure.
 */
enum
{
    // Some channel abbreviations used below:
    // L - left
    // R - right
    // C - center
    // Ls - left surround
    // Rs - right surround
    // Cs - center surround
    // Rls - rear left surround
    // Rrs - rear right surround
    // Lw - left wide
    // Rw - right wide
    // Lsd - left surround direct
    // Rsd - right surround direct
    // Lc - left center
    // Rc - right center
    // Ts - top surround
    // Vhl - vertical height left
    // Vhc - vertical height center
    // Vhr - vertical height right
    // Lt - left matrix total. for matrix encoded stereo.
    // Rt - right matrix total. for matrix encoded stereo.
    
    //  General layouts
    kAudioChannelLayoutTag_UseChannelDescriptions   = (0U<<16) | 0,     // use the array of AudioChannelDescriptions to define the mapping.
    kAudioChannelLayoutTag_UseChannelBitmap         = (1U<<16) | 0,     // use the bitmap to define the mapping.
    
    kAudioChannelLayoutTag_Mono                     = (100U<<16) | 1,   // a standard mono stream
    kAudioChannelLayoutTag_Stereo                   = (101U<<16) | 2,   // a standard stereo stream (L R) - implied playback
    kAudioChannelLayoutTag_StereoHeadphones         = (102U<<16) | 2,   // a standard stereo stream (L R) - implied headphone playback
    kAudioChannelLayoutTag_MatrixStereo             = (103U<<16) | 2,   // a matrix encoded stereo stream (Lt, Rt)
    kAudioChannelLayoutTag_MidSide                  = (104U<<16) | 2,   // mid/side recording
    kAudioChannelLayoutTag_XY                       = (105U<<16) | 2,   // coincident mic pair (often 2 figure 8's)
    kAudioChannelLayoutTag_Binaural                 = (106U<<16) | 2,   // binaural stereo (left, right)
    kAudioChannelLayoutTag_Ambisonic_B_Format       = (107U<<16) | 4,   // W, X, Y, Z
    
    kAudioChannelLayoutTag_Quadraphonic             = (108U<<16) | 4,   // L R Ls Rs  -- 90 degree speaker separation
    kAudioChannelLayoutTag_Pentagonal               = (109U<<16) | 5,   // L R Ls Rs C  -- 72 degree speaker separation
    kAudioChannelLayoutTag_Hexagonal                = (110U<<16) | 6,   // L R Ls Rs C Cs  -- 60 degree speaker separation
    kAudioChannelLayoutTag_Octagonal                = (111U<<16) | 8,   // L R Ls Rs C Cs Lw Rw  -- 45 degree speaker separation
    kAudioChannelLayoutTag_Cube                     = (112U<<16) | 8,   // left, right, rear left, rear right
    // top left, top right, top rear left, top rear right
    
    //  MPEG defined layouts
    kAudioChannelLayoutTag_MPEG_1_0                 = kAudioChannelLayoutTag_Mono,         //  C
    kAudioChannelLayoutTag_MPEG_2_0                 = kAudioChannelLayoutTag_Stereo,       //  L R
    kAudioChannelLayoutTag_MPEG_3_0_A               = (113U<<16) | 3,                       //  L R C
    kAudioChannelLayoutTag_MPEG_3_0_B               = (114U<<16) | 3,                       //  C L R
    kAudioChannelLayoutTag_MPEG_4_0_A               = (115U<<16) | 4,                       //  L R C Cs
    kAudioChannelLayoutTag_MPEG_4_0_B               = (116U<<16) | 4,                       //  C L R Cs
    kAudioChannelLayoutTag_MPEG_5_0_A               = (117U<<16) | 5,                       //  L R C Ls Rs
    kAudioChannelLayoutTag_MPEG_5_0_B               = (118U<<16) | 5,                       //  L R Ls Rs C
    kAudioChannelLayoutTag_MPEG_5_0_C               = (119U<<16) | 5,                       //  L C R Ls Rs
    kAudioChannelLayoutTag_MPEG_5_0_D               = (120U<<16) | 5,                       //  C L R Ls Rs
    kAudioChannelLayoutTag_MPEG_5_1_A               = (121U<<16) | 6,                       //  L R C LFE Ls Rs
    kAudioChannelLayoutTag_MPEG_5_1_B               = (122U<<16) | 6,                       //  L R Ls Rs C LFE
    kAudioChannelLayoutTag_MPEG_5_1_C               = (123U<<16) | 6,                       //  L C R Ls Rs LFE
    kAudioChannelLayoutTag_MPEG_5_1_D               = (124U<<16) | 6,                       //  C L R Ls Rs LFE
    kAudioChannelLayoutTag_MPEG_6_1_A               = (125U<<16) | 7,                       //  L R C LFE Ls Rs Cs
    kAudioChannelLayoutTag_MPEG_7_1_A               = (126U<<16) | 8,                       //  L R C LFE Ls Rs Lc Rc
    kAudioChannelLayoutTag_MPEG_7_1_B               = (127U<<16) | 8,                       //  C Lc Rc L R Ls Rs LFE    (doc: IS-13818-7 MPEG2-AAC Table 3.1)
    kAudioChannelLayoutTag_MPEG_7_1_C               = (128U<<16) | 8,                       //  L R C LFE Ls Rs Rls Rrs
    kAudioChannelLayoutTag_Emagic_Default_7_1       = (129U<<16) | 8,                       //  L R Ls Rs C LFE Lc Rc
    kAudioChannelLayoutTag_SMPTE_DTV                = (130U<<16) | 8,                       //  L R C LFE Ls Rs Lt Rt
    //      (kAudioChannelLayoutTag_ITU_5_1 plus a matrix encoded stereo mix)
    
    //  ITU defined layouts
    kAudioChannelLayoutTag_ITU_1_0                  = kAudioChannelLayoutTag_Mono,         //  C
    kAudioChannelLayoutTag_ITU_2_0                  = kAudioChannelLayoutTag_Stereo,       //  L R
    
    kAudioChannelLayoutTag_ITU_2_1                  = (131U<<16) | 3,                       //  L R Cs
    kAudioChannelLayoutTag_ITU_2_2                  = (132U<<16) | 4,                       //  L R Ls Rs
    kAudioChannelLayoutTag_ITU_3_0                  = kAudioChannelLayoutTag_MPEG_3_0_A,   //  L R C
    kAudioChannelLayoutTag_ITU_3_1                  = kAudioChannelLayoutTag_MPEG_4_0_A,   //  L R C Cs
    
    kAudioChannelLayoutTag_ITU_3_2                  = kAudioChannelLayoutTag_MPEG_5_0_A,   //  L R C Ls Rs
    kAudioChannelLayoutTag_ITU_3_2_1                = kAudioChannelLayoutTag_MPEG_5_1_A,   //  L R C LFE Ls Rs
    kAudioChannelLayoutTag_ITU_3_4_1                = kAudioChannelLayoutTag_MPEG_7_1_C,   //  L R C LFE Ls Rs Rls Rrs
    
    // DVD defined layouts
    kAudioChannelLayoutTag_DVD_0                    = kAudioChannelLayoutTag_Mono,         // C (mono)
    kAudioChannelLayoutTag_DVD_1                    = kAudioChannelLayoutTag_Stereo,       // L R
    kAudioChannelLayoutTag_DVD_2                    = kAudioChannelLayoutTag_ITU_2_1,      // L R Cs
    kAudioChannelLayoutTag_DVD_3                    = kAudioChannelLayoutTag_ITU_2_2,      // L R Ls Rs
    kAudioChannelLayoutTag_DVD_4                    = (133U<<16) | 3,                       // L R LFE
    kAudioChannelLayoutTag_DVD_5                    = (134U<<16) | 4,                       // L R LFE Cs
    kAudioChannelLayoutTag_DVD_6                    = (135U<<16) | 5,                       // L R LFE Ls Rs
    kAudioChannelLayoutTag_DVD_7                    = kAudioChannelLayoutTag_MPEG_3_0_A,   // L R C
    kAudioChannelLayoutTag_DVD_8                    = kAudioChannelLayoutTag_MPEG_4_0_A,   // L R C Cs
    kAudioChannelLayoutTag_DVD_9                    = kAudioChannelLayoutTag_MPEG_5_0_A,   // L R C Ls Rs
    kAudioChannelLayoutTag_DVD_10                   = (136U<<16) | 4,                       // L R C LFE
    kAudioChannelLayoutTag_DVD_11                   = (137U<<16) | 5,                       // L R C LFE Cs
    kAudioChannelLayoutTag_DVD_12                   = kAudioChannelLayoutTag_MPEG_5_1_A,   // L R C LFE Ls Rs
    // 13 through 17 are duplicates of 8 through 12.
    kAudioChannelLayoutTag_DVD_13                   = kAudioChannelLayoutTag_DVD_8,        // L R C Cs
    kAudioChannelLayoutTag_DVD_14                   = kAudioChannelLayoutTag_DVD_9,        // L R C Ls Rs
    kAudioChannelLayoutTag_DVD_15                   = kAudioChannelLayoutTag_DVD_10,       // L R C LFE
    kAudioChannelLayoutTag_DVD_16                   = kAudioChannelLayoutTag_DVD_11,       // L R C LFE Cs
    kAudioChannelLayoutTag_DVD_17                   = kAudioChannelLayoutTag_DVD_12,       // L R C LFE Ls Rs
    kAudioChannelLayoutTag_DVD_18                   = (138U<<16) | 5,                       // L R Ls Rs LFE
    kAudioChannelLayoutTag_DVD_19                   = kAudioChannelLayoutTag_MPEG_5_0_B,   // L R Ls Rs C
    kAudioChannelLayoutTag_DVD_20                   = kAudioChannelLayoutTag_MPEG_5_1_B,   // L R Ls Rs C LFE
    
    // These layouts are recommended for AudioUnit usage
    // These are the symmetrical layouts
    kAudioChannelLayoutTag_AudioUnit_4              = kAudioChannelLayoutTag_Quadraphonic,
    kAudioChannelLayoutTag_AudioUnit_5              = kAudioChannelLayoutTag_Pentagonal,
    kAudioChannelLayoutTag_AudioUnit_6              = kAudioChannelLayoutTag_Hexagonal,
    kAudioChannelLayoutTag_AudioUnit_8              = kAudioChannelLayoutTag_Octagonal,
    // These are the surround-based layouts
    kAudioChannelLayoutTag_AudioUnit_5_0            = kAudioChannelLayoutTag_MPEG_5_0_B,   // L R Ls Rs C
    kAudioChannelLayoutTag_AudioUnit_6_0            = (139U<<16) | 6,                       // L R Ls Rs C Cs
    kAudioChannelLayoutTag_AudioUnit_7_0            = (140U<<16) | 7,                       // L R Ls Rs C Rls Rrs
    kAudioChannelLayoutTag_AudioUnit_7_0_Front      = (148U<<16) | 7,                       // L R Ls Rs C Lc Rc
    kAudioChannelLayoutTag_AudioUnit_5_1            = kAudioChannelLayoutTag_MPEG_5_1_A,   // L R C LFE Ls Rs
    kAudioChannelLayoutTag_AudioUnit_6_1            = kAudioChannelLayoutTag_MPEG_6_1_A,   // L R C LFE Ls Rs Cs
    kAudioChannelLayoutTag_AudioUnit_7_1            = kAudioChannelLayoutTag_MPEG_7_1_C,   // L R C LFE Ls Rs Rls Rrs
    kAudioChannelLayoutTag_AudioUnit_7_1_Front      = kAudioChannelLayoutTag_MPEG_7_1_A,   // L R C LFE Ls Rs Lc Rc
    
    kAudioChannelLayoutTag_AAC_3_0                  = kAudioChannelLayoutTag_MPEG_3_0_B,   // C L R
    kAudioChannelLayoutTag_AAC_Quadraphonic         = kAudioChannelLayoutTag_Quadraphonic, // L R Ls Rs
    kAudioChannelLayoutTag_AAC_4_0                  = kAudioChannelLayoutTag_MPEG_4_0_B,   // C L R Cs
    kAudioChannelLayoutTag_AAC_5_0                  = kAudioChannelLayoutTag_MPEG_5_0_D,   // C L R Ls Rs
    kAudioChannelLayoutTag_AAC_5_1                  = kAudioChannelLayoutTag_MPEG_5_1_D,   // C L R Ls Rs Lfe
    kAudioChannelLayoutTag_AAC_6_0                  = (141U<<16) | 6,                       // C L R Ls Rs Cs
    kAudioChannelLayoutTag_AAC_6_1                  = (142U<<16) | 7,                       // C L R Ls Rs Cs Lfe
    kAudioChannelLayoutTag_AAC_7_0                  = (143U<<16) | 7,                       // C L R Ls Rs Rls Rrs
    kAudioChannelLayoutTag_AAC_7_1                  = kAudioChannelLayoutTag_MPEG_7_1_B,   // C Lc Rc L R Ls Rs Lfe
    kAudioChannelLayoutTag_AAC_7_1_B                = (183U<<16) | 8,                       // C L R Ls Rs Rls Rrs LFE
    kAudioChannelLayoutTag_AAC_7_1_C                = (184U<<16) | 8,                       // C L R Ls Rs LFE Vhl Vhr
    kAudioChannelLayoutTag_AAC_Octagonal            = (144U<<16) | 8,                       // C L R Ls Rs Rls Rrs Cs
    
    kAudioChannelLayoutTag_TMH_10_2_std             = (145U<<16) | 16,                      // L R C Vhc Lsd Rsd Ls Rs Vhl Vhr Lw Rw Csd Cs LFE1 LFE2
    kAudioChannelLayoutTag_TMH_10_2_full            = (146U<<16) | 21,                      // TMH_10_2_std plus: Lc Rc HI VI Haptic
    
    kAudioChannelLayoutTag_AC3_1_0_1                = (149U<<16) | 2,                       // C LFE
    kAudioChannelLayoutTag_AC3_3_0                  = (150U<<16) | 3,                       // L C R
    kAudioChannelLayoutTag_AC3_3_1                  = (151U<<16) | 4,                       // L C R Cs
    kAudioChannelLayoutTag_AC3_3_0_1                = (152U<<16) | 4,                       // L C R LFE
    kAudioChannelLayoutTag_AC3_2_1_1                = (153U<<16) | 4,                       // L R Cs LFE
    kAudioChannelLayoutTag_AC3_3_1_1                = (154U<<16) | 5,                       // L C R Cs LFE
    
    kAudioChannelLayoutTag_EAC_6_0_A                = (155U<<16) | 6,                       // L C R Ls Rs Cs
    kAudioChannelLayoutTag_EAC_7_0_A                = (156U<<16) | 7,                       // L C R Ls Rs Rls Rrs
    
    kAudioChannelLayoutTag_EAC3_6_1_A               = (157U<<16) | 7,                       // L C R Ls Rs LFE Cs
    kAudioChannelLayoutTag_EAC3_6_1_B               = (158U<<16) | 7,                       // L C R Ls Rs LFE Ts
    kAudioChannelLayoutTag_EAC3_6_1_C               = (159U<<16) | 7,                       // L C R Ls Rs LFE Vhc
    kAudioChannelLayoutTag_EAC3_7_1_A               = (160U<<16) | 8,                       // L C R Ls Rs LFE Rls Rrs
    kAudioChannelLayoutTag_EAC3_7_1_B               = (161U<<16) | 8,                       // L C R Ls Rs LFE Lc Rc
    kAudioChannelLayoutTag_EAC3_7_1_C               = (162U<<16) | 8,                       // L C R Ls Rs LFE Lsd Rsd
    kAudioChannelLayoutTag_EAC3_7_1_D               = (163U<<16) | 8,                       // L C R Ls Rs LFE Lw Rw
    kAudioChannelLayoutTag_EAC3_7_1_E               = (164U<<16) | 8,                       // L C R Ls Rs LFE Vhl Vhr
    
    kAudioChannelLayoutTag_EAC3_7_1_F               = (165U<<16) | 8,                        // L C R Ls Rs LFE Cs Ts
    kAudioChannelLayoutTag_EAC3_7_1_G               = (166U<<16) | 8,                        // L C R Ls Rs LFE Cs Vhc
    kAudioChannelLayoutTag_EAC3_7_1_H               = (167U<<16) | 8,                        // L C R Ls Rs LFE Ts Vhc
    
    kAudioChannelLayoutTag_DTS_3_1                  = (168U<<16) | 4,                        // C L R LFE
    kAudioChannelLayoutTag_DTS_4_1                  = (169U<<16) | 5,                        // C L R Cs LFE
    kAudioChannelLayoutTag_DTS_6_0_A                = (170U<<16) | 6,                        // Lc Rc L R Ls Rs
    kAudioChannelLayoutTag_DTS_6_0_B                = (171U<<16) | 6,                        // C L R Rls Rrs Ts
    kAudioChannelLayoutTag_DTS_6_0_C                = (172U<<16) | 6,                        // C Cs L R Rls Rrs
    kAudioChannelLayoutTag_DTS_6_1_A                = (173U<<16) | 7,                        // Lc Rc L R Ls Rs LFE
    kAudioChannelLayoutTag_DTS_6_1_B                = (174U<<16) | 7,                        // C L R Rls Rrs Ts LFE
    kAudioChannelLayoutTag_DTS_6_1_C                = (175U<<16) | 7,                        // C Cs L R Rls Rrs LFE
    kAudioChannelLayoutTag_DTS_7_0                  = (176U<<16) | 7,                        // Lc C Rc L R Ls Rs
    kAudioChannelLayoutTag_DTS_7_1                  = (177U<<16) | 8,                        // Lc C Rc L R Ls Rs LFE
    kAudioChannelLayoutTag_DTS_8_0_A                = (178U<<16) | 8,                        // Lc Rc L R Ls Rs Rls Rrs
    kAudioChannelLayoutTag_DTS_8_0_B                = (179U<<16) | 8,                        // Lc C Rc L R Ls Cs Rs
    kAudioChannelLayoutTag_DTS_8_1_A                = (180U<<16) | 9,                        // Lc Rc L R Ls Rs Rls Rrs LFE
    kAudioChannelLayoutTag_DTS_8_1_B                = (181U<<16) | 9,                        // Lc C Rc L R Ls Cs Rs LFE
    kAudioChannelLayoutTag_DTS_6_1_D                = (182U<<16) | 7,                        // C L R Ls Rs LFE Cs
    
    kAudioChannelLayoutTag_DiscreteInOrder          = (147U<<16) | 0,                        // needs to be ORed with the actual number of channels
    kAudioChannelLayoutTag_Unknown                  = 0xFFFF0000                            // needs to be ORed with the actual number of channels
};
typedef uint32_t AudioChannelLayoutTag;

/*!
 @struct         AudioChannelDescription
 @abstract       This structure describes a single channel.
 @field          mChannelLabel
                 The AudioChannelLabel that describes the channel.
 @field          mChannelFlags
                 Flags that control the interpretation of mCoordinates.
 @field          mCoordinates
                 An ordered triple that specifies a precise speaker location.
 */
struct AudioChannelDescription
{
	AudioChannelLabel   mChannelLabel;
	AudioChannelFlags   mChannelFlags;
	float				mCoordinates[3];
};
typedef struct AudioChannelDescription AudioChannelDescription;

/*!
 @struct         AudioChannelLayout
 @abstract       This structure is used to specify channel layouts in files and hardware.
 @field          mChannelLayoutTag
                 The AudioChannelLayoutTag that indicates the layout.
 @field          mChannelBitmap
                 If mChannelLayoutTag is set to kAudioChannelLayoutTag_UseChannelBitmap, this
                 field is the channel usage bitmap.
 @field          mNumberChannelDescriptions
                 The number of items in the mChannelDescriptions array.
 @field          mChannelDescriptions
                 A variable length array of AudioChannelDescriptions that describe the
                 layout.
 */
struct AudioChannelLayout
{
	AudioChannelLayoutTag       mChannelLayoutTag;
	AudioChannelBitmap          mChannelBitmap;
	uint32_t					mNumberChannelDescriptions;
	AudioChannelDescription     mChannelDescriptions[1]; // variable length array of mNumberChannelDescriptions elements
};
typedef struct AudioChannelLayout AudioChannelLayout;
	
/*!
 @struct         AudioStreamPacketDescription
 @abstract       This structure describes the packet layout of a buffer of data where the size of
				 each packet may not be the same or where there is extraneous data between
				 packets.
 @field          mStartOffset
				 The number of bytes from the start of the buffer to the beginning of the
				 packet.
 @field          mVariableFramesInPacket
				 The number of sample frames of data in the packet. For formats with a
				 constant number of frames per packet, this field is set to 0.
 @field          mDataByteSize
				 The number of bytes in the packet.
 */
struct  AudioStreamPacketDescription
{
	int64_t   mStartOffset;
	uint32_t  mVariableFramesInPacket;
	uint32_t  mDataByteSize;
};
typedef struct AudioStreamPacketDescription AudioStreamPacketDescription;
	
#pragma pack(pop)
	
#ifdef __cplusplus
}
#endif

#endif // PRORESTYPES_H
