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
// .NAME ReaderIntern - An Implementation of the MPCDI Reader.
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Description
//
// Part of the reason of having this internal class is that it makes
// sure that all the 3rd party libraries that don't need to be exposed
// in mpcdi, are hidden to the user.
//
// Currently implements version 1.0
//

#pragma once
#include "tinyxml2.h"
#include "mpcdiHeader.h"
#include "mpcdiErrors.h"
#include "mpcdiMacros.h"
#include "mpcdiProfile.h"
#include "mpcdiReader.h"
#include "mpcdiXmlIO.h"
#include "mpcdiZipIO.h"

namespace mpcdi {

class ReaderIntern: public Reader{
public:
  // Description:
  // constructor
  ReaderIntern();

  // Description:
  // The several different ways to read.
  MPCDI_Error Read(std::istream &is,     Profile *profile);
  MPCDI_Error Read(std::string FileName, Profile *profile);

  // Description:
  // get supported versions
  std::string GetSupportedVersions();

protected:
  // Description:
  // Get the version of the current archive
  MPCDI_Error GetVersion(tinyxml2::XMLDocument &doc, mpcdi::ProfileVersion & version);

  // Description:
  // Check if the file is self-consistent.
  MPCDI_Error IsFileSelfConsistent() const;

  // Description:
  // Update MPCD to the latest version, not done by default;
  MPCDI_Error UpdateToLatestVersionIfNecessary();

  // Description:
  // Read a particular version.
  MPCDI_Error ReadMPCD(Profile & profile, ProfileVersion version);
  MPCDI_Error ReadMPCDVersion1_0(Profile & profile);
  MPCDI_Error ReadProfile1_0(tinyxml2::XMLElement *parent, Profile & profile);
  MPCDI_Error ReadDisplay1_0(tinyxml2::XMLElement *parent, ProfileType type, Display & display);
  MPCDI_Error ReadBuffer1_0(tinyxml2::XMLElement *bufferElement, ProfileType type, Display & buffer);
  MPCDI_Error ReadRegion(tinyxml2::XMLElement *regionElement, ProfileType type, Buffer & buffer);
  MPCDI_Error ReadFrustum1_0(tinyxml2::XMLElement *frustumElement, ProfileType type, Frustum & frustum);
  MPCDI_Error ReadCoordinateFrame1_0(tinyxml2::XMLElement *parent, ProfileType type, CoordinateFrame &coordinateFrame);
  MPCDI_Error ReadFileSet1_0(std::string regionid, ProfileType type, FileSet & fileSet);
  MPCDI_Error ReadAlphaMap1_0(tinyxml2::XMLElement *alphaMapElement, ProfileType type, FileSet & fileSet);
  MPCDI_Error ReadBetaMap1_0(tinyxml2::XMLElement *betaMapElement, ProfileType type, FileSet & fileSet);
  MPCDI_Error ReadGeometryWarpFile1_0(tinyxml2::XMLElement *gwfElement, ProfileType type, FileSet & fileSet);
  MPCDI_Error ReadDistortionMapFile1_0(tinyxml2::XMLElement *dmfElement, ProfileType type, FileSet & fileSet);

  // Description:
  // checks consistency of the archive 
  MPCDI_Error CheckConsistency();

  // Description:
  // helper function to read datamap file from archive
  MPCDI_Error ReadDataMap(std::string FileName, DataMap *&dataMap);

  // Description:
  // helper function to read pfm file from archive
  MPCDI_Error ReadPFM(std::string FileName, PFM *&pfm);

  // Description:
  // member variables
  XmlIO *m_XmlIO;
  ZipReader *m_Zipper;
  const static int s_MaxSupportedMajorVersion;
  const static int s_MaxSupportedMinorVersions[];
};

}; // end namespace mpcdi 




