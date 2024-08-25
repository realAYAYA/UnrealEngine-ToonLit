// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class DatasmithRhino7Target : DatasmithRhinoBaseTarget
{
	public DatasmithRhino7Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "7"; }

	public override string GetRhinoInstallFolderWindows()
	{
		try
		{
			return OperatingSystem.IsWindows() ? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\McNeel\Rhinoceros\7.0\Install", "Path", "") as string : null;
		}
		catch(Exception)
		{
			return "";
		}
	}

	public override string GetRhinoInstallFolderMac()
	{
		return "/Applications/Rhino 7.app/Contents/Frameworks/RhCore.framework/Versions/Current/Resources/";
	}
}
