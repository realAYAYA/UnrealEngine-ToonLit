// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_GameFrame.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FGameFrame
//-------------------------------------------------------------------------------------------------

FGameFrame::FGameFrame() :
	FrameNumber(0),
	WorldToMetersScale(100.f),
	ShowFlags(ESFIM_All0),
	PlayerOrientation(FQuat::Identity),
	PlayerLocation(FVector::ZeroVector),
	FFRDynamic(false)
{
	Flags.Raw = 0;
	Fov[0] = Fov[1] = ovrpFovf{0,0,0,0};
}

TSharedPtr<FGameFrame, ESPMode::ThreadSafe> FGameFrame::Clone() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> NewFrame = MakeShareable(new FGameFrame(*this));
	return NewFrame;
}


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
