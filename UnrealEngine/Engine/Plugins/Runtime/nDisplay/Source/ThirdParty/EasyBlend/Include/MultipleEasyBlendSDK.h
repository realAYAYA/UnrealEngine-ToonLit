/* =========================================================================

  Program:   Multiple Projector Library
  Language:  C++
  Date:      $Date: 2011-09-14 11:32:00 -0400 (Wed, 14 Sep 2011) $
  Version:   $Revision: 16985 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved
  The source code contained herein is confidential and is considered a 
  trade secret of Scalable Display Technologies, Inc

===================================================================auto== */
// .NAME MultipleEasyBlendSDK - Using Multiple SDK.
// .SECTION Description
//
// Consider a single computer driving 3 projectors off of a single graphics
// card. And, you are using the EasyBlend SDK to make the projectors appear
// like one gigantic display.
//
// In this case, you would begin by calibrating normally using
// ScalableDisplayManager or a similar product.
//
// Then, there are 3 cases:
//  (1) Orthegraphic Playback, or playing back a movie.
//  (2) Perspective Playback over a short frustum.
//  (3) Perspective Playback over a large frustume.
//
// (1) If you were looking to play back a movie, the easist thing to
// do is to render the movie across the entire computer into one
// render buffer, and then use one SDK to wark the entire output
// buffer. From the standpoint of the SDK, this case is exactly
// equivalent to a 3 projector system, and no special action is
// required on your part. That is, we use one mesh file, and the mesh
// file warps the entire buffer -- taking care of each of the 3
// projectors at the same time. For this case, you do not need this
// file at all. Please simply do the standard SDK integration, and ignore 
// this file.
//
// (2) If you were looking to render a perspective scene, you could
// treat the system as one large buffer. In this case, you would use a
// single frustum across the entire 3 projectors, using one output
// buffer for the entire system. From the standpoint of the SDK, this
// case is exactly equivalent to a 3 projector system, and no special
// action is required on your part. That is, we use one mesh file, and
// the mesh file warps the entire buffer -- taking care of each of the
// 3 projectors at the same time. For this case, you do not need this
// file at all. Please simply do the standard SDK integration, and
// ignore this file.
//
// (3) Sometimes you have case (2), but the frustum is too
// large. While a 60 degree frustum is probably OK across 3
// projectors, a frustum approaching 180 degrees is too large and will
// result in a loss of quality. That is, An OpenGL camera asked to
// take a picture that is 180 degrees or more will take a picture that
// looks like a fish-eye camera, which has very poor angular
// resolution. In is this case that this file exists to help you with.
//
// In this case, you really want to use 3 frusta, one for each projector.
// This file exists to help you do that.
//
// Note that everything in this file is essentially a convenience function.
// The great majority of the functions below simply call the function for
// a single SDK multiple times.
// 
// .SECTION Usage
// EasyBLendSDK_Mesh **gMesh = NULL;
// int NumSDK = 3
// err = EasyBlendSDK_Multiple_Allocate_And_Initialize(gMesh,NumSDK,File1,
//                                                     File2,File3);
// <check errorcode err>
// for(int i=0;i<NumSDK;i++)
//   <set the OpenGL view ( projection matrix ) based on gMesh[i]>
// <Note: see example code for the SetView function>
// ....
// <in display loop, right before SwapBuffers>
// for(int i=0;i<NumSDK;i++)
//   err = EasyBlendSDK_TransformInputToOutput( gMesh[i] ) 
//   <check errorcode err>
// ....
// <at program exit>
// err = EayBlendSDK_Multiple_DeAllocate_And_Uninitialize( gMesh )
// <check errorcode err>
// 

#ifndef _MultipleEasyBlendSDK_H_
#define _MultipleEasyBlendSDK_H_

#include "EasyBlendSDK.h"
#include <string>
#include <assert.h>
#include <sstream>

#ifndef NDEBUG
#  define VerboseErrors(x) std::cout;
#else
#  define VerboseErrors(x)
#endif

