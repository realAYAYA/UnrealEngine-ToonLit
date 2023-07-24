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
// .NAME Reader - An MPCDI Reader.
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Description
//
// Some comments on future compatibility. This implementation was
// written for version 1, draft 15 of the MCPDI standard. It seems
// likely there will be new versions in the future. The committee has
// not yet decided how future/backwards compatibility will be
// handled. This code makes the guess that extra tags will simply be
// added. 
//
// .SECTION Someday.
// It would be nice if the reader could find un-used tags for errors.
//

#pragma once
#include "mpcdiHeader.h"
#include "mpcdiErrors.h"
#include "mpcdiProfile.h"
#include "mpcdiMacros.h"
#include "mpcdiErrorHelper.h"

namespace mpcdi {

class EXPORT_MPCDI Reader {
public:

  virtual ~Reader()
  { }

  // Description:
  // creates a reader object
  static Reader *CreateReader();

  // Description:
  // The different ways to read. profile should be NULL. It will
  // be allocated. It needs to be de-allocated.
  virtual MPCDI_Error Read(std::istream &is, Profile *profile) = 0;
  virtual MPCDI_Error Read(std::string FileName, Profile *profile) = 0;

  // Description:
  // should we validate the profile after reading
  // default: false 
        mpcdiSetMacro(DoProfileValidation, bool);
  mpcdiGetConstMacro(DoProfileValidation, bool); 

  // Description:
  // should we check if version of profile being read in
  // is supported. If set to false a attempt is made to read it in
  // pretending it is the highest supported version
  // default: true
        mpcdiSetMacro(CheckVersionSupported, bool);
  mpcdiGetConstMacro(CheckVersionSupported, bool); 

  // Description:
  // get supported versions
  virtual std::string GetSupportedVersions()=0;

protected:
  // Description:
  // Member variables
  bool m_DoProfileValidation;
  bool m_CheckVersionSupported;
};

}; // end namespace mpcdi 




