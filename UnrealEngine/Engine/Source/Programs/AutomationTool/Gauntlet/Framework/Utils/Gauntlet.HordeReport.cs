// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Text.Json.Serialization;
using AutomationTool;
using System.Security.Cryptography;

namespace Gauntlet
{
	public static class HordeReport
	{
		/// <summary>
		/// Default location to store Test Data 
		/// </summary>
		public static string DefaultTestDataDir
		{
			get
			{
				return Path.GetFullPath(Environment.GetEnvironmentVariable("UE_TESTDATA_DIR") ?? Path.Combine(CommandUtils.CmdEnv.EngineSavedFolder, "TestData"));
			}
		}
		/// <summary>
		/// Default location to store test Artifacts 
		/// </summary>
		public static string DefaultArtifactsDir
		{
			get
			{
				return Path.GetFullPath(Environment.GetEnvironmentVariable("UE_ARTIFACTS_DIR") ?? CommandUtils.CmdEnv.LogFolder);
			}
		}

		public abstract class BaseHordeReport : BaseTestReport
		{

			/// <summary>
			/// Horde report version
			/// </summary>
			public int Version { get; set; } = 1;

			protected string OutputArtifactPath;
			protected HashSet<string> ArtifactProcessedHashes;
			protected Dictionary<string, object> ExtraReports;			

			/// <summary>
			/// Attach Artifact to the Test Report
			/// </summary>
			/// <param name="ArtifactPath"></param>
			/// <param name="Name"></param>
			/// <returns>return true if the file was successfully attached</returns>
			public override bool AttachArtifact(string ArtifactPath, string Name = null)
			{
				return AttachArtifact(ArtifactPath, Name, false);
			}
			/// <summary>
			/// Attach Artifact to the Test Report
			/// </summary>
			/// <param name="ArtifactPath"></param>
			/// <param name="Name"></param>
			/// <param name="Overwrite"></param>
			/// <returns>return true if the file was successfully attached</returns>
			public bool AttachArtifact(string ArtifactPath, string Name = null, bool Overwrite = false)
			{
				if (string.IsNullOrEmpty(OutputArtifactPath))
				{
					throw new InvalidOperationException("OutputArtifactPath must be set before attaching any artifact");
				}

				string ArtifactHash;
				{
					// Generate a hash from the artifact path
					using (SHA1 Sha1 = SHA1.Create())
					{
						ArtifactHash = Hasher.ComputeHash(ArtifactPath, Sha1, 8);
					}
				}

				if (ArtifactProcessedHashes == null)
				{
					ArtifactProcessedHashes = new HashSet<string>();
				}
				else
				{
					if(ArtifactProcessedHashes.Contains(ArtifactHash))
					{
						// already processed
						return true;
					}
				}

				// Mark as processed. Even in case of failure, we don't want to try over again.
				ArtifactProcessedHashes.Add(ArtifactHash);

				string TargetPath = Utils.SystemHelpers.GetFullyQualifiedPath(Path.Combine(OutputArtifactPath, Name ?? Path.GetFileName(ArtifactPath)));
				ArtifactPath = Utils.SystemHelpers.GetFullyQualifiedPath(ArtifactPath);
				bool isFileExist = File.Exists(ArtifactPath);
				if (isFileExist && (!File.Exists(TargetPath) || Overwrite))
				{
					try
					{
						string TargetDirectry = Path.GetDirectoryName(TargetPath);
						if (!Directory.Exists(TargetDirectry)) { Directory.CreateDirectory(TargetDirectry); }
						File.Copy(ArtifactPath, TargetPath, true);
						return true;
					}
					catch (Exception Ex)
					{
						Log.Error("Failed to copy artifact '{0}'. {1}", Path.GetFileName(ArtifactPath), Ex);
					}
					return false;
				}
				return isFileExist;
			}

			/// <summary>
			/// Set Output Artifact Path and create the directory if missing
			/// </summary>
			/// <param name="InPath"></param>
			/// <returns></returns>
			public void SetOutputArtifactPath(string InPath)
			{
				OutputArtifactPath = Path.GetFullPath(InPath);

				if (!Directory.Exists(OutputArtifactPath))
				{
					Directory.CreateDirectory(OutputArtifactPath);
				}
				Log.Verbose(string.Format("Test Report output artifact path is set to: {0}", OutputArtifactPath));
			}

			public void AttachDependencyReport(object InReport, string Key = null)
			{
				if(ExtraReports == null)
				{
					ExtraReports = new Dictionary<string, object>();
				}
				if(InReport is BaseHordeReport InHordeReport)
				{
					Key = InHordeReport.GetTestDataKey(Key);
				}
				ExtraReports[Key] = InReport;
			}

			public override Dictionary<string, object> GetReportDependencies()
			{
				var Reports = base.GetReportDependencies();
				if (ExtraReports != null)
				{
					ExtraReports.ToList().ForEach(Item => Reports.Add(Item.Key, Item.Value));
				}
				return Reports;
			}

			public virtual string GetTestDataKey(string BaseKey = null)
			{
				if(BaseKey == null)
				{
					return Type;
				}
				return string.Format("{0}::{1}", Type, BaseKey);
			}
		}

