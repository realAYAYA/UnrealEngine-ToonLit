/** 
*
*   \file   dnx_mxf_sdk.h
*           Extension API allows to read\write DNxUncompressed and DNxHR/HD streams to/from MXF files according to SMPTE ST 377, RDD 50 and SMPTE ST 2019-4
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

#ifndef DNXMXF_SDK_H
#define DNXMXF_SDK_H

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

/**
Error codes
*/
typedef enum DNXMXF_Err_t
{
    DNXMXF_SUCCESS     = 0,                 ///< API call succeeded
    DNXMXF_FAIL        = 1,                 ///< API call failed with message, use callback to get it
    DNXMXF_INVALID_ARG = 2,                 ///< Invalid argument passed to API
    DNXMXF_STRUCT_SIZE = 3,                 ///< structSize field is not equal to sizeof(struct) - probably header\lib version mismatch
    DNXMXF_EOF         = 4                  ///< End of file reached
} DNXMXF_Err_t;

/**
MXF operational pattern(SMPTE ST 377 Section 8)
*/
typedef enum DNXMXF_OpPattern_t
{
    DNXMXF_OP_INVALID = 0,                  ///< Invalid value
    DNXMXF_OP_1a      = 1,                  ///< According to SMPTE ST 378
    DNXMXF_OP_Atom    = 2,                  ///< According to SMPTE ST 390
} DNXMXF_OpPattern_t;

/**
MXF wrapping type(SMPTE ST 379)
*/
typedef enum DNXMXF_Wrap_t
{
    DNXMXF_WRAP_INVALID = 0,                ///< Invalid value
    DNXMXF_WRAP_FRAME   = 1,                ///< Frame wrapping
    DNXMXF_WRAP_CLIP    = 2,                ///< Clip wrapping
} DNXMXF_Wrap_t;

/**
Essence type
*/
typedef enum DNXMXF_Essence_t
{
    DNXMXF_ESSENCE_INVALID         = 0,     ///< Invalid value
    DNXMXF_ESSENCE_DNXUNCOMPRESSED = 1,     ///< DNxUncompressed essence(see RDD 50)
    DNXMXF_ESSENCE_DNXHR_HD        = 2,     ///< DNxHR/HD essence(see SMPTE ST 2019-1)
} DNXMXF_Essence_t;

#pragma pack(push, 1)

/**
Rational number type(SMPTE ST 377 Section 4.3)
*/
typedef struct DNXMXF_Rational_t
{
    unsigned int num;                       ///< Numerator
    unsigned int den;                       ///< Denominator
} DNXMXF_Rational_t;

/**
Product Version type(SMPTE ST 377 Section 4.3)
*/
typedef struct DNXMXF_ProductVer_t
{
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint16_t build;
    uint16_t release;
} DNXMXF_ProductVer_t;

/**
Type of client function to call when error occurs
\param[in]  message               Error message
\param[in]  userData              Pointer to the object from DNXMXF_Options_t userData field, might be useful if client wants to pass additional object into callback
*/
typedef void(*DNXMXF_ErrorCallback_t)(const char *message, void *userData);

/**
Reader\Writer options
*/
typedef struct DNXMXF_Options_t
{
    size_t                    structSize;   ///< Size of struct in bytes
    void                     *userData;     ///< Any client data to pass into callbacks
    DNXMXF_ErrorCallback_t    errCallback;  ///< Client function to call when error occurs, if NULL errors will be ignored
} DNXMXF_Options_t;

/**
Params of MXF file for writer
*/
typedef struct DNXMXF_WriterParams_t
{
    size_t                     structSize;    ///< Size of struct in bytes
    const wchar_t             *filePath;      ///< Path to mxf file to write
    DNXMXF_OpPattern_t         op;            ///< Operational pattern to use
    DNXMXF_Wrap_t              wrapType;      ///< MXF wrapping to use
    DNXMXF_Rational_t          frameRate;     ///< Frame rate of mxf file
    const wchar_t             *companyName;   ///< [required] Manufacturer of the equipment or application that created or modified the file(SMPTE ST 377 Section A.3)
    const wchar_t             *productName;   ///< [required] Name of the application which created or modified this file(SMPTE ST 377 Section A.3)
    const wchar_t             *verString;     ///< [required] Human readable name of this application version(SMPTE ST 377 Section A.3)
    const wchar_t             *productUID;    ///< [required] An unique identification for the product which created this file(defined by the manufacturer)(SMPTE ST 377 Section A.3)
                                              ///< 16 bytes in hex separated by dots e.g. 00.00.00.00.00.00.00.00.00.00.00.00.00.00.00.00
    const DNXMXF_ProductVer_t *productVer;    ///< [optional] Version number of this application(SMPTE ST 377 Section A.3)
    /* Without specifying aspectRatio, videoLineMap1, videoLineMap2 the library produces 'Closed/Incomplete' MXF                                           */
    /* Library is able to set videoLineMap1, videoLineMap2 for standard rasters itself, otherwise this is client's responsibility to provide proper values */
    DNXMXF_Rational_t          aspectRatio;   ///< [best effort] Specifies the horizontal to vertical aspect ratio of the whole image(SMPTE ST 377 Section G.2.4)
    uint32_t                   videoLineMap1; ///< [best effort] First active line in first field(SMPTE ST 377 Section G.2.12)
    uint32_t                   videoLineMap2; ///< [best effort] First active line in second field(SMPTE ST 377 Section G.2.12)
    DNXMXF_Essence_t           essence;       ///< [required] Type of essence to write
} DNXMXF_WriterParams_t;

