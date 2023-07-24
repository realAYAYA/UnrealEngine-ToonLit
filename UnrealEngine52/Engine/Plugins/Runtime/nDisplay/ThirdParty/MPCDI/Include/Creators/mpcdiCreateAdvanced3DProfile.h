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
// .NAME CreateAdvanced3DProfile - Create Advanced 3D Media Playback Profile
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Description
//
// Create an advanced 3D MPCDI Profile
//
// . SECTION Usage
//
// See Parent Class.
// 

#pragma once
#include "mpcdiCreateProfile.h"
namespace mpcdi {

class CreateAdvanced3DProfile : public CreateProfile {
public:
  // Description:
  // Constructor/Destructor.
  inline  CreateAdvanced3DProfile(){m_Profile->SetProfileType(ProfileTypea3);}
  inline ~CreateAdvanced3DProfile() {};

  // Description:
  // Geometric Unit Tag required.
  void SetGeometricUnit(GeometryWarpFile *gwf, const GeometricUnit &gu)
  { if (gwf) gwf->SetGeometricUnit(gu); }

  // Description:
  // Origin of 3D Data required.
  void SetOriginOf3DData(GeometryWarpFile *gwf, const OriginOf3DData &origin)
  { if (gwf) gwf->SetOriginOf3DData(origin); }
};




}; // end namespace mpcdi 




