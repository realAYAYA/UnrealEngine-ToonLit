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
// .NAME CreateShaderLampProfile - Create a Shader Lamp Profile
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Description
//
// Create an MPCDI Profile for Shader Lamps
//
// . SECTION Usage
//
// See Parent Class.
// 

#pragma once
#include "mpcdiCreateProfile.h"
namespace mpcdi {

class CreateShaderLampProfile : public CreateProfile {
public:
  // Description:
  // Constructor/Destructor.
  inline  CreateShaderLampProfile()
  {m_Profile->SetProfileType(ProfileTypesl);}
  inline ~CreateShaderLampProfile() {};

  // Description:
  // Need a frustum.
  MPCDI_Error CreateFrustum(Region *r) {return r->SetFrustum(); }

// Description:
  // Need a coordinate frame
  MPCDI_Error CreateCoordinateFrame(Region *r)
  {return r->SetCoordinateFrame(); }

  // Description:
  // Create a new DisortionMap
  MPCDI_Error CreateDistortionMap(Region *r,
                                  const int &xRes,
                                  const int &yRes)
  { m_HaveDistortionMap = true; UpdateLevel();
    return r->GetFileSet()->SetDistortionMap(xRes,yRes); }

  // Description:
  // Geometric Unit Tag required.
  void SetGeometricUnit(GeometryWarpFile *gwf, const GeometricUnit &gu)
  { if (gwf) gwf->SetGeometricUnit(gu); }

  // Description:
  // Origin of 3D Data required.
  void SetOriginOf3DData(GeometryWarpFile *gwf, const OriginOf3DData &origin)
  { if (gwf) gwf->SetOriginOf3DData(origin); }

  // Description:
  // Get the created data.
  Frustum          *GetFrustum(Region *r)        {return r->GetFrustum();}
  CoordinateFrame  *GetCoordinateFrame(Region *r)
                                         {return r->GetCoordinateFrame();}
  DistortionMap *GetDistortionMap(Region *r)
                           { return r->GetFileSet()->GetDistortionMap(); }
};

/* ====================================================================== */


}; // end namespace mpcdi 




