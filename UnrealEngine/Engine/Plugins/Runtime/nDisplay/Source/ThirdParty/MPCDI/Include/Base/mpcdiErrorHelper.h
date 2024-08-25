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
// .NAME ErrorHelper - Helper Functions for error messages.
// .SECTION Description
//
// 
//
// 
// .AUTHOR Scalable Display Technologies, Inc.
//
//

#pragma once
#ifndef __MPCDI_ErrorHelper_H_
#define __MPCDI_ErrorHelper_H_

#include "mpcdiHeader.h"
#include "mpcdiErrors.h"
#include <string>
#include <sstream>

namespace mpcdi 
{
  #define CREATE_ERROR_MSG(_name_,_msg_) \
    std::stringstream ss##_name_; \
    ss##_name_ << _msg_; \
    std::string _name_ = ss##_name_.str(); 

  #define ReturnCustomErrorMacro(err,msg) \
     return ErrorHelper::SetLastError(err,msg);

  class EXPORT_MPCDI ErrorHelper
  {
  public:
    static std::string GetLastError(bool addLeadingSpace=true);
  
    static MPCDI_Error SetLastError(MPCDI_Error error, std::string msg);
    
  private:
    static std::string m_LastErrorMsg;
    static MPCDI_Error m_Error;
  };
}// namespace mpcdi

#endif
