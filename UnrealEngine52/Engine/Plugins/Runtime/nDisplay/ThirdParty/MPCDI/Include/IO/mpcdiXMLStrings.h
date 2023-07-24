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
// .NAME XMLStrings - Strings for the XML Nodes
//
//

#pragma once

namespace mpcdi{

#define XML_STRING(_name_) const std::string _name_ = #_name_;

  namespace XML_NODES
  {
    XML_STRING(MPCDI);
    XML_STRING(alphaMap);
    XML_STRING(betaMap);
    XML_STRING(bitDepth);
    XML_STRING(buffer);
    XML_STRING(componentDepth);
    XML_STRING(coordinateFrame);
    XML_STRING(display);
    XML_STRING(distortionMap);
    XML_STRING(downAngle);
    XML_STRING(files);
    XML_STRING(fileset);
    XML_STRING(frustum);
    XML_STRING(gammaEmbedded);
    XML_STRING(geometricUnit);
    XML_STRING(geometryWarpFile);
    XML_STRING(interpolation);
    XML_STRING(leftAngle);
    XML_STRING(originOf3DData);
    XML_STRING(path);
    XML_STRING(pitch);
    XML_STRING(pitchx);
    XML_STRING(pitchy);
    XML_STRING(pitchz);
    XML_STRING(posx);
    XML_STRING(posy);
    XML_STRING(posz);
    XML_STRING(region);
    XML_STRING(rightAngle);
    XML_STRING(roll);
    XML_STRING(rollx);
    XML_STRING(rolly);
    XML_STRING(rollz);
    XML_STRING(upAngle);
    XML_STRING(yaw);
    XML_STRING(yawx);
    XML_STRING(yawy);
    XML_STRING(yawz);
  }
  
  namespace XML_ATTR
  {
    XML_STRING(profile);
    XML_STRING(date);
    XML_STRING(id);
    XML_STRING(level);
    XML_STRING(version);
    XML_STRING(region);
    XML_STRING(x);
    XML_STRING(Xresolution);
    XML_STRING(xsize);
    XML_STRING(y);
    XML_STRING(Yresolution);
    XML_STRING(ysize);
  }

#undef XML_STRING

}
