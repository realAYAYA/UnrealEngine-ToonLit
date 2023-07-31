/*!
 * @file    ProResEncoder.h
 *
 * @brief   ProRes encoding API.
 *
 * Copyright (c) 2009-2016 Apple Inc. All rights reserved.
 */

#ifndef PRORES_ENCODER_H
#define PRORES_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif


/*! ProRes codec types */
typedef enum {
    kPRType422HQ    = 'apch',           //!< ProRes 422 HQ    (~220 Mbps at 1920x1080x29.97)
    kPRType422      = 'apcn',           //!< ProRes 422       (~145 Mbps at 1920x1080x29.97)
    kPRType422LT    = 'apcs',           //!< ProRes 422 LT    (~100 Mbps at 1920x1080x29.97)
    kPRType422Proxy = 'apco',           //!< ProRes 422 Proxy (~ 45 Mbps at 1920x1080x29.97)
    kPRType4444     = 'ap4h',           //!< ProRes 4444      (~330 Mbps at 1920x1080x29.97 excluding alpha)
    kPRType4444XQ   = 'ap4x'            //!< ProRes 4444 XQ   (~495 Mbps at 1920x1080x29.97 excluding alpha)
} PRCodecType;


/*! ProRes interlace encoding modes */
typedef enum {
    kPRProgressiveScan            = 0,  //!< Progressive encoding mode
    kPRInterlacedTopFieldFirst    = 1,  //!< Interlaced mode; first (top) image line belongs to first temporal field 
    kPRInterlacedBottomFieldFirst = 2   //!< Interlaced mode; second (bottom) image line belongs to first temporal field
} PRInterlaceMode;


#ifndef PR_PIXEL_FORMAT_DEFINED
/*! Source pixel formats */
typedef enum {
    kPRFormat_2vuy  = '2vuy',           //!< 4:2:2   Y'CbCr  8-bit video range (http://developer.apple.com/library/mac/technotes/tn2162/)
    kPRFormat_v210  = 'v210',           //!< 4:2:2   Y'CbCr 10-bit video range (http://developer.apple.com/library/mac/technotes/tn2162/)
    kPRFormat_v216  = 'v216',           //!< 4:2:2   Y'CbCr 16-bit little endian video range (http://developer.apple.com/library/mac/technotes/tn2162/)
    kPRFormat_y416  = 'y416',           //!< 4:4:4:4 AY'CbCr 16-bit little endian full range alpha, video range Y'CbCr
    kPRFormat_r4fl  = 'r4fl',           //!< 4:4:4:4 AY'CbCr 32-bit float (http://developer.apple.com/library/mac/technotes/tn2201/)
    kPRFormat_R10k  = 'R10k',           //!< 4:4:4   Full-range (0-1023)  RGBxx 10-bit RGB, 2 bits padding, 32-bit big endian word per pixel
    kPRFormat_r210  = 'r210',           //!< 4:4:4   Full-range (0-1023)  xxRGB 10-bit RGB, 2 bits padding, 32-bit big endian word per pixel
    kPRFormat_b64a  = 'b64a'            //!< 4:4:4:4 Full-range (0-65535) ARGB  16-bit big endian per component
} PRPixelFormat;
#define PR_PIXEL_FORMAT_DEFINED
#endif


/*!
 * Represents an encoder instance created by PROpenEncoder() and disposed by
 * PRCloseEncoder().
 */
typedef struct PREncoder* PREncoderRef;


/*!
 * Specifies parameters to use for encoding a frame.
 */
struct PREncodingParams {
    PRCodecType     proResType;     //!< Specifies HQ, standard, LT, or Proxy
    PRInterlaceMode interlaceMode;  //!< Specifies interlaced or progressive encoding mode
    bool            preserveAlpha;  //!< When true, the alpha channel in 4:4:4:4 source buffers will be encoded (only applicable to kPRType4444).
};
typedef struct PREncodingParams PREncodingParams;


/*!
 * Specifies a source frame to be encoded.
 */
