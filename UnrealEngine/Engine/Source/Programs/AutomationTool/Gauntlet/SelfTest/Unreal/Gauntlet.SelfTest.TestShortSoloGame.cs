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
	[TestGroup("Unreal", 10)]
	class TestOrionShortSoloGame : TestUnrealInstallAndRunBase
	{

		class ShortSoloOptions : UnrealOptions
		{
			public ShortSoloOptions()
			{
			}

			public override void ApplyToConfig(UnrealAppConfig AppConfig)
			{
				base.ApplyToConfig(AppConfig);

				AppConfig.CommandLine += " -gauntlet=SoakTest -soak.matchcount=1 -soak.matchlength=3";

				if (AppConfig.ProcessType.IsClient() && AppConfig.Platform == UnrealTargetPlatform.Win64)
				{            
					AppConfig.CommandLine += " -windowed -ResX=1280 -ResY=720 -nullrhi";
				}
			}
		}

		void TestClientPlatform(UnrealTargetPlatform Platform)
		{
			string GameName = this.ProjectFile.FullName;
			string BuildPath = this.BuildPath;
			string DevKit = this.DevkitName;

			if (GameName.Equals("OrionGame", StringComparison.OrdinalIgnoreCase) == false)
			{
				Log.Info("Skipping test {0} due to non-Orion project!", this);
				MarkComplete();
				return;
			}

			// create a new build
			UnrealBuildSource Build = new UnrealBuildSource(this.ProjectName, ProjectFile, this.UnrealPath, UsesSharedBuildType, BuildPath);

			// check it's valid
			if (!CheckResult(Build.GetBuildCount(Platform) > 0, "staged build was invalid"))
			{
				MarkComplete();
				return;
			}

			// Create devices to run the client and server
			IDeviceFactory HostFactory = Gauntlet.Utils.InterfaceHelpers.FindImplementations<IDeviceFactory>()
					.Where(F => F.CanSupportPlatform(BuildHostPlatform.Current.Platform))
					.FirstOrDefault();
			ITargetDevice ServerDevice = HostFactory.CreateDevice(BuildHostPlatform.Current.Platform.ToString() + " Server", Gauntlet.Globals.TempDir);
			ITargetDevice ClientDevice = null;

			string DeviceName = Gauntlet.Globals.Params.ParseValue("device", "default");
			IDeviceFactory Factory = Gauntlet.Utils.InterfaceHelpers.FindImplementations<IDeviceFactory>()
					.Where(F => F.CanSupportPlatform(Platform))
					.FirstOrDefault();

			ClientDevice = Factory.CreateDevice(DeviceName, Gauntlet.Globals.TempDir);

			UnrealAppConfig ServerConfig = Build.CreateConfiguration(new UnrealSessionRole(UnrealTargetRole.Server, ServerDevice.Platform, UnrealTargetConfiguration.Development));
			UnrealAppConfig ClientConfig = Build.CreateConfiguration(new UnrealSessionRole(UnrealTargetRole.Client, ClientDevice.Platform, UnrealTargetConfiguration.Development));

			if (!CheckResult(ServerConfig != null && ServerConfig != null, "Could not create configs!"))
			{
				MarkComplete();
				return;
			}

			ShortSoloOptions Options = new ShortSoloOptions();
			Options.ApplyToConfig(ClientConfig);
			Options.ApplyToConfig(ServerConfig);

			IAppInstall ClientInstall = ClientDevice.InstallApplication(ClientConfig);
			IAppInstall ServerInstall = ServerDevice.InstallApplication(ServerConfig);

			if (!CheckResult(ServerConfig != null && ServerConfig != null, "Could not create configs!"))
			{
				MarkComplete();
				return;
			}

			IAppInstance ClientInstance = ClientInstall.Run();
			IAppInstance ServerInstance = ServerInstall.Run();

			DateTime StartTime = DateTime.Now;
			bool RunWasSuccessful = true;

			while (ClientInstance.HasExited == false)
			{
				if ((DateTime.Now - StartTime).TotalSeconds > 800)
				{
					RunWasSuccessful = false;
					break;
				}
			}

			ClientInstance.Kill();
			ServerInstance.Kill();

			UnrealLogParser LogParser = new UnrealLogParser(ClientInstance.StdOut);

			UnrealLog.CallstackMessage ErrorInfo = LogParser.GetFatalError();
			if (ErrorInfo != null)
			{
				CheckResult(false, "FatalError - {0}", ErrorInfo.Message);
			}

			RunWasSuccessful = LogParser.HasRequestExit();

			CheckResult(RunWasSuccessful, "Failed to run for platform {0}", Platform);

		}

		public override void TickTest()
		{
			string PlatformString = Gauntlet.Globals.Params.ParseValue("Platform", "Win64");
			TestClientPlatform(UnrealTargetPlatform.Parse(PlatformString));

			MarkComplete();
		}
	}
}
