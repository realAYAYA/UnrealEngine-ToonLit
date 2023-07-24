/** 
*
*   \file   dnx_uncompressed_sdk.h
*
*   \copyright
*           Copyright 2019 Avid Technology, Inc.
*           All rights reserved.
*
*   \par    License
*           The following programs are the sole property of Avid Technology, Inc.,
*           and contain its proprietary and confidential information.
*   
*   \details mailto: partners@avid.com
*
*/

#ifndef DNX_UNCOMPRESSED_SDK_H
#define DNX_UNCOMPRESSED_SDK_H

#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

#define DNX_UNCOMPRESSED_VERSION_MAJOR 2
#define DNX_UNCOMPRESSED_VERSION_MINOR 5
#define DNX_UNCOMPRESSED_VERSION_PATCH 2

#ifdef BUILD_NUMBER
#define DNX_UNCOMPRESSED_VERSION_BUILD BUILD_NUMBER
#endif

#ifndef DNX_UNCOMPRESSED_VERSION_BUILD
#define DNX_UNCOMPRESSED_VERSION_BUILD 0
#endif

#define DNX_UNCOMPRESSED_VERSION_RELEASE 'r'
#define DNX_UNCOMPRESSED_VERSION_ALPHA   'a'
#define DNX_UNCOMPRESSED_VERSION_BETA    'b'
#define DNX_UNCOMPRESSED_VERSION_TYPE    DNX_UNCOMPRESSED_VERSION_RELEASE

/**
Error codes
*/
typedef enum DNXUncompressed_Err_t
{
    DNX_UNCOMPRESSED_ERR_SUCCESS     = 0,  ///< API call succeeded
    DNX_UNCOMPRESSED_ERR_FAIL        = 1,  ///< API call failed with message, use callback to get it
    DNX_UNCOMPRESSED_ERR_INVALID_ARG = 2,  ///< Invalid argument passed to API
    DNX_UNCOMPRESSED_ERR_STRUCT_SIZE = 3   ///< structSize field is not equal to sizeof(struct) - probably header\lib version mismatch
} DNXUncompressed_Err_t;

/**
Color component order
*/
typedef enum DNXUncompressed_CCO_t
{
    DNX_UNCOMPRESSED_CCO_INVALID       = 0,     ///< Invalid value
    DNX_UNCOMPRESSED_CCO_YCbYCr        = 1,     ///< YCbYCr   4:2:2   interleaved
    DNX_UNCOMPRESSED_CCO_CbYCrY        = 2,     ///< CbYCrY   4:2:2   interleaved
    DNX_UNCOMPRESSED_CCO_ARGB          = 3,     ///< ARGB             interleaved
    DNX_UNCOMPRESSED_CCO_BGRA          = 4,     ///< BGRA             interleaved
    DNX_UNCOMPRESSED_CCO_RGB           = 5,     ///< RGB              interleaved
    DNX_UNCOMPRESSED_CCO_BGR           = 6,     ///< BGR              interleaved
    DNX_UNCOMPRESSED_CCO_RGBA          = 7,     ///< RGBA             interleaved
    DNX_UNCOMPRESSED_CCO_ABGR          = 8,     ///< ABGR             interleaved
    DNX_UNCOMPRESSED_CCO_YCbCr         = 9,     ///< YCbCr    4:4:4   interleaved
    DNX_UNCOMPRESSED_CCO_YCbCr_Planar  = 10,    ///< YCbCr    4:2:0   planar
    DNX_UNCOMPRESSED_CCO_CbYACrYA      = 11,    ///< CbYACrYA 4:2:2:4 interleaved
    DNX_UNCOMPRESSED_CCO_CbYCrA        = 12,    ///< CbYCrA   4:4:4:4 interleaved
    DNX_UNCOMPRESSED_CCO_YCbCrA_Planar = 13,    ///< YCbCrA   4:2:0:4 planar
    DNX_UNCOMPRESSED_CCO_Alpha         = 14,    ///< A                mono
    DNX_UNCOMPRESSED_CCO_YCbYCr_A      = 15,    ///< YCbYCr   4:2:2   interleaved pixels + planar alpha pixels at the end of frame
    DNX_UNCOMPRESSED_CCO_CbYCrY_A      = 16,    ///< CbYCrY   4:2:2   interleaved pixels + planar alpha pixels at the end of frame
} DNXUncompressed_CCO_t;

