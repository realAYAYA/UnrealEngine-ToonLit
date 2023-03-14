// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class ShaderConductor : ModuleRules
{
	public ShaderConductor(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "ShaderConductor", "ShaderConductor", "Include"));

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string SCBinariesDir = Path.Combine(Target.UEThirdPartyBinariesDirectory, "ShaderConductor", "Mac");
			AddDependency(SCBinariesDir, "libdxcompiler.dylib");
			AddDependency(SCBinariesDir, "libShaderConductor.dylib");
		}
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			string SCBinariesDir = Path.Combine(Target.UEThirdPartyBinariesDirectory, "ShaderConductor", "Win64");
			AddDependency(SCBinariesDir, "dxcompiler.dll");
			AddDependency(SCBinariesDir, "ShaderConductor.dll", bAddPDB: true);
			
			string SCLibPath = Path.Combine(Target.UEThirdPartySourceDirectory, "ShaderConductor", "ShaderConductor", "lib", "Win64", "ShaderConductor.lib");
			PublicAdditionalLibraries.Add(SCLibPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string SCBinariesDir = Path.Combine(Target.UEThirdPartyBinariesDirectory, "ShaderConductor", "Linux", "x86_64-unknown-linux-gnu");
			AddDependency(SCBinariesDir, "libdxcompiler.so");
			AddDependency(SCBinariesDir, "libShaderConductor.so");
		}
		else
		{
			string Err = string.Format("Attempt to build against ShaderConductor on unsupported platform {0}", Target.Platform);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}
	}
	
	private void AddDependency(string BinariesDir, string Filename, bool bAddPDB = false)
	{
		string FullPath = Path.Combine(BinariesDir, Filename);
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDelayLoadDLLs.Add(Filename);
			RuntimeDependencies.Add(FullPath, StagedFileType.NonUFS);
			if (bAddPDB)
			{
				RuntimeDependencies.Add(Path.ChangeExtension(FullPath, ".pdb"), StagedFileType.DebugNonUFS);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDelayLoadDLLs.Add(FullPath);
			RuntimeDependencies.Add(FullPath);
			PublicAdditionalLibraries.Add(FullPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
			PublicDelayLoadDLLs.Add(FullPath);
			RuntimeDependencies.Add(FullPath);
			PublicAdditionalLibraries.Add(FullPath);
		}
	}
}
