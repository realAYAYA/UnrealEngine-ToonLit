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
// .NAME CreateProfile - A Base Class for Creating a Profile
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Description
//
// Use the Sub-Classes, not this one. The goal is to encapsulate the
// most common mechanisms for creating a profile in one location.
//
// . SECTION Usage
//
//  (1) Set the Level (Somewhat optional. Read the docs below.)
//  (2) Create 1 or More Buffers (CreateNewBuffer)
//  (3) Create 1 or More Regions for each Buffer (CreateNewRegion)
//  (4) For each region, create and fill in
//      (i) Alpha Map (including gamma)
//      (ii) Beta Map, optionally
//      (iii) Warp Map File.
//  (5) If appropriate, add other data.
//  (6) Validate the profile.
//  (7) Get the Profile
//  (8) Decide if you want this class to delete the Profile or not.
// 

#pragma once
#include "mpcdiProfile.h"
namespace mpcdi {

class EXPORT_MPCDI CreateProfile {
public:
  // Description:
  // Constructor/Destructor.
  virtual ~CreateProfile();

  // Description:
  // Set the Level. 1...4 as of May 2013.  To some degree, this
  // command is optional. If you know the correct level, enter it. If,
  // however, you leave the data at level 1, we will do our best to
  // calculate it. We aren't yet perfect at this calculation.
  void SetLevel(const int &Level) { m_Profile->SetLevel(Level); }

  // Description:
  // Create a New Buffer
  MPCDI_Error CreateNewBuffer(const std::string &newBufferId)
  { return m_Profile->GetDisplay()->NewBuffer(newBufferId); }

  // Description:
  // Create a New Region
  MPCDI_Error CreateNewRegion(const std::string &BufferId,
                              const std::string &RegionId)
 { return CreateNewRegion(GetBuffer(BufferId),RegionId); }
  MPCDI_Error CreateNewRegion(Buffer *Buff,
                              const std::string &newRegionId)
  { return Buff->NewRegion(newRegionId); }

  // Description:
  // Get the buffer.
  Buffer *GetBuffer(const std::string &BufferId)
  { return m_Profile->GetDisplay()->GetBuffer(BufferId); }

  // Description:
  // Get the buffer.
  Region *GetRegion(Buffer *Buff, const std::string &RegionId)
  { return Buff->GetRegion(RegionId); }
  Region *GetRegion(const std::string &BufferId,
                    const std::string &RegionId)
  { return GetRegion(GetBuffer(BufferId),RegionId); }


  // Description:
  // Create a new AlphaMap
  MPCDI_Error CreateAlphaMap(Region *r,
                             const int &sizeX,
                             const int &sizeY,
                             const ComponentDepth &cd,
							 const BitDepth &bd)
  {
	  return r->GetFileSet()->SetAlphaMap(sizeX, sizeY, cd, bd);
  }

  // Description:
  // Set the gamma for the alpha map
  void SetGammaEmbedded(Region *r, const double &gamma)
  { if (GetAlphaMap(r)!= NULL) GetAlphaMap(r)->SetGammaEmbedded(gamma); }

  // Description:
  // Optional: Create a new BetaMap
  MPCDI_Error CreateBetaMap(Region *r,
                            const int &sizeX,
                            const int &sizeY,
                            const ComponentDepth &cd,
							const BitDepth &bd)
  { m_HaveBetaMap = true; UpdateLevel();
    return r->GetFileSet()->SetBetaMap(sizeX,sizeY,cd,bd); }


  // Description:
  // Create a new GeometryWarpFile
  MPCDI_Error CreateGeometryWarpFile(Region *r,
                                     const int &xRes,
                                     const int &yRes)
  { CheckPerPixelResolution(r,xRes,yRes); UpdateLevel();
    return r->GetFileSet()->SetGeometryWarpFile(xRes,yRes); }

  // Description:
  // Get any of the created maps. If not created, NULL is returned.
  BetaMap  *GetBetaMap(Region *r)  {return r->GetFileSet()->GetBetaMap();}
  AlphaMap *GetAlphaMap(Region *r) {return r->GetFileSet()->GetAlphaMap();}
  GeometryWarpFile *GetGeometryWarpFile(Region *r)
  { return r->GetFileSet()->GetGeometryWarpFile(); }

  // Description:
  // Validate the Profile. Look for inconsistencies, or things not
  // allowed by the standard. It does not find every issues,
  // but should be pretty good.
  MPCDI_Error ValidateProfile() { return m_Profile->ValidateProfile(); }

  // Description:
  // The Results.
  Profile *GetProfile() { return m_Profile; }
  
  // Description:
  // When we delete this class, should we delete the profile?
  // The Default is 'yes'.
  // Why do this? You might want to create the profile, and then
  // make a copy of the pointer, and delete the creation class. It
  // means you don't have to copy the entire profile.
       mpcdiSetMacro(DeleteProfile, bool);
  mpcdiGetConstMacro(DeleteProfile, bool);

  // Description:
  // Update the Level. Guaranteed to never decrease the level.
  // The level can go up if you set something that requires it 
  // to go up. This is implemented differently PerType. Returns the
  // new level.
  unsigned int UpdateLevel();

 protected:
  // Description:
  // The Constructor is protected because we should use the sub-classes.
  CreateProfile();

  // Description:
  // The profile to be created.
  Profile *m_Profile;
  bool m_DeleteProfile;

  // Information for calculating the level.
  bool m_HaveBetaMap; // Guarantees at least level 2.
  bool m_HaveDistortionMap; // Guarantees at least level 3 for Shader Lamps
  bool m_HavePerPixelResolution;     // Level 3 for 2D and 3D and a3D

  // Description:
  // A helper function. Checks if the resolution of the warp
  // is close enough to the resolution of the region to call
  // the warp per pixel, and then sets the HavePerPixelResolution flag.
  //
  // One of the profiles checks for 32x32 blocks, we don't do that yet.
  void CheckPerPixelResolution(Region *r,
                               const int &xres,
                               const int &yres);
};

}; // end namespace mpcdi 




