// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

namespace Gauntlet
{
	public class LinuxBuildSource : StagedBuildSource<StagedBuild>
	{
		public override string BuildName { get { return "LinuxStagedBuild"; } }

		public override UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.Linux; } }

		public override string PlatformFolderPrefix { get { return "Linux"; } }
	}
}
