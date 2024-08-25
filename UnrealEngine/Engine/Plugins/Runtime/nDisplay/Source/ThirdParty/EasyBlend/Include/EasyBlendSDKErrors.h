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

#ifndef _EasyBlendSDKErrors_H_
#define _EasyBlendSDKErrors_H_

#include "EasyBlendSDKTypes.h"
typedef EasyBlendSDKError MeshSDKError;

#include "EasyBlendSDKPlatforms.h"

// Description:
// Standard error codes returned by the EasyBlend SDK
#define EasyBlendSDK_ERR_S_OK                         0
#define EasyBlendSDK_ERR_E_UNIMPLEMENTED              1
#define EasyBlendSDK_ERR_E_UNABLE_TO_OPEN_FILE        2
#define EasyBlendSDK_ERR_E_FILE_NOT_PARSEABLE         3
#define EasyBlendSDK_ERR_E_NOT_LICENSED               4
#define EasyBlendSDK_ERR_E_FAIL                       5
#define EasyBlendSDK_ERR_E_BAD_ARGUMENTS              6
#define EasyBlendSDK_ERR_E_OUT_OF_MEMORY              7
#define EasyBlendSDK_ERR_E_UNABLE_TO_GENERATE_TEXTURE 8
#define EasyBlendSDK_ERR_E_UNABLE_TO_GENERATE_LIST    9
#define EasyBlendSDK_ERR_E_VIEWPORT_INCORRECT         10
#define EasyBlendSDK_ERR_E_INCORRECT_FILE_VERSION     11
#define EasyBlendSDK_ERR_E_ALREADY_INITIALIZED        12
#define EasyBlendSDK_ERR_E_UNABLE_TO_INITIALIZE_GL_EXTENSIONS   13
#define EasyBlendSDK_ERR_E_INCORRECT_INPUT_TYPE       14
#define EasyBlendSDK_ERR_E_UNSUPPORTED_BY_HARDWARE    15
#define EasyBlendSDK_ERR_E_UNABLE_TO_WRITE_FILE       16
#define EasyBlendSDK_ERR_E_DATA_ALTERED               17
#define EasyBlendSDK_ERR_E_INVALID_MACHINE            18
#define EasyBlendSDK_ERR_E_UNABLE_TO_CREATE_CLIENT_MESH 19
#define EasyBlendSDK_ERR_E_UNABLE_TO_INITIALIZE_VK_EXTENSIONS 20

#define EasyBlendSDK_SUCCEEDED(x) (EasyBlendSDK_ERR_S_OK == (x))
#define EasyBlendSDK_FAILED(x)    (EasyBlendSDK_ERR_S_OK != (x))

// Description:
// Returns a string describing an EasyBlendSDKError Code.
EasyBlendSDK_API const char *
EasyBlendSDK_GetErrorMessage(const EasyBlendSDKError &error);

#endif
