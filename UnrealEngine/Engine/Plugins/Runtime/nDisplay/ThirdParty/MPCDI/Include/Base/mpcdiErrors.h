/* =========================================================================

  Program:   MPCDI Library
  Language:  C++
  Date:      $Date: 2012-02-08 11:39:41 -0500 (Wed, 08 Feb 2012) $
  Version:   $Revision: 18341 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved.
  The MPCDI Library is distributed under the BSD license.
  Please see License.txt distributed with this package.

===================================================================auto== */

#pragma once

#ifndef __mcpcdiError_H_
#define __mcpcdiError_H_

namespace mpcdi 
{
  typedef enum
  {
      MPCDI_SUCCESS  =  0, // Success
      MPCDI_FAILURE  = 1,
      MPCDI_PNG_READ_ERROR = 2,
      MPCDI_PNG_WRITE_ERROR = 3,
      MPCDI_PFM_READ_ERROR = 4,
      MPCDI_PFM_WRITE_ERROR = 5,
      MPCDI_FILE_ALREADY_EXISTS = 6,
      MPCDI_FILE_NOT_FOUND = 7,
      MPCDI_FILE_PERMISSION_ERROR = 8,
      MPCDI_INDEX_OUT_OF_RANGE = 9,
      MPCDI_NON_UNIQUE_ID = 10,
      MPCDI_FAILED_TO_FIND_XML_ATTRIBUTE = 11,
      MPCDI_XML_FORMAT_ERROR = 12, 
      MPCDI_REGION_DOES_NOT_EXISTS = 13,
      MPCDI_ARCHIVE_ERROR = 14,
      MPCDI_VALUE_OUT_OF_RANGE = 15,
      MPCDI_UNSPORTED_PROFILE_VERSION = 16
  } MPCDI_Error;

  #define MPCDI_SUCCEEDED(num) ((num) == mpcdi::MPCDI_SUCCESS)
  #define MPCDI_FAILED(num)    ((num) != mpcdi::MPCDI_SUCCESS)
} //mpcdi namespace

#endif // __mcpcdiError_H_
