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
// .NAME Create3DSimulationProfile - Create a 3D Simulation Profile
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Description
//
// Create a 3D Simulation MPCDI Profile
//
// . SECTION Usage
//
// See Parent Class.
// 

#pragma once
#include "mpcdiCreateProfile.h"
namespace mpcdi {

class Create3DSimulationProfile : public CreateProfile {
public:
  // Description:
  // Constructor/Destructor.
  inline  Create3DSimulationProfile()
  {m_Profile->SetProfileType(ProfileType3d);}
  inline ~Create3DSimulationProfile() {};

  // Description:
  // 3D Specific creates.
  MPCDI_Error CreateFrustum(Region *r) {return r->SetFrustum(); }

  // Description:
  // Get the created frusutm
  Frustum  *GetFrustum(Region *r)  {return r->GetFrustum();}
};

}; // end namespace mpcdi 




