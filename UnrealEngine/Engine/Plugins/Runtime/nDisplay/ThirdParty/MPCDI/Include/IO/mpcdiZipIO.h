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
// .NAME ZipWriter - Zip Archive Writer
// .NAME ZipReader - Zip Archive Reader
// .SECTION Description
//

#pragma once

#include "tinyxml2.h"
#include "mpcdiHeader.h"
#include <string>

#include "mpcdiErrors.h"
#include "mpcdiXMLStrings.h"
#include <string>
#include "zip.h"
#include "unzip.h"

namespace mpcdi 
{
  class ZipWriter
  {
    public:
      // Description:
      // default constructor/destructor
      ZipWriter(std::string archiveName);
      ~ZipWriter();

      // Description:
      // open archive
      MPCDI_Error OpenArchive();

      // Description:
      // add a file to current archive
      MPCDI_Error AddFile(std::string fileName,void* buffer, int bufferSize);

      // Description:
      // write archive to disk
      MPCDI_Error WriteArchive();

    protected:

      // Description:
      // member variables
      std::string m_ArchiveName;
      zipFile m_ZipFile;
  };

  class ZipReader
  {
    public:
      // Description:
      // default constructor/destructor
      ZipReader(std::string archiveName);
      ~ZipReader();

      // Description:
      // open archive
      MPCDI_Error OpenArchive();

      // Description:
      // read a file from current archive
      MPCDI_Error GetFile(std::string fileName, void*& buffer, int &readSize);
      
      // Description:
      // close archive
      MPCDI_Error CloseArchive();

    protected:

      // Description:
      // member variables
      std::string m_ArchiveName;
      unzFile m_UnzFile;
  };

}// namespace mpcdi
