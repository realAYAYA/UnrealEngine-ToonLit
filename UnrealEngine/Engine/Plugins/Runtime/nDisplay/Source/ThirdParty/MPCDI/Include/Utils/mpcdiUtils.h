/* =========================================================================

  Program:   MPCDI Library
  Language:  C++
  Date:      $Date: 2012-08-22 20:19:58 -0400 (Wed, 22 Aug 2012) $
  Version:   $Revision: 19513 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved.
  The MPCDI Library is distributed under the BSD license.
  Please see License.txt distributed with this package.

===================================================================auto== */

#pragma once
#ifndef __MPCDI_Utils_H_
#define __MPCDI_Utils_H_
#include <sstream>
#include "mpcdiProfile.h"

namespace mpcdi {

template <typename T>
T StringToNumber ( const std::string &Text)
{                               
  std::stringstream ss(Text);
  T result;
  return ss >> result ? result : 0;
}

template <typename T>
std::string NumberToString(const T &number)
{
  std::stringstream ss;
  ss<<number;
  std::string result;
  return ss >> result ? result : ""; 
}

struct EXPORT_MPCDI Utils
{
public:
  // Description:
  // Profile Version conversion functions
  static std::string ProfileVersionToString(const ProfileVersion& pv);
  static MPCDI_Error StringToProfileVersion(const std::string &text, ProfileVersion& pv);
};

} // end namespace mpcdi

#endif //__MPCDI_Utils_H_
