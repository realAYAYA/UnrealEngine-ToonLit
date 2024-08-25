// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Linq;
using UnrealBuildTool;

public class DX11 : ModuleRules
{
	public DX11(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicDependencyModuleNames.Add("DirectX");

			string[] AllD3DLibs = new string[]
			{
				"dxgi.lib",
				"d3d9.lib",
				"d3d11.lib",
				"dxguid.lib",
				"dinput8.lib",
				"xapobase.lib",
				// do not add d3dcompiler to the list - the engine must explicitly load 
				// the bundled compiler library to make shader compilation repeatable
			};

			string DirectXSDKDir = Target.WindowsPlatform.DirectXLibDir;
			PublicAdditionalLibraries.AddRange(AllD3DLibs.Select(LibName => Path.Combine(DirectXSDKDir, LibName)));

			PublicDelayLoadDLLs.Add("d3d9.dll");
			PublicDelayLoadDLLs.Add("d3d11.dll");
		}
	}
}

