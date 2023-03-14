// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	/// <summary>
	/// Validates that we can install and run all Windows based targets that are supported. P8 because this is the longest
	/// running Unreal-based test
	/// </summary>
	[TestGroup("Unreal", 8)]
	class TestUnrealInstallThenRunMac : TestUnrealInstallAndRunBase
	{
		public override void TickTest()
		{
			// create a new build
			UnrealBuildSource Build = new UnrealBuildSource(this.ProjectName, this.ProjectFile, this.UnrealPath, this.UsesSharedBuildType, this.BuildPath, new string[] { "" });

			// check it's valid
			if (!CheckResult(Build.GetBuildCount(UnrealTargetPlatform.Mac) > 0, "staged build was invalid"))
			{
				MarkComplete();
				return;
			}

			// Create devices to run the client and server
			ITargetDevice PCDevice = new TargetDeviceMac("LocalMac", Globals.TempDir);

			UnrealOptions GameOptions = new UnrealOptions();
			GameOptions.Windowed = true;
			GameOptions.ResX = 1280;
			GameOptions.ResY = 720;
			GameOptions.CommonArgs = "-log";

			UnrealOptions OtherOptions = new UnrealOptions();
			OtherOptions.CommonArgs = "-log";

			// editor configs
			TestInstallThenRun(Build, UnrealTargetRole.EditorGame, PCDevice, UnrealTargetConfiguration.Development, GameOptions);
			TestInstallThenRun(Build, UnrealTargetRole.EditorServer, PCDevice, UnrealTargetConfiguration.Development, OtherOptions);
			TestInstallThenRun(Build, UnrealTargetRole.Editor, PCDevice, UnrealTargetConfiguration.Development, OtherOptions);

			// Monolithic clients
			//TestInstallThenRun(Build, UnrealRoleType.Client, PCDevice, UnrealTargetConfiguration.Development, GameOptions);
			TestInstallThenRun(Build, UnrealTargetRole.Client, PCDevice, UnrealTargetConfiguration.Test, GameOptions);

			// Monolithic servers
			//TestInstallThenRun(Build, UnrealRoleType.Server, PCDevice, UnrealTargetConfiguration.Development, OtherOptions);
			//TestInstallThenRun(Build, UnrealRoleType.Server, PCDevice, UnrealTargetConfiguration.Test, OtherOptions);

			MarkComplete();
		}
	}
}
