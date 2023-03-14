/* =========================================================================

  Program:   MPCDI Library
  Language:  C++
  Date:      $Date: 2012-08-22 20:19:58 -0400 (Wed, 22 Aug 2012) $
  Version:   $Revision: 19513 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved.
  The MPCDI Library is distributed under the BSD license.
  Please see License.txt distributed with this package.

===================================================================auto== */

#pragma once
#ifndef __MPCDI_FileUtils_H_
#define __MPCDI_FileUtils_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mpcdi {

bool exist_file(const char * const filename);
bool delete_file(const char * const filename);
bool exist_directory(const char * const directory);
bool delete_directory(const char * const directory);
bool create_dir(const char * const directory);
bool move_file(const char * const file_in,
                               const char * const file_out,
                               const bool &OverWrite = true);
bool copy_file(const char * const file_in,
                               const char * const file_out,
                               const bool &OverWrite = true);
int time_since_modification_file(const char * const filename);
int time_since_modification_file(const char * const filename,const time_t &base_time);

} // end namespace mpcdi

#endif //__MPCDI_FileUtils_H_
