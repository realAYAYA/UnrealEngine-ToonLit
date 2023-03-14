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
// .NAME Region - An MPCDI Region.
// .SECTION Description
//
// For 2d media, and a3d, the region tag is empty.
// For 
// 
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include "mpcdiHeader.h"
#include "mpcdiFrustum.h"
#include "mpcdiCoordinateFrame.h"
#include "mpcdiFileSet.h"

namespace mpcdi {

struct EXPORT_MPCDI Region {
  // Description:
  // No Default contructor/destructor
  Region(std::string id);
  ~Region();

  // Description:
  // get set id
  mpcdiGetConstMacro(Id,std::string);

  // Description:
  // resolution get set functions
       mpcdiSetMacro(Xresolution,int);
  mpcdiGetConstMacro(Xresolution,int);
    mpcdiGetRefMacro(Xresolution,int);
       mpcdiSetMacro(Yresolution,int);
  mpcdiGetConstMacro(Yresolution,int);
    mpcdiGetRefMacro(Yresolution,int);
  mpcdiSet2Macro(Resolution,Xresolution,Yresolution,int);

  // Description:
  // start point region
  mpcdiSetMacroRange(X, float, 0, 1);
    mpcdiGetRefMacro(X, float);
  mpcdiGetConstMacro(X, float);
  mpcdiSetMacroRange(Y, float, 0, 1);
    mpcdiGetRefMacro(Y, float);
  mpcdiGetConstMacro(Y, float);
  mpcdiSet2Macro(XY,X,Y, float);

  // Description:
  // size of region region
  mpcdiSetMacroRange(Xsize, float, 0, 1);
    mpcdiGetRefMacro(Xsize, float);
  mpcdiGetConstMacro(Xsize, float);
  mpcdiSetMacroRange(Ysize, float, 0, 1);
    mpcdiGetRefMacro(Ysize, float);
  mpcdiGetConstMacro(Ysize, float);
  mpcdiSet2Macro(Size,Xsize,Ysize, float);

  // Description:
  // fileset get set
  mpcdiGetMacro(FileSet,FileSet*);

  // Description:
  // Frustum get set
  mpcdiGetMacro(Frustum,Frustum*);
  MPCDI_Error SetFrustum();

  // Description:
  mpcdiGetMacro(CoordinateFrame, CoordinateFrame*);
  MPCDI_Error SetCoordinateFrame();

protected:
  // Description
  // member variables
  std::string m_Id;
  int m_Xresolution; // Mandatory. Total Horizontal Resolution.
  int m_Yresolution; // Mandatory. Total Vertical Resolution.
  float m_X;         // Where the region starts, in normalized coordinates, 0..1
  float m_Y;         // Where the region starts, in normalized coordinates, 0..1
  float m_Xsize;     // The size of the region in normalized coordinates.
  float m_Ysize;     // The size of the region in normalized coordinates.

  FileSet *m_FileSet;

  // 3D Simulation and Shader Lamps Only
  Frustum *m_Frustum;

  // Shader Lamps Only
  CoordinateFrame *m_CoordinateFrame;
};

}; // end namespace mpcdi 

