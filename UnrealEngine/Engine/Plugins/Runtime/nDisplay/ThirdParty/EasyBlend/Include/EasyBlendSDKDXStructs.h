/* =========================================================================

  Program:   Multiple Projector Library
  Language:  C++
  Date:      $Date: 2013-07-26 18:15:26 -0400 (Fri, 26 Jul 2013) $
  Version:   $Revision: 22221 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved
  The source code contained herein is confidential and is considered a 
  trade secret of Scalable Display Technologies, Inc

===================================================================auto== */

#ifndef _EasyBlendSDKDXStructs_H_
#define _EasyBlendSDKDXStructs_H_


// Standard types
typedef unsigned long        EasyBlendSDKDXUINT32;
typedef EasyBlendSDKDXUINT32 EasyBlendSDKDXMapping;
typedef EasyBlendSDKDXUINT32 EasyBlendSDKDXSampling;
typedef EasyBlendSDKDXUINT32 EasyBlendSDKDXProjection;

// Values for Mapping
#define EasyBlendSDKDX_MAPPING_Normalized 0x01
#define EasyBlendSDKDX_MAPPING_Pixel 0x02

// Values for Sampling
#define EasyBlendSDKDX_SAMPLING_Linear      0x01
#define EasyBlendSDKDX_SAMPLING_Anisotropic 0x02
#define EasyBlendSDKDX_SAMPLING_Cubic       0x03
#define EasyBlendSDKDX_SAMPLING_CubicFast   0x04
#define EasyBlendSDKDX_SAMPLING_Lanczos2    0x05
#define EasyBlendSDKDX_SAMPLING_Lanczos3    0x06

// Values for Projection. If you use perspective, then the 
// EasyBlendSDKDX_Frustum is used.
#define EasyBlendSDKDX_PROJECTION_Perspective  0x01
#define EasyBlendSDKDX_PROJECTION_Orthographic 0x02

// The Data Structure
struct EasyBlendSDKDX_Mesh
{
  EasyBlendSDKDXMapping    Mapping;    // Mapping coordinates (s,t) 
                                       // (normalized or pixel)
  EasyBlendSDKDXSampling   Sampling;   // Sampling mode(linear,...)
  EasyBlendSDKDXProjection Projection; // Persective or Orthographic
  EasyBlendSDK_Frustum   Frustum;      // Used if we have perspective viewing
    float AnisValue;
    float Bottom;
    float Top;
    float Left;
    float Right;
    float Version;
    unsigned long Xres;
    unsigned long Yres;
    unsigned long ApproxXres;
    unsigned long ApproxYres;
    void* token;
};

#endif // _EasyBlendSDKDXStructs_H_
