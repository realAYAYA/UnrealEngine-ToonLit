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

#ifndef _EasyBlendSDKTypes_H_
#define _EasyBlendSDKTypes_H_


// Description:
// Standard types used within EasyBlend SDK
typedef unsigned long      EasyBlendSDKUINT32;
typedef EasyBlendSDKUINT32 EasyBlendSDKError;
typedef EasyBlendSDKUINT32 EasyBlendSDKMapping;
typedef EasyBlendSDKUINT32 EasyBlendSDKSampling;
typedef EasyBlendSDKUINT32 EasyBlendSDKProjection;


// Description:
// OpenGL Interface Types
typedef unsigned int       EasyBlendSDKGLBuffer;
typedef unsigned int       EasyBlendSDKGLTexture;

// Description:
// Values for Mapping
#define EasyBlendSDK_MAPPING_Normalized   0x01
#define EasyBlendSDK_MAPPING_Pixel        0x02


// Description:
// Values for Sampling
#define EasyBlendSDK_SAMPLING_Linear      0x01
#define EasyBlendSDK_SAMPLING_Anisotropic 0x02
#define EasyBlendSDK_SAMPLING_Cubic       0x03
#define EasyBlendSDK_SAMPLING_CubicFast   0x04
#define EasyBlendSDK_SAMPLING_Keystone    0x05

// Description:
// Values for Projection. 
// If you use perspective projection, then the 
// EasyBlendSDK_Frustum structure is valid.
#define EasyBlendSDK_PROJECTION_Perspective  0x01
#define EasyBlendSDK_PROJECTION_Orthographic 0x02


// Description:
// Profiling flags, usable only with a profiling build of
// the SDK:
//  * DisableExtensionSupports makes EasyBlend ignore extensions;
//    it is used to test EasyBlend performance without extension
//    support.
#define EasyBlendSDK_PROFILING_DisableExtensionSupport  0x01


#endif
