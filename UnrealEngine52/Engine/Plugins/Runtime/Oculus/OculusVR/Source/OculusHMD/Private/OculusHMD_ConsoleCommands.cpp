// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusHMD_ConsoleCommands.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"
#include "OculusSceneCaptureCubemap.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FConsoleCommands
//-------------------------------------------------------------------------------------------------

/// @cond DOXYGEN_WARNINGS

FConsoleCommands::FConsoleCommands(class FOculusHMD* InHMDPtr)
	: UpdateOnRenderThreadCommand(TEXT("vr.oculus.bUpdateOnRenderThread"),
		*NSLOCTEXT("OculusRift", "CCommandText_UpdateRT", "Oculus Rift specific extension.\nEnables or disables updating on the render thread.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::UpdateOnRenderThreadCommandHandler))
	, PixelDensityMinCommand(TEXT("vr.oculus.PixelDensity.min"),
		*NSLOCTEXT("OculusRift", "CCommandText_PixelDensityMin", "Oculus Rift specific extension.\nMinimum pixel density when adaptive pixel density is enabled").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::PixelDensityMinCommandHandler))
	, PixelDensityMaxCommand(TEXT("vr.oculus.PixelDensity.max"),
		*NSLOCTEXT("OculusRift", "CCommandText_PixelDensityMax", "Oculus Rift specific extension.\nMaximum pixel density when adaptive pixel density is enabled").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::PixelDensityMaxCommandHandler))
	, HQBufferCommand(TEXT("vr.oculus.bHQBuffer"),
		*NSLOCTEXT("OculusRift", "CCommandText_HQBuffer", "Oculus Rift specific extension.\nEnable or disable using floating point texture format for the eye layer.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::HQBufferCommandHandler))
	, HQDistortionCommand(TEXT("vr.oculus.bHQDistortion"),
		*NSLOCTEXT("OculusRift", "CCommandText_HQDistortion", "Oculus Rift specific extension.\nEnable or disable using multiple mipmap levels for the eye layer.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::HQDistortionCommandHandler))
	, ShowGlobalMenuCommand(TEXT("vr.oculus.ShowGlobalMenu"),
		*NSLOCTEXT("OculusRift", "CCommandText_GlobalMenu", "Oculus Rift specific extension.\nOpens the global menu.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::ShowGlobalMenuCommandHandler))
	, ShowQuitMenuCommand(TEXT("vr.oculus.ShowQuitMenu"),
		*NSLOCTEXT("OculusRift", "CCommandText_QuitMenu", "Oculus Rift specific extension.\nOpens the quit menu.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::ShowQuitMenuCommandHandler))

#if !UE_BUILD_SHIPPING
	, StatsCommand(TEXT("vr.oculus.Debug.bShowStats"),
		*NSLOCTEXT("OculusRift", "CCommandText_Stats", "Oculus Rift specific extension.\nEnable or disable rendering of stats.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::StatsCommandHandler))
	, CubemapCommand(TEXT("vr.oculus.Debug.CaptureCubemap"),
		*NSLOCTEXT("OculusRift", "CCommandText_Cubemap", "Oculus Rift specific extension.\nCaptures a cubemap for Oculus Home.\nOptional arguments (default is zero for all numeric arguments):\n  xoff=<float> -- X axis offset from the origin\n  yoff=<float> -- Y axis offset\n  zoff=<float> -- Z axis offset\n  yaw=<float>  -- the direction to look into (roll and pitch is fixed to zero)\n  mobile       -- Generate a Mobile format cubemap\n    (height of the captured cubemap will be 1024 instead of 2048 pixels)\n").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&UOculusSceneCaptureCubemap::CaptureCubemapCommandHandler))
	, ShowSettingsCommand(TEXT("vr.oculus.Debug.Show"),
		*NSLOCTEXT("OculusRift", "CCommandText_Show", "Oculus Rift specific extension.\nShows the current value of various stereo rendering params.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::ShowSettingsCommandHandler))
	, IPDCommand(TEXT("vr.oculus.Debug.IPD"),
		*NSLOCTEXT("OculusRift", "CCommandText_IPD", "Oculus Rift specific extension.\nShows or changes the current interpupillary distance in meters.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::IPDCommandHandler))
#endif // !UE_BUILD_SHIPPING
{
}

bool FConsoleCommands::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* OrigCmd = Cmd;
	FString AliasedCommand;
	
	if (FParse::Command(&Cmd, TEXT("OVRGLOBALMENU")))
	{
		AliasedCommand = TEXT("vr.oculus.ShowGlobalMenu");
	}
	else if (FParse::Command(&Cmd, TEXT("OVRQUITMENU")))
	{
		AliasedCommand = TEXT("vr.oculus.ShowQuitMenu");
	}
#if !UE_BUILD_SHIPPING
	else if (FParse::Command(&Cmd, TEXT("vr.oculus.Debug.EnforceHeadTracking")))
	{
		AliasedCommand = TEXT("vr.HeadTracking.bEnforced");
	}
#endif // !UE_BUILD_SHIPPING

	if (!AliasedCommand.IsEmpty())
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("%s is deprecated. Use %s instead"), OrigCmd, *AliasedCommand);
		return IConsoleManager::Get().ProcessUserConsoleInput(*AliasedCommand, Ar, InWorld);
	}
	return false;
}

/// @endcond

} // namespace OculusHmd

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
