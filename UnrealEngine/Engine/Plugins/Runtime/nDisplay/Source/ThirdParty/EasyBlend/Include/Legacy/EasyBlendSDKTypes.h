/* =========================================================================

  Program:   Multiple Projector Library
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved
  The source code contained herein is confidential and is considered a 
  trade secret of Scalable Display Technologies, Inc

===================================================================auto== */

#ifndef _EasyBlend1SDKTypes_H_
#define _EasyBlend1SDKTypes_H_


// Description:
// Standard types used within EasyBlend SDK
typedef unsigned long      EasyBlend1SDKUINT32;
typedef EasyBlend1SDKUINT32 EasyBlend1SDKError;
typedef EasyBlend1SDKUINT32 EasyBlend1SDKMapping;
typedef EasyBlend1SDKUINT32 EasyBlend1SDKSampling;
typedef EasyBlend1SDKUINT32 EasyBlend1SDKProjection;


// Description:
// OpenGL Interface Types
typedef unsigned int       EasyBlend1SDKGLBuffer;
typedef unsigned int       EasyBlend1SDKGLTexture;

// Description:
// Values for Mapping
#define EasyBlend1SDK_MAPPING_Normalized   0x01
#define EasyBlend1SDK_MAPPING_Pixel        0x02


// Description:
// Values for Sampling
#define EasyBlend1SDK_SAMPLING_Linear      0x01
#define EasyBlend1SDK_SAMPLING_Anisotropic 0x02
#define EasyBlend1SDK_SAMPLING_Cubic       0x03
#define EasyBlend1SDK_SAMPLING_CubicFast   0x04
#define EasyBlend1SDK_SAMPLING_Keystone    0x05

// Description:
// Values for Projection. 
// If you use perspective projection, then the 
// EasyBlend1SDK_Frustum structure is valid.
#define EasyBlend1SDK_PROJECTION_Perspective  0x01
#define EasyBlend1SDK_PROJECTION_Orthographic 0x02


// Description:
// Profiling flags, usable only with a profiling build of
// the SDK:
//  * DisableExtensionSupports makes EasyBlend ignore extensions;
//    it is used to test EasyBlend performance without extension
//    support.
#define EasyBlend1SDK_PROFILING_DisableExtensionSupport  0x01


#endif
