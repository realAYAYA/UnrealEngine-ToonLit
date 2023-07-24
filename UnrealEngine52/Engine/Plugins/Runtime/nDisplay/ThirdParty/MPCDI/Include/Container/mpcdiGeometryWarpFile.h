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
// .NAME GeometryWarpFile - An MPCDI GeometryWarpFile Tag
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

#define GEOMETRIC_UNIT_ENUMS(XX,PREFIX) XX(2D,PREFIX,) \
            XX(mm,PREFIX,) \
            XX(cm,PREFIX,) \
            XX(dm,PREFIX,) \
            XX(m,PREFIX,) \
            XX(in,PREFIX,) \
            XX(ft,PREFIX,) \
            XX(yd,PREFIX,) \
            XX(unkown,PREFIX,) 

#define INTERPOLATION_ENUMS(XX,PREFIX) XX(linear,PREFIX,) \
            XX(keystone,PREFIX,) \
            XX(smooth,PREFIX,) \
            XX(unkown,PREFIX,) 

#define ORGIN_OF_3D_ENUMS(XX,PREFIX) XX(centerOfMass,PREFIX,) \
            XX(idealEyePoint,PREFIX,) \
            XX(floorCenter,PREFIX,) \
            XX(unkown,PREFIX,) 

MPCDI_DECLARE_ENUM_TYPEDEF(GeometricUnit,GEOMETRIC_UNIT_ENUMS);
MPCDI_DECLARE_ENUM_CONV_FUNC(GeometricUnit);

MPCDI_DECLARE_ENUM_TYPEDEF(OriginOf3DData,ORGIN_OF_3D_ENUMS);
MPCDI_DECLARE_ENUM_CONV_FUNC(OriginOf3DData);

MPCDI_DECLARE_ENUM_TYPEDEF(Interpolation,INTERPOLATION_ENUMS);
MPCDI_DECLARE_ENUM_CONV_FUNC(Interpolation);

struct EXPORT_MPCDI GeometryWarpFile: PFM {
  // Description:
  // Set Default Values
  GeometryWarpFile(unsigned int sizeX, unsigned int sizeY);

  // Description:
  // get/set file path. should not be used
  mpcdiSetMacro(Path, std::string);
  mpcdiGetConstMacro(Path, std::string);

  // Description:
  // get set functions for GeometricUnit
  mpcdiSetMacro(GeometricUnit, GeometricUnit);
  mpcdiGetConstMacro(GeometricUnit, GeometricUnit);

  // Description:
  // get set function for origin of 3d data
  mpcdiSetMacro(OriginOf3DData, OriginOf3DData);
  mpcdiGetConstMacro(OriginOf3DData, OriginOf3DData);

  // Description:
  // get set function for interpolation
  mpcdiSetMacro(Interpolation, Interpolation);
  mpcdiGetConstMacro(Interpolation, Interpolation);

protected:
  std::string     m_Path;
  GeometricUnit   m_GeometricUnit;
  OriginOf3DData  m_OriginOf3DData;
  Interpolation   m_Interpolation;
};

}; // end namespace mpcdi 