		/// <summary>
		/// Contains detailed information about device that run tests
		/// </summary>
		public class Device
		{
			public string DeviceName { get; set; }
			public string Instance { get; set; }
			public string Platform { get; set; }
			public string OSVersion { get; set; }
			public string Model { get; set; }
			public string GPU { get; set; }
			public string CPUModel { get; set; }
			public int RAMInGB { get; set; }
			public string RenderMode { get; set; }
			public string RHI { get; set; }
		}

		/// <summary>
		/// Contains reference to files used or generated for file comparison
		/// </summary>
		public class ComparisonFiles
		{
			public string Difference { get; set; }
			public string Approved { get; set; }
			public string Unapproved { get; set; }
		}
		/// <summary>
		/// Contains information about test artifact
		/// </summary>
		public class Artifact
		{
			public Artifact()
			{
				Files = new ComparisonFiles();
			}

			public string Id { get; set; }
			public string Name { get; set; }
			public string Type { get; set; }
			public ComparisonFiles Files { get; set; }
		}
		/// <summary>
		/// Contains information about test entry event
		/// </summary>
		public class Event
		{
			public EventType Type { get; set; }
			public string Message { get; set; }
			public string Context { get; set; }
			public string Artifact { get; set; }
		}
		/// <summary>
		/// Contains information about test entry
		/// </summary>
		public class Entry
		{
			public Entry()
			{
				Event = new Event();
			}

			public Event Event { get; set; }
			public string Filename { get; set; }
			public int LineNumber { get; set; }
			public string Timestamp { get; set; }
		}
		/// <summary>
		/// Contains detailed information about test result. This is to what TestPassResult refere to for each test result. 
		/// </summary>
		public class TestResultDetailed
		{
			public TestResultDetailed()
			{
				Artifacts = new List<Artifact>();
				Entries = new List<Entry>();
			}

			public string TestDisplayName { get; set; }
			public string FullTestPath { get; set; }
			public TestStateType State { get; set; }
			public string DeviceInstance { get; set; }
			public int Warnings { get; set; }
			public int Errors { get; set; }
			public List<Artifact> Artifacts { get; set; }
			public List<Entry> Entries { get; set; }

			/// <summary>
			/// Add a new Artifact to the test result and return it 
			/// </summary>
			public Artifact AddNewArtifact()
			{
				Artifact NewArtifact = new Artifact();
				Artifacts.Add(NewArtifact);

				return NewArtifact;
			}

			/// <summary>
			/// Add a new Entry to the test result and return it 
			/// </summary>
			public Entry AddNewEntry()
			{
				Entry NewEntry = new Entry();
				Entries.Add(NewEntry);

				return NewEntry;
			}
		}
		/// <summary>
		/// Contains a brief information about test result.
		/// </summary>
		public class TestResult
		{
			public TestResult()
			{
				TestDetailed = new TestResultDetailed();
			}

			public string TestDisplayName
			{
				get { return TestDetailed.TestDisplayName; }
				set { TestDetailed.TestDisplayName = value; }
			}
			public string FullTestPath
			{
				get { return TestDetailed.FullTestPath; }
				set { TestDetailed.FullTestPath = value; }
			}
			public TestStateType State
			{
				get { return TestDetailed.State; }
				set { TestDetailed.State = value; }
			}
			public string DeviceInstance
			{
				get { return TestDetailed.DeviceInstance; }
				set { TestDetailed.DeviceInstance = value; }
			}
			public int Errors
			{
				get { return TestDetailed.Errors; }
				set { TestDetailed.Errors = value; }
			}
			public int Warnings
			{
				get { return TestDetailed.Warnings; }
				set { TestDetailed.Warnings = value; }
			}

			public string ArtifactName { get; set; }


			private TestResultDetailed TestDetailed { get; set; }

			/// <summary>
			/// Return the underlying TestResultDetailed 
			/// </summary>
			public TestResultDetailed GetTestResultDetailed()
			{
				return TestDetailed;
			}
			/// <summary>
			/// Set the underlying TestResultDetailed
			/// </summary>
			public void SetTestResultDetailed(TestResultDetailed InTestResultDetailed)
			{
				TestDetailed = InTestResultDetailed;
			}
		}

		/// <summary>
		/// Contains information about an entire test pass 
		/// </summary>
		public class UnrealEngineTestPassResults : BaseHordeReport
		{
			public override string Type
			{
				get { return "Unreal Automated Tests"; }
			}

			public UnrealEngineTestPassResults() : base()
			{
				Devices = new List<Device>();
				Tests = new List<TestResult>();
			}

			public List<Device> Devices { get; set; }
			public string ReportCreatedOn { get; set; }
			public string ReportURL { get; set; }
			public int SucceededCount { get; set; }
			public int SucceededWithWarningsCount { get; set; }
			public int FailedCount { get; set; }
			public int NotRunCount { get; set; }
			public int InProcessCount { get; set; }
			public float TotalDurationSeconds { get; set; }
			public List<TestResult> Tests { get; set; }

			/// <summary>
			/// Add a new Device to the pass results and return it 
			/// </summary>
			private Device AddNewDevice()
			{
				Device NewDevice = new Device();
				Devices.Add(NewDevice);

				return NewDevice;
			}

			/// <summary>
			/// Add a new TestResult to the pass results and return it 
			/// </summary>
			private TestResult AddNewTestResult()
			{
				TestResult NewTestResult = new TestResult();
				Tests.Add(NewTestResult);

				return NewTestResult;
			}

