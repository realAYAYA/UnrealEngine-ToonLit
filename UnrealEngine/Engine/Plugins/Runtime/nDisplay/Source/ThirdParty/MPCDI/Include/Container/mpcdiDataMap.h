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
// .NAME DataMap - An MPCDI Data Map 
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

	enum ComponentDepth { CD_ONE = 1, CD_TWO = 3, CD_THREE = 3, CD_FOUR = 4 };
enum BitDepth { BD_EIGHT = 8, BD_SIXTEEN = 16, BD_THIRTYTWO = 32 };

struct EXPORT_MPCDI DataMap {
  // Description:
  // public constructor/destructor
  DataMap(unsigned int sizeX, unsigned int sizeY, ComponentDepth componentDepth, BitDepth bitDepth);
  virtual ~DataMap();
  
  // Description:
  // get functions for size
  mpcdiGetConstMacro(SizeX,int);
  mpcdiGetConstMacro(SizeY,int);

  // Description:
  // get functions for componentdepth
  mpcdiGetConstMacro(ComponentDepth, ComponentDepth);

  // Description:
  // get set functions for bitdepth
       mpcdiSetMacro(BitDepth, BitDepth)
    mpcdiGetRefMacro(BitDepth, BitDepth)
  mpcdiGetConstMacro(BitDepth, BitDepth);

  // Description:
  // get raw data
  std::vector<unsigned char> * GetData() { return &(this->m_Data);}

  // Description:
  // copies data from other DataMap
  MPCDI_Error CopyData(const DataMap &source);

  // Description:
  // array access function array[x][y][z] -> array(x,y,z)
  inline unsigned char &operator()(const int &x, const int &y, const int &z)
  { 
    return  (m_Data[GetIndexOffset(x,y,z)]); 
  }  

  // Description:
  // helper function to get array index
  inline int GetIndexOffset(const int &x, const int &y, const int &z) const
  {
    assert( (unsigned int) x<m_SizeX); assert(x>=0);
    assert( (unsigned int) y<m_SizeY); assert(y>=0);
    assert( z<m_ComponentDepth); assert(z>=0);
    return  (m_SizeX * m_ComponentDepth * y + m_ComponentDepth*x + z);
  } 

protected:
  // Description:
  // member variables
  ComponentDepth m_ComponentDepth; // 1 or 3
  BitDepth m_BitDepth;       // 8,16, or 32
  unsigned int m_SizeX;
  unsigned int m_SizeY;

#pragma warning(disable : 4251)
  std::vector<unsigned char> m_Data;
#pragma warning(default : 4251)
};

} // namespace mpcdi

