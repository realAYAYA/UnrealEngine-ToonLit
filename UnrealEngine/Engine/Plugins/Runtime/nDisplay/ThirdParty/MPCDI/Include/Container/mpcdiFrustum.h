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
// .NAME Frustum - Specify the Viewing Pyramid.
// .SECTION Description
//
// This is the viewing pyrimid typically used during rendering 
// a scene, ofter using OpenGL and/or DirectX.
//
// .AUTHOR Scalable Display Technologies, Inc.
//
// .SECTION Example Usage
// 
// Here is an example usage in OpenGL. Do NOT use this code blindly.
// The axes on the rotations will be different for each user.  Also,
// you may need to do the inverse rotations, rather than the ones
// listed here. It is very important to run tests to make sure you have
// the angles correct.
//
// double DegreesToRad = 3.14159/180.0;
// glMatrixMode(GL_PROJECTION);
// glLoadIdentity();
// glFrustum(near*tan(DegreesToRad*Frustum.LeftAngle),
// near*tan(DegreesToRad*Frustum.RightAngle),
// near*tan(DegreesToRad*Frustum.BottomAngle),
// near*tan(DegreesToRad*Frustum.TopAngle),
// near,far);
// glRotated(Frustum.Yaw,   0.0, 1.0, 0.0);
// glRotated(Frustum.Pitch, 1.0, 0.0, 0.0);
// glRotated(Frustum.Roll,  0.0, 0.0, 1.0);


#pragma once
#include "mpcdiHeader.h"
#include "mpcdiErrors.h"
#include "mpcdiMacros.h"

namespace mpcdi {

struct Frustum {
  // Description:
  // Set Default Values
  inline Frustum() : m_Yaw(-1), m_Pitch(-1), m_Roll(-1),
                     m_LeftAngle(-1), m_RightAngle(-1), 
                     m_UpAngle(-1), m_DownAngle(-1) {}

  // Description:
  // For Testing
  inline bool operator==(const Frustum &b) const 
  { return m_Yaw==b.m_Yaw && m_Pitch == b.m_Pitch && m_Roll == b.m_Roll &&
            m_LeftAngle==b.m_LeftAngle && m_RightAngle==b.m_RightAngle &&
            m_UpAngle==b.m_UpAngle && m_DownAngle == b.m_DownAngle
           ;}

  // Description:
  // yaw pitch rol get set
  mpcdiSetMacro(Yaw,double);
  mpcdiGetRefMacro(Yaw,double);
  mpcdiGetConstMacro(Yaw,double);

  mpcdiSetMacro(Pitch,double);
  mpcdiGetRefMacro(Pitch,double);
  mpcdiGetConstMacro(Pitch,double);

  mpcdiSetMacro(Roll,double);
  mpcdiGetRefMacro(Roll,double);
  mpcdiGetConstMacro(Roll,double);

  // Description:
  // Field of View
  mpcdiSetMacro(LeftAngle,double);
  mpcdiGetRefMacro(LeftAngle,double);
  mpcdiGetConstMacro(LeftAngle,double);

  mpcdiSetMacro(RightAngle,double);
  mpcdiGetRefMacro(RightAngle,double);
  mpcdiGetConstMacro(RightAngle,double);
  
  mpcdiSetMacro(UpAngle,double);
  mpcdiGetRefMacro(UpAngle,double);
  mpcdiGetConstMacro(UpAngle,double);
  
  mpcdiSetMacro(DownAngle,double);
  mpcdiGetRefMacro(DownAngle,double);
  mpcdiGetConstMacro(DownAngle,double);

protected:
  double m_Yaw;       // Viewer Rotates head upwards. First Rotation.
  double m_Pitch;     // Viewer Rotates head clockwise. Second Rotation.
  double m_Roll;      // Viewer Rotates head rightwards. Third Rotation.
  double m_LeftAngle; // Field of View. Typically Negative (Degrees) 
  double m_RightAngle;// Field of View. Typically Positive (Degrees)
  double m_UpAngle;   // Field of View. Typically Positive (Degrees)
  double m_DownAngle; // Field of View. Typically Negative (Degrees)
};

} // end namespace mpcdi 
