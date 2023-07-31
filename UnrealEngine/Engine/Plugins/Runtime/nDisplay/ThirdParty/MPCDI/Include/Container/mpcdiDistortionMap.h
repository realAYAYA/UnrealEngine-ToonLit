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
// .NAME DistortionMap - An MPCDI DistortionMap Tag, a file.
// .SECTION Description
//
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include "mpcdiHeader.h"
#include "mpcdiMacros.h"
#include "mpcdiPFM.h"
#include <string>

namespace mpcdi {

struct EXPORT_MPCDI DistortionMap: PFM {
  // Description:
  // Set Default Values
  inline 
  DistortionMap(unsigned int sizeX, unsigned int sizeY) : PFM(sizeX,sizeY) {}
 
  // Description:
  // Get/set for path
       mpcdiSetMacro(Path, std::string);
    mpcdiGetRefMacro(Path, std::string);
  mpcdiGetConstMacro(Path, std::string);

protected:
  // Description:
  // member variables
  std::string   m_Path;
};

}; // end namespace mpcdi 


