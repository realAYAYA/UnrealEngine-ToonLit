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
// .NAME ProfileVersion - The MPCDI Profile Version
// .SECTION Description
//
// Readers are supposed to be able to read Profiles with the same
// major version, independent of the minor version.
// 
// .AUTHOR Scalable Display Technologies, Inc.

#pragma once
#include "mpcdiMacros.h"

namespace mpcdi {

// .NAME Profile - The MPCDI Profile Version.
struct EXPORT_MPCDI ProfileVersion
{
public:
  ProfileVersion() { MajorVersion = -1; MinorVersion = -1; }

  int MajorVersion;
  int MinorVersion;

 inline bool operator==(const ProfileVersion &pv) const 
{return (MajorVersion==pv.MajorVersion) && (MinorVersion==pv.MinorVersion);} 
};

  
}; // end namespace mpcdi 
