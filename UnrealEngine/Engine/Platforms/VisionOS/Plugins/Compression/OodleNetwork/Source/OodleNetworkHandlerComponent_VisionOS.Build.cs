// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OodleNetworkHandlerComponent_VisionOS : OodleNetworkHandlerComponent
{
	string LibExt { get { return Target.Architecture == UnrealArch.IOSSimulator ? ".sim.a" : ".a"; } }
	protected override string ReleaseLibraryName { get { return "liboo2netvisionos" + LibExt; } }
	protected override string DebugLibraryName { get { return "liboo2netvisionos_dbg" + LibExt; } }

	public OodleNetworkHandlerComponent_VisionOS(ReadOnlyTargetRules Target) : base(Target)
	{
	}
}
