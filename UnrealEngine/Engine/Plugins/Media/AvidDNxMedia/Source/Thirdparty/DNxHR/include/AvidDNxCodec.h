/** 
*
*   \file   AvidDNxCodec.h
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

#ifndef DNXCODECSDK_H
#define DNXCODECSDK_H

#define DNX_VERSION_MAJOR 2
#define DNX_VERSION_MINOR 5
#define DNX_VERSION_PATCH 2

#ifdef BUILD_NUMBER
#define DNX_VERSION_BUILD BUILD_NUMBER
#endif

#ifndef DNX_VERSION_BUILD
#define DNX_VERSION_BUILD 0
#endif

#define DNX_VERSION_RELEASE 'r'
#define DNX_VERSION_ALPHA   'a'
#define DNX_VERSION_BETA    'b'
#define DNX_VERSION_TYPE    DNX_VERSION_RELEASE

/**
Default values for SHORT_2_14 conversions params
*/
#define DNX_DEFAULT_SHORT_2_14_BLACK_POINT            4096
#define DNX_DEFAULT_SHORT_2_14_WHITE_POINT            60160
#define DNX_DEFAULT_CHROMA_SHORT_2_14_EXCURSION       28672

/**
No error.
*/
#define DNX_NO_ERROR                           0

/**
Exception has occured.
*/
#define DNX_EXCEPTION_ERROR                -1000

/**
Invalid Compression ID.
*/
#define DNX_INVALID_COMPRESSION_ID_ERROR   -1001

/**
Invalid UncompInfo.
*/
#define DNX_INVALID_UNCOMPINFO_ERROR       -1002

/**
Not enough data.
*/
#define DNX_NEED_MORE_DATA_ERROR           -1003

/**
Failed to initialize.
*/
#define DNX_NOT_INITIALIZED_ERROR          -1004

/**
Invalid value.
*/
#define DNX_INVALID_VALUE_ERROR            -1005

/**
Not sufficient input precision
*/
#define DNX_NOT_SUFFICIENT_INPUT_PRECISION -1006

/**
User data label is wrong
*/
#define DNX_WRONG_USER_DATA_LABEL          -1007

/**
User data size is too big
*/
#define DNX_USER_DATA_SIZE_IS_TOO_BIG      -1008

/**
Data is invalid
*/
#define DNX_INVALID_DATA                   -1009


/**
Incompatible param version.
*/
#define DNX_INCOMPATIBLE_PARAM_VERSION    -1010

/**
Invalid lossless alpha usage.
*/
#define DNX_INVALID_LOSSLESS_ALPHA_USAGE  -1011

/**
Usage of V3 compressions for V1/2 workflows.
*/
#define DNX_FORBIDDEN_WORKFLOW            -1012

/**
Alpha is forbidden for LB compression ID.
*/
#define DNX_INVALID_ALPHA_USAGE           -1013

