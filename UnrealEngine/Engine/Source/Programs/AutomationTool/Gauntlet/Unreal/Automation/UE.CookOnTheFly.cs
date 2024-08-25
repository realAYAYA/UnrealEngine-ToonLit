// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using Gauntlet;
using System.IO;
using System;
using static Gauntlet.UnrealSessionInstance;
using AutomationTool;
using UnrealBuildTool;

namespace UE
{
	/// <summary>
	/// Runs cook on the fly 2 test
	/// </summary>
	public class CookOnTheFly : UnrealTestNode<UnrealTestConfiguration>
	{
		string CookedProjectDirectoryPath;
		string CookedSettingsFilePath;
		DateTime StartTime = DateTime.Now;
		bool DeferredClientStarted = false;
		bool ServerStarted = false;
		bool ClientConnected = false;
		bool CookingRequest = false;
		bool CookOnTheFlyModeChange = false;
		int LastEditorLogLine = 0;
		int LastClientLogLine = 0;
		IEnumerable<string> LogCategories = new string[] { "LogCookOnTheFly", "LogCook" };

		public CookOnTheFly(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{
		}

		protected virtual string GetStartedCookServerString()
		{
			return "Unreal Network File Server is ready";
		}

		protected virtual string GetClientConnectedString()
		{
			return "Client connected";
		}

		protected virtual string GetCookingRequestString()
		{
			return "Received cook request for unknown package";
		}

		protected virtual string GetCookingProcessString()
		{
			return "Cooked packages";
		}

		protected virtual string GetGameStartString()
		{
			return "Starting Game";
		}

		protected virtual string GetCookModeString()
		{
			return "CookMode=CookOnTheFly";
		}

		protected virtual string GetEngineInitializedString()
		{
			return "Engine is initialized";
		}

		protected virtual string GetReceivedPackagesCookedString()
		{
			return "Received 'PackagesCooked' message";
		}

		protected virtual string GetCreatedTransportString()
		{
			return "Created transport";
		}

		public override UnrealTestConfiguration GetConfiguration()
		{
			UnrealTestConfiguration Config = base.GetConfiguration();
			UnrealTestRole EditorRole = Config.RequireRole(UnrealTargetRole.Editor);
			EditorRole.CommandLine += "-run=cook -cookonthefly -zenstore -log";

			UnrealTargetPlatform TargetPlatform = Context.GetRoleContext(Config.GetMainRequiredRole().Type).Platform;
			CookedProjectDirectoryPath = Path.GetDirectoryName(Context.Options.ProjectPath.ToNormalizedPath());
			string ProjectName = Context.Options.Project;
			if (TargetPlatform == UnrealTargetPlatform.Win64)
			{
				CookedSettingsFilePath = CookedProjectDirectoryPath + string.Format("\\Saved\\Cooked\\Windows\\{0}\\Metadata\\CookedSettings.txt", ProjectName);
			}
			else
			{
				CookedSettingsFilePath = CookedProjectDirectoryPath + string.Format("\\Saved\\Cooked\\{0}\\{1}\\Metadata\\CookedSettings.txt", TargetPlatform.ToString(), ProjectName);
			}

			UnrealTestRole ClientRole = Config.RequireRole(UnrealTargetRole.Client);
			string HostIP = UnrealHelpers.GetHostIpAddress();
			if (HostIP == null)
			{
				throw new AutomationException("Could not find local IP address");
			}

			ClientRole.DeferredLaunch = true;
			ClientRole.CommandLine += string.Format("-cookonthefly -filehostip=\"{0}\" -log -LogCmds=\"LogCookOnTheFly Verbose\"", HostIP);

			return Config;
		}

		public override bool StartTest(int Pass, int InNumPasses)
		{
			if (!base.StartTest(Pass, InNumPasses))
			{
				return false;
			}

			LastEditorLogLine = 0;
			LastClientLogLine = 0;
			StartTime = DateTime.Now;

			return true;
		}

		public override void TickTest()
		{
			base.TickTest();

			const int TimeoutDuration = 5;
			if ((DateTime.Now - StartTime).TotalMinutes > TimeoutDuration)
			{
				Log.Error("No logfile activity observed in last {0} minutes. Ending test", TimeoutDuration);
				MarkTestComplete();
				SetUnrealTestResult(Gauntlet.TestResult.TimedOut);
			}

			IAppInstance EditorInstance = TestInstance.EditorApp;
			if (EditorInstance == null)
			{
				Log.Error("Editor instance shouldn't be null");
				MarkTestComplete();
				SetUnrealTestResult(Gauntlet.TestResult.Failed);
			}

			UnrealLogStreamParser EditorLogParser = new UnrealLogStreamParser();
			LastEditorLogLine += EditorLogParser.ReadStream(EditorInstance.StdOut, LastEditorLogLine);
			IEnumerable<string> EditorCookEntries = EditorLogParser.GetLogFromChannels(LogCategories);

			if (EditorCookEntries.Any())
			{
				EditorCookEntries.ToList().ForEach(S => Log.Info("[Editor] " + S));
			}
			string CompletionString = GetStartedCookServerString();

			if (!ServerStarted)
			{
				if (EditorLogParser.GetLogLinesContaining(CompletionString).Any())
				{
					Log.Info("Found '{0}'. Cook Server Started", CompletionString);
					ServerStarted = true;
				}
			}

			if (ServerStarted && !DeferredClientStarted)
			{
				UnrealSessionInstance SessionInstance = TestSessions.GetValueOrDefault(Name).SessionInstance;
				if (SessionInstance != null)
				{
					IEnumerable<RoleInstance> DeferredRoles = SessionInstance.DeferredRoles;

					if (DeferredRoles.Count().Equals(1))
					{
						try
						{
							SessionInstance.LaunchDeferredRole(DeferredRoles.First().Role);
						}
						catch
						{
							throw new AutomationException("Couldn't launch client");
						}
					}
					else
					{
							Log.Error("There should only be one deferred instance");
							MarkTestComplete();
							SetUnrealTestResult(Gauntlet.TestResult.Failed);
					}
					DeferredClientStarted = true;
				}
			}

			if (DeferredClientStarted && !ClientConnected)
			{
				string ClientConnectedString = GetClientConnectedString();

				if (EditorLogParser.GetLogLinesContaining(ClientConnectedString).Any())
				{
					Log.Info("Found '{0}'. Client connected", ClientConnectedString);
					ClientConnected = true;
				}
			}

			if (ClientConnected && !CookingRequest)
			{
				string CookingRequestString = GetCookingRequestString();

				if (EditorLogParser.GetLogLinesContaining(CookingRequestString).Any())
				{
					Log.Info("Found '{0}'. Cooking request exists", CookingRequestString);
					CookingRequest = true;
				}
			}

			if (CookingRequest && !CookOnTheFlyModeChange)
			{
				string CookedSettingsFileText = File.ReadAllText(CookedSettingsFilePath);
				string CookModeString = GetCookModeString();
				DateTime CookedDirectoryLastModifiedTime = File.GetLastWriteTime(CookedProjectDirectoryPath + "\\Saved\\Cooked\\");
				if (CookedSettingsFileText.Contains(CookModeString) && (CookedDirectoryLastModifiedTime>StartTime))
				{
					Log.Info("Found '{0}'. The cook mode changed for the project", CookModeString);
					CookOnTheFlyModeChange = true;
				}
			}

			if (CookOnTheFlyModeChange)
			{
				IAppInstance[] ClientInstances = TestInstance.ClientApps;
				IAppInstance ClientInstance = null;
				if (ClientInstances.Count().Equals(1))
				{
					ClientInstance = ClientInstances.First();
				}
				else
				{
					Log.Error("There should only be one client instance");
					MarkTestComplete();
					SetUnrealTestResult(Gauntlet.TestResult.Failed);
				}

				if (ClientInstance == null)
				{
					Log.Error("Client instance shouldn't be null");
					MarkTestComplete();
					SetUnrealTestResult(Gauntlet.TestResult.Failed);
				}

				UnrealLogStreamParser ClientLogParser = new UnrealLogStreamParser();
				ClientLogParser.ReadStream(ClientInstance.StdOut);

				string EngineInitializedString = GetEngineInitializedString();
				if (ClientLogParser.GetLogLinesContaining(EngineInitializedString).Any())
				{
					string GameStartedString = GetGameStartString();
					string CookingProcessString = GetCookingProcessString();
					string ReceivedPackagesCookedString = GetReceivedPackagesCookedString();
					string TransportCreatedString = GetCreatedTransportString();
					IEnumerable<string> ClientCookEntries = ClientLogParser.GetLogFromChannels(LogCategories);

					if (ClientCookEntries.Count() > LastClientLogLine)
					{
						ClientCookEntries.Skip(LastClientLogLine).ToList().ForEach(S => Log.Info("[Client] " + S));
						LastClientLogLine = ClientCookEntries.Count();
					}

					if (ClientLogParser.GetLogLinesContaining(GameStartedString).Any()
						&& EditorLogParser.GetLogLinesContaining(CookingProcessString).Any()
						&& ClientCookEntries.Any()
						&& ClientLogParser.GetLogLinesContaining(ReceivedPackagesCookedString).Any()
						&& ClientLogParser.GetLogLinesContaining(TransportCreatedString).Any())
					{
						Log.Info("Found '{0}', '{1}', '{2}', '{3}'. The CookOnTheFly log channel is active. The cooking process is taking place.", GameStartedString, CookingProcessString, ReceivedPackagesCookedString, TransportCreatedString);
						MarkTestComplete();
						SetUnrealTestResult(Gauntlet.TestResult.Passed);
					}
				}
			}
		}
	
		protected override UnrealProcessResult GetExitCodeAndReason(StopReason InReason, UnrealLog InLog, UnrealRoleArtifacts InArtifacts, out string ExitReason, out int ExitCode)
		{
			UnrealProcessResult UnrealResult = base.GetExitCodeAndReason(InReason, InLog, InArtifacts, out ExitReason, out ExitCode);
			// Rewriting the flags to handle the EngineInitialized error that appears if we run the editor with the -run=cook and -cookonthefly parameters.
			if (!InLog.EngineInitialized && InArtifacts.SessionRole.RoleType == UnrealTargetRole.Editor && ServerStarted)
			{
				InLog.EngineInitialized = true;
				InLog.RequestedExit = true;
				UnrealResult = base.GetExitCodeAndReason(InReason, InLog, InArtifacts, out ExitReason, out ExitCode);
			}
			return UnrealResult;
		}
	}
}