// Description:
// Do all allocation, including the array of Meshes.(the input msm ==
// NULL).  Do the initialization for each mesh. In all failure cases,
// cleanup can be done by EasyBlendSDK_Multiple_DeAllocate_And_Uninitialize;
// The function returns the last error that it comes to. If there is
// one or more errors, an error string is formed from all the errors
// and returned in ErrorMsg.
//
// This function effectively just allocates memory and call
// EasyBlendSDK_Initialize multiple times
//
inline EasyBlendSDKError
EasyBlendSDK_Multiple_Allocate_And_Initialize( EasyBlendSDK_Mesh ** &msm,
                                               const unsigned int &NumSDK,
                                               const char** Filename,
                                               std::string &ErrorMessage);
// Description:
// This function does all the initialization, without allocation.
inline EasyBlendSDKError
EasyBlendSDK_Multiple_Initialize( EasyBlendSDK_Mesh ** &msm,
                                  const unsigned int &NumSDK,
                                  const char** Filename,
                                  std::string &ErrorMessage);


// Description:
// Do all de-allocation and un-initialization.  Handles all failure
// cases from EasyBlendSDK_Multiple_Allocate_And_Initialize. The
// function returns the last error that it comes to. If there is one
// or more errors, an error string is formed from all the errors and
// returned as ErrorMsg.
//
inline EasyBlendSDKError
EasyBlendSDK_Multiple_DeAllocate_And_Uninitialize( EasyBlendSDK_Mesh** &msm,
                                                   const int NumSDK,
                                                   std::string &ErrorMessage);

// Description:
// This function just does the un-initialize, without de-allocation.
inline EasyBlendSDKError
EasyBlendSDK_Multiple_And_Uninitialize( EasyBlendSDK_Mesh** &msm,
                                        const int NumSDK,
                                        std::string &ErrorMessage);
  
/* ====================================================================== */

// Description:
// One way this is used is one big input buffer, and one big output buffer.
// This function is for that.
#if 0
inline EasyBlendSDKError 
EasyBlendSDK_Multiple_SetupOutputBuffer(const int i,
                                        EasyBlendSDKGLBuffer DrawBuffer,
                                        int Offsetx, int Offsety,
                                        int Width, int Height)
{
  return EasyBeldSDK_SetOutputDrawSubBufferWithOffret(msm[i],
                                                      DrawBuffer,
                                                      Offsetx,
                                                      Offsety,
                                                      Width,Height);
}
#endif

/* ====================================================================== */

// Description:
// This calls EasyBlendSDK_TransformInputToOutput in a loop.
// It returns the last error message. You MAY NOT want to use
// this function as the individual Transforms can be done in 
// parallel.
inline EasyBlendSDKError
EasyBlendSDK_Multiple_TransformInputToOutput ( EasyBlendSDK_Mesh **msm,
                                               const int &NumSDK);

/* ====================================================================== */


inline EasyBlendSDKError
EasyBlendSDK_Multiple_Allocate_And_Initialize( EasyBlendSDK_Mesh ** &msm,
                                               const unsigned int &NumSDK,
                                               const char** Filename,
                                               std::string &ErrorMessage)
{
  ErrorMessage.clear();
  assert(msm == NULL);
  EasyBlendSDKError err = EasyBlendSDK_ERR_S_OK;
  // Allocate. Be sure to do entire array.
  msm = new EasyBlendSDK_Mesh*[NumSDK];
  if (msm == NULL) return EasyBlendSDK_ERR_E_OUT_OF_MEMORY;
  unsigned int i;
  for(i=0;i < NumSDK; i++)
    {
      msm[i] = new EasyBlendSDK_Mesh;
      if (msm[i] == NULL)
          {
            ErrorMessage =
              std::string("Error while Allocating memory for EasyBlend SDK. ");
            err =  EasyBlendSDK_ERR_E_OUT_OF_MEMORY;
          }
    }
  if (EasyBlendSDK_FAILED(err)) return err;

  return EasyBlendSDK_Multiple_Initialize(msm,NumSDK,Filename,ErrorMessage);
}

/* ====================================================================== */

inline EasyBlendSDKError 
EasyBlendSDK_Multiple_CheckForNullPointer(EasyBlendSDK_Mesh ** &msm,
                                                              std::string &ErrorMessage)
{
  if(NULL==msm)
    { 
      ErrorMessage = "The Array of EasyBlend SDK Meshes is NULL";
      return EasyBlendSDK_ERR_E_BAD_ARGUMENTS;
    }
  return EasyBlendSDK_ERR_S_OK;
}

