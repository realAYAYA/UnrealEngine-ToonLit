// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Text.RegularExpressions;
using System.Drawing;
using EpicGames.Core;

namespace Gauntlet
{
	public class TestExecutorOptions
	{
		[AutoParamWithNames(1, "iterations", "repeat")]
		public int TestIterations;

		[AutoParam(false)]
		public bool StopOnError;

		[AutoParam(false)]
		public bool NoTimeout;

		[AutoParam(300)]
		public int Wait;

		[AutoParam(1)]
		public int Parallel;

		[AutoParam(true)]
		public bool DeferReports;
	}

	/// <summary>
	/// Class that is manages the creation and execution of one or more tests
	/// </summary>
	public class TestExecutor
	{
		class TestExecutionInfo
		{
			public enum ExecutionResult
			{
				NotStarted,
				TimedOut,
				Passed,
				Failed,
				Skipped
			}

			public ITestNode		TestNode;
			public DateTime			FirstReadyCheckTime;
			public DateTime			TimeSetupBegan;
			public DateTime			TimeSetupEnded;
			public DateTime			TimeTestEnded;
			public ExecutionResult	Result;
			public TestResult		FinalResult;
			public string			CancellationReason;
			
			/// <summary>
			/// Time the test had to wait before running
			/// </summary>
			public TimeSpan			WaitDuration { get { return (TimeSetupBegan - FirstReadyCheckTime); } }

			/// <summary>
			/// Time the test had to wait before running
			/// </summary>
			public TimeSpan			SetupDuration { get { return (TimeSetupEnded - TimeSetupBegan); } }

			/// <summary>
			/// Time the test took to run
			/// </summary>
			public TimeSpan			TestDuration { get { return (TimeTestEnded - TimeSetupEnded); } }

			/// <summary>
			/// Creates TestExecutionInfo instance from an ITestNode. Sets FirstReadyCheckTime, copies the node
			/// and sets CancellationReason to empty string.
			/// </summary>
			/// <param name="InNode"></param>
			public TestExecutionInfo(ITestNode InNode)
			{
				FirstReadyCheckTime = TimeSetupBegan = TimeSetupEnded = TimeTestEnded = DateTime.MinValue;
				TestNode = InNode;
				CancellationReason = string.Empty;
			}

			public override string ToString()
			{
				return TestNode.ToString();
			}
		};

		private List<TestExecutionInfo> RunningTests;

		private DateTime StartTime;

		private int CurrentTestPass;

		TestExecutorOptions Options;

		protected string BuildCommandThatLaunchedExecutor;

		public bool IsRunning { get; private set; }
		public bool IsCancelled { get; private set; }
		protected bool HaveReceivedPostAbort { get; private set; }

		/// <summary>
		/// Constructor that fills in some member variables
		/// @Param the BuildCommand that was used to start this Test Executor.
		/// Useful to log out or know where this test executor came from.
		/// </summary>
		public TestExecutor(string InBuildCommand)
		{
			BuildCommandThatLaunchedExecutor = InBuildCommand.Split(".").Last();
			RunningTests = new List<TestExecutionInfo>();
		}

		public void Dispose()
		{
		}

