// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DirectStorage : ModuleRules
{
	public DirectStorage(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string ArchFolder = Target.WindowsPlatform.Architecture.ToString();
			string IncludeFolder = Path.Combine(ModuleDirectory, "Include", "DirectStorage");
			string LibFolder = Path.Combine(ModuleDirectory, "Lib", ArchFolder);
			string BinariesFolder = Path.Combine("$(EngineDir)", "Binaries", "ThirdParty", "Windows", "DirectStorage", ArchFolder);

			PublicSystemIncludePaths.Add(IncludeFolder);
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "dstorage.lib"));

			PublicDelayLoadDLLs.Add("dstorage.dll");
			PublicDelayLoadDLLs.Add("dstoragecore.dll");
			RuntimeDependencies.Add(Path.Combine(BinariesFolder, "dstoragecore.dll"));
			RuntimeDependencies.Add(Path.Combine(BinariesFolder, "dstorage.dll"));
		}
	}
}
