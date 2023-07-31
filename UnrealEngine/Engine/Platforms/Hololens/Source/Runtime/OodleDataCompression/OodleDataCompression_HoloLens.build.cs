// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OodleDataCompression_HoloLens : OodleDataCompression
	{
		protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(GetType()).FullName; } }
		protected override string ReleaseLibraryName
		{
			get
			{
				if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64) // emulation target, bBuildForEmulation
				{
					return "oo2core_winuwp64.lib";

				}
				else if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64) // device target, bBuildForDevice
				{
					return "oo2core_winuwparm64.lib";
				}
				else
				{
					throw new System.Exception("Unknown architecture for HoloLens platform!");
				}
			}
		}
		protected override string DebugLibraryName
		{
			get
			{
				if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64) // emulation target, bBuildForEmulation
				{
					return "oo2core_winuwp64_debug.lib";

				}
				else if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64) // device target, bBuildForDevice
				{
					return "oo2core_winuwparm64_debug.lib";
				}
				else
				{
					throw new System.Exception("Unknown architecture for HoloLens platform!");
				}
			}
		}

		public OodleDataCompression_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
