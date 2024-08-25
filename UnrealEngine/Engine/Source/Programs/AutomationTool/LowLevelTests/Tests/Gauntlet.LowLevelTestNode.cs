// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.Core;
using Gauntlet;
using System;
using System.IO;
using System.Linq;
using UnrealBuildBase;
using UnrealBuildTool;

using Log = Gauntlet.Log;

namespace LowLevelTests
{
	public class LowLevelTests : BaseTest
	{
		private LowLevelTestContext Context;

		private IAppInstance TestInstance;

		private DateTime SessionStartTime = DateTime.MinValue;

		private TestResult LowLevelTestResult;

		public LowLevelTestsSession LowLevelTestsApp { get; private set; }

		private int LastStdoutSeekPos = 0;
		private string[] CurrentProcessedLines;

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

		private DateTime InactivityStart = DateTime.MinValue;
		private TimeSpan InactivityPeriod = TimeSpan.Zero;

		public override bool IsReadyToStart()
		{
			if (LowLevelTestsApp == null)
			{
				LowLevelTestsApp = new LowLevelTestsSession(Context.BuildInfo, Context.Options);
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

		public override void AddTestEvent(UnrealTestEvent InEvent)
		{
			if (InEvent.Summary.Equals("Insufficient devices found"))
			{
				Log.Error(KnownLogEvents.Gauntlet_TestEvent, "Test didn't run due to insufficient devices.");
			}
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

			foreach (ILowLevelTestsExtension LowLevelTestsExtension in Context.BuildInfo.LowLevelTestsExtensions)
			{
				LowLevelTestsExtension.PreRunTests();
			}

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
			if (TestInstance != null)
			{
				if (TestInstance.HasExited)
				{
					if (TestInstance.WasKilled)
					{
						LowLevelTestResult = TestResult.Failed;
					}
					MarkTestComplete();
				}
				else
				{
					ParseLowLevelTestsLog();

					// Print stdout when -captureoutput, certain platforms don't always redirect stdout
					if (CurrentProcessedLines != null && Context.Options.CaptureOutput)
					{
						foreach (string OutputLine in CurrentProcessedLines)
						{
							Console.WriteLine(OutputLine);
						}
					}

					if (CheckForTimeout())
					{
						Log.Error("Timeout detected from application logged events, stopping.");
						MarkTestComplete();
						LowLevelTestResult = TestResult.TimedOut;
					}
					else if (CurrentProcessedLines != null && CurrentProcessedLines.Length > 0)
					{
						InactivityStart = DateTime.MinValue;
					}
					else if ((CurrentProcessedLines == null || CurrentProcessedLines.Length == 0) && InactivityStart == DateTime.MinValue)
					{
						InactivityStart = DateTime.Now;
					}
					else if (InactivityStart != DateTime.MinValue)
					{
						InactivityPeriod = DateTime.Now - InactivityStart;
					}

					if (Context.Options.Timeout != 0 && InactivityPeriod.TotalMinutes > Context.Options.Timeout + 0.5)
					{
						Log.Error($"Test application didn't log any test events after timeout period of {Context.Options.Timeout} minutes, stopping.");
						MarkTestComplete();
						LowLevelTestResult = TestResult.TimedOut;
					}

					CurrentProcessedLines = null;
				}
			}
		}

		public override void StopTest(StopReason InReason)
		{
			try
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

					string DestClientLogFile = Path.Combine(LogDir, ClientLogFile);
					if (DestClientLogFile != ClientOutputLog)
					{
						File.Copy(ClientOutputLog, DestClientLogFile, true);
					}
				}

				bool? ReportCopied = null;
				string ReportPath = null;
				if (!string.IsNullOrEmpty(Context.Options.ReportType))
				{
					ILowLevelTestsReporting LowLevelTestsReporting = Gauntlet.Utils.InterfaceHelpers.FindImplementations<ILowLevelTestsReporting>(true)
						.Where(B => B.CanSupportPlatform(Context.Options.Platform))
						.First();

					try
					{
						ReportPath = LowLevelTestsReporting.CopyDeviceReportTo(LowLevelTestsApp.Install, Context.Options.Platform, Context.Options.TestApp, Context.Options.Build, LogDir);
						ReportCopied = true;
					}
					catch (Exception ex)
					{
						ReportCopied = false;
						Log.Error("Failed to copy report: {0}", ex.ToString());
					}
				}


				string ExitReason = "";
				if (TestInstance.WasKilled)
				{
					if (InReason == StopReason.MaxDuration || LowLevelTestResult == TestResult.TimedOut)
					{
						LowLevelTestResult = TestResult.TimedOut;
						ExitReason = "Timed Out";
					}
					else
					{
						LowLevelTestResult = TestResult.Failed;
						ExitReason = $"Process was killed by Gauntlet with reason {InReason.ToString()}.";
					}
				}
				else if (TestInstance.ExitCode != 0)
				{
					LowLevelTestResult = TestResult.Failed;
					ExitReason = $"Process exited with exit code {TestInstance.ExitCode}";
				}
				else if (ReportCopied.HasValue && !ReportCopied.Value)
				{
					LowLevelTestResult = TestResult.Failed;
					ExitReason = "Uabled to read test report";
				}
				else if (ReportPath != null)
				{
					string ReportContents = File.ReadAllText(ReportPath);
					if (Context.Options.LogReportContents) // Some tests prefer to log report contents
					{
						Log.Info(ReportContents);
					}
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
							ExitReason = "Tests failed according to console report";
						}
					}
					else if (ReportType == "xml")
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
							ExitReason = "Tests failed according to xml report";
						}
					}
				}
				else // ReportPath == null
				{
					if (TestInstance.ExitCode != 0)
					{
						LowLevelTestResult = TestResult.Failed;
						ExitReason = "Tests failed (no report to parse)";
					}
					else
					{
						LowLevelTestResult = TestResult.Passed;
						ExitReason = "Tests passed (no report to parse)";
					}
				}
				Log.Info($"Low level test exited with code {TestInstance.ExitCode} and reason: {ExitReason}");
			}
			catch
			{
				throw;
			} 
			finally 
			{ 
				foreach (ILowLevelTestsExtension LowLevelTestsExtension in Context.BuildInfo.LowLevelTestsExtensions)
				{
					LowLevelTestsExtension.PostRunTests();
				}
			}
		}

		public override void CleanupTest()
		{
			if (LowLevelTestsApp != null)
			{
				LowLevelTestsApp.Dispose();
				LowLevelTestsApp = null;
			}
		}

		private void ParseLowLevelTestsLog()
		{
			// Parse new lines from Stdout, if any
			if (LastStdoutSeekPos < TestInstance.StdOut.Length)
			{
				CurrentProcessedLines = TestInstance.StdOut
					.Substring(LastStdoutSeekPos)
					.Split("\n")
					.Where(Line => Line.Contains("LogLowLevelTests"))
					.ToArray();
				LastStdoutSeekPos = TestInstance.StdOut.Length - 1;
			}
		}

		private bool CheckForTimeout()
		{
			if (CurrentProcessedLines == null || CurrentProcessedLines.Length == 0)
			{
				return false;
			}
			foreach (string Line in CurrentProcessedLines)
			{
				if (Line.Contains("Timeout detected"))
				{
					return true;
				}
			}
			return false;
		}
	}
}