			public override void AddEvent(EventType Type, string Message, object Context = null)
			{
				throw new System.NotImplementedException("AddEvent not implemented");
			}

			/// <summary>
			/// Convert UnrealAutomatedTestPassResults to Horde data model
			/// </summary>
			/// <param name="InTestPassResults"></param>
			/// <param name="ReportURL"></param>
			public static UnrealEngineTestPassResults FromUnrealAutomatedTests(UnrealAutomatedTestPassResults InTestPassResults, string ReportURL)
			{
				UnrealEngineTestPassResults OutTestPassResults = new UnrealEngineTestPassResults();
				if (InTestPassResults.Devices != null)
				{
					foreach (UnrealAutomationDevice InDevice in InTestPassResults.Devices)
					{
						Device ConvertedDevice = OutTestPassResults.AddNewDevice();
						ConvertedDevice.DeviceName = InDevice.DeviceName;
						ConvertedDevice.Instance = InDevice.Instance;
						ConvertedDevice.Platform = InDevice.Platform;
						ConvertedDevice.OSVersion = InDevice.OSVersion;
						ConvertedDevice.Model = InDevice.Model;
						ConvertedDevice.GPU = InDevice.GPU;
						ConvertedDevice.CPUModel = InDevice.CPUModel;
						ConvertedDevice.RAMInGB = InDevice.RAMInGB;
						ConvertedDevice.RenderMode = InDevice.RenderMode;
						ConvertedDevice.RHI = InDevice.RHI;
					}
				}
				OutTestPassResults.ReportCreatedOn = InTestPassResults.ReportCreatedOn;
				OutTestPassResults.ReportURL = ReportURL;
				OutTestPassResults.SucceededCount = InTestPassResults.Succeeded;
				OutTestPassResults.SucceededWithWarningsCount = InTestPassResults.SucceededWithWarnings;
				OutTestPassResults.FailedCount = InTestPassResults.Failed;
				OutTestPassResults.NotRunCount = InTestPassResults.NotRun;
				OutTestPassResults.InProcessCount = InTestPassResults.InProcess;
				OutTestPassResults.TotalDurationSeconds = InTestPassResults.TotalDuration;
				if (InTestPassResults.Tests != null)
				{
					foreach (UnrealAutomatedTestResult InTestResult in InTestPassResults.Tests)
					{
						TestResult ConvertedTestResult = OutTestPassResults.AddNewTestResult();
						ConvertedTestResult.TestDisplayName = InTestResult.TestDisplayName;
						ConvertedTestResult.FullTestPath = InTestResult.FullTestPath;
						ConvertedTestResult.State = InTestResult.State;
						ConvertedTestResult.DeviceInstance = InTestResult.DeviceInstance.FirstOrDefault();
						Guid TestGuid = Guid.NewGuid();
						ConvertedTestResult.ArtifactName = TestGuid + ".json";
						InTestResult.ArtifactName = ConvertedTestResult.ArtifactName;
						// Copy Test Result Detail
						TestResultDetailed ConvertedTestResultDetailed = ConvertedTestResult.GetTestResultDetailed();
						ConvertedTestResultDetailed.Errors = InTestResult.Errors;
						ConvertedTestResultDetailed.Warnings = InTestResult.Warnings;
						foreach (UnrealAutomationArtifact InTestArtifact in InTestResult.Artifacts)
						{
							Artifact NewArtifact = ConvertedTestResultDetailed.AddNewArtifact();
							NewArtifact.Id = InTestArtifact.Id;
							NewArtifact.Name = InTestArtifact.Name;
							NewArtifact.Type = InTestArtifact.Type;
							ComparisonFiles ArtifactFiles = NewArtifact.Files;
							ArtifactFiles.Difference = InTestArtifact.Files.Difference;
							ArtifactFiles.Approved = InTestArtifact.Files.Approved;
							ArtifactFiles.Unapproved = InTestArtifact.Files.Unapproved;
						}
						foreach (UnrealAutomationEntry InTestEntry in InTestResult.Entries)
						{
							Entry NewEntry = ConvertedTestResultDetailed.AddNewEntry();
							NewEntry.Filename = InTestEntry.Filename;
							NewEntry.LineNumber = InTestEntry.LineNumber;
							NewEntry.Timestamp = InTestEntry.Timestamp;
							Event EntryEvent = NewEntry.Event;
							EntryEvent.Artifact = InTestEntry.Event.Artifact;
							EntryEvent.Context = InTestEntry.Event.Context;
							EntryEvent.Message = InTestEntry.Event.Message;
							EntryEvent.Type = InTestEntry.Event.Type;
						}
					}
				}
				return OutTestPassResults;
			}

