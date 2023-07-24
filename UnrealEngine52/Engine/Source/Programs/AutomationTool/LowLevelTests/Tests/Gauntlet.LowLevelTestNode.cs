// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using Gauntlet;
using System;
using System.IO;
using System.Linq;
using UnrealBuildBase;
using UnrealBuildTool;

namespace LowLevelTests
{
	public class LowLevelTests : BaseTest
	{
		private LowLevelTestContext Context;

		private IAppInstance TestInstance;

		private DateTime SessionStartTime = DateTime.MinValue;

		private TestResult LowLevelTestResult;

		public LowLevelTestsSession LowLevelTestsApp { get; private set; }

		public LowLevelTests(LowLevelTestContext InContext)
		{
			Context = InContext;

			MaxDuration = 60 * 30;
			LowLevelTestResult = TestResult.Invalid;
		}

		public string DefaultCommandLine;
		private string ArtifactPath;

		public override string Name { get { return "LowLevelTest"; } }

		public override float MaxDuration { protected set; get; }

		public override bool IsReadyToStart()
		{
			if (LowLevelTestsApp == null)
			{
				LowLevelTestsApp = new LowLevelTestsSession(Context.BuildInfo, Context.Options.Tags, Context.Options.Sleep, Context.Options.AttachToDebugger, Context.Options.ReportType);
			}

			return LowLevelTestsApp.TryReserveDevices();
		}

		public override string GetTestSummary()
		{
			return "Low Level Test";
		}

		public override TestResult GetTestResult()
		{
			return LowLevelTestResult;
		}

		public override void SetTestResult(TestResult testResult)
		{
			LowLevelTestResult = testResult;
		}

		public override bool StartTest(int Pass, int NumPasses)
		{
			if (LowLevelTestsApp == null)
			{
				throw new AutomationException("Node already has a null LowLevelTestsApp, was IsReadyToStart called?");
			}

			ArtifactPath = Path.Join(Context.Options.LogDir, Context.Options.TestApp);
			Log.Info("LowLevelTestNode.StartTest Creating artifacts path at {0}", ArtifactPath);
			Directory.CreateDirectory(ArtifactPath);

			TestInstance = LowLevelTestsApp.InstallAndRunNativeTestApp();
			if (TestInstance != null)
			{
				IDeviceUsageReporter.RecordComment(TestInstance.Device.Name, (UnrealTargetPlatform)TestInstance.Device.Platform, IDeviceUsageReporter.EventType.Device, Context.Options.JobDetails);
				IDeviceUsageReporter.RecordComment(TestInstance.Device.Name, (UnrealTargetPlatform)TestInstance.Device.Platform, IDeviceUsageReporter.EventType.Test, this.GetType().Name);
			}

			if (SessionStartTime == DateTime.MinValue)
			{
				SessionStartTime = DateTime.Now;
			}

			if (TestInstance != null)
			{
				MarkTestStarted();
			}

			return TestInstance != null;
		}

		public override void TickTest()
		{
			if (TestInstance != null && TestInstance.HasExited)
			{
				if (TestInstance.WasKilled)
				{
					LowLevelTestResult = TestResult.Failed;
				}
				MarkTestComplete();
			}
		}

		public override void StopTest(StopReason InReason)
		{
			base.StopTest(InReason);

			if (TestInstance != null && !TestInstance.HasExited)
			{
				TestInstance.Kill();
			}

			string StdOut;
			if (TestInstance is IWithUnfilteredStdOut)
			{
				StdOut = ((IWithUnfilteredStdOut)TestInstance).UnfilteredStdOut;
			}
			else
			{
				StdOut = TestInstance.StdOut;
			}

			string LogDir = Path.Combine(Unreal.EngineDirectory.FullName, "Programs", "AutomationTool", "Saved", "Logs");

			if (StdOut == null || string.IsNullOrEmpty(StdOut.Trim()))
			{
				Log.Warning("No StdOut returned from low level test app.");
			}
			else // Save log artifact
			{
				const string ClientLogFile = "ClientOutput.log";
				string ClientOutputLog = Path.Combine(ArtifactPath, ClientLogFile);

				using (var ClientOutputWriter = File.CreateText(ClientOutputLog))
				{
					ClientOutputWriter.Write(StdOut);
				}
				File.Copy(ClientOutputLog, Path.Combine(LogDir, ClientLogFile));
			}

			ILowLevelTestsReporting LowLevelTestsReporting = Gauntlet.Utils.InterfaceHelpers.FindImplementations<ILowLevelTestsReporting>(true)
				.Where(B => B.CanSupportPlatform(Context.Options.Platform))
				.First();

			string ReportPath = null;
			bool ReportCopied = false;
			try
			{
				ReportPath = LowLevelTestsReporting.CopyDeviceReportTo(LowLevelTestsApp.Install, Context.Options.Platform, Context.Options.TestApp, Context.Options.Build, LogDir);
				ReportCopied = true;
			}
			catch (Exception ex)
			{
				Log.Error("Failed to copy report: {0}", ex.ToString());
			}


			string ExitReason = "";
			if (TestInstance.WasKilled)
			{
				if (InReason == StopReason.MaxDuration)
				{
					LowLevelTestResult = TestResult.TimedOut;
					ExitReason = "Timed Out";
				}
				else
				{
					LowLevelTestResult = TestResult.Failed;
					ExitReason = "Process was killed by Gauntlet.";
				}
			}
			else if (TestInstance.ExitCode != 0)
			{
				LowLevelTestResult = TestResult.Failed;
				ExitReason = $"Process exited with exit code {TestInstance.ExitCode}";
			}
			else if (!ReportCopied)
			{
				LowLevelTestResult = TestResult.Failed;
				ExitReason = "Uabled to read test report";
			}
			else
			{
				string ReportContents = File.ReadAllText(ReportPath);
				Log.Info(ReportContents);
				string ReportType = Context.Options.ReportType.ToLower();
				if (ReportType == "console")
				{
					LowLevelTestsLogParser LowLevelTestsLogParser = new LowLevelTestsLogParser(ReportContents);
					if (LowLevelTestsLogParser.GetCatchTestResults().Passed)
					{
						LowLevelTestResult = TestResult.Passed;
						ExitReason = "Tests passed";
					}
					else
					{
						LowLevelTestResult = TestResult.Failed;
						ExitReason = "Tests failed";
					}
				} else if (ReportType == "xml")
				{
					LowLevelTestsReportParser LowLevelTestsReportParser = new LowLevelTestsReportParser(ReportContents);
					if (LowLevelTestsReportParser.HasPassed())
					{
						LowLevelTestResult = TestResult.Passed;
						ExitReason = "Tests passed";
					}
					else
					{
						LowLevelTestResult = TestResult.Failed;
						ExitReason = "Tests failed";
					}
				}
			}
			Log.Info($"Low level test exited with code {TestInstance.ExitCode} and reason: {ExitReason}");
		}

		public override void CleanupTest()
		{
			if (LowLevelTestsApp != null)
			{
				LowLevelTestsApp.Dispose();
				LowLevelTestsApp = null;
			}
		}
	}
}
