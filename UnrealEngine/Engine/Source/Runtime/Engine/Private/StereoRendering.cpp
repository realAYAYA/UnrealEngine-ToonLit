// Copyright Epic Games, Inc. All Rights Reserved.

#include "StereoRendering.h"
#include "SceneView.h"
#include "GeneralProjectSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"


bool IStereoRendering::IsStereoEyeView(const FSceneView& View)
{
	return IsStereoEyePass(View.StereoPass);
}

bool IStereoRendering::IsAPrimaryView(const FSceneView& View)
{
	return IsAPrimaryPass(View.StereoPass);
}

bool IStereoRendering::IsASecondaryView(const FSceneView& View)
{
	return IsASecondaryPass(View.StereoPass);
}

bool IStereoRendering::IsStartInVR()
{
	bool bStartInVR = false;

	if (IsClassLoaded<UGeneralProjectSettings>())
	{
		bStartInVR = GetDefault<UGeneralProjectSettings>()->bStartInVR;
	}
	else
	{
		GConfig->GetBool(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("bStartInVR"), bStartInVR, GGameIni);
	}

	return FParse::Param(FCommandLine::Get(), TEXT("vr")) || bStartInVR;
}
