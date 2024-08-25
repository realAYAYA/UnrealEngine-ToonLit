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
// .NAME Writer - An MPCDI Writer Wrapper.
// .SECTION Description
//
// Some comments on future compatibility. This implementation was
// written for version 1, draft 15 of the MCPDI standard. It seems
// likely there will be new versions in the future. The committee has
// not yet decided how future/backwards compatibility will be
// handled. This code makes the guess that extra tags will simply be
// added. That is,
//
//
//
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include "mpcdiHeader.h"
#include "mpcdiErrors.h"
#include "mpcdiProfile.h"
#include "mpcdiMacros.h"
#include "mpcdiErrorHelper.h"

namespace mpcdi {
class EXPORT_MPCDI Writer {
public:

  virtual ~Writer()
  { }

  // Description:
  // creates a writer object
  static Writer *CreateWriter();

  // Description:
  // The several different ways to Write.
  virtual MPCDI_Error Write(std::string FileName, mpcdi::Profile &profile) = 0;

  // Description: 
  // Should we overwrite an existing file?
  // Default: yes (on error, will return MPCDI_FILE_ALREADY_EXISTS)
       mpcdiSetMacro(OverwriteExistingFile, bool);
  mpcdiGetConstMacro(OverwriteExistingFile, bool);

  // Description:
  // should we validate the profile before writing
  // default: true 
        mpcdiSetMacro(DoProfileValidation, bool);
  mpcdiGetConstMacro(DoProfileValidation, bool); 

protected:
  // Description:
  // Member variables
  bool m_OverwriteExistingFile;
  bool m_DoProfileValidation;
};

}; // end namespace mpcdi 
