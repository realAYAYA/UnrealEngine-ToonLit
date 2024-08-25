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
// .NAME Buffer - An MPCDI Buffer.
// .SECTION Description
//
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include <string>
#include <map>
#include "mpcdiHeader.h"
#include "mpcdiRegion.h"
#include "mpcdiMacros.h"

namespace mpcdi {

  struct EXPORT_MPCDI Buffer 
  {
  public:
    // Description:
    // The resolutions are optional, and are set to be negative when not in use.
    inline Buffer(std::string id="") {m_Id=id; m_Xresolution = -1; m_Yresolution = -1;};

    // Description:
    // default destructor
    ~Buffer();

    // Description:
    // get for region id 
    mpcdiGetConstMacro(Id,std::string);

    // Description:
    // get set for Xresolution
         mpcdiSetMacro(Xresolution,int);
      mpcdiGetRefMacro(Xresolution,int);
    mpcdiGetConstMacro(Xresolution,int);

    // Description:
    // get set for Yresolution
         mpcdiSetMacro(Yresolution,int);
      mpcdiGetRefMacro(Yresolution,int);
    mpcdiGetConstMacro(Yresolution,int);

    // Description:
    // region access methods
    Region *GetRegion(std::string regionId);
    std::vector<std::string> GetRegionNames();
    
    // Description:
    // region create and delete method
    MPCDI_Error NewRegion(std::string regionId);
    MPCDI_Error DeleteRegion(std::string regionId);
  
    // Description:
    // Region iterators
    typedef std::map<std::string,Region*>::iterator RegionIterator;
    typedef std::pair<std::string,Region*> RegionPair;
    RegionIterator GetRegionBegin() { return m_Regions.begin(); }
    RegionIterator GetRegionEnd()  { return m_Regions.end(); }

  protected:
    // Description:
    // member variables
    std::string m_Id;
    int m_Xresolution; // optional, set to negative if not in use.
    int m_Yresolution; // optional, set to negative if not in use.
#pragma warning(disable : 4251)
    std::map<std::string, Region*> m_Regions;  // The Regions within the buffer.
#pragma warning(default : 4251)
  };

} // end namespace mpcdi 

