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
// .NAME Display - A Collection of Buffers
// .SECTION Description
//
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once

#include "mpcdiBuffer.h"
#include <map>

namespace mpcdi {

struct EXPORT_MPCDI Display {
  // Description:
  // No Default Values
  inline Display() {};

  ~Display();

  // Description:
  // Buffer access methods
  Buffer *GetBuffer(std::string bufferId);
  std::vector<std::string> GetBufferNames();

  // Description:
  // create and delete new buffers
  MPCDI_Error NewBuffer(std::string bufferId);
  MPCDI_Error DeleteBuffer(std::string bufferId);

  // Description:
  // Iterator access for buffer
  typedef std::map<std::string,Buffer*>::iterator BufferIterator;
  typedef std::pair<std::string,Buffer*> BufferPair;
  BufferIterator GetBufferBegin() { return m_Buffers.begin(); }
  BufferIterator GetBufferEnd()  { return m_Buffers.end(); }

protected:
#pragma warning(disable : 4251)
  std::map<std::string, Buffer*> m_Buffers;
#pragma warning(default : 4251)
};

} // end namespace mpcdi 