#ifdef __cplusplus
extern "C" {
#endif

/**
The Compression ID.
*/
typedef enum DNX_CompressionID_t
{
    DNX_HQX_1080p_COMPRESSION_ID           = 1235, ///<  Compression ID 1235.
    DNX_SQ_1080p_COMPRESSION_ID            = 1237, ///<  Compression ID 1237.
    DNX_HQ_1080p_COMPRESSION_ID            = 1238, ///<  Compression ID 1238.
    DNX_HQX_720p_COMPRESSION_ID            = 1250, ///<  Compression ID 1250.
    DNX_HQ_720p_COMPRESSION_ID             = 1251, ///<  Compression ID 1251.
    DNX_SQ_720p_COMPRESSION_ID             = 1252, ///<  Compression ID 1252.
    DNX_HQX_1080i_COMPRESSION_ID           = 1241, ///<  Compression ID 1241.
    DNX_SQ_1080i_COMPRESSION_ID            = 1242, ///<  Compression ID 1242.
    DNX_HQ_1080i_COMPRESSION_ID            = 1243, ///<  Compression ID 1243.
    DNX_HQ_TR_1080i_COMPRESSION_ID         = 1244, ///<  Compression ID 1244. Thin raster - stored as 1440 width x 1080 height
    DNX_LB_1080p_COMPRESSION_ID            = 1253, ///<  Compression ID 1253.
    DNX_444_1080p_COMPRESSION_ID           = 1256, ///<  Compression ID 1256.
    DNX_SQ_TR_720p_COMPRESSION_ID          = 1258, ///<  Compression ID 1258. Thin raster - stored as 960 width x 720 height
    DNX_SQ_TR_1080p_COMPRESSION_ID         = 1259, ///<  Compression ID 1259. Thin raster - stored as 1440 width x 1080 height
    DNX_SQ_TR_1080i_COMPRESSION_ID         = 1260, ///<  Compression ID 1260. Thin raster - stored as 1440 width x 1080 height
    DNX_444_COMPRESSION_ID                 = 1270, ///<  Compression ID 1270.
    DNX_HQX_COMPRESSION_ID                 = 1271, ///<  Compression ID 1271.
    DNX_HQ_COMPRESSION_ID                  = 1272, ///<  Compression ID 1272.
    DNX_SQ_COMPRESSION_ID                  = 1273, ///<  Compression ID 1273.
    DNX_LB_COMPRESSION_ID                  = 1274  ///<  Compression ID 1274.
} DNX_CompressionID_t;


/**
The uncompressed Color Component Order.
*/
typedef enum DNX_ColorComponentOrder_t
{
    DNX_CCO_INVALID              = 0x00000000    ///< Invalid value
    ,DNX_CCO_YCbYCr_NoA          = 0x00000001    ///< Y0CbY1Cr
    ,DNX_CCO_CbYCrY_NoA          = 0x00000002    ///< CbY0CrY1
    ,DNX_CCO_ARGB_Interleaved    = 0x00000004    ///< ARGB
    ,DNX_CCO_BGRA_Interleaved    = 0x00000008    ///< BGRA
    ,DNX_CCO_RGB_NoA             = 0x00000040    ///< RGB
    ,DNX_CCO_BGR_NoA             = 0x00000080    ///< BGR
    ,DNX_CCO_RGBA_Interleaved    = 0x00000800    ///< RGBA
    ,DNX_CCO_ABGR_Interleaved    = 0x00001000    ///< ABGR
    ,DNX_CCO_YCbCr_Interleaved   = 0x00002000    ///< YCbCr 444
    ,DNX_CCO_Ch1Ch2Ch3           = 0x00004000    ///< Arbitrary 444 subsampled color components for any colorspace other than RGB and YCbCr
    ,DNX_CCO_Ch1Ch2Ch1Ch3        = 0x00008000    ///< Arbitrary 422 subsampled color components for any colorspace other than RGB and YCbCr with not subsampled channel first
    ,DNX_CCO_Ch2Ch1Ch3Ch1        = 0x00010000    ///< Arbitrary 422 subsampled color components for any colorspace other than RGB and YCbCr with subsampled channel first
    ,DNX_CCO_YCbCr_Planar        = 0x00020000    ///< YCbCr 420 stored in planar form. Y followed by Cb followed by Cr
    ,DNX_CCO_CbYACrYA_Interleaved= 0x00040000    ///< YCbCrA 4224
    ,DNX_CCO_CbYCrA_Interleaved  = 0x00080000    ///< YCbCrA 4444
    ,DNX_CCO_YCbCrA_Planar       = 0x00100000    ///< YCbCrA 4204 stored in planar form. Y followed by Cb followed by Cr followed by A
    ,DNX_CCO_Ch1Ch2Ch3A          = 0x00200000    ///< Arbitrary 4444 subsampled color components for any colorspace other than RGB and YCbCr with Alpha (Alpha channel last)
    ,DNX_CCO_Ch3Ch2Ch1A          = 0x00400000    ///< Arbitrary 4444 subsampled color components for any colorspace other than RGB and YCbCr with Alpha with reversed channel order (Alpha channel last)
    ,DNX_CCO_ACh1Ch2Ch3          = 0x00800000    ///< Arbitrary 4444 subsampled color components for any colorspace other than RGB and YCbCr with Alpha (Alpha channel first)
    ,DNX_CCO_Ch2Ch1ACh3Ch1A      = 0x01000000    ///< Arbitrary 4224 subsampled color components for any colorspace other than RGB and YCbCr with not subsampled channel first with Alpha
    ,DNX_CCO_CbYCrY_A            = 0x02000000    ///< YCbCrA 4224 mixed-planar (separate Alpha plane)
} DNX_ColorComponentOrder_t;

/**
The signal standard.
*/
typedef enum DNX_SignalStandard_t
{
    DNX_SS_INVALID       = 0x000 ///< Invalid value
    ,DNX_SS_Interlaced   = 0x001 ///< HD Interlaced. Refers to SMPTE standard 274M for interlaced material.
    ,DNX_SS_Progressive  = 0x002 ///< HD Progressive. Refers to all SMPTE standards for progressive material.
} DNX_SignalStandard_t;

/**
The type representation of the component data.
*/
typedef enum DNX_ComponentType_t
{
    DNX_CT_INVALID       = 0x000   ///< Invalid value
    ,DNX_CT_UCHAR        = 0x001   ///< 8 bit
    ,DNX_CT_USHORT_10_6  = 0x004   ///< 10 bit
    ,DNX_CT_SHORT_2_14   = 0x008   ///< Fixed point 
    ,DNX_CT_SHORT        = 0x010   ///< 16 bit. Premultiplied by 257. Byte ordering is machine dependent.
    ,DNX_CT_10Bit_2_8    = 0x040   ///< 10 bit in 2_8 format. Byte ordering is fixed. This is to be used with 10-bit 4:2:2 YCbCr components.
    ,DNX_CT_V210         = 0x400   ///< Apple's V210 
    ,DNX_CT_USHORT_12_4  = 0x20000 ///< 12 bit
} DNX_ComponentType_t;

/**
The raster geometry of the uncompressed buffer. One of the characteristics defined by the raster geometry type is the dimensions of the raster.
*/
typedef enum DNX_RasterGeometryType_t
{
    DNX_RGT_INVALID              = 0x0   ///< Invalid value
    ,DNX_RGT_Display             = 0x1   ///< Raster Geometry type is equal to the display size . Should be used to compress 1920 x 1080 frames using thin raster Compression IDs.
    ,DNX_RGT_NativeCompressed    = 0x4   ///< Raster Geometry type is native to the codec. Should be used to compress 1440 x 1080 frames using thin raster Compression IDs.
} DNX_RasterGeometryType_t;

/**
The decode resolution size.
*/
typedef enum DNX_DecodeResolution_t
{
    DNX_DR_INVALID           = 0x0       ///< Invalid value
    ,DNX_DR_Full             = 0x1       ///< Full resolution
    ,DNX_DR_Half             = 0x2       ///< Half resolution
    ,DNX_DR_Quarter          = 0x4       ///< Quarter resolution
} DNX_DecodeResolution_t;

/**
The buffer field order.
*/
typedef enum DNX_BufferFieldOrder_t
{
    DNX_BFO_INVALID          = 0x000     ///< Invalid value
    ,DNX_BFO_Merged_F1_First = 0x001     ///< First line of F1 is first in memory, followed by first line of F2
    ,DNX_BFO_Split_F1_First  = 0x004     ///< All lines of F1 come first, followed by all lines of F2
    ,DNX_BFO_F1_Only         = 0x010     ///< Only lines from F1 are present
    ,DNX_BFO_F2_Only         = 0x020     ///< Only lines from F2 are present
    ,DNX_BFO_Progressive     = 0x040     ///< All lines of the progressive frame are stored in order
    ,DNX_BFO_Prog_Odd_Only   = 0x200     ///< Only odd lines of the progressive frame
    ,DNX_BFO_Prog_Even_Only  = 0x400     ///< Only even lines of the progressive frame
} DNX_BufferFieldOrder_t;

/**
The color volume.
*/
typedef enum DNX_ColorVolume_t
{
    DNX_CV_INVALID          = 0x00     ///< Invalid value
    ,DNX_CV_709             = 0x01     ///< Rec. 709
    ,DNX_CV_2020            = 0x02     ///< Non-constant luminance Rec. 2020
    ,DNX_CV_2020c           = 0x04     ///< Constant luminance Rec. 2020
    ,DNX_CV_OutOfBand       = 0x08     ///< Ay other
} DNX_ColorVolume_t;

/**
The color format.
*/
typedef enum DNX_ColorFormat_t
{
    DNX_CF_INVALID          = 0x00     ///< Invalid value
    ,DNX_CF_YCbCr           = 0x01     ///< YUV
    ,DNX_CF_RGB             = 0x02     ///< RGB
} DNX_ColorFormat_t;


/**
The subsampling.
*/
typedef enum DNX_SubSampling_t
{
    DNX_SSC_INVALID    = 0x00     ///< Invalid value
    ,DNX_SSC_422       = 0x01     ///< 4:2:2
    ,DNX_SSC_444       = 0x02     ///< 4:4:4
    ,DNX_SSC_420       = 0x04     ///< 4:2:0
} DNX_SubSampling_t;

#pragma pack(push, 1) //Set alignment for structs below

/**
The compressed params.
structSize            Size of struct in bytes
width                 Width of the raster, ignored for not RI rasters.
height                Height of the raster, ignored for not RI rasters.
compressionID         Enum representing the compression bitrate and format.
colorVolume           Color volume (DNX_CV_709 etc).
colorFormat           Color format (YCbCr/RGB).
subSampling           Subsampling, used only for RI compressions, LB-HQX, might be DNX_SSC_422 or DNX_SSC_420.
depth                 Workflow bitdepth, used only for RI compressions, might be 10/12 bit for DNX_444_COMPRESSION_ID, DNX_HQX_COMPRESSION_ID and 
                      8/10/12 bit for DNX_HQ_COMPRESSION_ID, DNX_SQ_COMPRESSION_ID, DNX_LB_COMPRESSION_ID. Not the same as SBD field (7.2.3 of ST 2019-1) in bit-stream.
PARC, PARN            Pixel aspect ratio as defined in VC-3, might be nonzero only for RI compressions. In this case 0<PARC,PARN<1024.
CRCpresence           If nonzero, an encoder shall calculate and write to a bitstream CRC value.
VBR                   If nonzero, a bitstream encoded in VBR mode, if zero CBR mode was used. For HD compressions VBR is ignored.
alphaPresence         If nonzero, a bitstream stores an encoded alpha. For HD compressions alphaPresence is ignored.
losslessAlpha         If nonzero, an alpha stored in a bitstream is compressed lossless RLE-based technique.
premultAlpha          If nonzero, a video fill stored in a bitstream is premultiplied by alpha channel.
*/
typedef struct DNX_CompressedParams_t
{
    size_t                    structSize;
    unsigned int              width;
    unsigned int              height;
    DNX_CompressionID_t       compressionID;
    DNX_ColorVolume_t         colorVolume;
    DNX_ColorFormat_t         colorFormat;
    DNX_SubSampling_t         subSampling;
    unsigned int              depth;
    unsigned int              PARC;
    unsigned int              PARN;
    unsigned int              CRCpresence;
    unsigned int              VBR;
    unsigned int              alphaPresence;
    unsigned int              losslessAlpha;
    unsigned int              premultAlpha;
} DNX_CompressedParams_t;

/**
The uncompressed params.
structSize            Size of struct in bytes
compType              Component type (DNX_CT_UCHAR, DNX_CT_SHORT etc).
colorVolume           Color volume (DNX_CV_709 etc).
colorFormat           Color format (YCbCr/RGB).
compOrder             Component order (DNX_CCO_ARGB_Interleaved etc).
fieldOrder            Buffer field order
rgt                   Raster geometry type: use RGT_Display unless you know what you're doing.
interFieldGapBytes    The number of bytes.
rowBytes              The number of bytes between two successive lines of luma component for 4:2:0 subsampling or between two successive lines in 4:2:2 and 4:4:4 cases. Could be 0 for default value.
blackPoint            The black point for conversion in/from S214 (DNX_DEFAULT_SHORT_2_14_BLACK_POINT or custom value)
whitePoint            The white point for conversion in/from S214 (DNX_DEFAULT_SHORT_2_14_WHITE_POINT or custom value)
chromaExcursion       The excursion for conversion in/from S214 (DNX_DEFAULT_CHROMA_SHORT_2_14_EXCURSION or custom value)
rowBytes2             The number of bytes between two successive lines of chroma component for 4:2:0 subsampling. Could be 0 for default value.
*/
typedef struct DNX_UncompressedParams_t
{
    size_t                    structSize;
    DNX_ComponentType_t       compType;
    DNX_ColorVolume_t         colorVolume;
    DNX_ColorFormat_t         colorFormat;
    DNX_ColorComponentOrder_t compOrder;
    DNX_BufferFieldOrder_t    fieldOrder;
    DNX_RasterGeometryType_t  rgt;
    unsigned int              interFieldGapBytes;
    unsigned int              rowBytes;
    //Only for custom SHORT_2_14
    unsigned short            blackPoint;
    unsigned short            whitePoint;
    unsigned short            chromaExcursion;
    //Only for 4:2:0
    unsigned int              rowBytes2;
} DNX_UncompressedParams_t;

/**
The encode operation params.
structSize            Size of struct in bytes
numThreads            The number of threads.
*/
typedef struct DNX_EncodeOperationParams_t
{
    size_t                    structSize;
    unsigned int              numThreads;

} DNX_EncodeOperationParams_t;

/**
The decode operation params.
structSize            Size of struct in bytes
numThreads            The number of threads.
decodeResolution      Decode resolution.
verifyCRC             Nonzero if decoder should verify CRC value if stored.
decodeAlpha           Nonzero if decoder should decode alpha channel if stored.
*/
typedef struct DNX_DecodeOperationParams_t
{
    size_t                    structSize;
    unsigned int              numThreads;
    DNX_DecodeResolution_t    decodeResolution;
    unsigned int              verifyCRC;
    unsigned int              decodeAlpha;

} DNX_DecodeOperationParams_t;

#pragma pack(pop)

/// Opaque encode handle
typedef struct _DNX_Encoder* DNX_Encoder;
/// Opaque decode handle
typedef struct _DNX_Decoder* DNX_Decoder;

/**
Reports the version of the API.
\param[out]  major          The major version.
\param[out]  minor          The minor version.
\param[out]  patch          The patch version.
\param[out]  build          The build number.
\param[out]  releaseType    'a' stands for alpha, 'b' stands for beta, 'r' stands for release.
\return void.
*/
void DNX_GetVersion(
    int*  major, 
    int*  minor, 
    int*  patch, 
    int*  build, 
    char* releaseType
    );

/**
Initializes library.
Should be called before any other method.
\return    Error code or DNX_NO_ERROR if successful.
*/
int DNX_Initialize();

/**
Finalizes library.
No other methods can be called after DNX_Finalize().
\return void.
*/
void DNX_Finalize();

/**
This method parses picture header data and retrieves compression information.
\param[in]   cmpFrame         Pointer to the beginning of a compressed AVID DNxHD frame.
\param[in]   availableSize    Number of bytes available in cmpFrame. Must be at least 45 bytes.
\param[out]  compressedParams Struct with compressed params (See DNX_CompressedParams_t)
\param[out]  signalStandard   Signal standard.
\return                       Error code or DNX_NO_ERROR if successful.
*/
int DNX_GetInfoFromCompressedFrame(
    const void*             cmpFrame,
    unsigned int            availableSize,
    DNX_CompressedParams_t* compressedParams,
    DNX_SignalStandard_t*   signalStandard
    );


/**
Sets the user data.
\param cmpFrame         Pointer to the compressed frame.
\param cmpSize          Number of bytes available in cmpFrame.
\param UDL              User data label. Use "0" to clear user data payload.
\param userData         A pointer to the user data to put into the compressed frame. Ignored if UDL == 0.
\param userDataSize     Size of the userData, maximum is 260. Ignored if UDL == 0.
\return                 Error code or DNX_NO_ERROR if successful.
*/
int DNX_SetUserData(
    void*          cmpFrame,
    unsigned int   cmpSize,
    unsigned int   UDL,
    void*          userData,
    unsigned int   userDataSize
    );

/**
Retrieves the user data from a compressed DNx frame.
\param[in]  cmpFrame              Pointer to the compressed frame.
\param[in]  cmpSize               Number of bytes available in cmpFrame.
\param[out] UDL                   Pointer to the variable where the user data label value will be stored. Pointer can be zero.
\param[out] userData              Pointer to a buffer where the user data will be stored. Pointer can be NULL.
\param[out] userDataBufferSize    Size of the userData buffer.
\return                           Error code or DNX_NO_ERROR if successful.
*/
int DNX_GetUserData(
    void*          cmpFrame,
    unsigned int   cmpSize,
    unsigned int*  UDL,
    void*          userData,
    unsigned int   userDataBufferSize
    );

/**
Returns the maximum possible size in bytes of the DNxHD compressed frame for specified params.
\param[in]  compressedParams Struct with compressed params (See DNX_CompressedParams_t)
\return                     Size of compressed buffer required.
*/
unsigned int DNX_GetCompressedBufferSize(const DNX_CompressedParams_t* compressedParams);

/**
Gets a description of the error associated with an error code.
\param[in]   errorCode      Error code.
\param[out]  errorStrPtr    Pointer to a buffer which receives an error description string. Buffer should be at least 60 characters long.
\return void.
*/
void DNX_GetErrorString(
    int   errorCode, 
    char* errorStrPtr
    );

/**
Creates an instance of DNX encoder and passes it to the caller.
Use DNX_DeleteEncoder to delete the instance of DNX encoder.
\param[in]  compressedParams      Struct with compressed params (See DNX_CompressedParams_t)
\param[in]  uncompressedParams    Struct with compressed params (See DNX_UncompressedParams_t)
\param[in]  opParams              Struct with encode operation params (See DNX_EncodeOperationParams_t)
\param[out] encoder               DNX_Encoder created for these parameters.
\return                           Error code or DNX_NO_ERROR if successful.
*/
int DNX_CreateEncoder(
    const DNX_CompressedParams_t*      compressedParams,
    const DNX_UncompressedParams_t*    uncompressedParams,
    const DNX_EncodeOperationParams_t* opParams,
    DNX_Encoder*                 encoder
    );

/**
Configures existing instance of DNX encoder.
Use DNX_DeleteEncoder to delete the instance of DNX encoder.
\param[in]  compressedParams      Struct with compressed params (See DNX_CompressedParams_t)
\param[in]  uncompressedParams    Struct with compressed params (See DNX_UncompressedParams_t)
\param[in]  opParams              Struct with encode operation params (See DNX_EncodeOperationParams_t)
\param[in,out]  encoder           DNX_Encoder configured for these parameters.
\return                           Error code or DNX_NO_ERROR if successful.
*/
int DNX_ConfigureEncoder(
    const DNX_CompressedParams_t*      compressedParams,
    const DNX_UncompressedParams_t*    uncompressedParams,
    const DNX_EncodeOperationParams_t* opParams,
    DNX_Encoder encoder
    );

/**
Creates an instance of DNX decoder and passes it to the caller.
Use DNX_DeleteDecoder to delete the instance of DNX decoder.
\param[in]  compressedParams      Struct with compressed params (See DNX_CompressedParams_t)
\param[in]  uncompressedParams    Struct with compressed params (See DNX_UncompressedParams_t)
\param[in]  opParams              Struct with decode operation params (See DNX_DecodeOperationParams_t)
\param[out] decoder                 DNX_Decoder created for these parameters.
\return                             Error code or DNX_NO_ERROR if successful.
*/
int DNX_CreateDecoder(
    const DNX_CompressedParams_t*      compressedParams,
    const DNX_UncompressedParams_t*    uncompressedParams,
    const DNX_DecodeOperationParams_t* opParams,
    DNX_Decoder*                 decoder
    );

/**
Configures existing instance of DNX decoder.
Use DNX_DeleteDecoder to delete the instance of DNX decoder.
\param[in]  compressedParams      Struct with compressed params (See DNX_CompressedParams_t)
\param[in]  uncompressedParams    Struct with compressed params (See DNX_UncompressedParams_t)
\param[in]  opParams              Struct with decode operation params (See DNX_DecodeOperationParams_t)
\param[in,out]  decoder             DNX_Decoder configured for these parameters.
\return                             Error code or DNX_NO_ERROR if successful.
*/
int DNX_ConfigureDecoder(
    const DNX_CompressedParams_t*      compressedParams,
    const DNX_UncompressedParams_t*    uncompressedParams,
    const DNX_DecodeOperationParams_t* opParams,
    DNX_Decoder decoder
    );

/**
Encodes frame.
\param  encoder                 A DNX_Encoder object.
\param  inBuf                   Pointer to the uncompressed data buffer. Memory is managed by the caller. The buffer should be 16 byte aligned.
\param  outBuf                  Pointer to the encoded data buffer. Memory is managed by the caller. The buffer should be 16 byte aligned.
\param  inBufSize               Size of the input buffer in bytes.
\param  outBufSize              Size of the output buffer in bytes.
\param  compressedFrameSize     Actual size of encoded data in bytes.
\return                         Error code or DNX_NO_ERROR if successful.
*/
int DNX_EncodeFrame(
    DNX_Encoder   encoder,
    const void*   inBuf,
    void*         outBuf,
    unsigned int  inBufSize,
    unsigned int  outBufSize,
    unsigned int* compressedFrameSize
    );

/**
Decodes frame.
\param  decoder             A DNX_Decoder object.
\param  inBuf               Pointer to the encoded data buffer. Memory is managed by the caller. The buffer should be 16 byte aligned.
\param  outBuf              Pointer to the uncompressed data buffer. Memory is managed by the caller. The buffer should be 16 byte aligned.
\param  inBufSize           Size of the input buffer in bytes.
\param  outBufSize          Size of the output buffer in bytes.
\return                     Error code or DNX_NO_ERROR if successful.
*/
int DNX_DecodeFrame(
    DNX_Decoder  decoder,
    const void*  inBuf,
    void*        outBuf,
    unsigned int inBufSize,
    unsigned int outBufSize
    );

/**
Deletes an instance of a DNX_Encoder passed by the caller.
\param  encoder        A DNX_Encoder object. encoder has been created with a call to DNX_CreateEncoder().
\return void.
*/
void DNX_DeleteEncoder(
    DNX_Encoder encoder
    );

/**
Deletes an instance of a DNX_Decoder passed by the caller.
\param  decoder        A DNX_Decoder object. decoder has been created with a call to DNX_CreateDecoder().
\return void.
*/
void DNX_DeleteDecoder(
    DNX_Decoder decoder
    );

#ifdef __cplusplus
}
#endif

#endif
