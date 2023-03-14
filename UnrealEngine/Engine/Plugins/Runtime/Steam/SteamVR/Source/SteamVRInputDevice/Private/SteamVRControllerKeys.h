/*
Copyright 2019 Valve Corporation under https://opensource.org/licenses/BSD-3-Clause
This code includes modifications by Epic Games.  Modifications (c) Epic Games, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "InputCoreTypes.h"

namespace GenericKeys
{
	const FKey SteamVR_MotionController_None("SteamVR_MotionController_None");
	const FKey SteamVR_HMD_Proximity("SteamVR_HMD_Proximity");
}

namespace IndexControllerKeys
{
	// Special
	const FKey ValveIndex_Left_Grip_Grab("ValveIndex_Left_Grip_Grab");
	const FKey ValveIndex_Left_Pinch_Grab("ValveIndex_Left_Pinch_Grab");
	const FKey ValveIndex_Right_Grip_Grab("ValveIndex_Right_Grip_Grab");
	const FKey ValveIndex_Right_Pinch_Grab("ValveIndex_Right_Pinch_Grab");
}

namespace CosmosKeys
{
	// Regular Controller Keys
	const FKey Cosmos_Left_X_Click("Cosmos_Left_X_Click");
	const FKey Cosmos_Left_Y_Click("Cosmos_Left_Y_Click");
	const FKey Cosmos_Left_X_Touch("Cosmos_Left_X_Touch");
	const FKey Cosmos_Left_Y_Touch("Cosmos_Left_Y_Touch");
	const FKey Cosmos_Left_Menu_Click("Cosmos_Left_Menu_Click");
	const FKey Cosmos_Left_Grip_Click("Cosmos_Left_Grip_Click");
	const FKey Cosmos_Left_Grip_Axis("Cosmos_Left_Grip_Axis");
	const FKey Cosmos_Left_Trigger_Click("Cosmos_Left_Trigger_Click");
	const FKey Cosmos_Left_Trigger_Axis("Cosmos_Left_Trigger_Axis");
	const FKey Cosmos_Left_Trigger_Touch("Cosmos_Left_Trigger_Touch");
	const FKey Cosmos_Left_Thumbstick_Vector("Cosmos_Left_Thumbstick_Vector");
	const FKey Cosmos_Left_Thumbstick_X("Cosmos_Left_Thumbstick_X");
	const FKey Cosmos_Left_Thumbstick_Y("Cosmos_Left_Thumbstick_Y");
	const FKey Cosmos_Left_Thumbstick_Click("Cosmos_Left_Thumbstick_Click");
	const FKey Cosmos_Left_Thumbstick_Touch("Cosmos_Left_Thumbstick_Touch");
	const FKey Cosmos_Left_Bumper_Click("Cosmos_Left_Bumper_Click");
	const FKey Cosmos_Right_A_Click("Cosmos_Right_A_Click");
	const FKey Cosmos_Right_B_Click("Cosmos_Right_B_Click");
	const FKey Cosmos_Right_A_Touch("Cosmos_Right_A_Touch");
	const FKey Cosmos_Right_B_Touch("Cosmos_Right_B_Touch");
	const FKey Cosmos_Right_System_Click("Cosmos_Right_System_Click");
	const FKey Cosmos_Right_Grip_Click("Cosmos_Right_Grip_Click");
	const FKey Cosmos_Right_Grip_Axis("Cosmos_Right_Grip_Axis");
	const FKey Cosmos_Right_Trigger_Click("Cosmos_Right_Trigger_Click");
	const FKey Cosmos_Right_Trigger_Axis("Cosmos_Right_Trigger_Axis");
	const FKey Cosmos_Right_Trigger_Touch("Cosmos_Right_Trigger_Touch");
	const FKey Cosmos_Right_Thumbstick_Vector("Cosmos_Right_Thumbstick_Vector");
	const FKey Cosmos_Right_Thumbstick_X("Cosmos_Right_Thumbstick_X");
	const FKey Cosmos_Right_Thumbstick_Y("Cosmos_Right_Thumbstick_Y");
	const FKey Cosmos_Right_Thumbstick_Click("Cosmos_Right_Thumbstick_Click");
	const FKey Cosmos_Right_Thumbstick_Touch("Cosmos_Right_Thumbstick_Touch");
	const FKey Cosmos_Right_Bumper_Click("Cosmos_Right_Bumper_Click");

	// Thumbstick Directions
	const FKey Cosmos_Left_Thumbstick_Up("Cosmos_Left_Thumbstick_Up");
	const FKey Cosmos_Left_Thumbstick_Down("Cosmos_Left_Thumbstick_Down");
	const FKey Cosmos_Left_Thumbstick_Left("Cosmos_Left_Thumbstick_Left");
	const FKey Cosmos_Left_Thumbstick_Right("Cosmos_Left_Thumbstick_Right");
	const FKey Cosmos_Right_Thumbstick_Up("Cosmos_Right_Thumbstick_Up");
	const FKey Cosmos_Right_Thumbstick_Down("Cosmos_Right_Thumbstick_Down");
	const FKey Cosmos_Right_Thumbstick_Left("Cosmos_Right_Thumbstick_Left");
	const FKey Cosmos_Right_Thumbstick_Right("Cosmos_Right_Thumbstick_Right");
}

namespace InputKeys
{
	// Valve Index - Additional input keys not implemented in OpenXR yet
	const FKey ValveIndex_Left_Trackpad_Up_Touch( "ValveIndex_Left_Trackpad_Up_Touch" );
	const FKey ValveIndex_Left_Trackpad_Down_Touch( "ValveIndex_Left_Trackpad_Down_Touch" );
	const FKey ValveIndex_Left_Trackpad_Left_Touch( "ValveIndex_Left_Trackpad_Left_Touch" );
	const FKey ValveIndex_Left_Trackpad_Right_Touch( "ValveIndex_Left_Trackpad_Right_Touch" );

	const FKey ValveIndex_Right_Trackpad_Up_Touch( "ValveIndex_Right_Trackpad_Up_Touch" );
	const FKey ValveIndex_Right_Trackpad_Down_Touch( "ValveIndex_Right_Trackpad_Down_Touch" );
	const FKey ValveIndex_Right_Trackpad_Left_Touch( "ValveIndex_Right_Trackpad_Left_Touch" );
	const FKey ValveIndex_Right_Trackpad_Right_Touch( "ValveIndex_Right_Trackpad_Right_Touch" );

	// HTC Vive - Additional input keys not implemented in OpenXR yet
	const FKey Vive_Left_Trackpad_Up_Touch( "Vive_Left_Trackpad_Up_Touch" );
	const FKey Vive_Left_Trackpad_Down_Touch( "Vive_Left_Trackpad_Down_Touch" );
	const FKey Vive_Left_Trackpad_Left_Touch( "Vive_Left_Trackpad_Left_Touch" );
	const FKey Vive_Left_Trackpad_Right_Touch( "Vive_Left_Trackpad_Right_Touch" );

	const FKey Vive_Right_Trackpad_Up_Touch( "Vive_Right_Trackpad_Up_Touch" );
	const FKey Vive_Right_Trackpad_Down_Touch( "Vive_Right_Trackpad_Down_Touch" );
	const FKey Vive_Right_Trackpad_Left_Touch( "Vive_Right_Trackpad_Left_Touch" );
	const FKey Vive_Right_Trackpad_Right_Touch( "Vive_Right_Trackpad_Right_Touch" );

	// Windows Mixed Reality - Additional input keys not implemented in OpenXR yet
	const FKey MixedReality_Left_Trackpad_Up_Touch( "MixedReality_Left_Trackpad_Up_Touch" );
	const FKey MixedReality_Left_Trackpad_Down_Touch( "MixedReality_Left_Trackpad_Down_Touch" );
	const FKey MixedReality_Left_Trackpad_Left_Touch( "MixedReality_Left_Trackpad_Left_Touch" );
	const FKey MixedReality_Left_Trackpad_Right_Touch( "MixedReality_Left_Trackpad_Right_Touch" );

	const FKey MixedReality_Right_Trackpad_Up_Touch( "MixedReality_Right_Trackpad_Up_Touch" );
	const FKey MixedReality_Right_Trackpad_Down_Touch( "MixedReality_Right_Trackpad_Down_Touch" );
	const FKey MixedReality_Right_Trackpad_Left_Touch( "MixedReality_Right_Trackpad_Left_Touch" );
	const FKey MixedReality_Right_Trackpad_Right_Touch( "MixedReality_Right_Trackpad_Right_Touch" );
}
