// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	/// <summary>
	/// This test validates that the UnrealSession helper class brings everything together correctly
	/// </summary>
	[TestGroup("Unreal", 7)]
	class TestUnrealSession : TestUnrealBase
	{
		public override void TickTest()
		{
			AccountPool.Initialize();

			AccountPool.Instance.RegisterAccount(new EpicAccount("Foo", "Bar"));

			// Add three devices to the pool
			DevicePool.Instance.RegisterDevices(new ITargetDevice[] {
				new TargetDeviceWindows("Local PC1", Globals.TempDir)
				, new TargetDeviceWindows("Local PC2", Globals.TempDir)
			});
			
			// Create a new build (params come from our base class will be similar to "OrionGame" and "p:\builds\orion\branch-cl")
			UnrealBuildSource Build = new UnrealBuildSource(this.ProjectName, this.ProjectFile, this.UnrealPath, this.UsesSharedBuildType, this.BuildPath, new string[] { "" });

			// create a new options structure
			UnrealOptions Options = new UnrealOptions();

			// set the mapname, this will be applied automatically to the server
			Options.Map = "OrionEntry";
			Options.Log = true;

			// add some common options.
			string ServerArgs = " -nomcp -log";

			// We want the client to connect to the server, so get the IP address of this PC and add it to the client args as an ExecCmd
			string LocalIP = Dns.GetHostEntry(Dns.GetHostName()).AddressList.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork).First().ToString();
			string ClientArgs = string.Format(" -ExecCmds=\"open {0}\"", LocalIP);

			// create a new session with client & server roles
			string PlatformString = Gauntlet.Globals.Params.ParseValue("Platform", "Win64");
			UnrealTargetPlatform Platform = UnrealTargetPlatform.Parse(PlatformString);
			UnrealSession Session = new UnrealSession(Build, new[] {
				new UnrealSessionRole(UnrealTargetRole.Client, Platform, UnrealTargetConfiguration.Development, ClientArgs, Options)
				, new UnrealSessionRole(UnrealTargetRole.Server, UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development, ServerArgs, Options)
			});

			// launch an instance of this session
			UnrealSessionInstance SessionInstance = Session.LaunchSession();

			// wait for two minutes - long enough for anything to go wrong :)
			DateTime StartTime = DateTime.Now;

			while (SessionInstance.ClientsRunning && SessionInstance.ServerRunning)
			{
				if ((DateTime.Now - StartTime).TotalSeconds > 120)
				{
					break;
				}

				Thread.Sleep(1000);
			}

			// check these are both still running
			CheckResult(SessionInstance.ClientsRunning, "Clients exited during test");
			CheckResult(SessionInstance.ServerRunning, "Server exited during test");

			// shutdown the session
			SessionInstance.Shutdown();

			// shutdown the pools
			AccountPool.Shutdown();
			DevicePool.Shutdown();

			MarkComplete(TestResult.Passed);
		}
	}
}