/**
Component type
*/
typedef enum DNXUncompressed_CT_t
{
    DNX_UNCOMPRESSED_CT_INVALID     = 0,   ///< Invalid value
    DNX_UNCOMPRESSED_CT_UCHAR       = 1,   ///< 8 bit
    DNX_UNCOMPRESSED_CT_USHORT      = 2,   ///< 16 bit
    DNX_UNCOMPRESSED_CT_USHORT_10_6 = 3,   ///< 10 bit - 6 LSB bits unused
    DNX_UNCOMPRESSED_CT_USHORT_12_4 = 4,   ///< 12 bit - 4 LSB bits unused
    DNX_UNCOMPRESSED_CT_V210        = 5,   ///< V210(padding to 6px). If you want to use apple v210(padding to 48px) set rowBytes to corresponding value
    DNX_UNCOMPRESSED_CT_FLOAT16     = 6,   ///< IEEE 754 half-precision floating-point format
    DNX_UNCOMPRESSED_CT_FLOAT32     = 7,   ///< IEEE 754 single-precision floating-point format
    DNX_UNCOMPRESSED_CT_SHORT_2_14  = 214  ///< 16 bit, fixed point
} DNXUncompressed_CT_t;

/**
Frame Layout
*/
typedef enum DNXUncompressed_FL_t
{
    DNX_UNCOMPRESSED_FL_FULL_FRAME   = 0,   ///< progressive frames (full frame)
    DNX_UNCOMPRESSED_FL_MIXED_FIELDS = 1,   ///< interlaced frames (mixed fields)
} DNXUncompressed_FL_t;

#pragma pack(push, 1)

/**
Uncompressed media params
*/
typedef struct DNXUncompressed_UncompressedParams_t
{
    size_t                structSize;             ///< Size of struct in bytes
    DNXUncompressed_CCO_t colorComponentOrder;    ///< Specifies component order (see DNXUncompressed_CCO_t)
    DNXUncompressed_CT_t  componentType;          ///< Specifies component type (see DNXUncompressed_CT_t)
                                                  ///< Note: decoder may apply conversion when this value doesn't match to internal byte-stream component type
                                                  ///< Now only DNX_UNCOMPRESSED_SHORT_2_14 -> DNX_UNCOMPRESSED_CT_FLOAT32 conversion is supported
    uint32_t              width;                  ///< Frame width in pixels
    uint32_t              height;                 ///< Frame height in pixels
    uint32_t              rowBytes;               ///< Specifies how many bytes each row consumes, useful when row contains extra padding at the end. Could be 0 for default value.
                                                  ///< Applies for interleaved and mono color component orders, for planar applied to Y and A planes
    DNXUncompressed_FL_t  frameLayout;            ///< Specifies frame layout (see DNXUncompressed_FL_t)
    uint32_t              rowBytes2;              ///< Similar to rowBytes, applies to Cb, Cr planes in planar color component orders and for A plane in interleaved pixels + planar alpha
} DNXUncompressed_UncompressedParams_t;

/**
Compressed media params
*/
typedef struct DNXUncompressed_CompressedParams_t
{
    size_t  structSize;                  ///< Size of struct in bytes
    bool    compressAlpha;               ///< Enables Run-length Encoding(RLE), affects alpha channel only
    uint8_t slicesCount;                 ///< Frame slice count 0..255, for the RLE compression only (for the alpha channel and compressAlpha = true)
                                         ///< The settings allows multi-threaded encode/decode for RLE compression because every slice is encoded and decoded separately
                                         ///< The value is saved in byte-stream during encode and decoder won't use more threads than this value for parallel decode of single frame
                                         ///< 0 means default value(16)
} DNXUncompressed_CompressedParams_t;

/**
Type of client function to call when error occurs
\param[in]  message               Error message
\param[in]  userData              Pointer to the object from DNXUncompressed_Options_t userData field, might be useful if client wants to pass additional object into callback
*/
typedef void(*DNXUncompressed_ErrorCallback_t)(const char *message, void *userData);

/**
Encoder\Decoder options
*/
typedef struct DNXUncompressed_Options_t
{
    size_t                          structSize;   ///< Size of struct in bytes
    unsigned int                    threadsCount; ///< Count of threads to use, 0 - current CPU cores count
    void*                           userData;     ///< Any client data to pass into callbacks
    DNXUncompressed_ErrorCallback_t errCallback;  ///< Client function to call when error occurs, if NULL errors will be ignored
} DNXUncompressed_Options_t;

#pragma pack(pop)

/**
Reports the version of the API
\param[out]  major                Major version part
\param[out]  minor                Minor version part
\param[out]  patch                Patch version part
\param[out]  build                Build number version part
\param[out]  releaseType          Release type: 'a' stands for alpha, 'b' stands for beta, 'r' stands for release
*/
void DNXUncompressed_GetVersion(int *major, int *minor, int *patch, int *build, char *releaseType);

/**
Returns buffer size enough to keep encoded frame data
\param[in]  uncParams             Struct describing uncompressed media (DNXUncompressed_UncompressedParams_t)
\param[in]  cmpParams             Struct describing compressed media (DNXUncompressed_CompressedParams_t)
\return                           Size of compressed buffer required
*/
unsigned int DNXUncompressed_GetCompressedBufSize(const DNXUncompressed_UncompressedParams_t *uncParams,
                                                  const DNXUncompressed_CompressedParams_t   *cmpParams);

