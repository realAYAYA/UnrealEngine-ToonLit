/* =========================================================================

  Program:   Multiple Projector Library
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved
  The source code contained herein is confidential and is considered a 
  trade secret of Scalable Display Technologies, Inc

===================================================================auto== */

#ifndef _EasyBlendSDKFrustum_H_
#define _EasyBlendSDKFrustum_H_

// This structure is exported from the EasyBlendSDK.dll

// Description:
// A frustum defines a particualr viewing pyramid required
// to render the needed input for an EasyBlend SDK mesh.
typedef struct  {

  // Description:
  // The origin of the viewping pyramid in 3-Space
  // the units and coordinate system for the point is
  // defined by the configuration of the EasyBlend
  // calibration system.
  // Note: this origin is generally the same for all meshes
  //       generated for a particualr calibration
  double  XOffset;
  double  YOffset;
  double  ZOffset;

  // Description:
  // The Orientation of the Open GL camera (in degrees).
  double  ViewAngleA;  // Rotation about z-axis (first rotation, also called Heading)
  double  ViewAngleB;  // Rotation about y-axis (second rotation)
  double  ViewAngleC;  // Rotation about x-axis (third rotation)

  // Description:
  // The angles definiing the extend of the viewing pyramid,
  // extending from the ViewAngle direction represented by 
  // the three view angles above.
  double  LeftAngle;    // Range: from -90 to Right
  double  RightAngle;   // Range: from Left to 90
  double  TopAngle;     // Range: from Down to 90
  double  BottomAngle;  // Range: from -90 to Up


} EasyBlendSDK_Frustum;


#endif /* ifndef _EasyBlendSDKFrustum_H_ */

