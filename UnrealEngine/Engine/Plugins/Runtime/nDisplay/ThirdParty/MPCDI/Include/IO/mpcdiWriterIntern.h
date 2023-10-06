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
// .NAME WriterIntern - Internal Functions for the Writer.
// .SECTION Description
//
//
// Part of the reason of having this internal class is that it makes sure that all
// the 3rd party libraries that don't need to be exposed in mpcdi, are hidden to the
// user.
//
//
//
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include "mpcdiHeader.h"
#include "mpcdiErrors.h"
#include "mpcdiMacros.h"
#include "mpcdiWriter.h"
#include "mpcdiProfile.h"
#include "mpcdiXmlIO.h"
#include "mpcdiZipIO.h"

namespace mpcdi {

class WriterIntern : public Writer {
public:
  // Description:
  // constructor
  WriterIntern();

  // Description:
  // The several different ways to Write.
  MPCDI_Error Write(std::string FileName, Profile & profile);

protected:

  // Description:
  // Write a particular version.
  MPCDI_Error WriteMPCD(Profile & profile);
  MPCDI_Error WriteMPCDVersion1_0(Profile & profile);
  MPCDI_Error WriteProfile1_0(tinyxml2::XMLElement *parent, Profile & profile);
  MPCDI_Error WriteDisplay1_0(tinyxml2::XMLElement *parent, Display & display);
  MPCDI_Error WriteBuffer1_0(tinyxml2::XMLElement *parent, Buffer & buffer);
  MPCDI_Error WriteRegion(tinyxml2::XMLElement *parent, Region & region);
  MPCDI_Error WriteFrustum1_0(tinyxml2::XMLElement *parent, Frustum & frustum);
  MPCDI_Error WriteCoordinateFrame1_0(tinyxml2::XMLElement *parent, CoordinateFrame &coordinateFrame);
  //MPCDI_Error WriteFiles1_0(tinyxml2::XMLElement *parent, const Files & files);
  MPCDI_Error WriteFileSet1_0(std::string regionid, FileSet & fileSet);
  MPCDI_Error WriteAlphaMap1_0(tinyxml2::XMLElement *parent, AlphaMap & alphaMap);
  MPCDI_Error WriteBetaMap1_0(tinyxml2::XMLElement *parent, BetaMap & betaMap);
  MPCDI_Error WriteGeometryWarpFile1_0(tinyxml2::XMLElement *parent, GeometryWarpFile & geometryWarpFile);
  MPCDI_Error WriteDistortionMapFile1_0(tinyxml2::XMLElement *parent, DistortionMap & distortionMap);

  // Description:
  // helper functio to write datamap
  MPCDI_Error WriteDataMap(std::string FileName, DataMap &dataMap);

  // Description:
  // helper function to write PFM
  MPCDI_Error WritePFM(std::string fileName, PFM &pfm);

  // Description:
  // member variables
  XmlIO *m_XmlIO;
  ZipWriter *m_Zipper;
};

}; // end namespace mpcdi 




