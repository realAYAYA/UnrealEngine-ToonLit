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
// .NAME CoordinateFrame - Specify the Viewing Pyramid.
// .SECTION Description
//
//
//
// .AUTHOR Scalable Display Technologies, Inc.
//

#pragma once
#include "mpcdiHeader.h"

namespace mpcdi {

struct CoordinateFrame {

  // Description:
  // Set Default Values
  inline CoordinateFrame() 
  {
    m_Posx = m_Posy = m_Posz = 0.0;
    m_Yawx = m_Yawy = m_Yawz = 0.0; 
    m_Pitchx = m_Pitchy = m_Pitchz = 0.0;
    m_Rollx = m_Rolly = m_Rollz = 0.0;
  }

  // Description:
  // position get set functions
       mpcdiSetMacro(Posx,double);
    mpcdiGetRefMacro(Posx,double);
  mpcdiGetConstMacro(Posx,double);

      mpcdiSetMacro(Posy,double);
    mpcdiGetRefMacro(Posy,double);
  mpcdiGetConstMacro(Posy,double);

       mpcdiSetMacro(Posz,double);
    mpcdiGetRefMacro(Posz,double);
  mpcdiGetConstMacro(Posz,double);
  mpcdiSet3Macro(Pos,Posx,Posy,Posz,double);

  // Description:
  // position get set functions
       mpcdiSetMacro(Yawx,double);
    mpcdiGetRefMacro(Yawx,double);
  mpcdiGetConstMacro(Yawx,double);

       mpcdiSetMacro(Yawy,double);
    mpcdiGetRefMacro(Yawy,double);
  mpcdiGetConstMacro(Yawy,double);

       mpcdiSetMacro(Yawz,double);
    mpcdiGetRefMacro(Yawz,double);
  mpcdiGetConstMacro(Yawz,double);
  mpcdiSet3Macro(Yaw,Yawx,Yawy,Yawz,double);

  // Description:
  // position get set functions
       mpcdiSetMacro(Pitchx,double);
    mpcdiGetRefMacro(Pitchx,double);
  mpcdiGetConstMacro(Pitchx,double);

       mpcdiSetMacro(Pitchy,double);
    mpcdiGetRefMacro(Pitchy,double);
  mpcdiGetConstMacro(Pitchy,double);

       mpcdiSetMacro(Pitchz,double);
    mpcdiGetRefMacro(Pitchz,double);
  mpcdiGetConstMacro(Pitchz,double);
  mpcdiSet3Macro(Pitch,Pitchx,Pitchy,Pitchz,double);

  // Description:
  // roll get set functions
       mpcdiSetMacro(Rollx,double);
    mpcdiGetRefMacro(Rollx,double);
  mpcdiGetConstMacro(Rollx,double);

       mpcdiSetMacro(Rolly,double);
    mpcdiGetRefMacro(Rolly,double);
  mpcdiGetConstMacro(Rolly,double);

       mpcdiSetMacro(Rollz,double);
    mpcdiGetRefMacro(Rollz,double);
  mpcdiGetConstMacro(Rollz,double);
  mpcdiSet3Macro(Roll,Rollx,Rolly,Rollz,double);

 protected:
  double m_Posx, m_Posy, m_Posz;
  double m_Yawx, m_Yawy, m_Yawz;
  double m_Pitchx, m_Pitchy, m_Pitchz;
  double m_Rollx, m_Rolly, m_Rollz;
};

} // end namespace mpcdi 

