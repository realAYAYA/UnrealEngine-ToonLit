//
//  ProResMetadataReader.h
//  Copyright (c) 2017 Apple. All rights reserved.
//

#ifndef PRORESMETADATAREADER_H
#define PRORESMETADATAREADER_H	1

#include "ProResTypes.h"

#ifdef __cplusplus
extern "C" {
#endif
	
#pragma pack(push, 4)
	
/*!
	 @enum				ProResMetadataReader Errors
	 @discussion		Errors returned from the ProResMetadataReader APIs
	 
	 @constant			kProResMetadataReaderError_RequiredParameterMissing
						Returned if a required parameter is missing.
	 @constant			kProResMetadataReaderError_InvalidParameter
						Returned if any of the parameters is invalid.
	 @constant			kProResMetadataReaderError_AllocationFailed
						Returned if memory allocation failed.
	 @constant			kProResMetadataReaderError_NoSuchKey
						Returned if container does not contain the given key.
	 @constant			kProResMetadataReaderError_UnsupportedCommonKey
						Returned if container does not support the given common key.
	 @constant			kProResMetadataReaderError_NoSuchItem
						Returned if container does not contain the given item.
	 @constant			kProResMetadataReaderError_NoSuchValueType
						Returned if the requested value type is not available.
	 @constant			kProResMetadataReaderError_BadData
						Returned if the container contains bad data.
	 @constant			kProResMetadataReaderError_InsufficientData
						Returned if there is insufficient data to complete the request.
	 @constant			kProResMetadataReaderError_InsufficientKeyBufferSize
						Returned if the key's buffer needs to be larger.
	 @constant			kProResMetadataReaderError_InsufficientValueBufferSize
						Returned if the value's buffer needs to be larger.
*/
enum {
	kProResMetadataReaderError_RequiredParameterMissing			= -12600,
	kProResMetadataReaderError_InvalidParameter					= -12601,
	kProResMetadataReaderError_AllocationFailed					= -12602,
	kProResMetadataReaderError_NoSuchKey						= -12604,
	kProResMetadataReaderError_UnsupportedCommonKey				= -12605,
	kProResMetadataReaderError_NoSuchItem						= -12607,
	kProResMetadataReaderError_NoSuchValueType					= -12608,
	kProResMetadataReaderError_BadData							= -12609,
	kProResMetadataReaderError_InsufficientData					= -12610,
	kProResMetadataReaderError_NoSuchDataType					= -12611,
	kProResMetadataReaderError_InsufficientKeyBufferSize		= -12612,
	kProResMetadataReaderError_InsufficientValueBufferSize		= -12613,
};

/*!
	@typedef			ProResMetadataReaderRef
	@abstract			A ProResMetadataReaderRef represents a metadata container reader.
	@discussion			ProResMetadataReaderRef is an abstract format-agnostic reader interface to
						metadata. The metadata can represent different metadata formats (QT
						user data, etc.).
 
						A ProResMetadataReaderRef is a PRTypeRef object and must be retained/released.
*/
typedef struct OpaqueProResMetadataReader *ProResMetadataReaderRef;
	
/*!
	 @function			ProResMetadataReaderGetFormat
	 @abstract			Returns the format of the data represented by the metadata reader (e.g., QuickTime metadata
						vs. QuickTime user data).
*/
PR_EXPORT ProResMetadataFormat ProResMetadataReaderGetFormat(ProResMetadataReaderRef metadataReader);

/*!
	 @function			ProResMetadataReaderCopyValue
	 @abstract			Copy the value of an item with the given key.
	 @discussion		This is the simple high level API that can be used to retrieve the value
						with the given key.
 
						Common usage sample (doesn't cover all cases; only for illustrative purposes):
						
						status = ProResMetadataReaderCopyValue(reader,
															   kProResMetadataKeyFormatLong,
															   key,
															   &valueDataType,
															   NULL,
															   &valueSize);
						if (valueDataType == kProResQuickTimeMetadataTypeBinary) {
							uint8_t *data = malloc(valueSize);
							status = ProResMetadataReaderCopyValue(reader,
																   kProResMetadataKeyFormatLong,
																   key,
																   NULL,
																   data,
																   NULL);
						}
						else if (valueDataType == kProResQuickTimeMetadataTypeSignedIntegerBE
								 && valueSize == 4) {
							int32_t number;
							status = ProResMetadataReaderCopyValue(reader,
																   kProResMetadataKeyFormatLong,
																   key,
																   NULL,
																   &number,
																   NULL);
						}
						else if (valueDataType == kProResQuickTimeMetadataTypeUTF16BE) {
							valueSize += 2; // add 2 bytes for NULL character padding, otherwise
											// ProResMetadataReaderCopyValue will return
											// kProResMetadataReaderError_InsufficientValueBufferSize
							uint8_t *utf16str = malloc(valueSize);
							status = ProResMetadataReaderCopyValue(reader,
																   kProResMetadataKeyFormatLong,
																   key,
																   NULL,
																   &utf16str,
																   NULL);
						}
						else if (valueDataType == kProResQuickTimeMetadataTypeUTF8 ||
								 valueDataType == kProResQuickTimeMetadataTypeMacEncodedText) {
							valueSize += 1; // add 1 byte for NULL character padding, otherwise
											// ProResMetadataReaderCopyValue will return
											// kProResMetadataReaderError_InsufficientValueBufferSize
							uint8_t *string = malloc(valueSize);
							status = ProResMetadataReaderCopyValue(reader,
																   kProResMetadataKeyFormatLong,
																   key,
															       NULL,
																   &string,
																   NULL);
						}
 }
 
	 @param	reader				 The metadata reader.
	 @param keyFormat			 The format of key.
	 @param	key					 The key. If the keyFormat is kProResMetadataKeyFormatLong, the C string
								 must be NULL terminated.
	 @param	valueDataTypeOut	 The data type of the value. Can be NULL.
	 @param	valueBuffer			 Copies the value to this pointer. Can be NULL.
	 @param	valueSizeOut		 The size in bytes that valueBuffer's data must be. Can be NULL.
*/
PR_EXPORT PRStatus ProResMetadataReaderCopyValue(ProResMetadataReaderRef reader,
												 const ProResMetadataKeyFormat keyFormat,
												 const uint8_t *key,
												 ProResMetadataDataType *valueDataTypeOut,
												 void *valueBuffer,
												 PRIndex *valueSizeOut);

/*!
	@function		ProResMetadataReaderCopyLocale
	@abstract		Copy the locale information of an item with the given key.
					This is only applicable to QuickTime metadata, _not_ user data.
	@discussion		If status != 0, this information is unavailable or not applicable.
	@param			countryIndicatorOut	 The country indicator, e.g., 0 (default), GB, CA, FR, DE.
	@param			languageIndicatorOut The language indicator, e.g. 0 (default), eng, fra, deu.
*/
PR_EXPORT PRStatus ProResMetadataReaderCopyLocale(ProResMetadataReaderRef reader,
												  const uint8_t *key,
												  uint16_t *countryIndicatorOut,
												  uint16_t *languageIndicatorOut);
	
/*!
	@function		ProResMetadataReaderCopyLanguageCode
	@abstract		Copy the language code of an item with the given key.
					This is only applicable to QuickTime user data, _not_ metadata.
	@discussion		If status != 0, this information is unavailable or not applicable.
*/
PR_EXPORT PRStatus ProResMetadataReaderCopyLanguageCode(ProResMetadataReaderRef reader,
														const uint8_t *key,
														uint16_t *languageCode);

/*!
	 @function			ProResMetadataReaderHasKey
	 @abstract			Returns a bool value that indicates whether the given key
						is currently in the container.
 
						Common usage:
 
						uint32_t key = kProResUserDataShortKey_CreationDate;
						bool hasKey = ProResMetadataReaderHasKey(reader,
																 kProResMetadataKeyFormatShort,
																 (const uint8_t *)&key);
 
						- or -
 
						bool hasKey = ProResMetadataReaderHasKey(reader,
																 kProResMetadataKeyFormatLong,
																 (const uint8_t *)kProResQuickTimeMetadataKey_Author);
	 
	 @param	reader		The metadata reader.
	 @param keyFormat	The format of key.
	 @param	key			The key. If the keyFormat is kProResMetadataKeyFormatLong, the C string
						must be NULL terminated.
*/
PR_EXPORT bool ProResMetadataReaderHasKey(ProResMetadataReaderRef reader,
										  const ProResMetadataKeyFormat keyFormat,
										  const uint8_t *key);

/*!
	 @function			ProResMetadataReaderGetKeyCount
	 @abstract			Returns the total number of all keys in the container.
	 
	 @param	reader		The metadata reader.
*/
PR_EXPORT PRIndex ProResMetadataReaderGetKeyCount(ProResMetadataReaderRef reader);

/*!
	 @function			ProResMetadataReaderCopyKeyAtIndex
	 @abstract			Returns the key at the given key index.
 
						Common usage:
				
						PRIndex keyCount = ProResMetadataReaderGetKeyCount(metadataReader);
						for (PRIndex ii = 0; ii < keyCount && status == 0; ii++) {
							ProResMetadataKeyFormat keyFormat;
							PRIndex keySize;
							status = ProResMetadataReaderCopyKeyAtIndex(metadataReader,
																	    ii,
																	    &keyFormat,
																	    NULL,
																	    &keySize);
							if (status == 0) {
								if (keyFormat == kProResMetadataKeyFormatLong)
									keySize += 1; // add a byte for the NULL character
								uint8_t *key = malloc(keySize);
								status = ProResMetadataReaderCopyKeyAtIndex(metadataReader,
																			ii,
																			NULL,
																			key,
																			NULL);
								if (status == 0) {
									// use ProResMetadatareaderCopyValue sample code above.
								}
							}
						}
	 
	 @param	reader		 The metadata reader.
	 @param	keyIndex	 The position of the key in the key list.
	 @param keyFormatOut keyOut's format (short or long). Can be NULL.
	 @param	keyBuffer	 Copies the key to this pointer. Can be NULL.
	 @param keySizeOut	 The size in bytes that keyBuffer's data must be. Can be NULL.
						 If the keyFormat is kProResMetadataKeyFormatShort, this will be 4.
						 If the keyFormat is kProResMetadataKeyFormatLong, this will be the
						 equivalent of calling strlen(key); the keySize does not reflect
						 the trailing NULL character of the C string (see above).
*/
PR_EXPORT PRStatus ProResMetadataReaderCopyKeyAtIndex(ProResMetadataReaderRef reader,
													  PRIndex keyIndex,
													  ProResMetadataKeyFormat *keyFormatOut,
													  uint8_t *keyBuffer,
													  PRIndex *keySizeOut);
	
#pragma pack(pop)
	
#ifdef __cplusplus
}
#endif

#endif // PRORESMETADATAREADER_H
