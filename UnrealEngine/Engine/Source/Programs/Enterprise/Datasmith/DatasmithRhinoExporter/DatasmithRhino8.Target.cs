// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class DatasmithRhino8Target : DatasmithRhinoBaseTarget
{
	public DatasmithRhino8Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "8"; }

	public override string GetRhinoInstallFolderWindows()
	{
		try
		{
			return OperatingSystem.IsWindows() ? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\McNeel\Rhinoceros\8.0\Install", "Path", "") as string : null;
		}
		catch(Exception)
		{
			return "";
		}
	}

	public override string GetRhinoInstallFolderMac()
	{
		return "/Applications/Rhino 8.app/Contents/Frameworks/RhCore.framework/Versions/Current/Resources/";
	}

	public override string GetRhinoThirdPartyFolder()
	{
		// Use SDK for Rhino 7 so that we don't have to build against .Net 7
		return Path.Combine("$(EngineDir)", "Restricted", "NotForLicensees", "Source", "ThirdParty", "Enterprise", "RhinoCommonSDK_7");
	}

}
