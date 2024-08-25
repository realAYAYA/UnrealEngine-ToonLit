// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OodleDataCompression_VisionOS : OodleDataCompression
{
	string LibExt { get { return Target.Architecture == UnrealArch.IOSSimulator ? ".sim.a" : ".a"; } }
	protected override string ReleaseLibraryName { get { return "liboo2corevisionos" + LibExt; } }
	protected override string DebugLibraryName { get { return "liboo2corevisionos_dbg" + LibExt; } }

	public OodleDataCompression_VisionOS(ReadOnlyTargetRules Target) : base(Target)
	{
	}
}