			/// <summary>
			/// Copy Test Results Artifacts
			/// </summary>
			/// <param name="ReportPath"></param>
			/// <param name="OutputArtifactPath"></param>
			public void CopyTestResultsArtifacts(string ReportPath, string InOutputArtifactPath)
			{
				SetOutputArtifactPath(InOutputArtifactPath);
				int ArtifactsCount = 0;
				foreach (TestResult OutputTestResult in Tests)
				{
					TestResultDetailed OutputTestResultDetailed = OutputTestResult.GetTestResultDetailed();
					// copy artifacts
					foreach (Artifact TestArtifact in OutputTestResultDetailed.Artifacts)
					{
						
						string[] ArtifactPaths= { TestArtifact.Files.Difference, TestArtifact.Files.Approved, TestArtifact.Files.Unapproved };
						foreach (string ArtifactPath in ArtifactPaths)
						{
							if (!string.IsNullOrEmpty(ArtifactPath) && AttachArtifact(Path.Combine(ReportPath, ArtifactPath), ArtifactPath)) { ArtifactsCount++; }
						}
					}
					// write test json blob
					string TestResultFilePath = Path.Combine(OutputArtifactPath, OutputTestResult.ArtifactName);
					try
					{
						File.WriteAllText(TestResultFilePath, JsonSerializer.Serialize(OutputTestResultDetailed, GetDefaultJsonOptions()));
						ArtifactsCount++;
					}
					catch (Exception Ex)
					{
						Log.Error("Failed to save Test Result for '{0}'. {1}", OutputTestResult.TestDisplayName, Ex);
					}
				}
				Log.Verbose("Copied {0} artifacts for Horde to {1}", ArtifactsCount, OutputArtifactPath);
			}
		}

		/// <summary>
		/// Contains information about a test session 
		/// </summary>
		public class AutomatedTestSessionData : BaseHordeReport
		{
			public override string Type
			{
				get { return "Automated Test Session"; }
			}

			public class TestResult
			{
				public string Name { get; set; }
				public string TestUID { get; set; }
				public string Suite { get; set; }
				public TestStateType State { get; set; }
				public List<string> DeviceAppInstanceName { get; set; }
				public uint ErrorCount { get; set; }
				public uint WarningCount { get; set; }
				public string ErrorHashAggregate { get; set; }
				public string DateTime { get; set; }
				public float TimeElapseSec { get; set; }
				// not part of json output
				public List<TestEvent> Events;
				public SortedSet<string> ErrorHashes;
				public TestResult(string InName, string InSuite, TestStateType InState, List<string> InDevices, string InDateTime)
				{
					Name = InName;
					TestUID = GenerateHash(Name);
					Suite = InSuite;
					State = InState;
					DeviceAppInstanceName = InDevices;
					ErrorHashes = new SortedSet<string>();
					ErrorHashAggregate = "";
					DateTime = InDateTime;
					Events = new List<TestEvent>();
				}
				private string GenerateHash(string InName)
				{
					using (SHA1 Sha1 = SHA1.Create())
					{
						return Hasher.ComputeHash(InName, Sha1, 8);
					}
				}
				public TestEvent NewEvent(EventType InType, string InMessage, string InTag, string InContext, string InDateTime)
				{
					TestEvent NewItem = new TestEvent(InType, InMessage, InTag, InContext ?? "", InDateTime);
					Events.Add(NewItem);
					switch (InType)
					{
						case EventType.Error:
							ErrorCount++;
							if (ErrorHashes.Count == 0 || !ErrorHashes.Contains(NewItem.Hash))
							{
								// Avoid error duplication and order sensitivity
								ErrorHashes.Add(NewItem.Hash);
								if (ErrorHashes.Count > 1)
								{
									ErrorHashAggregate = GenerateHash(string.Join("", ErrorHashes.ToArray()));
								}
								else
								{
									ErrorHashAggregate = NewItem.Hash;
								}
							}
							break;
						case EventType.Warning:
							WarningCount++;
							break;
					}

					return NewItem;
				}
				public TestEvent GetLastEvent()
				{
					if (Events.Count == 0)
					{
						throw new AutomationException("A test event must be added first to the report!");
					}
					return Events.Last();
				}
			}
			public class TestSession
			{
				public string DateTime { get; set; }
				public float TimeElapseSec { get; set; }
				public Dictionary<string, TestResult> Tests { get; set; }
				public string TestResultsTestDataUID { get; set; }
				public TestSession()
				{
					Tests = new Dictionary<string, TestResult>();
					TestResultsTestDataUID = Guid.NewGuid().ToString();
				}
				public TestResult NewTestResult(string InName, string InSuite, TestStateType InState, List<string> InDevices, string InDateTime)
				{
					TestResult NewItem = new TestResult(InName, InSuite, InState, InDevices, InDateTime);
					Tests[NewItem.TestUID] = NewItem;
					return NewItem;
				}
			}
			public class Device
			{
				public string Name { get; set; }
				public string AppInstanceName { get; set; }
				public string AppInstanceLog { get; set; }
				public Dictionary<string, string> Metadata { get; set; }
				public Device(string InName, string InInstance, string InInstanceLog)
				{
					Name = InName;
					AppInstanceName = InInstance;
					AppInstanceLog = InInstanceLog;
					Metadata = new Dictionary<string, string>();
				}
				public void SetMetaData(string Key, string Value)
				{
					Metadata.Add(Key, Value);
				}
			}
			public class TestArtifact
			{
				public string Tag { get; set; }
				public string ReferencePath { get; set; }
			}
			public class TestEvent
			{
				public string Message { get; set; }
				public string Context { get; set; }
				public EventType Type { get; set; }
				public string Tag { get; set; }
				public string Hash { get; set; }
				public string DateTime { get; set; }
				public List<TestArtifact> Artifacts { get; set; }
				public TestEvent(EventType InType, string InMessage, string InTag, string InContext, string InDateTime)
				{
					Type = InType;
					Message = InMessage;
					Context = InContext;
					Tag = InTag;
					Hash = InType != EventType.Info ? GenerateEventHash() : "";
					DateTime = InDateTime;
					Artifacts = new List<TestArtifact>();
				}
				public TestArtifact NewArtifacts(string InTag, string InReferencePath)
				{
					TestArtifact NewItem = new TestArtifact();
					NewItem.Tag = InTag;
					NewItem.ReferencePath = InReferencePath;
					Artifacts.Add(NewItem);
					return NewItem;
				}
				private string FilterEvent(string Text)
				{
					// Filter out time stamp in message event
					// [2021.05.20-09.55.28:474][  3]
					Regex Regex_timestamp = new Regex(@"\[[0-9]+\.[0-9]+\.[0-9]+-[0-9]+\.[0-9]+\.[0-9]+(:[0-9]+)?\](\[ *[0-9]+\])?");
					Text = Regex_timestamp.Replace(Text, "");

					// Filter out worker number in message event
					// Worker #13
					Regex Regex_workernumber = new Regex(@"Worker #[0-9]+");
					Text = Regex_workernumber.Replace(Text, "");

					// Filter out path
					// D:\build\U5+R5.0+Inc\Sync\Engine\Source\Runtime\...
					Regex Regex_pathstring = new Regex(@"([A-Z]:)?([\\/][^\\/]+)+[\\/]");
					Text = Regex_pathstring.Replace(Text, "");

					// Filter out hexadecimal string in message event
					// 0x00007ffa7b04e533
					// Mesh 305f21682cf3a231a0947ffb35c51567
					// <lambda_2a907a23b64c2d53ad869d04fdc6d423>
					Regex Regex_hexstring = new Regex(@"(0x)?[0-9a-fA-F]{6,32}");
					Text = Regex_hexstring.Replace(Text, "");

					return Text;
				}
				private string GenerateEventHash()
				{
					string FilteredEvent = FilterEvent(string.Format("{0}{1}{2}{3}", Type.ToString(), Message, Context, Tag));
					using (SHA1 Sha1 = SHA1.Create())
					{
						return Hasher.ComputeHash(FilteredEvent, Sha1, 8);
					}
				}
			}
			public class TestResultData
			{
 				public List<TestEvent> Events
				{
					get { return Result.Events; }
					set { Result.Events = value; }
				}
				private TestResult Result;
				public TestResultData(TestResult InTestResult)
				{
					Result = InTestResult;
				}
			}
			public class IndexedError
			{
				public string Message { get; set; }
				public string Tag { get; set; }
				public List<string> TestUIDs { get; set; }
				public IndexedError(string InMessage, string InTag)
				{
					Message = InMessage;
					Tag = InTag;
					TestUIDs = new List<string>();
				}
			}

