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
// .NAME AlphaMap - An MPCDI Alpha Map 
// .SECTION Description
//
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once

#include "mpcdiHeader.h"
#include "mpcdiMacros.h"
#include "mpcdiDataMap.h"
#include <string>

namespace mpcdi {

  struct AlphaMap: DataMap {
    // Description:
    // Set Default Values
    AlphaMap(unsigned int sizeX, unsigned int sizeY, ComponentDepth componentDepth, BitDepth bitDepth)
		: DataMap(sizeX, sizeY, componentDepth, bitDepth) 
	{}

    // Description:
    // get/set file path. should not be used
         mpcdiSetMacro(Path,std::string);
    mpcdiGetConstMacro(Path,std::string);  

    // Description:
    // Get/Set embedded gamma
         mpcdiSetMacro(GammaEmbedded, double);
    mpcdiGetConstMacro(GammaEmbedded, double);  

  protected:
    // Description:
    // member variables
    std::string m_Path;
    double m_GammaEmbedded; // typically 2.2; 1.0 is linear.
  };

}; // end namespace mpcdi 
