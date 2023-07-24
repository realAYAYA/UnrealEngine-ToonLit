/*!
 * @file    ProResDecoder.h
 *
 * @brief   ProRes decoding API.
 *
 * Copyright (c) 2009-2016 Apple Inc. All rights reserved.
 */

#ifndef PRORES_DECODER_H
#define PRORES_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif


#ifndef PR_PIXEL_FORMAT_DEFINED
/*! Pixel formats */
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
 * Downscale modes.  Half and quarter may be used for fast, draft-mode decoding
 * and are only supported with the '2vuy' pixel format.
 */
typedef enum {
    kPRFullSize,                        ///!< Full size
    kPRHalfSize,                        ///!< Full width, half height (single-field)
    kPRQuarterSize                      ///!< Half width, half height (single-field)
} PRDownscaleMode;


/*!
 * Represents a decoder instance created by PROpenDecoder() and disposed by
 * PRCloseDecoder().
 */
typedef struct PRDecoder* PRDecoderRef;


/*!
 * Specifies a destination buffer for the decoded image.
 */
struct PRPixelBuffer {
    unsigned char*  baseAddr;       //!< Pointer to first pixel (must be 16-byte aligned)
    int             rowBytes;       //!< Number of bytes from first pixel of one line to first pixel of next line (must be multiple of 16)
    PRPixelFormat   format;         //!< Pixel format (2vuy, v210, v216, etc.)
    int             width;          //!< Buffer width in pixels
    int             height;         //!< Buffer height in pixels
};
typedef struct PRPixelBuffer PRPixelBuffer;


/*!
 * Utility routine that returns the minimum bytes per row needed in the
 * destination pixel buffer in order to decode a frame of the specified width
 * using the specified pixel format and downscale mode.
 *
 * @param frameWidth        Full-size frame width (not the downscaled width)
 * @param pixelFormat       Pixel format of the pixel buffer
 * @param downscaleMode     Downscaling mode
 */
int
PRBytesPerRowNeededInPixelBuffer(
        int frameWidth,
        PRPixelFormat pixelFormat, 
        PRDownscaleMode downscaleMode);


/*!
 * Opens a decoder instance and spawns "worker" threads.  If a
 * threadStartupCallback callback function is supplied, each worker
 * thread will call this function upon startup to provide a way for you
 * to adjust the threads' priorities if necessary.
 *
 * @param numThreads    The number of simultaneous processing threads to spawn.
 *                      Set this to 0 to have the decoder spawn according to
 *                      the number of processors detected in the system.
 * @param threadStartupCallback     An optional callback function for each
 *                                  thread to call upon startup.  Set to NULL
 *                                  if no callback is needed.
 *
 * @return A reference to the instantiated decoder, or NULL on failure.
 */
PRDecoderRef
PROpenDecoder(int numThreads, void (*threadStartupCallback)());


/*!
 * Decodes a frame.  It is the caller's responsibility to ensure that
 * destinationImage is a buffer of sufficient size and dimensions to contain
 * the decoded image.
 *
 * @param decoder               A decoder instance returned by PROpenDecoder.
 * @param compressedFrame       Pointer to the compresed frame.
 * @param compressedFrameSize   Size of the compressed frame in bytes.
 * @param destinationBuffer     Output buffer for the decoded image.
 * @param downscaleMode         Requests decoder to perform downscaling.
 * @param ignoreAlpha           If true, decoder will ignore encoded alpha
 *                              channel data and output all opaque values.
 *
 * @return The number of bytes decoded for this frame, or a negative number in case of error.
 */
int
PRDecodeFrame(
        PRDecoderRef decoder,
        const void* compressedFrame,
        int compressedFrameSize,
        PRPixelBuffer* destinationBuffer,
        PRDownscaleMode downscaleMode,
        bool ignoreAlpha);


/*!
 * Closes the encoder, shuts down threads, and releases all resources
 * associated with the decoder instance allocated in PROpenDecoder().
 *
 * @param decoder   The decoder instance to dispose.
 */
void
PRCloseDecoder(PRDecoderRef decoder);


#ifdef __cplusplus
}
#endif

#endif // PRORES_DECODER_H
