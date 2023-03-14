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
// .NAME FileSet - An MPCDI Fileset Tag
// .SECTION Description
//
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include "mpcdiHeader.h"
#include "mpcdiMacros.h"
#include "mpcdiGeometryWarpFile.h"
#include "mpcdiAlphaMap.h"
#include "mpcdiBetaMap.h"
#include "mpcdiDistortionMap.h"

namespace mpcdi {

struct EXPORT_MPCDI FileSet {
  // Description:
  // default constructor/destructor
  FileSet(std::string regionId);
  ~FileSet();
  
  mpcdiGetConstMacro(RegionId,std::string);
  //mpcdiSetMacro(AlphaMap,AlphaMap*);
  MPCDI_Error SetAlphaMap(unsigned int sizeX, unsigned int sizeY, mpcdi::ComponentDepth componentDepth, mpcdi::BitDepth bitDepth);
  mpcdiGetMacro(AlphaMap,AlphaMap*);
  //mpcdiSetMacro(BetaMap,BetaMap*);
  MPCDI_Error SetBetaMap(unsigned int sizeX, unsigned int sizeY, mpcdi::ComponentDepth componentDepth, mpcdi::BitDepth bitDepth);
  mpcdiGetMacro(BetaMap,BetaMap*);

  // Description:
  // Get/Set for distortionmap
  MPCDI_Error SetDistortionMap(unsigned int sizeX, unsigned int sizeY);
  mpcdiGetMacro(DistortionMap,DistortionMap*);

  // Description
  // get set for GeometryWarpFile
  MPCDI_Error SetGeometryWarpFile(unsigned int sizeX, unsigned int sizeY);
  mpcdiGetMacro(GeometryWarpFile,GeometryWarpFile*);

protected:
  //std::vector<std::string> m_FileNames;

  GeometryWarpFile *m_GeometryWarpFile; // size() == numChannels
  AlphaMap         *m_AlphaMap;         // size() == numChannels
  // optional. Only used for live l 2 or 4
  BetaMap          *m_BetaMap; // optional, size() == numChannels
  // only for shader map
  DistortionMap    *m_DistortionMap; 
  std::string      m_RegionId; // The corresponding region.
};

}; // end namespace mpcdi 