/* ====================================================================== */

inline EasyBlendSDKError
EasyBlendSDK_Multiple_Initialize( EasyBlendSDK_Mesh ** &msm,
                                  const unsigned int &NumSDK,
                                  const char** FileName,
                                  std::string &ErrorMessage)
{
  EasyBlendSDKError err = 
    EasyBlendSDK_Multiple_CheckForNullPointer(msm,ErrorMessage);
  if (EasyBlendSDK_FAILED(err)) return err;

  ErrorMessage.clear();
  unsigned int i=0;
  for(i=0;i < NumSDK; i++)
    {
      EasyBlendSDKError localerr = EasyBlendSDK_Initialize(FileName[i],
                                                           msm[i]);
      if (EasyBlendSDK_FAILED(localerr)) 
        {
          err = localerr;
          ErrorMessage += 
            std::string("Error while Initializing EasyBlend SDK with File ")
            + FileName[i] + " : " + EasyBlendSDK_GetErrorMessage(err) + ". ";
          VerboseErrors(<< "Initialize of file " << i+1 << " with Error " << 
                        EasyBlendSDK_GetErrorMessage(err));
        }
    }
  return err;
}

/* ====================================================================== */

template <class T>
inline std::string EasyBlendSDK_ToString(const T &Var)
{
  std::ostringstream stream;
  stream << Var;
  return stream.str();
}

/* ====================================================================== */

inline EasyBlendSDKError
EasyBlendSDK_Multiple_Uninitialize( EasyBlendSDK_Mesh** &msm,
                                                   const unsigned int &NumSDK,
                                                   std::string &ErrorMessage)
{
  EasyBlendSDKError err = 
    EasyBlendSDK_Multiple_CheckForNullPointer(msm,ErrorMessage);
  if (EasyBlendSDK_FAILED(err)) return err;

  ErrorMessage.clear();
  unsigned int i;
  for(i=0;i<NumSDK;i++) 
    {
      EasyBlendSDKError localerr = EasyBlendSDK_Uninitialize(msm[i]);
      if (EasyBlendSDK_FAILED(localerr))
        {
          err = localerr;
          ErrorMessage += 
            std::string("Error while UnInitializing EasyBlend SDK ")
            + EasyBlendSDK_ToString(i+1)
            + " : " + EasyBlendSDK_GetErrorMessage(err) + ". ";
          VerboseErrors(<< "UnInitialize of SDK " << i+1 << " with Error " << 
                        EasyBlendSDK_GetErrorMessage(err));
        }
    }
  return err;
}

/* ====================================================================== */

inline EasyBlendSDKError
EasyBlendSDK_Multiple_DeAllocate_And_Uninitialize( EasyBlendSDK_Mesh** &msm,
                                                   const unsigned int NumSDK,
                                                   std::string &ErrorMessage)
{
  EasyBlendSDKError err = 
    EasyBlendSDK_Multiple_Uninitialize(msm,NumSDK,ErrorMessage);
  if (EasyBlendSDK_FAILED(err)) return err;
  // Above function would return error on NULL msm.
  assert(msm != NULL);

  unsigned int i;
  for(i=0;i<NumSDK;i++) 
    {
      if (msm[i] != NULL) delete msm[i];
      msm[i] = NULL;
    }
  delete[] msm;
  msm = NULL;
  return err;
}

/* ====================================================================== */

inline EasyBlendSDKError
EasyBlendSDK_Multiple_TransformInputToOutput ( EasyBlendSDK_Mesh **msm,
                                               const unsigned int &NumSDK)
{
  if (NULL==msm) return EasyBlendSDK_ERR_S_OK;
  EasyBlendSDKError err = EasyBlendSDK_ERR_S_OK;

  unsigned int i=0;
  for(i=0;i<NumSDK;i++) 
    {
      EasyBlendSDKError localerr = 
        EasyBlendSDK_TransformInputToOutput (msm[i] );
      if (EasyBlendSDK_FAILED(localerr)) 
        {
          err = localerr;
          VerboseErrors(<< "Warp " << i << " with Error " << 
                        EasyBlendSDK_GetErrorMessage(err));
        }
    }
  return err;
}

/* ====================================================================== */

#endif  /* ifndef _MultipleEasyBlendSDK_H_ */