			public AutomatedTestSessionData(string InName) : base()
			{
				Name = InName;
				PreFlightChange = "";
				TestSessionInfo = new TestSession();
				Devices = new List<Device>();
				IndexedErrors = new Dictionary<string, IndexedError>();
				TestResults = new Dictionary<string, TestResultData>();
			}

			public string Name { get; set; }
			public string PreFlightChange { get; set; }
			public TestSession TestSessionInfo { get; set; }
			public List<Device> Devices { get; set; }
			/// <summary>
			/// Database side optimization: Index the TestUID by TestError hash.
			/// Key is meant to be the TestError hash and Value the list of related TestUID.
			/// </summary>
			public Dictionary<string, IndexedError> IndexedErrors { get; set; }
			/// <summary>
			/// Allow the Database to pull only one test result from the TestData.
			/// Key is meant to be the TestUID and Value the test result detailed events.
			/// </summary>
			private Dictionary<string, TestResultData> TestResults { get; set; }

			private string CurrentTestUID;

			private Device NewDevice(string InName, string InInstance, string InInstanceLog = null)
			{
				Device NewItem = new Device(InName, InInstance, InInstanceLog);
				Devices.Add(NewItem);
				return NewItem;
			}

			private void IndexTestError(string ErrorHash, string TestUID, string Message, string Tag)
			{
				if (!IndexedErrors.ContainsKey(ErrorHash))
				{
					IndexedErrors[ErrorHash] = new IndexedError(Message, Tag);
				}
				else if (IndexedErrors[ErrorHash].TestUIDs.Contains(TestUID))
				{
					// already stored
					return;
				}
				IndexedErrors[ErrorHash].TestUIDs.Add(TestUID);
			}

			private TestResult GetCurrentTest()
			{
				if (string.IsNullOrEmpty(CurrentTestUID))
				{
					throw new AutomationException("A test must be set to the report!");
				}
				return TestSessionInfo.Tests[CurrentTestUID];
			}

