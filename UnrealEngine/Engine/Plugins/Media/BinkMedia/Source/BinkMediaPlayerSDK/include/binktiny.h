// Copyright Epic Games, Inc. All Rights Reserved.
// Licensed under the terms of a valid Unreal Engine license agreement,
//   and the separate 'Unreal Engine End User License Agreement for Publishing'.

#ifndef __BINKH__
#define __BINKH__

typedef struct BINK * HBINK;

#define BINKALPHA              0x00100000L // Decompress alpha plane (if present)
#define BINKHDR                0x00000004L // Video is an HDR video
#define BINKUSETRIPLEBUFFERING 0x00000008L // Use triple buffering in the framebuffers

typedef struct BINKRECT {
  S32 Left,Top,Width,Height;
} BINKRECT;

#define BINKMAXDIRTYRECTS 1

typedef struct BINKPLANE
{
  void * Buffer;
  S32 Allocate;
  U32 BufferPitch;
} BINKPLANE;

typedef struct BINKFRAMEPLANESET
{
  BINKPLANE YPlane;
  BINKPLANE cRPlane;
  BINKPLANE cBPlane;
  BINKPLANE APlane;
  BINKPLANE HPlane;
} BINKFRAMEPLANESET;

#define BINKMAXFRAMEBUFFERS 3

typedef struct BINKFRAMEBUFFERS
{
  S32 TotalFrames;
  U32 YABufferWidth;
  U32 YABufferHeight;
  U32 cRcBBufferWidth;
  U32 cRcBBufferHeight;

  U32 FrameNum;  // 0 to (TotalFrames-1)
  BINKFRAMEPLANESET Frames[ BINKMAXFRAMEBUFFERS ];
} BINKFRAMEBUFFERS;


typedef struct BINK {
  U32 Width;                  // Width (1 based, 640 for example)
  U32 Height;                 // Height (1 based, 480 for example)
  U32 Frames;                 // Number of frames (1 based, 100 = 100 frames)
  U32 FrameNum;               // Frame to *be* displayed (1 based)

  U32 LastFrameNum;           // Last frame decompressed or skipped (1 based)
  U32 FrameRate;              // Frame Rate Numerator
  U32 FrameRateDiv;           // Frame Rate Divisor (frame rate=numerator/divisor)
  U32 ReadError;              // Non-zero if a read error has ocurred

  U32 OpenFlags;              // flags used on open
  U32 BinkType;               // Bink flags
  U32 LargestFrameSize;       // Largest frame size
  U32 FrameSize;              // The current frame's size in bytes

  U32 SndSize;                // The current frame sound tracks' size in bytes
  U32 FrameChangePercent;     // very rough percentage of the frame that changed
  S32 NumTracks;              // how many tracks
  S32 NumRects;

  U32 soundon;                // sound turned on?
  U32 videoon;                // video turned on?
  U32 needio;                 // set to 1, if we need an io before the next binkframe
  S32 closing;                // are we closing?

  BINKRECT FrameRects[BINKMAXDIRTYRECTS];// Dirty rects from BinkGetRects

  F32 ColorSpace[16];         // 4x4 matrix to use for colorspace conversion
                              // in HDR, 0 = smpte2084(maxnits/10k)*0.5, 1/2=ct/cp scale, 3/4=ct/cp bias, 5 = maxnits

  U64 FileOffset;             // Offset into the file where the Bink starts        
  U64 Highest1SecRate;        // Highest 1 sec data rate

  BINKFRAMEBUFFERS * FrameBuffers; // Bink frame buffers that we decompress to

} BINK;

RADEXPFUNC void RADEXPLINK BinkGetFrameBuffersInfo( HBINK bink, BINKFRAMEBUFFERS * fbset );
RADEXPFUNC S32  RADEXPLINK BinkAllocateFrameBuffers( HBINK bp, BINKFRAMEBUFFERS * set, U32 minimum_alignment );
RADEXPFUNC void RADEXPLINK BinkRegisterFrameBuffers( HBINK bink, BINKFRAMEBUFFERS * fbset );

#endif