		/// <summary>
		/// Executes the provided tests. Currently tests are executed synchronously
		/// </summary>
		/// <param name="Context"></param>
		public bool ExecuteTests(TestExecutorOptions InOptions, IEnumerable<ITestNode> RequiredTests)
		{
			Options = InOptions;

			Log.Info("Preparing to start {Number} automation test(s)", RequiredTests.Count());

			// install a cancel handler so we can stop parallel-for gracefully
			Action CancelHandler = delegate ()
			{
				Log.Info("Cancelling Tests");
				IsCancelled = true;
			};

			Action PostCancelHandler = delegate ()
			{
				HaveReceivedPostAbort = true;
			};

			Globals.AbortHandlers.Add(CancelHandler);
			Globals.PostAbortHandlers.Add(PostCancelHandler);

			StartTime = DateTime.Now;
			CurrentTestPass = 0;

			IsRunning = true;

			int MaxParallelTasks = 0;
			int MaxStartingTasks = 0;

			// sort by priority
			if (Options.Parallel > 1)
			{
				RequiredTests = RequiredTests.OrderBy(Node => Node.Priority);
			}

			Dictionary<string, int> TestIterationsPasssed = new Dictionary<string, int>();
			Dictionary<string, int> TestIterationsFailed = new Dictionary<string, int>();
			Dictionary<string, int> TestIterationsPassedWithWarnings = new Dictionary<string, int>();


			for (CurrentTestPass = 0; CurrentTestPass < Options.TestIterations; CurrentTestPass++)
			{
				// do not start a pass if canceled
				if (IsCancelled)
				{
					break;
				}

				if (CurrentTestPass > 0)
				{
					// if repeating tests wait a little bit. If there was a crash CR might still be
					// processing things.
					Thread.Sleep(10000);
				}

				DateTime StartPassTime = DateTime.Now;

				Log.Info("Starting test iteration {Index} of {Total}", CurrentTestPass + 1, Options.TestIterations);

				// Tests that we want to run
				List<TestExecutionInfo> PendingTests = RequiredTests.Select(N => new TestExecutionInfo(N)).ToList();

				// Tests that are in the process of starting
				List<TestExecutionInfo> StartingTests = new List<TestExecutionInfo>();

				List<Thread> StartingTestThreads = new List<Thread>();

				// Tests that have been started and we're ticking/checking
				List<TestExecutionInfo> RunningTests = new List<TestExecutionInfo>();

				// Completed tests
				List<TestExecutionInfo> CompletedTests = new List<TestExecutionInfo>();

				DateTime LastUpdateMsg = DateTime.MinValue;
				DateTime LastReadyCheck = DateTime.MinValue;
				DateTime LastStatusUpdateTime = DateTime.MinValue;

				const double ReadyCheckPeriod = 30.0;
				const double StatusUpdatePeriod = 60.0;

				while (CompletedTests.Count() < RequiredTests.Count() && IsCancelled == false)
				{
					Monitor.Enter(Globals.MainLock);

					int SecondsRunning = (int)(DateTime.Now - StartPassTime).TotalSeconds;

					int InProgressCount = RunningTests.Count() + StartingTests.Count();

					double TimeSinceLastReadyCheck = (DateTime.Now - LastReadyCheck).TotalSeconds;

					// Are any tests ready to run?
					if (InProgressCount < Options.Parallel
						&& PendingTests.Count() > 0
						&& TimeSinceLastReadyCheck >= ReadyCheckPeriod)
					{
						TestExecutionInfo TestToStart = null;

						List<ITestNode> TestsFailingToStart = new List<ITestNode>();

						// find a node that can run, and
						// find the first test that can run....
						for (int i = 0; i < PendingTests.Count(); i++)
						{
							TestExecutionInfo NodeInfo = PendingTests[i];
							ITestNode Node = NodeInfo.TestNode;

							bool IsTestReady = false;

							try
							{
								IsTestReady = Node.IsReadyToStart();
							}
							catch (System.Exception ex)
							{
								Log.Error(KnownLogEvents.Gauntlet_TestEvent, "Test {Name} threw an exception during ready check. Ex: {Exception}", Node, ex);
								Node.AddTestEvent(new UnrealTestEvent(EventSeverity.Error, "Test Failed to Start", new List<string> {ex.Message}));
								PendingTests[i] = null;
								NodeInfo.TimeSetupBegan = NodeInfo.TimeSetupEnded = NodeInfo.TimeTestEnded = DateTime.Now;
								CompletedTests.Add(NodeInfo);
							}

							if (IsTestReady)
							{
								// if ready then take it and stop looking
								TestToStart = NodeInfo;

								if (NodeInfo.FirstReadyCheckTime == DateTime.MinValue)
								{
									NodeInfo.FirstReadyCheckTime = DateTime.Now;
								}
								break;
							}
							else
							{
								// track the time that this test should have been able to run due to no other tests
								// consuming resources (at least locally...)
								// TODO - what about the situation where no tests can run so all FirstCheck times are set, but 
								// then a test starts and consumes all resources?
								if (RunningTests.Count() == 0 && StartingTests.Count() == 0)
								{
									if (NodeInfo.FirstReadyCheckTime == DateTime.MinValue)
									{
										NodeInfo.FirstReadyCheckTime = DateTime.Now;
									}

									double TimeWaiting = (DateTime.Now - NodeInfo.FirstReadyCheckTime).TotalSeconds;
									if (TimeWaiting >= Options.Wait)
									{
										Log.Error(KnownLogEvents.Gauntlet_DeviceEvent, "Test {TestName} has been waiting to run resource-free for {Time:00} seconds. Removing from wait list", Node, TimeWaiting);
										DevicePool.Instance.ReportDeviceReservationState();
										Node.AddTestEvent(new UnrealTestEvent(EventSeverity.Error, "Insufficient devices found", new List<string> {string.Format("Test {0} was unable to find enough devices after trying for {1:00} seconds.", Node, TimeWaiting), "This is not a test-related failure."}));
										PendingTests[i] = null;
										NodeInfo.TimeSetupBegan = NodeInfo.TimeSetupEnded = NodeInfo.TimeTestEnded = DateTime.Now;
										NodeInfo.Result = TestExecutionInfo.ExecutionResult.Failed;
										CompletedTests.Add(NodeInfo);
									}
								}
							}
						}

						// remove anything we nulled
						PendingTests = PendingTests.Where(T => T != null).ToList();

						if (TestToStart != null)
						{
							Log.Info("Test {Name} is ready to run", TestToStart);

							PendingTests.Remove(TestToStart);
							StartingTests.Add(TestToStart);

							// StartTest is the only thing we do on a thread because it's likely to be the most time consuming
							// as build are copied so will get the most benefit from happening in parallel
							Thread StartThread = new Thread(() =>
							{
								Thread.CurrentThread.IsBackground = true;

								// start the test, this also fills in the pre/post start times
								bool Started = StartTest(TestToStart, CurrentTestPass, Options.TestIterations);

								lock (Globals.MainLock)
								{
									if (Started == false)
									{
										TestToStart.TimeSetupEnded = TestToStart.TimeTestEnded = DateTime.Now;
										CompletedTests.Add(TestToStart);
										Log.Error(KnownLogEvents.Gauntlet_TestEvent, "Test {Name} failed to start", TestToStart);
									}
									else
									{
										RunningTests.Add(TestToStart);
									}

									StartingTests.Remove(TestToStart);
									StartingTestThreads.Remove(Thread.CurrentThread);
								}
							});

							if (StartingTests.Count > MaxStartingTasks)
							{
								MaxStartingTasks = StartingTests.Count;
							}

							// track the thread and start it
							StartingTestThreads.Add(StartThread);
							StartThread.Start();
						}
						else
						{
							// don't check for a while as querying kits for availability can be expensive
							LastReadyCheck = DateTime.Now;
						}
					}

					// Tick all running tests
					foreach (TestExecutionInfo TestInfo in RunningTests)
					{
						// TickTest contains logic for determining run time, timeouts, cancellations, and many other
						// parts of the test process. If overriding TickTest in your Test class, be sure to call base.TickTest.
						TestResult Result = TickTest(TestInfo);

						// invalid = no result yet
						if (Result == TestResult.Invalid)
						{
							TimeSpan RunningTime = DateTime.Now - TestInfo.TimeSetupEnded;

							if ((SecondsRunning % 60) == 0)
							{
								Log.Verbose("Test {Name} is still running. {Elapsed:00} seconds elapsed, will timeout in {Max:00} seconds",
									TestInfo,
									RunningTime.TotalSeconds,
									TestInfo.TestNode.MaxDuration - RunningTime.TotalSeconds);

								LastUpdateMsg = DateTime.Now;
							}
						}
						else
						{
							TestInfo.TimeTestEnded = DateTime.Now;
							TestInfo.Result = Result == TestResult.Passed ? TestExecutionInfo.ExecutionResult.Passed : TestExecutionInfo.ExecutionResult.Failed;
							CompletedTests.Add(TestInfo);
						}
					}

					// remove any tests that were completed
					RunningTests = RunningTests.Where(R => CompletedTests.Contains(R) == false).ToList();

					if ((DateTime.Now - LastStatusUpdateTime).TotalSeconds >= StatusUpdatePeriod)
					{
						LastStatusUpdateTime = DateTime.Now;
						Log.Info("Status: Completed:{Completed}, Running:{Running}, Starting: {Starting}, Waiting:{Waiting}",
							CompletedTests.Count(), RunningTests.Count(), StartingTests.Count(), PendingTests.Count());
					}

					if (InProgressCount > MaxParallelTasks)
					{
						MaxParallelTasks = RunningTests.Count();
					}

					// Release our global lock before we loop
					Monitor.Exit(Globals.MainLock);

					// sleep a while before we tick our running tasks again
					Thread.Sleep(500);
				}

				if (IsCancelled)
				{
					DateTime StartTime = DateTime.Now;
					Log.Info("Cleaning up pending and running tests.");
					while (HaveReceivedPostAbort == false)
					{
						Thread.Sleep(500);
						double Elapsed = (DateTime.Now - StartTime).TotalSeconds;

						if (Elapsed >= 5)
						{
							Log.Error(KnownLogEvents.Gauntlet_TestEvent, "Giving up waiting for tests after {Elapsed:00} seconds", Elapsed);
							break;
						}
					}

					if (StartingTestThreads.Count > 0)
					{
						foreach (Thread T in StartingTestThreads)
						{
							// SYSLIB0006: 'Thread.Abort()' is obsolete: 'Thread.Abord it not supported and throws PlatformNotSupportedException'
							//Log.Info("Aborting startup thread");
							//T.Abort();

							Log.Info($"Thread is still running: {T.Name} {T.ManagedThreadId}");
						}
						Thread.Sleep(1000);
					}

					foreach (TestExecutionInfo TestInfo in StartingTests)
					{
						Log.Info("Forcing pending test {Name} to run CleanupTest", TestInfo.TestNode.Name);
						TestInfo.TestNode.CleanupTest();
						CompletedTests.Add(TestInfo);
					}

					foreach (TestExecutionInfo TestInfo in RunningTests)
					{
						Log.Info("Ticking test {Name} to cancel", TestInfo.TestNode.Name);
						TestResult Res = TickTest(TestInfo);
						CompletedTests.Add(TestInfo);

						if (Res != TestResult.Failed)
						{
							Log.Warning(KnownLogEvents.Gauntlet_TestEvent, "Ticking of cancelled test {Name} returned {Result}", TestInfo.TestNode.Name, Res);
						}
					}
				}
				else
				{
					TimeSpan PassDuration = DateTime.Now - StartPassTime;

					int FailedCount = 0;
					int TestCount = CompletedTests.Count;

					CompletedTests.ForEach(T =>
					{
						TimeSpan TimeWaiting = T.FirstReadyCheckTime - T.TimeSetupBegan;
						TimeSpan SetupTime = T.TimeSetupEnded - T.TimeSetupBegan;
						TimeSpan TestDuration = T.TimeTestEnded - T.TimeSetupEnded;

						// status msg, kept uniform to avoid spam on notifiers (ie. don't include timestamps, etc) 
						string Msg = string.Format("Test {0} {1}", T.TestNode, T.Result);

						bool TestHadErrors = T.Result != TestExecutionInfo.ExecutionResult.Passed && T.Result != TestExecutionInfo.ExecutionResult.Skipped;
						bool TestHadWarnings = T.TestNode.GetWarnings().Any();

						if (TestHadErrors)
						{
							FailedCount++;
						}

						// increment counts for each test
						if (!TestIterationsPasssed.ContainsKey(T.TestNode.Name))
						{
							TestIterationsPasssed[T.TestNode.Name] = 0;
							TestIterationsFailed[T.TestNode.Name] = 0;
							TestIterationsPassedWithWarnings[T.TestNode.Name] = 0;
						}

						if (TestHadErrors)
						{
							TestIterationsFailed[T.TestNode.Name]++;
						}
						else if (TestHadWarnings)
						{
							TestIterationsPassedWithWarnings[T.TestNode.Name]++;
						}
						else
						{
							TestIterationsPasssed[T.TestNode.Name]++;
						}

						Log.Info(Msg);

						// log test timing to info
						Log.Info("Test Time: {Duration:mm\\:ss} (Waited:{Waited:mm\\:ss}, Setup:{Setup:mm\\:ss})", TestDuration, TimeWaiting, SetupTime);

					});

					if (Options.Parallel > 1)
					{
						Log.Info("MaxParallelTasks: {Count}", MaxParallelTasks);
						Log.Info("MaxStartingTasks: {Count}", MaxStartingTasks);
					}

					// report all tests
					ReportMainSummary(CurrentTestPass + 1, Options.TestIterations, PassDuration, CompletedTests);

					if (FailedCount > 0 && Options.StopOnError)
					{
						break;
					}
				}
			} // foreach pass

			int TotalTests = RequiredTests.Count() * Options.TestIterations;
			int FailedTestCount = TestIterationsFailed.Values.Sum();

			// show details for multi passes				
			if (Options.TestIterations > 1)
			{
				MarkdownBuilder MB = new MarkdownBuilder();

				MB.HorizontalLine();					

				MB.Paragraph(string.Format("All iterations completed. {0} of {1} executed tests completed without error", TotalTests - FailedTestCount, TotalTests));

				List<string> Lines = new List<string>();
				foreach (ITestNode Test in RequiredTests)
				{
					int IterationsWithErrors = TestIterationsFailed[Test.Name];
					int IterationsWithWarnings = TestIterationsPassedWithWarnings[Test.Name];
					int IterationsWithPasses = TestIterationsPasssed[Test.Name];				

					Lines.Add(string.Format("{0}: {1} Iterations, {2} Passed, {3} Passed with Warnings, {4} Failed", 
						Test.Name, Options.TestIterations, IterationsWithPasses, IterationsWithWarnings, IterationsWithErrors));
				}

				MB.UnorderedList(Lines);
					
				if (FailedTestCount > 0)
				{
					MB.Paragraph(string.Format("Error: {0} of {1} executed tests failed.", FailedTestCount, TotalTests));
				}
				else
				{
					MB.Paragraph(string.Format("{0} total tests passed.", TotalTests));
				}

				MB.HorizontalLine();

				Log.Info(MB.ToString());
			}			

			IsRunning = false;

			Globals.AbortHandlers.Remove(CancelHandler);
			Globals.PostAbortHandlers.Remove(PostCancelHandler);
			
			return FailedTestCount == 0 && !IsCancelled;
		}
		
