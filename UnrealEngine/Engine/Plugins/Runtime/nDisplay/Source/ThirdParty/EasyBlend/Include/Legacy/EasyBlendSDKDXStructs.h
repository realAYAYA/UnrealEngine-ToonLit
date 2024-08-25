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

#ifndef _EasyBlend1SDKDXStructs_H_
#define _EasyBlend1SDKDXStructs_H_


// Standard types
typedef unsigned long        EasyBlend1SDKDXUINT32;
typedef EasyBlend1SDKDXUINT32 EasyBlend1SDKDXMapping;
typedef EasyBlend1SDKDXUINT32 EasyBlend1SDKDXSampling;
typedef EasyBlend1SDKDXUINT32 EasyBlend1SDKDXProjection;

// Values for Mapping
#define EasyBlend1SDKDX_MAPPING_Normalized 0x01
#define EasyBlend1SDKDX_MAPPING_Pixel 0x02

// Values for Sampling
#define EasyBlend1SDKDX_SAMPLING_Linear      0x01
#define EasyBlend1SDKDX_SAMPLING_Anisotropic 0x02
#define EasyBlend1SDKDX_SAMPLING_Cubic       0x03
#define EasyBlend1SDKDX_SAMPLING_CubicFast   0x04
#define EasyBlend1SDKDX_SAMPLING_Lanczos2    0x05
#define EasyBlend1SDKDX_SAMPLING_Lanczos3    0x06

// Values for Projection. If you use perspective, then the 
// EasyBlend1SDKDX_Frustum is used.
#define EasyBlend1SDKDX_PROJECTION_Perspective  0x01
#define EasyBlend1SDKDX_PROJECTION_Orthographic 0x02

// The Data Structure
struct EasyBlend1SDKDX_Mesh
{
  EasyBlend1SDKDXMapping    Mapping;    // Mapping coordinates (s,t) 
                                       // (normalized or pixel)
  EasyBlend1SDKDXSampling   Sampling;   // Sampling mode(linear,...)
  EasyBlend1SDKDXProjection Projection; // Persective or Orthographic
  EasyBlend1SDK_Frustum   Frustum;      // Used if we have perspective viewing
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

#endif // _EasyBlend1SDKDXStructs_H_
