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
// .NAME Profile - The MPCDI Profile. The root of the tree structure.
// .SECTION Description
//
// This is the root of the tree of the MPCDI Profile Container.
// It contains all the data for a profile.
// 
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include "mpcdiDisplay.h"
#include "mpcdiMacros.h"
#include "mpcdiProfileVersion.h"

namespace mpcdi {

#define PROFILE_TYPE_ENUMS(XX,PREFIX) XX(2d,PREFIX,) \
            XX(3d,PREFIX,) \
            XX(a3,PREFIX,) \
            XX(sl,PREFIX,) \

MPCDI_DECLARE_ENUM_TYPEDEF(ProfileType,PROFILE_TYPE_ENUMS);
MPCDI_DECLARE_ENUM_CONV_FUNC(ProfileType);

struct EXPORT_MPCDI Profile {
  static Profile *CreateProfile();

  // Description:
  // constructor destructor
  Profile();
  ~Profile();

  // Description:
  // validates profile returns MPCDI_SUCCESS if a validate profile
  MPCDI_Error ValidateProfile();

  // Description:
  // get level
  mpcdiSetMacro(Level,int);
  mpcdiGetConstMacro(Level,int);

  // Description:
  // get set function for profiletype
  mpcdiGetConstMacro(ProfileType,ProfileType);
       mpcdiSetMacro(ProfileType,ProfileType);
  
  // Description:
  // get the date
       mpcdiSetMacro(Date,std::string);
  mpcdiGetConstMacro(Date,std::string);

  // Description:
  // Get the Display
  //mpcdiSetMacro(Display, Display);
  mpcdiGetMacro(Display,Display*);

  // Description:
  // get the version
       mpcdiSetMacro(Version, ProfileVersion);
  mpcdiGetConstMacro(Version, ProfileVersion);

protected:
  int         m_Level;               // 1..4
  ProfileType m_ProfileType;         // 2D, 3D, a3 or sl
  std::string m_Date;                // yyyy-MM-dd HH:mm:ss. ISO 8601 Format
  Display     *m_Display;             // This is a system of Display Units.
  ProfileVersion  m_Version;             // The MPACS Version
};
  
}; // end namespace mpcdi 