		/// <summary>
		/// Executes a single test
		/// </summary>
		/// <param name="Test">Test to execute</param>
		/// <param name="Context">The context to execute this test under</param>
		/// <returns></returns>
		private bool StartTest(TestExecutionInfo TestInfo, int Pass, int NumPasses)
		{
			string Name = TestInfo.TestNode.Name;

			Log.Info("Starting Test {Name}", TestInfo);

			try
			{
				TestInfo.TimeSetupBegan = DateTime.Now;
				if (TestInfo.TestNode.StartTest(Pass, NumPasses))
				{
					TestInfo.TimeSetupEnded = DateTime.Now;
					Log.Info("Launched test {Name} at {Time}", Name, TestInfo.TimeSetupEnded.ToString("h:mm:ss"));
					return true;
				}
			}
			catch (Exception Ex)
			{
				Log.Error(KnownLogEvents.Gauntlet_TestEvent, "Test {Name} threw an exception during launch. Skipping test. Ex: {Exception}\n{Callstack}", Name, Ex.Message, Ex.StackTrace);
			}			

			return false;			
		}

		/// <summary>
		/// Report the summary for a single test
		/// </summary>
		/// <param name="TestInfo"></param>
		/// <returns></returns>
		void ReportTestSummary(TestExecutionInfo TestInfo)
		{
			Log.SuspendSanitization();

			string Summary = TestInfo.TestNode.GetTestSummary();

			// Show summary
			Summary.Split('\n').ToList().ForEach(L => {
				if (L.Contains("Error: "))
				{
					Log.Error(KnownLogEvents.Gauntlet_TestEvent, " " + L);
				}
				else if (L.Contains("Warning: "))
				{
					Log.Warning(KnownLogEvents.Gauntlet_TestEvent, " " + L);
				}
				else
				{
					Log.Info(" " + L);
				}
			});

			Log.ResumeSanitization();

			// list warnings/errors if the test wants that
			if (TestInfo.TestNode.LogWarningsAndErrorsAfterSummary)
			{
				TestInfo.TestNode.GetErrors().ToList().ForEach(E => Log.Error(KnownLogEvents.Gauntlet_TestEvent, E));

				TestInfo.TestNode.GetWarnings().ToList().ForEach(E => Log.Warning(KnownLogEvents.Gauntlet_TestEvent, E));
			}

			// display the final result
			if (TestInfo.FinalResult != TestResult.Passed)
			{
				Log.Info("{TestInfo} result={Result}", TestInfo, TestInfo.FinalResult);
				if (string.IsNullOrEmpty(TestInfo.CancellationReason) == false)
				{
					Log.Info("\tReason: {Reason}", TestInfo.CancellationReason);
				}				
			}
			else
			{
				if (TestInfo.TestNode.GetWarnings().Any())
				{
					Log.Info("{TestInfo} result={Result} with warnings", TestInfo, TestInfo.FinalResult);
				}
				else
				{
					Log.Info("{TestInfo} result={Result}", TestInfo, TestInfo.FinalResult);
				}
			}

			// display how the test could be ran locally
			string RunLocalString = TestInfo.TestNode.GetRunLocalCommand(BuildCommandThatLaunchedExecutor);
			Log.Info("How to run locally: ({0})", RunLocalString);
		}

