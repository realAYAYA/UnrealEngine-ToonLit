//
//  ProResMetadataWriter.h
//  Copyright (c) 2017 Apple. All rights reserved.
//

#ifndef PRORESMETADATAWRITER_H
#define PRORESMETADATAWRITER_H	1

#include "ProResTypes.h"

#ifdef __cplusplus
extern "C" {
#endif
	
#pragma pack(push, 4)
	
/*!
	 @enum				ProResMetadataWriter Errors
	 @discussion		PRStatus errors returned from the ProResMetadataWriter APIs
	 
	 @constant			kProResMetadataWriterError_RequiredParameterMissing
						Returned if a required parameter is missing.
	 @constant			kProResMetadataWriterError_InvalidParameter
						Returned if any of the parameters is invalid.
	 @constant			kProResMetadataWriterError_AllocationFailed
						Returned if memory allocation failed.
	 @constant			kProResMetadataWriterError_UnsupportedCommonKey
						Returned if the common key is invalid.
	 @constant			kProResMetadataWriterError_ItemAlreadyPresent
						Returned if the item is already present.
	 @constant			kProResMetadataWriterError_NoSuchProperty
						Returned if the property is not supported.
	 @constant			kProResMetadataWriterError_BadData
						Returned if the container contains bad data.
 */
enum {
	kProResMetadataWriterError_RequiredParameterMissing				= -12580,
	kProResMetadataWriterError_InvalidParameter						= -12581,
	kProResMetadataWriterError_AllocationFailed						= -12582,
	kProResMetadataWriterError_UnsupportedCommonKey					= -12583,
	kProResMetadataWriterError_ItemAlreadyPresent					= -12585,
	kProResMetadataWriterError_NoSuchProperty						= -12586,
	kProResMetadataWriterError_BadData								= -12587,
};
	
/*!
	@typedef			ProResMetadataWriterRef
	@abstract			A ProResMetadataWriterRef represents metadata to be written using
						ProResFileWriter.
	@discussion			ProResMetadataWriterRef is an abstract format-agnostic interface to
						metadata to be written. The metadata can represent different metadata
						formats (QT user data, etc.).
 
						A ProResMetadataWriterRef is a PRTypeRef object and must be retained/released.
 */
typedef struct OpaqueProResMetadataWriter *ProResMetadataWriterRef;
	
/*!
	@function			ProResMetadataWriterGetFormat
	@abstract			Returns the format of the data represented by the metadata writer (e.g.,
						QuickTime metadata vs. QuickTime user data).
*/
PR_EXPORT ProResMetadataFormat ProResMetadataWriterGetFormat(ProResMetadataWriterRef writer);
	
/*!
	 @function			ProResMetadataWriterAddItem
	 @abstract			Adds a new item to the metadata container.
 
						Common usage:
 
						ProResMetadataDataType valueDataType = kProResQuickTimeMetadataType32BitSignedInteger;
						int32_t value = 1984;
						PRIndex valueSize = sizeof(int32_t);
 
						status = ProResMetadataWriterAddItem(writer,
															 kProResMetadataKeyFormatLong, 
															 (const uint8_t *)kProResQuickTimeMetadataKey_Author,
															 kProResQuickTimeMetadataType32BitSignedInteger, 
															 &value, 
															 valueSize);
 
						- or -
 
						uint32_t key = kProResUserDataShortKey_Description;
						ProResMetadataDataType valueDataType = kProResQuickTimeUserDataTypeUTF8;
						const char* value = "foo";
						PRIndex valueSize = 3;
						 
						status = ProResMetadataWriterAddItem(writer, 
															 kProResMetadataKeyFormatShort,
															 (const uint8_t *)&key, 
															 valueDataType, 
															 value, 
															 valueSize);

	 @param	writer		The metadata writer.
	 @param keyFormat	The format of key.
	 @param	key			The key. If the keyFormat is kProResMetadataKeyFormatLong, the C string
						must be NULL terminated.
	 @param	value		The data type of the value. Cannot be NULL.
	 @param	valueBuffer	"valueSize" bytes are copied from the valueBuffer pointer. Cannot be NULL.
						valueBuffer may be freed after call.
	 @param	valueSize	The size in bytes to copy from valueBuffer. Cannot be NULL.
						Do not add padding for C strings.
	 @param countryIndicator  The country indicator, e.g., 0 (default), GB, CA, FR, DE. Can be NULL.
							  Only applies to QuickTime metadata.
	 @param languageIndicator The language indicator, e.g. 0 (default), eng, fra, deu. Can be NULL.
							  Only applies to QuickTime metadata.
	 @param languageCode	  The language code, e.g. 0 (English), 1 (French), 32768 (Unspecified). Can be NULL.
							  Only applies to QuickTime user data.
*/
PR_EXPORT PRStatus ProResMetadataWriterAddItem(ProResMetadataWriterRef writer,
											   const ProResMetadataKeyFormat keyFormat,
											   const uint8_t *key,
											   ProResMetadataDataType valueDataType,
											   void *valueBuffer,
											   PRIndex valueSize,
											   uint16_t *countryIndicator,
											   uint16_t *languageIndicator,
											   uint16_t *languageCode);
	
/*!
	 @function			ProResMetadataWriterHasKey
	 @abstract			Returns a bool value that indicates whether the given key
						is currently in the container.
 
						 Common usage:
 
						 uint32_t key = kProResUserDataShortKey_CreationDate;
						 bool hasKey = ProResMetadataWriterHasKey(writer,
																  kProResMetadataKeyFormatShort,
																  (const uint8_t *)&key);
						 
						 - or -
						 
						 bool hasKey = ProResMetadataWriterHasKey(writer,
																  kProResMetadataKeyFormatLong,
																  (const uint8_t *)kProResQuickTimeMetadataKey_Author);
 
	 @param	writer		The metadata writer.
	 @param keyFormat	The format of key.
	 @param	key			The key. If the keyFormat is kProResMetadataKeyFormatLong, the C string
						must be NULL terminated.
*/
PR_EXPORT bool ProResMetadataWriterHasKey(ProResMetadataWriterRef writer,
										  const ProResMetadataKeyFormat keyFormat,
										  const uint8_t *key);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // PRORESMETADATAWRITER_H
