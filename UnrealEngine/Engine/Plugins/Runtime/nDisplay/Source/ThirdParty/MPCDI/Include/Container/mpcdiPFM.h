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
// .NAME PFM - A PFM data format. 2D with 3 vars.
// .SECTION Description
//
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once

#include "mpcdiHeader.h"
#include "mpcdiMacros.h"
#include <vector>

namespace mpcdi {

/* ====================================================================== */

struct NODE {
 float r,g,b;

 inline bool operator==(const NODE &node) const 
 {return (r == node.r) && (g == node.g) && (b == node.b);  }
 inline bool operator!=(const NODE &node) const 
 { return (r != node.r) || (g != node.g) || (b != node.b);  }
};

/* ====================================================================== */


struct EXPORT_MPCDI PFM {
  // Description:
  // public constructor/destructor
  PFM(unsigned int sizeX, unsigned int sizeY);
  virtual ~PFM();
  
  // Description:
  // get functions for size
  mpcdiGetConstMacro(SizeX,int);
  mpcdiGetConstMacro(SizeY,int);

  // Description:
  // get raw data
  NODE* GetData() { return this->m_Data; }

  // Description:
  // copies data from other DataMap
  MPCDI_Error CopyData(const PFM &source);

  // Description:
  // array access function array[x][y][z] -> array(x,y,z)
  inline NODE &operator()(const int &x, const int &y)
  { 
    return  (m_Data[GetIndexOffset(x,y)]); 
  }

  // Description:
  // helper function to get array index
  inline int GetIndexOffset(const int &x, const int &y) const
  {
    assert( (unsigned int) x<m_SizeX); assert(x>=0);
    assert( (unsigned int) y<m_SizeY); assert(y>=0);
    return  (m_SizeX * y + x);
  }

protected:
  // Description:
  // member variables
  unsigned int m_SizeX;
  unsigned int m_SizeY;

  NODE *m_Data;
};

} // namespace mpcdi