struct PRSourceFrame {
    const void*     baseAddr;       //!< Pointer to first pixel of source buffer (must be 16-byte aligned)
    int             rowBytes;       //!< Number of bytes from first pixel of one line to first pixel of next line (must be multiple of 16)
    PRPixelFormat   format;         //!< Pixel format of source buffer (2vuy, v210, or v216)
    int             width;          //!< Frame width in pixels (must be multiple of 16)
    int             height;         //!< Frame height in pixels
};
typedef struct PRSourceFrame PRSourceFrame;


/*!
 * Utility routine that returns the maximum and target compressed frame sizes
 * (in bytes) for the specified ProRes codec type and frame dimensions.  The
 * ProRes encoder attempts to encode each frame as close to the target size as
 * possible, while never exceeding the maximum size.
 *
 * @param proResType                Specifies HQ, standard, LT, Proxy, or 4444.
 * @param preserveAlpha             When true and proResType is kPRType4444, 
 *                                  maxCompressedFrameSize is increased to
 *                                  accommodate an upper bound for the losslessly
 *                                  compressed alpha channel.  This argument is
 *                                  ignored for 422 proResTypes.
 * @param frameWidth                Frame width in pixels.
 * @param frameHeight               Frame height in pixels.
 * @param maxCompressedFrameSize    The maximum size of a compressed frame.
 * @param targetCompressedFrameSize The target size of a compressed frame.
 */
void
PRGetCompressedFrameSize(
        PRCodecType proResType,
        bool preserveAlpha,
        int frameWidth,
        int frameHeight,
        int* maxCompressedFrameSize,
        int* targetCompressedFrameSize);


/*!
 * Opens an encoder instance and spawns "worker" threads.  If a
 * threadStartupCallback callback function is supplied, each worker
 * thread will call this function upon startup to provide a way for you
 * to set the threads' priorities.
 *
 * @param numThreads    The number of simultaneous processing threads to spawn.
 *                      Set this to 0 to have the encoder determine this
 *                      automatically based on the number of processors in
 *                      the system.
 * @param threadStartupCallback     An optional callback function for each
 *                                  thread to call upon startup.  Set to NULL
 *                                  if no callback is needed.
 *
 * @return A reference to the instantiated encoder, or NULL on failure.
 */
PREncoderRef
PROpenEncoder(int numThreads, void (*threadStartupCallback)());


/*!
 * Encodes a frame.  It is the caller's responsibility to ensure that
 * destinationPtr is a buffer of sufficient size to contain the compressed
 * frame.  The minimum required size is provided by the maxCompressedFrameSize
 * value returned by the PRGetCompressedFrameSize() function.
 *
 * @param encoder               An encoder instance returned by PROpenEncoder.
 * @param encodingParams        The parameters to use for encoding this frame.
 * @param sourceFrame           The source frame to encode.
 * @param destinationPtr        Location for encoded bitstream to be written.
 * @param destinationSize       Number of bytes available at destinationPtr.
 * @param compressedFrameSize   Returns the actual size of the compressed frame.
 * @param allOpaqueAlpha        Returns true if the encoded alpha channel is
 *                              entirely opaque.  If all frames are opaque, the
 *                              QuickTime movie should be written with the
 *                              'depth' field of the image description set to 24.
 *
 * @return 0 if successful or a nonzero value if an error occurred.
 */
int
PREncodeFrame(
        PREncoderRef encoder,
        const PREncodingParams* encodingParams,
        const PRSourceFrame* sourceFrame,
        void* destinationPtr,
        int   destinationSize,
        int*  compressedFrameSize,
        bool* allOpaqueAlpha);


/*!
 * Closes the encoder, shuts down threads, and releases all resources
 * associated with the encoder instance allocated in PROpenEncoder().
 *
 * @param encoder   The encoder instance to dispose.
 */
void
PRCloseEncoder(PREncoderRef encoder);


#ifdef __cplusplus
}
#endif

#endif // PRORES_ENCODER_H
