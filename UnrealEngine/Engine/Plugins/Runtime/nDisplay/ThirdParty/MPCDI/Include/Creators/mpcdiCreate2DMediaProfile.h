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
// .NAME Create2DMediaProfile - Create 2D Media Playback Profile
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Description
//
// Create an MPCDI Profile for 2D Media Playback.
//
// . SECTION Usage
//
// See Parent Class.
// 

#pragma once
#include "mpcdiCreateProfile.h"
namespace mpcdi {

class Create2DMediaProfile : public CreateProfile {
public:
  // Description:
  // Constructor/Destructor.
  inline  Create2DMediaProfile() {m_Profile->SetProfileType(ProfileType2d);}
  inline ~Create2DMediaProfile() {};
};

}; // end namespace mpcdi 




