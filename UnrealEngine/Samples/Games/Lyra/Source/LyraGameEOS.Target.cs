// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LyraGameEOSTarget : LyraGameTarget
{
	public LyraGameEOSTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "EOS";
	}
}
