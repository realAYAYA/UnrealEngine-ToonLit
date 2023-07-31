// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class BaseTextureBuildWorkerTarget : TextureBuildWorkerTarget
{
	public BaseTextureBuildWorkerTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "BaseTextureBuildWorker";
	}
}