typedef struct DNXUncompressed_Encoder DNXUncompressed_Encoder;

/**
Creates encoder object
\param[in]  options               Struct describing encoder options, NULL is defaults
\param[out] encoder               Pointer to encoder object
\return                           Error code
*/
DNXUncompressed_Err_t DNXUncompressed_CreateEncoder(const DNXUncompressed_Options_t *options,
                                                    DNXUncompressed_Encoder        **encoder);

/**
Free memory used by encoder object
\param[in]  encoder               Pointer to encoder object
*/
void DNXUncompressed_DestroyEncoder(DNXUncompressed_Encoder *encoder);

/**
Encodes frame into dnx_uncompressed byte-stream
\param[in]  encoder               Pointer to encoder object
\param[in]  uncParams             Struct describing uncompressed media (DNXUncompressed_UncompressedParams_t)
\param[in]  cmpParams             Struct describing compressed media (DNXUncompressed_CompressedParams_t)
\param[in]  src                   Pointer to frame data buffer
\param[in]  srcSize               Size of frame data buffer in bytes
\param[out] dst                   Pointer to encoded buffer
\param[in]  dstSize               Size of encoded buffer in bytes(use DNXUncompressed_GetCompressedBufSize() to get minimum required size)
\param[out] bytesWritten          Actual size of encoded data in dst buffer in bytes
\return                           Error code
*/
DNXUncompressed_Err_t DNXUncompressed_EncodeFrame(DNXUncompressed_Encoder *encoder,
                                                  const DNXUncompressed_UncompressedParams_t *uncParams,
                                                  const DNXUncompressed_CompressedParams_t   *cmpParams,
                                                  const void *src, unsigned int srcSize,
                                                  void *dst, unsigned int dstSize,
                                                  unsigned int *bytesWritten);

/**
Returns buffer size enough to keep decoded frame data
\param[in]  uncParams             Struct describing uncompressed media (DNXUncompressed_UncompressedParams_t)
\return                           Size of uncompressed buffer required
*/
unsigned int DNXUncompressed_GetUncompressedBufSize(const DNXUncompressed_UncompressedParams_t *uncParams);

typedef struct DNXUncompressed_Decoder DNXUncompressed_Decoder;

/**
Creates encoder object
\param[in]  options               Struct describing decoder options, NULL is defaults
\param[out] decoder               Pointer to decoder object
\return                           Error code
*/
DNXUncompressed_Err_t DNXUncompressed_CreateDecoder(const DNXUncompressed_Options_t *options,
                                                    DNXUncompressed_Decoder        **decoder);

/**
Free memory used by decoder object
\param[in]  decoder               Pointer to decoder object
*/
void DNXUncompressed_DestroyDecoder(DNXUncompressed_Decoder *decoder);

/**
Returns automatically detected uncompressed frame description based on byte-stream headers.
Notice that some byte-streams can be decoded into multiple formats and this func returns only the first available (for example y210 can be decoded either as 10-bit YCbYCr or V210 CbYCrY)
\param[in]  decoder               Pointer to decoder object
\param[in]  src                   Pointer to encoded frame data buffer
\param[in]  srcSize               Size of encoded frame data buffer in bytes
\param[out] uncParams             Struct describing uncompressed media (DNXUncompressed_UncompressedParams_t)
\return                           Error code
*/
DNXUncompressed_Err_t DNXUncompressed_ReadMetadata(DNXUncompressed_Decoder *decoder,
                                                   const void *src, unsigned int srcSize,
                                                   DNXUncompressed_UncompressedParams_t *uncParams);

/**
Decodes frame from dnx_uncompressed byte-stream
\param[in]  decoder               Pointer to decoder object
\param[in]  uncParams             Struct describing uncompressed media. Value returned by DNXUncompressed_ReadMetadata() or manually filled struct
\param[in]  src                   Pointer to encoded frame data buffer
\param[in]  srcSize               Size of encoded frame data buffer in bytes
\param[out] dst                   Pointer to decoded buffer
\param[in]  dstSize               Size of decoded buffer in bytes(use DNXUncompressed_GetUncompressedBufSize() to get minimum required size)
\param[out] bytesWritten          Actual size of decoded data in dst buffer in bytes
\return                           Error code
*/
DNXUncompressed_Err_t DNXUncompressed_DecodeFrame(DNXUncompressed_Decoder *decoder,
                                                  const DNXUncompressed_UncompressedParams_t *uncParams,
                                                  const void *src, unsigned int srcSize,
                                                  void *dst, unsigned int dstSize,
                                                  unsigned int *bytesWritten);

#ifdef __cplusplus
}
#endif

#endif
