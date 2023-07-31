// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	/// <summary>
	/// Base class that provides a utility function for running a config and checking that it was plausibly successful
	/// </summary>
	abstract class TestUnrealInstallAndRunBase : TestUnrealBase
	{
		protected void TestInstallThenRun(UnrealBuildSource Build, UnrealTargetRole ProcessType, ITargetDevice Device, UnrealTargetConfiguration Config, UnrealOptions InOptions = null)
		{
			// create a config based on the passed in params

			UnrealSessionRole Role = new UnrealSessionRole(ProcessType, Device.Platform, Config, InOptions);

			Log.Info("Testing {0}", Role);

			UnrealAppConfig AppConfig = Build.CreateConfiguration(Role);

			if (!CheckResult(AppConfig != null, "Could not create config for {0} {1} with platform {2} from build.", Config, ProcessType, Device))
			{
				MarkComplete();
				return;
			}

			// Install the app on this device
			IAppInstall AppInstall = Device.InstallApplication(AppConfig);

			CheckResult(AppConfig != null, "Could not create AppInstall for {0} {1} with platform {2} from build.", Config, ProcessType, Device);


			DateTime StartTime = DateTime.Now;

			// Run the app and wait for either a timeout or it to exit
			IAppInstance AppProcess = AppInstall.Run();
		
			while (AppProcess.HasExited == false)
			{
				if ((DateTime.Now - StartTime).TotalSeconds > 60)
				{
					break;
				}

				Thread.Sleep(1000);
			}

			// Check it didn't exit unexpectedly
			CheckResult(AppProcess.HasExited == false, "Failed to run {0} {1} with platform {2}", Config, ProcessType, Device);

			// but kill it
			AppProcess.Kill();

			// Check that it left behind some artifacts (minimum should be a log)
			int ArtifactCount = new DirectoryInfo(AppProcess.ArtifactPath).GetFiles("*", SearchOption.AllDirectories).Length;
			CheckResult(ArtifactCount > 0, "No artifacts on device!");
		}
	}
}
