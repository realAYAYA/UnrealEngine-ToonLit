// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using Gauntlet;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace UE
{
	/// <summary>
	/// Define a Config class for UE.EditorAutomation and UE.TargetAutomation with the
	/// options that can be used
	/// </summary>
	public class AutomationTestConfig : UnrealTestConfiguration
	{
		/// <summary>
		/// Run with specify RHI
		/// </summary>
		[AutoParam]
		public string RHI = "";
		/// <summary>
		/// Valid RHI names based on Platform
		/// </summary>
		private static class ValidRHI
		{
			public enum Win64
			{
				d3d11,
				d3d12,
				vulkan
			}
			public enum Linux
			{
				vulkan
			}
			public enum Mac
			{
				metal
			}
		}

		/// <summary>
		/// Enable the RHI validation layer.
		/// </summary>
		[AutoParam]
		public virtual bool RHIValidation { get; set; } = false;

		/// <summary>
		/// Run with Nvidia cards for raytracing support
		/// </summary>
		[AutoParam]
		public bool PreferNvidia = false;

		/// <summary>
		/// Run forcing raytracing 
		/// </summary>
		[AutoParam]
		public bool RayTracing = false;

		/// <summary>
		/// Run forcing raytracing 
		/// </summary>
		[AutoParam]
		public bool StompMalloc = false;

		/// <summary>
		/// Run with d3ddebug
		/// </summary>
		[AutoParam]
		public bool D3DDebug = false;

		/// <summary>
		/// Disable capturing frame trace for image based tests
		/// </summary>
		[AutoParam]
		public bool DisableFrameTraceCapture = true;

		/// <summary>
		/// Filter or groups of tests to apply
		/// </summary>
		[AutoParam]
		public string RunTest = "";

		/// <summary>
		/// Absolute or project relative path to write an automation report to.
		/// </summary>
		[AutoParam]
		public string ReportExportPath = "";

		/// <summary>
		/// Path the report can be found at
		/// </summary>
		[AutoParam]
		public string ReportURL = "";

		/// <summary>
		/// Absolute or project relative directory path to write automation telemetry outputs.
		/// </summary>
		[AutoParam]
		public string TelemetryDirectory = "";

		/// <summary>
		/// Use Simple Horde Report instead of Unreal Automated Tests
		/// </summary>
		[AutoParam]
		public virtual bool SimpleHordeReport { get; set; } = false;

		/// <summary>
		/// Validate DDC during tests
		/// </summary>
		[AutoParam]
		public bool VerifyDDC = false;

		/// <summary>
		/// Validate DDC during tests
		/// </summary>
		[AutoParam]
		public string DDC = "";

		/// <summary>
		/// Used for having the editor and any client communicate
		/// </summary>
		public string SessionID = Guid.NewGuid().ToString();
		public void GenerateSessionID() { SessionID = Guid.NewGuid().ToString(); }


		/// <summary>
		/// Implement how the settings above are applied to different roles in the session. This is the main avenue for converting options
		/// into command line parameters for the different roles
		/// </summary>
		/// <param name="AppConfig"></param>
		/// <param name="ConfigRole"></param>
		/// <param name="OtherRoles"></param>
		public override void ApplyToConfig(UnrealAppConfig AppConfig, UnrealSessionRole ConfigRole, IEnumerable<UnrealSessionRole> OtherRoles)
		{

			base.ApplyToConfig(AppConfig, ConfigRole, OtherRoles);

			// The "RunTest" argument is required since it is what is passed to the editor to specify which tests to execute
			if (string.IsNullOrEmpty(RunTest))
			{
				throw new AutomationException("No AutomationTest argument specified. Use -RunTest=\"Group:AI\", -RunTest=\"Project\", -RunTest=\"Navigation.Landscape Ramp Test\" etc.");
			}

			if (ConfigRole.RoleType.IsEditor())
			{
				if (ResumeOnCriticalFailure)
				{
					AppConfig.CommandLine += " -ResumeRunTest";
				}
				// Are we writing out info for Horde?
				if (WriteTestResultsForHorde)
				{
					if (string.IsNullOrEmpty(ReportExportPath))
					{
						ReportExportPath = Path.Combine(Globals.TempDir, "TestReport");
					}
					if (string.IsNullOrEmpty(HordeTestDataPath))
					{
						HordeTestDataPath = HordeReport.DefaultTestDataDir;
					}
					if (string.IsNullOrEmpty(HordeArtifactPath))
					{
						HordeArtifactPath = HordeReport.DefaultArtifactsDir;
					}
				}
				// Arguments for writing out the report and providing a URL where it can be viewed
				string ReportArgs = "";
				if (!string.IsNullOrEmpty(ReportExportPath))
				{
					ReportArgs = string.Format(" -ReportExportPath=\"{0}\"", ReportExportPath);
				}
				if (!string.IsNullOrEmpty(ReportURL))
				{
					ReportArgs += string.Format(" -ReportURL=\"{0}\"", ReportURL);
				}
				AppConfig.CommandLine += ReportArgs;
			}

			// Setup commandline for telemetry outputs
			if (!string.IsNullOrEmpty(PublishTelemetryTo) || !string.IsNullOrEmpty(TelemetryDirectory))
			{
				if (string.IsNullOrEmpty(TelemetryDirectory))
				{
					TelemetryDirectory = Path.Combine(Globals.TempDir, "Telemetry");
				}
				if (Directory.Exists(TelemetryDirectory))
				{
					// clean any previous data
					Directory.Delete(TelemetryDirectory, true);
				}
				AppConfig.CommandLine += string.Format("-TelemetryDirectory=\"{0}\"", TelemetryDirectory);
			}

			string AutomationTestArgument = string.Format("RunTest {0};", RunTest);

			// if this is not attended then we'll quit the editor after the tests and disable any user interactions
			if (this.Attended == false)
			{
				AutomationTestArgument += "Quit;";
				AppConfig.CommandLine += " -unattended";
			}

			bool HasNoOtherRole = !OtherRoles.Any();
			// If there's only one role and it's the editor then tests are running under the editor with no target
			if (ConfigRole.RoleType.IsEditor() && HasNoOtherRole)
			{ 
				AppConfig.CommandLine += string.Format(" -ExecCmds=\"Automation {0}\"", AutomationTestArgument);		
			}
			else
			{
				// If the test isnt using the editor for both roles then pass the IP of the editor (us) to the client
				string HostIP = UnrealHelpers.GetHostIpAddress();

				if (ConfigRole.RoleType.IsClient())
				{
					// have the client list the tests it knows about. useful for troubleshooting discrepencies
					AppConfig.CommandLine += string.Format(" -sessionid={0} -messaging -log -TcpMessagingConnect={1}:6666 -ExecCmds=\"Automation list\"", SessionID, HostIP);
				}
				else if (ConfigRole.RoleType.IsEditor())
				{
					AppConfig.CommandLine += string.Format(" -ExecCmds=\"Automation StartRemoteSession {0}; {1}\" -TcpMessagingListen={2}:6666 -multihome={3}", SessionID, AutomationTestArgument, HostIP, HostIP);
				}
			}

			if ((ConfigRole.RoleType.IsEditor() && HasNoOtherRole) || ConfigRole.RoleType.IsClient())
			{
				// These are flags that are required only on the main role that is going to execute the tests. ie raytracing is required only on the client or if it is an editor test.
				if (DisableFrameTraceCapture || RayTracing)
				{
					AppConfig.CommandLine += " -DisableFrameTraceCapture";
				}

				if (RayTracing)
				{
					AppConfig.CommandLine += " -dpcvars=r.RayTracing=1,r.SkinCache.CompileShaders=1,AutomationAllowFrameTraceCapture=0";
				}
				else
				{
					AppConfig.CommandLine += " -dpcvars=r.RayTracing=0";
				}

				// Options specific to windows
				if (ConfigRole.Platform != null && ((UnrealTargetPlatform)ConfigRole.Platform).IsInGroup(UnrealPlatformGroup.Windows))
				{
					if (PreferNvidia)
					{
						AppConfig.CommandLine += " -preferNvidia";
					}

					if (!string.IsNullOrEmpty(RHI))
					{
						if (Enum.IsDefined(typeof(ValidRHI.Win64), RHI.ToLower()))
						{
							AppConfig.CommandLine += string.Format(" -{0}", RHI);
						}
						else
						{
							throw new AutomationException(string.Format("Unknown RHI target '{0}' for Win64", RHI));
						}
					}

					if (D3DDebug)
					{
						AppConfig.CommandLine += " -d3ddebug";
					}

					if (StompMalloc)
					{
						AppConfig.CommandLine += " -stompmalloc";
					}
				}
			}

			if (RHIValidation)
			{
				AppConfig.CommandLine += " -rhivalidation";
			}

			// Options specific to roles running under the editor
			if (ConfigRole.RoleType.UsesEditor())
			{
				if (VerifyDDC)
				{
					AppConfig.CommandLine += " -VerifyDDC";
				}

				if (!string.IsNullOrEmpty(DDC))
				{
					AppConfig.CommandLine += string.Format(" -ddc={0}", DDC);
				}
			}

			if (!string.IsNullOrEmpty(VerboseLogCategories))
			{
				string LogCmdsString = string.Join(',',
					VerboseLogCategories
					.Split(',')
					.Select(LogName => LogName + " Verbose"));
				AppConfig.CommandLine += string.Format(" -LogCmds=\"{0}\"", LogCmdsString);
			}

		}
	}

	/// <summary>
	/// Implements a node that runs Unreal automation tests using the editor. The primary argument is "RunTest". E.g
	/// RunUnreal -test=UE.EditorAutomation -RunTest="Group:Animation"
	/// </summary>
	public class EditorAutomation : AutomationNodeBase<AutomationTestConfig>
	{
		public EditorAutomation(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{

		}

		/// <summary>
		/// Define the configuration for this test. Most options are applied by the test config above
		/// </summary>
		/// <returns></returns>
		public override AutomationTestConfig GetConfiguration()
		{
			if (CachedConfig != null)
			{
				return CachedConfig;
			}

			AutomationTestConfig Config = base.GetConfiguration();

			// Tests in the editor only require a single role
			UnrealTestRole EditorRole = Config.RequireRole(Config.CookedEditor ? UnrealTargetRole.CookedEditor : UnrealTargetRole.Editor);
			EditorRole.CommandLineParams.AddRawCommandline("-NoWatchdog -stdout -FORCELOGFLUSH -CrashForUAT -log");

			return Config;
		}
	}

	/// <summary>
	/// Implements a node that runs Unreal automation tests on a target, monitored by an editor. The primary argument is "RunTest". E.g
	/// RunUnreal -test=UE.EditorAutomation -RunTest="Group:Animation"
	/// </summary>
	public class TargetAutomation : AutomationNodeBase<AutomationTestConfig>
	{
		public TargetAutomation(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{
		}

		/// <summary>
		/// Define the configuration for this test. Most options are applied by the test config above
		/// </summary>
		/// <returns></returns>
		public override AutomationTestConfig GetConfiguration()
		{
			if (CachedConfig != null)
			{
				return CachedConfig;
			}

			AutomationTestConfig Config = base.GetConfiguration();

			// Target tests require an editor which hosts the process
			UnrealTestRole EditorRole = Config.RequireRole(UnrealTargetRole.Editor);
			EditorRole.CommandLineParams.AddRawCommandline("-NoWatchdog -stdout -FORCELOGFLUSH -CrashForUAT -log");

			if (Config.Attended == false)
			{
				// if this is unattended we don't need the UI ( this also alows a wider range of machines to run the test under CIS)
				EditorRole.CommandLineParams.Add("nullrhi");
			}

			// target tests also require a client
			Config.RequireRole(UnrealTargetRole.Client);
			return Config;
		}
	}

	/// <summary>
	/// Base class for automation tests. Most of the magic is in here with the Editor/Target forms simply defining the roles
	/// </summary>
	public abstract class AutomationNodeBase<TConfigClass> : UnrealTestNode<TConfigClass>
		where TConfigClass : UnrealTestConfiguration, new()
	{
		// used to track stdout from the processes 
		private int LastAutomationEntryCount = 0;

		private UnrealAutomatedTestPassResults TestPassResults = null;

		private DateTime LastAutomationEntryTime = DateTime.MinValue;

		public AutomationNodeBase(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{
		}

	
		/// <summary>
		/// Override our name to include the filter we're testing
		/// </summary>
		public override string Name
		{
			get
			{
				string BaseName = base.Name;

				var Config = GetConfiguration();
				if (Config is AutomationTestConfig)
				{
					var AutomationConfig = Config as AutomationTestConfig;
					if (!string.IsNullOrEmpty(AutomationConfig.RunTest))
					{
						BaseName += string.Format("(RunTest={0})", AutomationConfig.RunTest);
					}
				}

				return BaseName;
			}
		}

		/// <summary>
		/// Override the Suite by RunTest if it is a Group
		/// </summary>
		public override string Suite
		{
			get
			{
				string BaseSuite = base.Suite;
				var Config = GetConfiguration();
				if (Config is AutomationTestConfig)
				{
					var AutomationConfig = Config as AutomationTestConfig;
					if (!string.IsNullOrEmpty(AutomationConfig.RunTest) && AutomationConfig.RunTest.ToLower().StartsWith("group:"))
					{
						BaseSuite  = AutomationConfig.RunTest.Substring(6);
					}
				}

				return BaseSuite;
			}
		}

		/// <summary>
		/// Override the Type
		/// </summary>
		public override string Type
		{
			get
			{
				string Base = "UE.Automation";
				var Config = GetConfiguration();
				if (Config is AutomationTestConfig)
				{
					var AutomationConfig = Config as AutomationTestConfig;
					if (!string.IsNullOrEmpty(AutomationConfig.RunTest))
					{
						return string.Format("{0}({1}) {2}", Base, AutomationConfig.RunTest, Context.BuildInfo.ProjectName);
					}
				}

				return string.Format("{0}({1})", Base, Suite);
			}
		}

		/// <summary>
		/// Called when a test is starting
		/// </summary>
		/// <param name="Pass"></param>
		/// <param name="InNumPasses"></param>
		/// <returns></returns>
		public override bool StartTest(int Pass, int InNumPasses)
		{
			LastAutomationEntryTime = DateTime.MinValue;
			LastAutomationEntryCount = 0;
			TestPassResults = null;

			if (GetConfiguration() is AutomationTestConfig Config)
			{
				if (Config.ResumeOnCriticalFailure && string.IsNullOrEmpty(Config.ReportExportPath))
				{
					Config.ReportExportPath = Path.Combine(Globals.TempDir, "TestReport");
				}
				string ReportExportPath = Config.ReportExportPath;
				if (!string.IsNullOrEmpty(ReportExportPath))
				{
					// clean up previous run if any on StartTest (not RestartTest)
					if (Directory.Exists(ReportExportPath))
					{
						DirectoryInfo ReportDirInfo = new DirectoryInfo(ReportExportPath);
						ReportDirInfo.Delete(true);
					}
				}
			}

			return base.StartTest(Pass, InNumPasses);
		}

		public override bool RestartTest()
		{
			LastAutomationEntryTime = DateTime.MinValue;
			LastAutomationEntryCount = 0;
			TestPassResults = null;

			if (GetConfiguration() is AutomationTestConfig Config)
			{
				Config.GenerateSessionID();
			}

			return base.RestartTest();
		}

		/// <summary>
		/// Override TickTest to log interesting things and make sure nothing has stalled
		/// </summary>
		public override void TickTest()
		{
			const float IdleTimeout = 30 * 60;

			List<string> ChannelEntries = new List<string>();

			// We are primarily interested in what the editor is doing
			var AppInstance = TestInstance.EditorApp;

			UnrealLogParser Parser = new UnrealLogParser(AppInstance.StdOut);
			ChannelEntries.AddRange(Parser.GetEditorBusyChannels());

			// Any new entries?
			if (ChannelEntries.Count > LastAutomationEntryCount)
			{
				// log new entries so people have something to look at
				ChannelEntries.Skip(LastAutomationEntryCount).ToList().ForEach(S => Log.Info("{0}", S));
				LastAutomationEntryTime = DateTime.Now;
				LastAutomationEntryCount = ChannelEntries.Count;
			}
			else
			{
				// Check for timeouts
				if (LastAutomationEntryTime == DateTime.MinValue)
				{
					LastAutomationEntryTime = DateTime.Now;
				}

				double ElapsedTime = (DateTime.Now - LastAutomationEntryTime).TotalSeconds;

				// Check for timeout
				if (ElapsedTime > IdleTimeout)
				{
					Log.Error("No activity observed in last {0:0.00} minutes. Aborting test", IdleTimeout / 60);
					MarkTestComplete();
					SetUnrealTestResult(TestResult.TimedOut);
				}
			}

			base.TickTest();
		}

		private UnrealAutomatedTestPassResults GetTestPassResults(UnrealLog InLog)
		{
			if (TestPassResults == null)
			{
				string AutomationReportPath = string.Empty;
				if (GetConfiguration() is AutomationTestConfig Config)
				{
					AutomationReportPath = Config.ReportExportPath;
					if (!string.IsNullOrEmpty(AutomationReportPath))
					{
						string JsonReportFile = Path.Combine(AutomationReportPath, "index.json");
						if (File.Exists(JsonReportFile))
						{
							try
							{
								TestPassResults = UnrealAutomatedTestPassResults.LoadFromJson(JsonReportFile);
							}
							catch (Exception Ex)
							{
								Log.Warning("Failed to load Json report. {0}", Ex);
							}
						}
					}
				}
				
				if(TestPassResults == null && InLog != null)
				{
					// Parse automaton info from the log then
					TestPassResults = new UnrealAutomatedTestPassResults();
					AutomationLogParser LogParser = new AutomationLogParser(InLog.FullLogContent);
					IEnumerable<UnrealAutomatedTestResult> LogTestResults = LogParser.GetResults();
					if (LogTestResults.Any())
					{
						foreach (UnrealAutomatedTestResult LogTestResult in LogTestResults)
						{
							TestPassResults.AddTest(LogTestResult);
						}
					}
					else
					{
						foreach (UnrealLog.LogEntry Entry in LogParser.AutomationWarningsAndErrors)
						{
							switch (Entry.Level)
							{
								case UnrealLog.LogLevel.Error:
									Events.Add(new UnrealAutomationEvent(EventType.Error, Entry.Message));
									break;
								case UnrealLog.LogLevel.Warning:
									Events.Add(new UnrealAutomationEvent(EventType.Warning, Entry.Message));
									break;
							}
						}
					}
				}
			}

			return TestPassResults;
		}

		/// <summary>
		/// Look for critical failure during the test session and update the test states
		/// </summary>
		/// <param name="JsonTestPassResults"></param>
		private void UpdateTestStateOnCriticalFailure(UnrealAutomatedTestPassResults JsonTestPassResults)
		{
			bool HasTimeout = RoleResults != null && RoleResults.Where(R => R.ProcessResult == UnrealProcessResult.TimeOut).Any();
			string HordeArtifactPath = GetConfiguration().HordeArtifactPath;
			var MainRole = GetConfiguration().GetMainRequiredRole();
			string LastTestWithCriticalFailure = string.Empty;
			if (JsonTestPassResults.InProcess > 0)
			{
				// The test pass did not run completely
				Log.Verbose("Found in-process tests: {0}", JsonTestPassResults.InProcess);
				// Get any critical error and push it to json report and resave it.
				if (RoleResults != null)
				{
					UnrealLog.CallstackMessage FatalError = null;
					foreach (var Result in RoleResults)
					{
						if (Result.LogSummary.FatalError != null)
						{
							FatalError = Result.LogSummary.FatalError;
							break;
						}
					}
					var Test = JsonTestPassResults.Tests.FirstOrDefault((T => T.State == TestStateType.InProcess));
					if (!String.IsNullOrEmpty(Test.TestDisplayName))
					{
						LastTestWithCriticalFailure = Test.FullTestPath;
						string ErrorMessage;
						if (HasTimeout)
						{
							ErrorMessage = String.Format("Session reached timeout after {0} seconds.", MaxDuration);
						}
						else
						{
							ErrorMessage = "Engine encountered a critical failure. \n";
							if (FatalError != null)
							{
								ErrorMessage += FatalError.FormatForLog();
							}
							else
							{
								ErrorMessage += "No callstack found in the log.";
							}
						}
						Test.AddError(ErrorMessage);
						if (!CanRetry() || JsonTestPassResults.NotRun == 0)
						{
							// Setting the test as fail because no retry will be done anymore.
							// The InProcess state won't be used for pass resume
							Test.State = TestStateType.Fail;
							if (!CanRetry())
							{
								Test.AddWarning(string.Format("Session reached maximum of retries({0}) to resume on critical failure!", Retries));
							}
						}
						JsonTestPassResults.WriteToJson();
					}
				}
			}
			if (JsonTestPassResults.NotRun > 0)
			{
				// The test pass did not run at all
				Log.Verbose("Found not-run tests: {0}", JsonTestPassResults.NotRun);
				if (GetConfiguration().ResumeOnCriticalFailure && !HasTimeout)
				{
					// Reschedule test to resume from last 'in-process' test.
					if (SetToRetryIfPossible())
					{
						// Attach current artifacts to Horde output
						if (SessionArtifacts != null && !string.IsNullOrEmpty(HordeArtifactPath))
						{
							HordeReport.SimpleTestReport TempReport = new HordeReport.SimpleTestReport();
							TempReport.SetOutputArtifactPath(HordeArtifactPath);
							foreach (UnrealRoleArtifacts Artifact in SessionArtifacts)
							{
								string LogName = Path.GetFullPath(Artifact.LogPath).Replace(Path.GetFullPath(Context.Options.LogDir), "").TrimStart(Path.DirectorySeparatorChar);
								TempReport.AttachArtifact(Artifact.LogPath, LogName);
								// Reference last run instance log
								if (Artifact.SessionRole.RoleType == MainRole.Type)
								{
									JsonTestPassResults.Devices.Last().AppInstanceLog = LogName.Replace("\\", "/");
								}
							}
							JsonTestPassResults.WriteToJson();
						}
						// Discard the report as we are going to do another pass.
					}
					else
					{
						Log.Error("Reach maximum of retries({0}) to resume on critical failure!", Retries);
						// Adding a note to the report about why the not-run are not going to be run
						string Message = string.Format("Session reached maximum of retries({0}) to resume on critical failure!", Retries);
						if (!string.IsNullOrEmpty(LastTestWithCriticalFailure))
						{
							Message += string.Format(" \nLast critical failure was caught on {0}", LastTestWithCriticalFailure);
						}
						var NotRunTests = JsonTestPassResults.Tests.Where((T => T.State == TestStateType.NotRun));
						foreach (var Test in NotRunTests)
						{
							Test.AddInfo(Message);
						}
						JsonTestPassResults.WriteToJson();
					}
				}
				else if (HasTimeout)
				{
					// Adding a note to the report about why the not-run are not going to be run
					string Message = string.Format("Session reached timeout after {0} seconds.", MaxDuration);
					var NotRunTests = JsonTestPassResults.Tests.Where((T => T.State == TestStateType.NotRun));
					foreach (var Test in NotRunTests)
					{
						Test.AddInfo(Message);
					}
					JsonTestPassResults.WriteToJson();
				}
			}
		}

		/// <summary>
		/// Override GetExitCodeAndReason to provide additional checking of success / failure based on what occurred
		/// </summary>
		/// <param name="InArtifacts"></param>
		/// <param name="ExitReason"></param>
		/// <returns></returns>
		protected override UnrealProcessResult GetExitCodeAndReason(StopReason InReason, UnrealLog InLog, UnrealRoleArtifacts InArtifacts, out string ExitReason, out int ExitCode)
		{
			UnrealProcessResult UnrealResult = base.GetExitCodeAndReason(InReason, InLog, InArtifacts, out ExitReason, out ExitCode);

			// The editor is an additional arbiter of success
			if (InArtifacts.SessionRole.RoleType == UnrealTargetRole.Editor 
				&& InLog.HasAbnormalExit == false)
			{
				// if no fatal errors, check test results
				if (InLog.FatalError == null)
				{
					var TestResults = GetTestPassResults(InLog);

					// Tests failed so list that as our primary cause of failure
					if (TestResults != null && TestResults.Failed > 0)
					{
						ExitReason = string.Format("{0} of {1} test(s) failed", TestResults.Failed, TestResults.Tests.Count());
						ExitCode = -1;
						return UnrealProcessResult.TestFailure;
					}

					// If no tests were run then that's a failure (possibly a bad RunTest argument?)
					if (TestResults == null || TestResults.Tests.Count() == 0)
					{
						ExitReason = "No tests were executed!";
						ExitCode = -1;
						return UnrealProcessResult.TestFailure;
					}
				}
			}

			return UnrealResult;
		}

		/// <summary>
		/// Optional function that is called on test completion and gives an opportunity to create a report
		/// </summary>
		/// <param name="Result"></param>
		public override ITestReport CreateReport(TestResult Result)
		{
			ITestReport Report = null;
			if (GetConfiguration() is AutomationTestConfig Config)
			{
				// Parse the json test pass results and look for critical failure to add to report
				var TestPassResults = GetTestPassResults(null);
				if (TestPassResults != null)
				{
					UpdateTestStateOnCriticalFailure(TestPassResults);
				}
				// Save test result data for Horde build system
				bool WriteTestResultsForHorde = Config.WriteTestResultsForHorde;
				if (WriteTestResultsForHorde)
				{
					if (Config.SimpleHordeReport)
					{
						Report = base.CreateReport(Result);
					}
					else
					{
						string ReportPath = Config.ReportExportPath;
						if (!string.IsNullOrEmpty(ReportPath))
						{
							Report = CreateUnrealEngineTestPassReport(ReportPath, Config.ReportURL);
							if (Report != null)
							{
								var MainRolePlatform = Context.GetRoleContext(Config.GetMainRequiredRole().Type).Platform;
								Report.SetMetadata("RHI", string.IsNullOrEmpty(Config.RHI) || !MainRolePlatform.IsInGroup(UnrealPlatformGroup.Windows) ? "default" : Config.RHI.ToLower());
							}
						}
					}
				}

				string TelemetryDirectory = Config.TelemetryDirectory;
				if (Report != null && !string.IsNullOrEmpty(TelemetryDirectory) && Directory.Exists(TelemetryDirectory))
				{
					if (Report is ITelemetryReport Telemetry)
					{
						UnrealAutomationTelemetry.LoadOutputsIntoReport(TelemetryDirectory, Telemetry);
					}
					else
					{
						Log.Warning("Publishing Telemetry is requested but '{0}' does not support telemetry input.", Report.GetType().FullName);
					}
				}
			}

			return Report;
		}

		/// <summary>
		/// Override the summary report so we can create a custom summary with info about our tests and
		/// a link to the reports
		/// </summary>
		/// <returns></returns>
		protected override string GetTestSummaryHeader()
		{
			const int kMaxErrorsOrWarningsToDisplay = 5;

			MarkdownBuilder MB = new MarkdownBuilder(base.GetTestSummaryHeader());

			// Everything we need is in the editor artifacts
			var EditorRole = RoleResults.Where(R => R.Artifacts.SessionRole.RoleType == UnrealTargetRole.Editor).FirstOrDefault();

			if (EditorRole != null)
			{
				UnrealAutomatedTestPassResults JsonTestPassResults = GetTestPassResults(EditorRole.LogSummary);

				// Filter our tests into categories
				IEnumerable<UnrealAutomatedTestResult> AllTests = JsonTestPassResults.Tests.Where(T => !T.WasSkipped);
				IEnumerable<UnrealAutomatedTestResult> IncompleteTests = AllTests.Where(T => !T.IsComplete);
				IEnumerable<UnrealAutomatedTestResult> FailedTests = AllTests.Where(T => T.IsComplete && T.HasFailed);
				IEnumerable<UnrealAutomatedTestResult> TestsWithWarnings = AllTests.Where(T => T.HasSucceeded && T.HasWarnings);

				Func<string, string> ErrorLineForHorde = (L) => string.Format("Err: {0}", L);
				Func<string, string> WarningLineForHorde = (L) => string.Format("Warn: {0}", L);
				Func<IEnumerable<string>, IEnumerable<string>> CapErrorOrWarningList = (E) =>
				{
					if (E.Count() > kMaxErrorsOrWarningsToDisplay)
					{
						E = E.Skip(E.Count() - kMaxErrorsOrWarningsToDisplay);
					}
					return E;
				};
				// If there were abnormal exits then look only at the incomplete tests to avoid confusing things.
				if (GetRolesThatExitedAbnormally().Any())
				{
					if (AllTests.Count() == 0)
					{
						MB.H3("Error: No tests were executed.");
					}
					else if (IncompleteTests.Count() > 0)
					{
						var InProcess = IncompleteTests.Where(T => T.State == TestStateType.InProcess);
						if (InProcess.Any())
						{
							MB.H3("The following test(s) were incomplete:");
							foreach (UnrealAutomatedTestResult Result in InProcess)
							{
								MB.H4(string.Format("Error: Test '{0}' did not complete", Result.TestDisplayName));
								MB.Paragraph("FullName: " + Result.FullTestPath);
								MB.UnorderedList(CapErrorOrWarningList(Result.ErrorEvents.Select(E => ErrorLineForHorde(E.Message)).Distinct()));
								MB.UnorderedList(CapErrorOrWarningList(Result.WarningEvents.Select(E => WarningLineForHorde(E.Message)).Distinct()));
							}
						}
						var NotRun = IncompleteTests.Where(T => T.State == TestStateType.NotRun);
						if (NotRun.Any())
						{
							MB.H3("The following test(s) were not run:");
							MB.UnorderedList(NotRun.Select(T => T.FullTestPath));
						}
					}
				}
				else
				{
					if (AllTests.Count() == 0)
					{
						MB.H3("Error: No tests were executed.");

						IEnumerable<string> WarningsAndErrors = Events.Where(E => E.IsError || E.IsWarning).Select(E => E.IsError? ErrorLineForHorde(E.Message) : WarningLineForHorde(E.Message)).Distinct();

						if (WarningsAndErrors.Any())
						{
							WarningsAndErrors = CapErrorOrWarningList(WarningsAndErrors);
							MB.UnorderedList(WarningsAndErrors);
						}
						else
						{
							MB.Paragraph("Unknown failure.");
						}
					}
					else
					{
						// Now list the tests that failed
						if (FailedTests.Count() > 0)
						{
							MB.H3("The following test(s) failed:");

							foreach (UnrealAutomatedTestResult Result in FailedTests)
							{
								// only show the last N items
								IEnumerable<string> Events = Result.ErrorEvents.Select(E => ErrorLineForHorde(E.Message)).Distinct();

								Events = CapErrorOrWarningList(Events);

								MB.H4(string.Format("Error: Test '{0}' failed", Result.TestDisplayName));
								MB.Paragraph("FullName: " + Result.FullTestPath);
								MB.UnorderedList(Events);
							}
						}

						if (TestsWithWarnings.Count() > 0)
						{
							MB.H3("The following test(s) completed with warnings:");

							foreach (UnrealAutomatedTestResult Result in TestsWithWarnings)
							{
								// only show the last N items
								IEnumerable<string> WarningEvents = Result.WarningEvents.Select(E => WarningLineForHorde(E.Message)).Distinct();

								WarningEvents = CapErrorOrWarningList(WarningEvents);

								MB.H4(string.Format("Warning: Test '{0}' completed with warnings", Result.TestDisplayName));
								MB.Paragraph("FullName: " + Result.FullTestPath);
								MB.UnorderedList(WarningEvents);
							}
						}

						if (IncompleteTests.Count() > 0)
						{
							MB.H3("The following test(s) timed out or did not run:");

							foreach (UnrealAutomatedTestResult Result in IncompleteTests)
							{
								// only show the last N items
								IEnumerable<string> ErrorAndWarningEvents = Result.WarningAndErrorEvents.Select(E => E.IsError? ErrorLineForHorde(E.Message) : WarningLineForHorde(E.Message)).Distinct();

								ErrorAndWarningEvents = CapErrorOrWarningList(ErrorAndWarningEvents);

								MB.H4(string.Format("Error: Test '{0}' did not run or complete", Result.TestDisplayName));
								MB.Paragraph("FullName: " + Result.FullTestPath);
								MB.UnorderedList(ErrorAndWarningEvents);
							}
						}

						// show a brief summary at the end where it's most visible
						List<string> TestSummary = new List<string>();

						int PassedTests = AllTests.Count() - (FailedTests.Count() + IncompleteTests.Count());
						int TestsPassedWithoutWarnings = PassedTests - TestsWithWarnings.Count();

						TestSummary.Add(string.Format("{0} Test(s) Requested", AllTests.Count()));

						// Print out a summary of each category of result
						if (TestsPassedWithoutWarnings > 0)
						{
							TestSummary.Add(string.Format("{0} Test(s) Passed", TestsPassedWithoutWarnings));
						}

						if (TestsWithWarnings.Count() > 0)
						{
							TestSummary.Add(string.Format("{0} Test(s) Passed with warnings", TestsWithWarnings.Count()));
						}

						if (FailedTests.Count() > 0)
						{
							TestSummary.Add(string.Format("{0} Test(s) Failed", FailedTests.Count()));
						}

						if (IncompleteTests.Count() > 0)
						{
							TestSummary.Add(string.Format("{0} Test(s) didn't complete", IncompleteTests.Count()));
						}

						MB.H3("Summary");
						MB.UnorderedList(TestSummary);
					}
				}

				if (EditorRole.LogSummary.EngineInitialized)
				{
					string AutomationReportPath = string.Empty;
					string AutomationReportURL = string.Empty;
					if (GetConfiguration() is AutomationTestConfig Config)
					{
						AutomationReportPath = Config.ReportExportPath;
						AutomationReportURL = Config.ReportURL;
					}
					if (!string.IsNullOrEmpty(AutomationReportPath) || !string.IsNullOrEmpty(AutomationReportURL))
					{
						MB.H3("Links");

						if (string.IsNullOrEmpty(AutomationReportURL) == false)
						{
							MB.Paragraph(string.Format("View results here: {0}", AutomationReportURL));
						}

						if (string.IsNullOrEmpty(AutomationReportPath) == false)
						{
							MB.Paragraph(string.Format("Open results in UnrealEd from {0}", AutomationReportPath));
						}
					}
				}
			}

			return MB.ToString();
		}

		/// <summary>
		/// Returns Errors found during tests. We call the base version to get standard errors then
		/// Add on any errors seen in tests
		/// </summary>
		public override IEnumerable<string> GetErrors()
		{
			List<string> AllErrors = new List<string>(base.GetErrors());

			foreach (var Role in GetRolesThatFailed())
			{
				if (Role.Artifacts.SessionRole.RoleType == UnrealTargetRole.Editor)
				{
					AutomationLogParser Parser = new AutomationLogParser(Role.LogSummary.FullLogContent);
					AllErrors.AddRange(
						Parser.GetResults().Where(R => R.HasFailed)
							.SelectMany(R => R.Entries
								.Where(E => E.Event.Type == EventType.Error)
								.Distinct().Select(E => string.Format("[test={0}] {1}", R.TestDisplayName, E))
							)
						);
				}
			}

			return AllErrors;
		}

		/// <summary>
		/// Returns warnings found during tests. We call the base version to get standard warnings then
		/// Add on any errors seen in tests
		/// </summary>
		public override IEnumerable<string> GetWarnings()
		{
			List<string> AllWarnings = new List<string>(base.GetWarnings());

			if (SessionArtifacts == null)
			{
				return AllWarnings;
			}

			foreach (var Role in RoleResults)
			{
				if (Role.Artifacts.SessionRole.RoleType == UnrealTargetRole.Editor)
				{
					AutomationLogParser Parser = new AutomationLogParser(Role.LogSummary.FullLogContent);
					AllWarnings.AddRange(
						Parser.GetResults()
							.SelectMany(R => R.Entries
								.Where(E => E.Event.Type == EventType.Warning)
								.Distinct().Select(E => string.Format("[test={0}] {1}", R.TestDisplayName, E))
							)
						);
				}
			}

			return AllWarnings;
		}
	}

}