		/// <summary>
		/// Reports the summary of this pass, including a sumamry for each test
		/// </summary>
		/// <param name="CurrentPass"></param>
		/// <param name="NumPasses"></param>
		/// <param name="Duration"></param>
		/// <param name="AllInfo"></param>
		/// <returns></returns>
		void ReportMainSummary(int CurrentPass, int NumPasses, TimeSpan Duration, IEnumerable<TestExecutionInfo> AllInfo)
		{

			MarkdownBuilder MB = new MarkdownBuilder();

			int TestCount = AllInfo.Count();
			int FailedCount = AllInfo.Where(T => T.FinalResult != TestResult.Passed).Count();
			int WarningCount = AllInfo.Where(T => T.TestNode.GetWarnings().Any()).Count();

			var SortedInfo = AllInfo;

			// sort our tests by failed/warning/ok
			if (FailedCount > 0 || WarningCount > 0)
			{
				SortedInfo = AllInfo.OrderByDescending(T =>
				{
					if (T.FinalResult != TestResult.Passed)
					{
						return 10;
					}

					if (T.TestNode.GetWarnings().Any())
					{
						return 5;
					}

					return 0;
				});
			}

			// write all test results at once
			if (Options.DeferReports)
			{
				// write each tests full summary
				foreach (TestExecutionInfo Info in SortedInfo)
				{
					ReportTestSummary(Info);
				}
			}

			// only show pass info for multiple passes
			if (NumPasses > 1)
			{
				Log.Info("Completed test pass {Index} of {Total}.", CurrentPass, NumPasses);
			}

			// only show count of passed/failed etc for multiple test
			if (TestCount > 1)
			{
				MB.H2(string.Format("{0} of {1} Tests Passed in {2:mm\\:ss}. ({3} Failed, {4} Passed with Warnings)",
					TestCount - FailedCount, TestCount, Duration, FailedCount, WarningCount));
			
				List<string> TestResults = new List<string>();
				foreach (TestExecutionInfo Info in SortedInfo)
				{
					string WarningString = Info.TestNode.GetWarnings().Any() ? " With Warnings" : "";
					TestResults.Add(string.Format("\t{0} result={1}{2} (Waited={3:mm\\:ss}, Duration={4:mm\\:ss})", 
						Info, Info.FinalResult, WarningString,
						Info.WaitDuration, Info.TestDuration));
				}

				MB.UnorderedList(TestResults);
			}

			// write the markdown out with each line indented
			MB.ToString().Split('\n').ToList().ForEach(L => Log.Info("  " + L));
		}