/**
Params of MXF file for reader
*/
typedef struct DNXMXF_ReaderParams_t
{
    size_t                     structSize;  ///< Size of struct in bytes
    const wchar_t             *filePath;    ///< Path to mxf file to read
} DNXMXF_ReaderParams_t;

#pragma pack(pop)

typedef struct DNXMXF_Writer DNXMXF_Writer;

/**
Creates MXF writer object
\param[in]  options               Struct describing writer options, NULL is defaults
\param[in]  params                Params of mxf file to write
\param[out] writer                Pointer to writer object
\return                           Error code
*/
DNXMXF_Err_t DNXMXF_CreateWriter(const DNXMXF_Options_t      *options,
                                 const DNXMXF_WriterParams_t *params,
                                 DNXMXF_Writer              **writer);

/**
Free memory used by MXF writer object
\param[in]  writer                Pointer to writer object
*/
void DNXMXF_DestroyWriter(DNXMXF_Writer *writer);

/**
Writes essence frame to MXF container
\param[in]  writer                Pointer to writer object
\param[in]  src                   Pointer to essence frame data buffer
\param[in]  srcSize               Size of essence frame data buffer in bytes
\return                           Error code
*/
DNXMXF_Err_t DNXMXF_WriteFrame(DNXMXF_Writer *writer,
                               const void *src, unsigned int srcSize);

/**
Stop writing MXF file and flush all content
\param[in]  writer                Pointer to writer object
\return                           Error code
*/
DNXMXF_Err_t DNXMXF_FinishWrite(DNXMXF_Writer *writer);

typedef struct DNXMXF_Reader DNXMXF_Reader;

/**
Creates MXF reader object
\param[in]  options               Struct describing reader options, NULL is defaults
\param[in]  params                Reader params
\param[out] reader                Pointer to reader object
\return                           Error code
*/
DNXMXF_Err_t DNXMXF_CreateReader(const DNXMXF_Options_t      *options,
                                 const DNXMXF_ReaderParams_t *params,
                                 DNXMXF_Reader              **reader);

/**
Free memory used by MXF reader object
\param[in]  reader                Pointer to reader object
*/
void DNXMXF_DestroyReader(DNXMXF_Reader *reader);

/**
Returns buffer size enough to keep any frame in MXF file
\param[in]  reader                Pointer to reader object
\param[out] maxBufSize            Size of buffer in bytes
\return                           Error code
*/
DNXMXF_Err_t DNXMXF_MaxBufSize(DNXMXF_Reader *reader, unsigned int *maxBufSize);

/**
Returns count of frames in MXF file
\param[in]  reader                Pointer to reader object
\param[out] framesCount           Count of frames
\return                           Error code
*/
DNXMXF_Err_t DNXMXF_FramesCount(DNXMXF_Reader *reader, unsigned int *framesCount);

/**
Set current read position to frame by index, useful for random access reading
\param[in]  reader                Pointer to reader object
\param[in]  offset                Number of frames to seek from origin
\param[in]  origin                Position used as reference for the offset (SEEK_SET, SEEK_CUR, SEEK_END - use <cstdio>)
\return                           Error code
*/
DNXMXF_Err_t DNXMXF_Seek(DNXMXF_Reader *reader, long int offset, int origin);

/**
Reads essence frame from MXF container
\param[in]  reader                Pointer to reader object
\param[in]  dst                   Pointer to buffer to write frame into
\param[in]  dstSize               Size of buffer in bytes
\param[out] bytesWritten          Actual frame size of dst in bytes
\return                           Error code, DNXMXF_EOF means all frames were read
*/
DNXMXF_Err_t DNXMXF_ReadFrame(DNXMXF_Reader *reader,
                              void *dst, unsigned int dstSize,
                              unsigned int *bytesWritten);

#ifdef __cplusplus
}
#endif

#endif
