/********************************************************************************//**
\file      OVR_Audio_DynamicRoom.h
\brief     OVR Audio SDK public header file
\copyright Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.
************************************************************************************/
#ifndef OVR_Audio_DynamicRoom_h
#define OVR_Audio_DynamicRoom_h

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------------
// ***** OVR_ALIGNAS
//
#if !defined(OVR_ALIGNAS)
#if defined(__GNUC__) || defined(__clang__)
#define OVR_ALIGNAS(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define OVR_ALIGNAS(n) __declspec(align(n))
#elif defined(__CC_ARM)
#define OVR_ALIGNAS(n) __align(n)
#else
#error Need to define OVR_ALIGNAS
#endif
#endif

/// A 3D vector with float components.
typedef struct OVR_ALIGNAS(4) ovrAudioVector3f_
{
    float x, y, z;
} ovrAudioVector3f;

typedef void(*OVRA_RAYCAST_CALLBACK)(ovrAudioVector3f origin, ovrAudioVector3f direction, ovrAudioVector3f* hit, ovrAudioVector3f* normal, void* pctx);


#ifdef __cplusplus
}
#endif

#endif // OVR_Audio_DynamicRoom_h