		TestResult TickTest(TestExecutionInfo TestInfo)
		{
			// Give the test a chance to update itself
			try
			{
				TestInfo.TestNode.TickTest();					
			}
			catch (Exception Ex)
			{
				TestInfo.CancellationReason = string.Format("Test {0} threw an exception. Cancelling. Ex: {1}\n{2}", TestInfo.TestNode.Name, Ex.Message, Ex.StackTrace);
				TestInfo.FinalResult = TestResult.Cancelled;
			}
		
			// Does the test still say it's running?
			bool TestIsRunning = TestInfo.TestNode.GetTestStatus() == TestStatus.InProgress;

			TimeSpan RunningTime = DateTime.Now - TestInfo.TimeSetupEnded;

			if (TestIsRunning && RunningTime.TotalSeconds > TestInfo.TestNode.MaxDuration && !Options.NoTimeout)
			{
				if (TestInfo.TestNode.MaxDurationReachedResult == EMaxDurationReachedResult.Failure)
				{
					TestInfo.CancellationReason = string.Format("Terminating Test {0} due to maximum duration of {1} seconds. ", TestInfo.TestNode, TestInfo.TestNode.MaxDuration);
					TestInfo.FinalResult = TestResult.TimedOut;
					Log.Error(KnownLogEvents.Gauntlet_TestEvent, TestInfo.CancellationReason);
				}
				else if (TestInfo.TestNode.MaxDurationReachedResult == EMaxDurationReachedResult.Success)
				{
					TestInfo.FinalResult = TestResult.Passed;
					TestIsRunning = false;
					Log.Info("Test {Name} successfully reached maximum duration of {Time} seconds. ", TestInfo.TestNode, TestInfo.TestNode.MaxDuration);
				}
			}

			if (IsCancelled)
			{
				TestInfo.CancellationReason = string.Format("Cancelling Test {0} on request", TestInfo.TestNode);
				TestInfo.FinalResult = TestResult.Cancelled;
				Log.Info(TestInfo.CancellationReason);
			}

			if (!string.IsNullOrEmpty(TestInfo.CancellationReason))
			{
				TestInfo.TestNode.SetCancellationReason(TestInfo.CancellationReason);
			}

			// if the test is not running. or we've determined a result for it..
			if (TestIsRunning == false || TestInfo.FinalResult != TestResult.Invalid)
			{
				// Request the test stop
				try
				{
					// Note - we log in this order to try and make it easy to grep the log and find the
					// artifcat links
					Log.Info("*");
					Log.Info("****************************************************************");
					Log.Info("Finished Test: {Name} in {Time:mm\\:ss}", TestInfo, DateTime.Now - TestInfo.TimeSetupEnded);

					// Tell the test it's done. If it still thinks its running it was cancelled
					TestInfo.TestNode.StopTest(TestIsRunning ? StopReason.MaxDuration : StopReason.Completed);
					TestInfo.TimeTestEnded = DateTime.Now;

					TestResult NodeResult = TestInfo.TestNode.GetTestResult();
					TestInfo.FinalResult = (TestInfo.FinalResult != TestResult.Invalid) ? TestInfo.FinalResult : NodeResult;

					bool bCanFinalizeTest = true;
					if (TestInfo.FinalResult == TestResult.WantRetry)
					{
						Log.Info("{Name} requested retry. Cleaning up old test and relaunching", TestInfo);

						DateTime OriginalStartTime = TestInfo.TimeSetupEnded;

						bool bIsRestarted = TestInfo.TestNode.RestartTest();
						if (bIsRestarted)
						{
							// Mark us as still running
							TestInfo.CancellationReason = "";
							TestInfo.FinalResult = TestResult.Invalid;
							bCanFinalizeTest = false;
						}
						else
						{
							TestInfo.CancellationReason = "Failed to restart during retry.";
							Log.Error(KnownLogEvents.Gauntlet_TestEvent, TestInfo.CancellationReason);
							TestInfo.FinalResult = TestResult.Failed;
						}
					}

					if (bCanFinalizeTest)
					{
						Log.Info("{Name} result={Result}", TestInfo, TestInfo.FinalResult);

						if (!Options.DeferReports)
						{
							ReportTestSummary(TestInfo);
						}
						TestInfo.TestNode.SetTestResult(TestInfo.FinalResult);
						// now cleanup
						try
						{
							TestInfo.TestNode.CleanupTest();
						}
						catch (System.Exception ex)
						{
							Log.Error(KnownLogEvents.Gauntlet_TestEvent, "Test {Name} threw an exception while cleaning up. Ex: {Exception}", TestInfo.TestNode.Name, ex.Message);
						}
					}

					Log.Info("****************************************************************");
					Log.Info("*");
				}
				catch (System.Exception ex)
				{
					if (TestIsRunning)
					{
						Log.Warning(KnownLogEvents.Gauntlet_TestEvent, "Cancelled Test {Name} threw an exception while stopping. Ex: {Eception}\n{Callstack}", 
							TestInfo.TestNode.Name, ex.Message, ex.StackTrace);
					}
					else
					{
						Log.Error(KnownLogEvents.Gauntlet_TestEvent, "Test {Name} threw an exception while stopping. Ex: {Exception}\n{Callstack}",
							TestInfo.TestNode.Name, ex.Message, ex.StackTrace);
					}

					TestInfo.FinalResult = TestResult.Failed;
				}				
			}

			return TestInfo.FinalResult;
		}

