// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using AutomationTool;
using UnrealBuildTool;


namespace Gauntlet
{
	[System.Runtime.Versioning.SupportedOSPlatform("windows")]
	internal class HoloLensPGOPlatform : IPGOPlatform
	{
		string LocalOutputDirectory;
		string LocalProfDataFile;

		public UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.HoloLens;
		}

		public void GatherResults(string ArtifactPath)
		{
			throw new NotImplementedException();
		}

		public void ApplyConfiguration(PGOConfig Config)
		{
			// TODO: Is this sufficient for HoloLens?
			LocalOutputDirectory = Path.GetFullPath(Config.ProfileOutputDirectory);
			LocalProfDataFile = Path.Combine(LocalOutputDirectory, "profile.profdata");

			UnrealTestRole ClientRole = Config.RequireRole(UnrealTargetRole.Client);
			ClientRole.CommandLine += " -pgoprofile";
		}

		public bool TakeScreenshot(ITargetDevice Device, string ScreenshotDirectory, out string ImageFilename)
		{
			ImageFilename = "";

			// Get device for screenshot
			TargetDeviceHoloLens HoloLensDevice = Device as TargetDeviceHoloLens;

			if (HoloLensDevice == null)
			{
				return false;
			}

			ImageFilename = "temp.bmp";
			return HoloLensDevice.TakeScreenshot(Path.Combine(ScreenshotDirectory, ImageFilename));
		}

	}
}