			/// <summary>
			/// Add a Test entry and set it as current test
			/// </summary>
			/// <param name="InName"></param>
			/// <param name="InSuite"></param>
			/// <param name="InDevice"></param>
			/// <param name="InDateTime"></param>
			public void SetTest(string InName, string InSuite, List<string> InDevices, string InDateTime = null)
			{
				TestResult Test = TestSessionInfo.NewTestResult(InName, InSuite, TestStateType.Unknown, InDevices, InDateTime);
				CurrentTestUID = Test.TestUID;
				// Index the test result data.
				TestResults[Test.TestUID] = new TestResultData(Test);
			}
			/// <summary>
			/// Set current test state
			/// </summary>
			/// <param name="InState"></param>
			public void SetTestState(TestStateType InState)
			{
				TestResult CurrentTest = GetCurrentTest();
				CurrentTest.State = InState;
			}
			/// <summary>
			/// Set current test time elapse in second
			/// </summary>
			/// <param name="InTimeElapseSec"></param>
			public void SetTestTimeElapseSec(float InTimeElapseSec)
			{
				TestResult CurrentTest = GetCurrentTest();
				CurrentTest.TimeElapseSec = InTimeElapseSec;
			}
			/// <summary>
			/// Add event to current test
			/// </summary>
			/// <param name="InType"></param>
			/// <param name="InMessage"></param>
			/// <param name="InContext"></param>
			public override void AddEvent(EventType InType, string InMessage, object InContext = null)
			{
				AddEvent(InType, InMessage, "gauntlet", InContext == null ? null : InContext.ToString());
			}
			/// <summary>
			/// Overload of AddEvent, add even to current test with date time
			/// </summary>
			/// <param name="InType"></param>
			/// <param name="InMessage"></param>
			/// <param name="InTag"></param>
			/// <param name="InContext"></param>
			/// <param name="InDateTime"></param>
			public void AddEvent(EventType InType, string InMessage, string InTag, string InContext = null, string InDateTime = null)
			{
				TestResult CurrentTest = GetCurrentTest();
				TestEvent Event = CurrentTest.NewEvent(InType, InMessage, InTag, InContext, InDateTime ?? DateTime.Now.ToString("yyyy.MM.dd-HH.mm.ss"));
				if(InType == EventType.Error)
				{
					IndexTestError(Event.Hash, CurrentTest.TestUID, InMessage, InTag);
				}
			}
			/// <summary>
			/// Add artifact to last test event
			/// </summary>
			/// <param name="InTag"></param>
			/// <param name="InFilePath"></param>
			/// <param name="InReferencePath"></param>
			/// <returns></returns>
			public bool AddArtifactToLastEvent(string InTag, string InFilePath, string InReferencePath = null)
			{
				TestEvent LastEvent = GetCurrentTest().GetLastEvent();
				if (AttachArtifact(InFilePath, InReferencePath))
				{
					if (InReferencePath == null)
					{
						InReferencePath = Path.GetFileName(InFilePath);
					}
					LastEvent.NewArtifacts(InTag, InReferencePath);

					return true;
				}

				return false;
			}
			public override string GetTestDataKey(string BaseKey = null)
			{
				return base.GetTestDataKey(); // Ignore BaseKey
			}
			public override Dictionary<string, object> GetReportDependencies()
			{

				var Reports = base.GetReportDependencies();
				// Test Result Details
				var Key = string.Format("{0} Result Details::{1}", Type, TestSessionInfo.TestResultsTestDataUID);
				Reports[Key] = TestResults;

				return Reports;
			}