		/// <summary>
		/// Waits for all pending tests to complete. Returns true/false based on whether
		/// all tests completed successfully.
		/// </summary>
		bool WaitForTests()
		{
			Log.Info("Waiting for {Count} tests to complete", RunningTests.Count);

			DateTime LastUpdateMsg = DateTime.Now;

			bool AllTestsPassed = true;

			while (RunningTests.Count > 0)
			{
				List<TestExecutionInfo > RemainingTests = new List<TestExecutionInfo>();

				foreach (TestExecutionInfo Process in RunningTests)
				{
					
					TestResult Result = TickTest(Process);

					// invalid = no
					if (Result == TestResult.Invalid)
					{ 
						RemainingTests.Add(Process);

						TimeSpan RunningTime = DateTime.Now - Process.TimeSetupEnded;

						if ((DateTime.Now - LastUpdateMsg).TotalSeconds > 60.0f)
						{
							Log.Verbose("Test {Name} is still running. {Elapsed:00} seconds elapsed, will timeout in {Max:00} seconds",
								Process.TestNode.Name,
								RunningTime.TotalSeconds,
								Process.TestNode.MaxDuration - RunningTime.TotalSeconds);

							LastUpdateMsg = DateTime.Now;
						}
					}
					else
					{
						if (Result != TestResult.Passed)
						{
							AllTestsPassed = false;
						}
						Log.Info("Test {Name} Result: {Result}", Process.TestNode.Name, Result);
					}
				}

				RunningTests = RemainingTests;

				Thread.Sleep(1000);
			}

			return AllTestsPassed;
		}
	}
}
