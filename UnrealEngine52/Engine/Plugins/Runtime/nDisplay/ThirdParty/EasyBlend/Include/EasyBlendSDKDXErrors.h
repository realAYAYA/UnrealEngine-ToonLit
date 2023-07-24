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

#ifndef _EasyBlendSDKDXErrors_H_
#define _EasyBlendSDKDXErrors_H_


typedef unsigned int EasyBlendSDKDXError;

#include "EasyBlendSDKTypes.h"
typedef EasyBlendSDKError MeshSDKError;

// Errors
#define EasyBlendSDKDX_ERR_S_OK                         0
#define EasyBlendSDKDX_ERR_E_UNIMPLEMENTED              1
#define EasyBlendSDKDX_ERR_E_UNABLE_TO_OPEN_FILE        2
#define EasyBlendSDKDX_ERR_E_FILE_NOT_PARSEABLE         3
#define EasyBlendSDKDX_ERR_E_NOT_LICENSED               4
#define EasyBlendSDKDX_ERR_E_FAIL                       5
#define EasyBlendSDKDX_ERR_E_BAD_ARGUMENTS              6
#define EasyBlendSDKDX_ERR_E_OUT_OF_MEMORY              7
#define EasyBlendSDKDX_ERR_E_UNABLE_TO_GENERATE_TEXTURE 8
#define EasyBlendSDKDX_ERR_E_VIEWPORT_INCORRECT         9
#define EasyBlendSDKDX_ERR_E_UNABLE_TO_WARP_FRAME       10
#define EasyBlendSDKDX_ERR_E_UNABLE_TO_CREATE_MESH      11
#define EasyBlendSDKDX_ERR_E_ALREADY_INITIALIZED        12
#define EasyBlendSDKDX_ERR_E_OUTPUT_TEXTURE_NOT_RENDER_TARGET 13
#define EasyBlendSDKDX_ERR_E_INPUT_OUTPUT_IDENTICAL     14
#define EasyBlendSDKDX_ERR_E_UNABLE_TO_WRITE_FILE       15
#define EasyBlendSDKDX_ERR_E_INCORRECT_FILE_VERSION     16

#define EasyBlendSDKDX_SUCCEEDED(x) (EasyBlendSDKDX_ERR_S_OK == (x))
#define EasyBlendSDKDX_FAILED(x)    (EasyBlendSDKDX_ERR_S_OK != (x))

#endif // _EasyBlendSDKDXErrors_H_
