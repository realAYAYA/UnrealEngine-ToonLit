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
// .NAME PfmIO - Read and write PFM files
// .SECTION Description
//
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once

#include "mpcdiHeader.h"
#include "mpcdiErrors.h"
#include "mpcdiPFM.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

namespace mpcdi {
  class PfmIO
  {
    public:
      // Description:
      // read functions
      static MPCDI_Error Read(std::string fileName, mpcdi::PFM *&pfm);
      static MPCDI_Error Read(std::istream &source, mpcdi::PFM *&pfm);

      // Description:
      // write functions
      static MPCDI_Error Write(std::string fileName, mpcdi::PFM &pfm);
      static MPCDI_Error Write(std::ostream &source, mpcdi::PFM &pfm);
  };
}; // end namespace mpcdi 