			/// <summary>
			/// Convert UnrealAutomatedTestPassResults to Horde data model
			/// </summary>
			/// <param name="InTestPassResults"></param>
			/// <param name="InName"></param>
			/// <param name="InSuite"></param>
			/// <param name="InReportPath"></param>
			/// <param name="InHordeArtifactPath"></param>
			/// <returns></returns>
			public static AutomatedTestSessionData FromUnrealAutomatedTests(UnrealAutomatedTestPassResults InTestPassResults, string InName, string InSuite, string InReportPath, string InHordeArtifactPath)
			{
				AutomatedTestSessionData OutTestPassResults = new AutomatedTestSessionData(InName);
				OutTestPassResults.SetOutputArtifactPath(InHordeArtifactPath);
				if (InTestPassResults.Devices != null)
				{
					foreach (UnrealAutomationDevice InDevice in InTestPassResults.Devices)
					{
						Device ConvertedDevice = OutTestPassResults.NewDevice(InDevice.DeviceName, InDevice.Instance, InDevice.AppInstanceLog);
						ConvertedDevice.SetMetaData("platform", InDevice.Platform);
						ConvertedDevice.SetMetaData("os_version", InDevice.OSVersion);
						ConvertedDevice.SetMetaData("model", InDevice.Model);
						ConvertedDevice.SetMetaData("gpu", InDevice.GPU);
						ConvertedDevice.SetMetaData("cpumodel", InDevice.CPUModel);
						ConvertedDevice.SetMetaData("ram_in_gb", InDevice.RAMInGB.ToString());
						ConvertedDevice.SetMetaData("render_mode", InDevice.RenderMode);
						ConvertedDevice.SetMetaData("rhi", InDevice.RHI);
					}
				}
				OutTestPassResults.TestSessionInfo.DateTime = InTestPassResults.ReportCreatedOn;
				OutTestPassResults.TestSessionInfo.TimeElapseSec = InTestPassResults.TotalDuration;
				if (InTestPassResults.Tests != null)
				{
					foreach (UnrealAutomatedTestResult InTestResult in InTestPassResults.Tests)
					{
						string TestDateTime = InTestResult.DateTime;
						if (TestDateTime == "0001.01.01-00.00.00")
						{
							// Special case: when UE Test DateTime is set this way, it means the value was 0 or null before getting converted to json.
							TestDateTime = null;
						}
						OutTestPassResults.SetTest(InTestResult.FullTestPath, InSuite, InTestResult.DeviceInstance, InTestResult.DateTime);
						foreach (UnrealAutomationEntry InTestEntry in InTestResult.Entries)
						{
							string Tag = "entry";
							string Context = InTestEntry.Event.Context;
							string Artifact = InTestEntry.Event.Artifact;
							UnrealAutomationArtifact IntArtifactEntry = null;
							// If Artifact values is not null nor a bunch on 0, then we have a file attachment.
							if (!string.IsNullOrEmpty(Artifact) && Artifact.Substring(0, 4) != "0000")
							{
								IntArtifactEntry = InTestResult.Artifacts.Find(A => A.Id == Artifact);
								if (IntArtifactEntry.Type == "Comparison")
								{
									// UE for now only produces one type of artifact that can be attached to an event and that's image comparison
									Tag = "image comparison";
									Context = IntArtifactEntry.Name;
								}
							}
							if (string.IsNullOrEmpty(Context) && !string.IsNullOrEmpty(InTestEntry.Filename))
							{
								Context = string.Format("{0} @line:{1}", InTestEntry.Filename, InTestEntry.LineNumber.ToString());
							}
							// Tag critical failure as crash
							if (InTestEntry.Event.Type == EventType.Error && Tag == "entry" && InTestEntry.Event.IsCriticalFailure)
							{
								Tag = "crash";
							}

							OutTestPassResults.AddEvent(InTestEntry.Event.Type, InTestEntry.Event.Message, Tag, Context, InTestEntry.Timestamp);
							bool IsInfo = InTestEntry.Event.Type == EventType.Info;
							// Add Artifacts
							if (IntArtifactEntry != null)
							{
								List<string> FailedToAttached = new List<string>();
								if (!IsInfo && !string.IsNullOrEmpty(IntArtifactEntry.Files.Difference))
								{
									if(!OutTestPassResults.AddArtifactToLastEvent(
										"difference",
										Path.Combine(InReportPath, IntArtifactEntry.Files.Difference),
										IntArtifactEntry.Files.Difference
									))
									{
										FailedToAttached.Add(IntArtifactEntry.Files.Difference);
									}
								}
								if (!IsInfo && !string.IsNullOrEmpty(IntArtifactEntry.Files.Approved))
								{
									if(!OutTestPassResults.AddArtifactToLastEvent(
										"approved",
										Path.Combine(InReportPath, IntArtifactEntry.Files.Approved),
										IntArtifactEntry.Files.Approved
									))
									{
										FailedToAttached.Add(IntArtifactEntry.Files.Approved);
									}
								}
								if (!string.IsNullOrEmpty(IntArtifactEntry.Files.Unapproved))
								{
									string AbsoluteLocation = Path.Combine(InReportPath, IntArtifactEntry.Files.Unapproved);
									if(!OutTestPassResults.AddArtifactToLastEvent(
										"unapproved",
										AbsoluteLocation,
										IntArtifactEntry.Files.Unapproved
									))
									{
										FailedToAttached.Add(IntArtifactEntry.Files.Unapproved);
									}
									// Add Json meta data if any
									string MetadataLocation = Utils.SystemHelpers.GetFullyQualifiedPath(Path.GetDirectoryName(AbsoluteLocation));
									if (!IsInfo && Directory.Exists(MetadataLocation))
									{
										string[] JsonMetadataFiles = System.IO.Directory.GetFiles(MetadataLocation, "*.json");
										if (JsonMetadataFiles.Length > 0)
										{
											int LastSlash = IntArtifactEntry.Files.Unapproved.LastIndexOf("/");
											string RelativeLocation = IntArtifactEntry.Files.Unapproved.Substring(0, LastSlash);
											OutTestPassResults.AddEvent(
												EventType.Info,
												"The image reference can be updated by pointing the Screen Comparison tab from the Test Automation window to the artifacts from this test.",
												"image comparison metadata", null,
												InTestEntry.Timestamp
											);
											foreach (string JsonFile in JsonMetadataFiles)
											{
												string JsonArtifactName = RelativeLocation + "/" + Path.GetFileName(JsonFile);
												if(!OutTestPassResults.AddArtifactToLastEvent("json metadata", JsonFile, JsonArtifactName))
												{
													FailedToAttached.Add(JsonArtifactName);
												}
											}
										}
									}
								}
								if(FailedToAttached.Count() > 0)
								{
									foreach(var Item in FailedToAttached)
									{
										OutTestPassResults.AddEvent(EventType.Warning, string.Format("Failed to attached Artifact {0}.", Item));
									}
								}
							}
						}
						OutTestPassResults.SetTestState(InTestResult.State);
						OutTestPassResults.SetTestTimeElapseSec(InTestResult.Duration);
					}
				}
				return OutTestPassResults;
			}
		}

		/// <summary>
		/// Contains Test success/failed status and a list of errors and warnings
		/// </summary>
		public class SimpleTestReport : BaseHordeReport
		{
			public class TestRole
			{
				public string Type { get; set; }
				public string Platform { get; set; }
				public string Configuration { get; set; }

