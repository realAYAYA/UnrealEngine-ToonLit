// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LyraServerEOSTarget : LyraServerTarget
{
	public LyraServerEOSTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "EOS";
	}
}
