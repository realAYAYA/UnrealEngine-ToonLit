// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11 : ModuleRules
{
	public DX11(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64 )
		{
			PublicSystemIncludePaths.Add(DirectX.GetIncludeDir(Target));

			string LibDir = DirectX.GetLibDir(Target);
			PublicAdditionalLibraries.AddRange(
				new string[] {
					LibDir + "dxgi.lib",
					LibDir + "d3d9.lib",
					LibDir + "d3d11.lib",
					LibDir + "dxguid.lib",
					LibDir + "dinput8.lib",
					LibDir + "xapobase.lib",
					LibDir + "XAPOFX.lib"
					// do not add d3dcompiler to the list - the engine must explicitly load 
					// the bundled compiler library to make shader compilation repeatable
					}
				);
		}
	}
}