				public TestRole(UnrealTestRole Role, UnrealTestRoleContext Context)
				{
					Type = Role.Type.ToString();
					Platform = Context.Platform.ToString();
					Configuration = Context.Configuration.ToString();
				}
			}

			public override string Type
			{
				get { return "Simple Report"; }
			}

			public SimpleTestReport() : base()
			{

			}

			public SimpleTestReport(Gauntlet.TestResult TestResult, UnrealTestContext Context, UnrealTestConfiguration Configuration) : base()
			{

				BuildChangeList = Context.BuildInfo.Changelist;
				this.TestResult = TestResult.ToString();

				// populate roles
				UnrealTestRole MainRole = Configuration.GetMainRequiredRole();
				this.MainRole = new TestRole(MainRole, Context.GetRoleContext(MainRole.Type));

				IEnumerable<UnrealTestRole> Roles = Configuration.RequiredRoles.Values.SelectMany(V => V);
				foreach (UnrealTestRole Role in Roles)
				{
					this.Roles.Add(new TestRole(Role, Context.GetRoleContext(Role.Type)));
				}
			}

			public string TestName { get; set; }
			public string Description { get; set; }
			public string ReportCreatedOn { get; set; }
			public float TotalDurationSeconds { get; set; }
			public bool HasSucceeded { get; set; }
			public string Status { get; set; }
			public string URLLink { get; set; }
			public int BuildChangeList { get; set; }
			public TestRole MainRole { get; set; }
			public List<TestRole> Roles { get; set; } = new List<TestRole>();
			public string TestResult { get; set; }
			public List<String> Logs { get; set; } = new List<String>();
			public List<String> Errors { get; set; } = new List<String>();
			public List<String> Warnings { get; set; } = new List<String>();

			public override void AddEvent(EventType Type, string Message, object Context = null)
			{
				switch (Type)
				{
					case EventType.Error:
						Errors.Add(Message);
						break;
					case EventType.Warning:
						Warnings.Add(Message);
						break;
				}
				// The rest is ignored with this report type.
			}

			public override bool AttachArtifact(string ArtifactPath, string Name = null)
			{
				if (base.AttachArtifact(ArtifactPath, Name))
				{
					Logs.Add(string.Format("Attached artifact: {0}", Name ?? Path.GetFileName(ArtifactPath)));
					return true;
				}
				return false;
			}
		}

		/// <summary>
		/// Container for Test Data items 
		/// </summary>
		public class TestDataCollection
		{
			public class DataItem
			{
				public string Key { get; set; }
				public object Data { get; set; }				
			}
			public TestDataCollection()
			{
				Items = new List<DataItem>();
			}

			public DataItem AddNewTestReport(ITestReport InData, string InKey = null)
			{
				if(InData is BaseHordeReport InHordeReport)
				{
					InKey = InHordeReport.GetTestDataKey(InKey);
				}
				DataItem NewDataItem = new DataItem();
				NewDataItem.Key = string.IsNullOrEmpty(InKey) ? InData.Type : InKey;
				NewDataItem.Data = InData;

				var FoundItemIndex = Items.FindIndex(I => I.Key == InKey);
				if (FoundItemIndex == -1)
				{
					Items.Add(NewDataItem);
				}
				else
				{
					Items[FoundItemIndex] = NewDataItem;
				}

				var ExtraItems = InData.GetReportDependencies();
				if (ExtraItems.Count() > 0)
				{
					foreach (string Key in ExtraItems.Keys)
					{
						DataItem ExtraDataItem = new DataItem();
						ExtraDataItem.Key = Key;
						ExtraDataItem.Data = ExtraItems[Key];
						Items.Add(ExtraDataItem);
					}
				}

				return NewDataItem;
			}

			public List<DataItem> Items { get; set; }

			/// <summary>
			/// Write Test Data Collection to json
			/// </summary>
			/// <param name="OutputTestDataFilePath"></param>
			public void WriteToJson(string OutputTestDataFilePath, bool bIncrementNameIfFileExists = false)
			{
				string OutputTestDataDir = Path.GetDirectoryName(OutputTestDataFilePath);
				if (!Directory.Exists(OutputTestDataDir))
				{
					Directory.CreateDirectory(OutputTestDataDir);
				}
				if (File.Exists(OutputTestDataFilePath) && bIncrementNameIfFileExists)
				{
					// increment filename if file exists
					string Ext = Path.GetExtension(OutputTestDataFilePath);
					string Filename = OutputTestDataFilePath.Replace(Ext, "");
					int Incr = 0;
					do
					{
						Incr++;
						OutputTestDataFilePath = string.Format("{0}{1}{2}", Filename, Incr, Ext);
					} while (File.Exists(OutputTestDataFilePath));
				}
				// write test pass summary
				Log.Verbose("Writing Test Data Collection for Horde at {0}", OutputTestDataFilePath);
				try
				{
					File.WriteAllText(OutputTestDataFilePath, JsonSerializer.Serialize(this, GetDefaultJsonOptions()));
				}
				catch (Exception Ex)
				{
					Log.Error("Failed to save Test Data Collection for Horde. {0}", Ex);
				}
			}
		}
		private static JsonSerializerOptions GetDefaultJsonOptions()
		{
			return new JsonSerializerOptions
			{
				WriteIndented = true
			};
		}
	}
}
