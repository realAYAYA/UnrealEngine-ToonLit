// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

class VS2019TargetPlatform_HoloLens : BuildCMakeLib.VS2019TargetPlatform
{
	public override string PlatformOrGroupName => nameof(UnrealTargetPlatform.HoloLens);
	public override string DebugDatabaseExtension => "pdb";
	public override string DynamicLibraryExtension => "dll";
	public override string StaticLibraryExtension => "lib";
	public override string VariantDirectory => Architecture.ToLower();
	public override bool IsPlatformExtension => true;

	private readonly string Architecture;

	public VS2019TargetPlatform_HoloLens(string Architecture = "Win64")
	{
		this.Architecture = Architecture;
	}

	public override string GetCMakeSetupArguments(BuildCMakeLib.TargetLib TargetLib, string TargetConfiguration)
	{
		return base.GetCMakeSetupArguments(TargetLib, TargetConfiguration)
			+ string.Format(" -A {0}", Architecture);
	}
}