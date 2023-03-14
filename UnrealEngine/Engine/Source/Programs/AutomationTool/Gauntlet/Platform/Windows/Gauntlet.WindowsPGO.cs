// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace Gauntlet
{
	/// <summary>
	/// Skeleton Windows PGO platform implementation (primarily used to test PGO locally with editor builds)
	/// </summary>
	internal abstract class WinBasePGOPlatform : IPGOPlatform
	{
		UnrealTargetPlatform Platform;

		protected WinBasePGOPlatform(UnrealTargetPlatform InPlatform)
		{
			Platform = InPlatform;
		}

		public UnrealTargetPlatform GetPlatform()
		{
			return Platform;
		}

		public void GatherResults(string ArtifactPath)
		{

		}

		public void ApplyConfiguration(PGOConfig Config)
		{

		}

		public bool TakeScreenshot(ITargetDevice Device, string ScreenshotDirectory, out string ImageFilename)
		{			
			ImageFilename = string.Empty;
			return false;
		}
	}

	internal class WindowsPGOPlatform : WinBasePGOPlatform
	{
		public WindowsPGOPlatform() : base(UnrealTargetPlatform.Win64)
		{
		}
	}
}



