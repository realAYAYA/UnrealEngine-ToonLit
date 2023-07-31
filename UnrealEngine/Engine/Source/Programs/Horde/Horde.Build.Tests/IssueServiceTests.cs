// Copyright Epic Games, Inc. All Rights Reserved.

extern alias HordeAgent;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading.Tasks;
using EpicGames.Core;
using HordeAgent.Horde.Agent.Parser;
using HordeAgent.Horde.Agent.Utility;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs;
using Horde.Build.Issues;
using Horde.Build.Logs;
using Horde.Build.Users;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Server;
using Horde.Build.Tests.Stubs.Services;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using MongoDB.Bson;
using MongoDB.Driver;
using Moq;
using Horde.Build.Issues.Handlers;
using System.Threading;

namespace Horde.Build.Tests
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;
	using UserId = ObjectId<IUser>;

	[TestClass]
	public class IssueServiceTests : TestSetup
	{
		class TestJsonLogger : ILogger, IAsyncDisposable
		{
			readonly ILogFileService _logFileService;
			readonly LogId _logId;
			readonly List<(LogLevel, ReadOnlyMemory<byte>)> _events = new List<(LogLevel, ReadOnlyMemory<byte>)>();

			public TestJsonLogger(ILogFileService logFileService, LogId logId)
			{
				_logFileService = logFileService;
				_logId = logId;
			}

			public async ValueTask DisposeAsync()
			{
				foreach ((LogLevel level, ReadOnlyMemory<byte> line) in _events)
				{
					byte[] lineWithNewLine = new byte[line.Length + 1];
					line.CopyTo(lineWithNewLine);
					lineWithNewLine[^1] = (byte)'\n';
					await WriteAsync(level, lineWithNewLine);
				}
			}

			public IDisposable BeginScope<TState>(TState state) => null!;

			public bool IsEnabled(LogLevel logLevel) => true;

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
			{
				JsonLogEvent logEvent = JsonLogEvent.FromLoggerState(logLevel, eventId, state, exception, formatter);
				_events.Add((logLevel, logEvent.Data));
			}

			private async Task WriteAsync(LogLevel level, byte[] line)
			{
				ILogFile logFile = (await _logFileService.GetLogFileAsync(_logId))!;
				LogMetadata metadata = await _logFileService.GetMetadataAsync(logFile);
				await _logFileService.WriteLogDataAsync(logFile, metadata.Length, metadata.MaxLineIndex, line, false);

				if (level >= LogLevel.Warning)
				{
					LogEvent @event = ParseEvent(line);
					if (@event.LineIndex == 0)
					{
						EventSeverity severity = (level == LogLevel.Warning) ? EventSeverity.Warning : EventSeverity.Error;
						await _logFileService.CreateEventsAsync(new List<NewLogEventData> { new NewLogEventData { LogId = _logId, LineIndex = metadata.MaxLineIndex, LineCount = @event.LineCount, Severity = severity } });
					}
				}
			}

			static LogEvent ParseEvent(byte[] line)
			{
				Utf8JsonReader reader = new Utf8JsonReader(line.AsSpan());
				reader.Read();
				return LogEvent.Read(ref reader);
			}
		}

		const string MainStreamName = "//UE4/Main";
		readonly StreamId _mainStreamId = StreamId.Sanitize(MainStreamName);

		const string ReleaseStreamName = "//UE4/Release";
		readonly StreamId _releaseStreamId = StreamId.Sanitize(ReleaseStreamName);

		const string DevStreamName = "//UE4/Dev";
		readonly StreamId _devStreamId = StreamId.Sanitize(DevStreamName);
		readonly IGraph _graph;
		readonly PerforceServiceStub _perforce;
		readonly UserId _timId;
		readonly UserId _jerryId;
		readonly UserId _bobId;
		readonly DirectoryReference _autoSdkDir;
		readonly DirectoryReference _workspaceDir;

		async Task<IStream> CreateStreamAsync(ProjectId projectId, StreamId streamId, string streamName)
		{
			string revision = $"config:{streamId}";

			StreamConfig streamConfig = new StreamConfig { Name = streamName };
			streamConfig.Tabs.Add(new CreateJobsTabRequest { Title = "General", Templates = new List<TemplateRefId> { new TemplateRefId("test-template") } });
			streamConfig.Templates.Add(new TemplateRefConfig { Id = new TemplateRefId("test-template") });
			await ConfigCollection.AddConfigAsync(revision, streamConfig);

			return Deref(await StreamCollection.TryCreateOrReplaceAsync(streamId, null, revision, projectId));
		}

		public IssueServiceTests()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				_autoSdkDir = new DirectoryReference("C:\\AutoSDK");
				_workspaceDir = new DirectoryReference("C:\\Horde");
			}
			else
			{
				_autoSdkDir = new DirectoryReference("/AutoSdk");
				_workspaceDir = new DirectoryReference("/Horde");
			}

			IProject project = ProjectCollection.AddOrUpdateAsync(new ProjectId("ue4"), "", "", 0, new ProjectConfig { Name = "UE4" }).Result!;

			IStream mainStream = CreateStreamAsync(project.Id, _mainStreamId, MainStreamName).Result;
			IStream releaseStream = CreateStreamAsync(project.Id, _releaseStreamId, ReleaseStreamName).Result;
			IStream devStream = CreateStreamAsync(project.Id, _devStreamId, DevStreamName).Result;

			IUser bill = UserCollection.FindOrAddUserByLoginAsync("Bill").Result;
			IUser anne = UserCollection.FindOrAddUserByLoginAsync("Anne").Result;
			IUser bob = UserCollection.FindOrAddUserByLoginAsync("Bob").Result;
			IUser jerry = UserCollection.FindOrAddUserByLoginAsync("Jerry").Result;
			IUser chris = UserCollection.FindOrAddUserByLoginAsync("Chris").Result;

			_timId = UserCollection.FindOrAddUserByLoginAsync("Tim").Result.Id;
			_jerryId = UserCollection.FindOrAddUserByLoginAsync("Jerry").Result.Id;
			_bobId = UserCollection.FindOrAddUserByLoginAsync("Bob").Result.Id;

			_perforce = PerforceService;
			_perforce.AddChange(MainStreamName, 100, bill, "Description", new string[] { "a/b.cpp" });
			_perforce.AddChange(MainStreamName, 105, anne, "Description", new string[] { "a/c.cpp" });
			_perforce.AddChange(MainStreamName, 110, bob, "Description", new string[] { "a/d.cpp" });
			_perforce.AddChange(MainStreamName, 115, jerry, "Description\n#ROBOMERGE-SOURCE: CL 75 in //UE4/Release/...", new string[] { "a/e.cpp", "a/foo.cpp" });
			_perforce.AddChange(MainStreamName, 120, chris, "Description\n#ROBOMERGE-OWNER: Tim", new string[] { "a/f.cpp" });
			_perforce.AddChange(MainStreamName, 125, chris, "Description", new string[] { "a/g.cpp" });

			List<INode> nodes = new List<INode>();
			nodes.Add(MockNode("Update Version Files", NodeAnnotations.Empty));
			nodes.Add(MockNode("Compile UnrealHeaderTool Win64", NodeAnnotations.Empty));
			nodes.Add(MockNode("Compile ShooterGameEditor Win64", NodeAnnotations.Empty));
			nodes.Add(MockNode("Cook ShooterGame Win64", NodeAnnotations.Empty));

			NodeAnnotations staticAnalysisAnnotations = new NodeAnnotations();
			staticAnalysisAnnotations.Add("CompileType", "Static analysis");
			nodes.Add(MockNode("Static Analysis Win64", staticAnalysisAnnotations));

			NodeAnnotations staticAnalysisAnnotations2 = new NodeAnnotations();
			staticAnalysisAnnotations2.Add("CompileType", "Static analysis");
			staticAnalysisAnnotations2.Add("IssueGroup", "StaticAnalysis");
			nodes.Add(MockNode("Static Analysis Win64 v2", staticAnalysisAnnotations2));

			Mock<INodeGroup> grp = new Mock<INodeGroup>(MockBehavior.Strict);
			grp.SetupGet(x => x.Nodes).Returns(nodes);

			Mock<IGraph> graphMock = new Mock<IGraph>(MockBehavior.Strict);
			graphMock.SetupGet(x => x.Groups).Returns(new List<INodeGroup> { grp.Object });
			_graph = graphMock.Object;
		}

		public static INode MockNode(string name, IReadOnlyNodeAnnotations annotations)
		{
			Mock<INode> node = new Mock<INode>(MockBehavior.Strict);
			node.SetupGet(x => x.Name).Returns(name);
			node.SetupGet(x => x.Annotations).Returns(annotations);
			return node.Object;
		}

		public IJob CreateJob(StreamId streamId, int change, string name, IGraph graph, TimeSpan time = default, bool promoteByDefault = true)
		{
			JobId jobId = JobId.GenerateNewId();

			List<IJobStepBatch> batches = new List<IJobStepBatch>();
			for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
			{
				INodeGroup @group = graph.Groups[groupIdx];

				List<IJobStep> steps = new List<IJobStep>();
				for (int nodeIdx = 0; nodeIdx < @group.Nodes.Count; nodeIdx++)
				{
					SubResourceId stepId = new SubResourceId((ushort)((groupIdx * 100) + nodeIdx));

					ILogFile logFile = LogFileService.CreateLogFileAsync(jobId, null, LogType.Json).Result;

					Mock<IJobStep> step = new Mock<IJobStep>(MockBehavior.Strict);
					step.SetupGet(x => x.Id).Returns(stepId);
					step.SetupGet(x => x.NodeIdx).Returns(nodeIdx);
					step.SetupGet(x => x.LogId).Returns(logFile.Id);
					step.SetupGet(x => x.StartTimeUtc).Returns(DateTime.UtcNow + time);

					steps.Add(step.Object);
				}

				SubResourceId batchId = new SubResourceId((ushort)(groupIdx * 100));

				Mock<IJobStepBatch> batch = new Mock<IJobStepBatch>(MockBehavior.Strict);
				batch.SetupGet(x => x.Id).Returns(batchId);
				batch.SetupGet(x => x.GroupIdx).Returns(groupIdx);
				batch.SetupGet(x => x.Steps).Returns(steps);
				batches.Add(batch.Object);
			}

			Mock<IJob> job = new Mock<IJob>(MockBehavior.Strict);
			job.SetupGet(x => x.Id).Returns(jobId);
			job.SetupGet(x => x.Name).Returns(name);
			job.SetupGet(x => x.StreamId).Returns(streamId);
			job.SetupGet(x => x.TemplateId).Returns(new TemplateRefId("test-template"));
			job.SetupGet(x => x.Change).Returns(change);
			job.SetupGet(x => x.Batches).Returns(batches);
			job.SetupGet(x => x.ShowUgsBadges).Returns(promoteByDefault);
			job.SetupGet(x => x.ShowUgsAlerts).Returns(promoteByDefault);
			job.SetupGet(x => x.PromoteIssuesByDefault).Returns(promoteByDefault);
			job.SetupGet(x => x.NotificationChannel).Returns("#devtools-horde-slack-testing");
			return job.Object;
		}

		async Task UpdateCompleteStep(IJob job, int batchIdx, int stepIdx, JobStepOutcome outcome)
		{
			IJobStepBatch batch = job.Batches[batchIdx];
			IJobStep step = batch.Steps[stepIdx];
			await IssueService.UpdateCompleteStep(job, _graph, batch.Id, step.Id);

			JobStepRefId jobStepRefId = new JobStepRefId(job.Id, batch.Id, step.Id);
			string nodeName = _graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx].Name;
			await JobStepRefCollection.InsertOrReplaceAsync(jobStepRefId, "TestJob", nodeName, job.StreamId, job.TemplateId, job.Change, step.LogId, null, null, outcome, null, null, 0.0f, 0.0f, step.StartTimeUtc!.Value, step.StartTimeUtc);
		}

		async Task AddEvent(IJob job, int batchIdx, int stepIdx, object data, EventSeverity severity = EventSeverity.Error)
		{
			LogId logId = job.Batches[batchIdx].Steps[stepIdx].LogId!.Value;

			List<byte> bytes = new List<byte>();
			bytes.AddRange(JsonSerializer.SerializeToUtf8Bytes(data));
			bytes.Add((byte)'\n');

			ILogFile logFile = (await LogFileService.GetLogFileAsync(logId))!;
			LogMetadata metadata = await LogFileService.GetMetadataAsync(logFile);
			await LogFileService.WriteLogDataAsync(logFile, metadata.Length, metadata.MaxLineIndex, bytes.ToArray(), false);

			await LogFileService.CreateEventsAsync(new List<NewLogEventData> { new NewLogEventData { LogId = logId, LineIndex = metadata.MaxLineIndex, LineCount = 1, Severity = severity } });
		}

		private TestJsonLogger CreateLogger(IJob job, int batchIdx, int stepIdx)
		{
			LogId logId = job.Batches[batchIdx].Steps[stepIdx].LogId!.Value;
			return new TestJsonLogger(LogFileService, logId);
		}

		private async Task ParseEventsAsync(IJob job, int batchIdx, int stepIdx, string[] lines)
		{
			LogId logId = job.Batches[batchIdx].Steps[stepIdx].LogId!.Value;

			await using (TestJsonLogger logger = new TestJsonLogger(LogFileService, logId))
			{
				PerforceLogger perforceLogger = new PerforceLogger(logger);
				perforceLogger.AddClientView(_autoSdkDir, "//depot/CarefullyRedist/...", 12345);
				perforceLogger.AddClientView(_workspaceDir, "//UE4/Main/...", 12345);

				using (LogParser parser = new LogParser(perforceLogger, new List<string>()))
				{
					for (int idx = 0; idx < lines.Length; idx++)
					{
						parser.WriteLine(lines[idx]);
					}
				}
			}
		}

		[TestMethod]
		public async Task DeleteStreamTest()
		{
			await IssueService.StartAsync(CancellationToken.None);

			// #1
			// Scenario: Warning in first step
			// Expected: Default issues is created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning), message = "" }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);
			}

			// #2
			// Scenario: Stream is deleted
			// Expected: Issue is closed
			{
				await StreamCollection.DeleteAsync(_mainStreamId);
				await Clock.AdvanceAsync(TimeSpan.FromHours(1.0));

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}
		}

		[TestMethod]
		public async Task DefaultIssueTest()
		{
			// #1
			// Scenario: Warning in first step
			// Expected: Default issues is created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning), message = "" }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);
			}

			// #2
			// Scenario: Errors in other steps on same job
			// Expected: Nodes are NOT added to issue
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await AddEvent(job, 0, 1, new { message = "" });
				await UpdateCompleteStep(job, 0, 1, JobStepOutcome.Failure);
				await AddEvent(job, 0, 2, new { message = "" });
				await UpdateCompleteStep(job, 0, 2, JobStepOutcome.Failure);

				List<IIssue> issues = (await IssueCollection.FindIssuesAsync()).OrderBy(x => x.Summary).ToList();
				Assert.AreEqual(3, issues.Count);

				Assert.AreEqual("Errors in Compile ShooterGameEditor Win64", issues[0].Summary);
				Assert.AreEqual("Errors in Compile UnrealHeaderTool Win64", issues[1].Summary);
				Assert.AreEqual("Warnings in Update Version Files", issues[2].Summary);
			}

			// #3
			// Scenario: Subsequent jobs also error
			// Expected: Nodes are added to issue, but change outcome to error
			{
				IJob job = CreateJob(_mainStreamId, 110, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { message = "" });
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = (await IssueCollection.FindIssuesAsync()).OrderBy(x => x.Summary).ToList();
				Assert.AreEqual(3, issues.Count);

				Assert.AreEqual("Errors in Compile ShooterGameEditor Win64", issues[0].Summary);
				Assert.AreEqual("Errors in Compile UnrealHeaderTool Win64", issues[1].Summary);
				Assert.AreEqual("Errors in Update Version Files", issues[2].Summary);
			}

			// #4
			// Scenario: Subsequent jobs also error, but in different node
			// Expected: Additional error is created
			{
				IJob job = CreateJob(_mainStreamId, 110, "Test Build", _graph);
				await AddEvent(job, 0, 3, new { message = "" });
				await UpdateCompleteStep(job, 0, 3, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(4, issues.Count);
			}

			// #5
			// Add a description to the issue
			{
				List<IIssue> issues = await IssueCollection.FindIssuesAsync();

				IIssue issue = issues[0];
				await IssueService.UpdateIssueAsync(issue.Id, description: "Hello world!");
				IIssue? newIssue = await IssueCollection.GetIssueAsync(issue.Id);
				Assert.AreEqual(newIssue?.Description, "Hello world!");
			}
		}

		[TestMethod]
		public async Task DefaultIssueTest2()
		{
			// #1
			// Scenario: Warning in first step
			// Expected: Default issues is created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Warning, issues[0].Severity);

				Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);
			}

			// #2
			// Scenario: Same step errors
			// Expected: Issue state changes to error
			{
				IJob job = CreateJob(_mainStreamId, 110, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Error) }, EventSeverity.Error);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Error, issues[0].Severity);

				Assert.AreEqual("Errors in Update Version Files", issues[0].Summary);
			}
		}

		[TestMethod]
		public async Task DefaultIssueTest3()
		{
			// #1
			// Scenario: Warning in first step
			// Expected: Default issues is created
			{
				string[] lines =
				{
					@"< enterprise.max.test_maxscript_datasmith_export.from_maxscript_to_unreal[vray_materials.ms] >",
					@"  [ :ERROR: ] [2022.14.06-12:08:55] [log_parser] keyword found 'logpython: error:'",
					@"   > [2022.06.14-12.08.44:338][  1]LogPython: Error: One or more instance material is loaded but not expected : ['VRayMtl__2_.VRayMtl__2_', 'VRayMtl__3_.VRayMtl__3_', 'VRayMtl__1_.VRayMtl__1_', 'VRayMtl__4_.VRayMtl__4_']",
					@"     [2022.06.14-12.08.44:338][  1]LogPython: Error: One or more instance material expected but not loaded: ['VRayMtl_2.VRayMtl_2', 'VRayMtl_3.VRayMtl_3', 'VRayMtl_4.VRayMtl_4', 'VRayMtl_7.VRayMtl_7']",
					@"   > File ""D:\build\++UE5\Sync\Engine\Saved\pydrover\session[2022.14.06-11.50.11]\from_maxscript_to_unreal_4d5057656576\ue_log[2022.14.06-12.08.31].txt"", line 1085",
					@"< enterprise.max.test_maxscript_datasmith_export.from_maxscript_to_unreal[vray_materials.ms] >"
				};

				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Error, issues[0].Severity);

				Assert.AreEqual("Errors in Update Version Files", issues[0].Summary);
			}
		}

		[TestMethod]
		public async Task PerforceCaseIssueTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 170 with P4 case error
			// Expected: Creates issue, identifies source file correctly
			{
				IUser chris = await UserCollection.FindOrAddUserByLoginAsync("Chris");
				_perforce.AddChange(MainStreamName, 150, chris, "Description", new string[] { "Engine/Foo/Bar.txt" });

				IUser john = await UserCollection.FindOrAddUserByLoginAsync("John");
				_perforce.AddChange(MainStreamName, 160, john, "Description", new string[] { "Engine/Foo/Baz.txt" });

				IJob job = CreateJob(_mainStreamId, 170, "Test Build", _graph);
				await using (TestJsonLogger logger = CreateLogger(job, 0, 0))
				{
					logger.LogWarning(KnownLogEvents.AutomationTool_PerforceCase, "    {DepotFile}", new LogValue(LogValueType.DepotPath, "//UE5/Main/Engine/Foo/Bar.txt"));
				}
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Warning, issues[0].Severity);
				Assert.AreEqual("PerforceCase", issues[0].Fingerprints[0].Type);
				Assert.AreEqual("//UE5/Main/Engine/Foo/Bar.txt", issues[0].Fingerprints[0].Keys.First());
				Assert.AreEqual(chris.Id, issues[0].OwnerId);

				Assert.AreEqual("Inconsistent case for Bar.txt", issues[0].Summary);
			}
		}

		[TestMethod]
		public async Task ShaderIssueTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 170 with shader compile error
			// Expected: Creates issue, identifies source file correctly
			{
				string[] lines =
				{
					@"LogModuleManager: Display: Unable to bootstrap from archive C:/Users/buildmachine/AppData/Local/Temp/UnrealXGEWorkingDir/E196A42E4BEB54E0AE6B0EB1573DBD7A//Bootstrap-5ABC233A-4168-4640-ABFD-61E8349924DD.modules, will fallback on normal initialization",
					@"LogShaderCompilers: Warning: 1 Shader compiler errors compiling global shaders for platform SF_PS5:",
					@"LogShaderCompilers: Error: " + FileReference.Combine(_workspaceDir, "Engine/Shaders/Private/Lumen/LumenScreenProbeTracing.usf").FullName + @"(810:95): Shader FScreenProbeTraceMeshSDFsCS, Permutation 95, VF None:	/Engine/Private/Lumen/LumenScreenProbeTracing.usf(810:95): (error, code:5476) - ambiguous call to 'select_internal'. Found 88 possible candidates:",
					@"LogWindows: Error: appError called: Fatal error: [File:D:\build\U5M+Inc\Sync\Engine\Source\Runtime\Engine\Private\ShaderCompiler\ShaderCompiler.cpp] [Line: 7718]",
					@"Took 64.0177761s to run UnrealEditor-Cmd.exe, ExitCode=3",
					@"Copying crash data to d:\build\U5M+Inc\Sync\Engine\Programs\AutomationTool\Saved\Logs\Crashes\UECC-Windows-9AD258A94110A61CB524848FE0D7196D_0000...",
					@"Editor terminated with exit code 3 while running Cook for D:\build\U5M+Inc\Sync\Samples\Games\ShooterGame\ShooterGame.uproject; see log d:\build\U5M+Inc\Sync\Engine\Programs\AutomationTool\Saved\Logs\Cook-2022.07.22-05.32.05.txt",
				};

				IUser chris = await UserCollection.FindOrAddUserByLoginAsync("Chris");
				_perforce.AddChange(MainStreamName, 150, chris, "Description", new string[] { "Engine/Foo/LumenScreenProbeTracing.usf" });

				IUser john = await UserCollection.FindOrAddUserByLoginAsync("John");
				_perforce.AddChange(MainStreamName, 160, john, "Description", new string[] { "Engine/Foo/Baz.txt" });

				IJob job = CreateJob(_mainStreamId, 170, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Error, issues[0].Severity);
				Assert.AreEqual("Shader", issues[0].Fingerprints[0].Type);
				Assert.AreEqual("LumenScreenProbeTracing.usf", issues[0].Fingerprints[0].Keys.First());
				Assert.AreEqual(chris.Id, issues[0].OwnerId);

				Assert.AreEqual("Shader compile errors in LumenScreenProbeTracing.usf", issues[0].Summary);
			}
		}

		[TestMethod]
		public async Task AutoSdkWarningTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync(resolved: false);
				Assert.AreEqual(0, openIssues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120 in AutoSDK file
			// Expected: Creates issue, identifies source file correctly
			{
				string[] lines =
				{
					FileReference.Combine(_autoSdkDir, @"HostWin64\GDK\200604\Microsoft GDK\200604\GRDK\ExtensionLibraries\Xbox.Game.Chat.2.Cpp.API\DesignTime\CommonConfiguration\Neutral\Include\GameChat2Impl.h").FullName + @"(90): warning C5043: 'xbox::services::game_chat_2::chat_manager::set_memory_callbacks': exception specification does not match previous declaration",
					FileReference.Combine(_autoSdkDir, @"HostWin64\GDK\200604\Microsoft GDK\200604\GRDK\ExtensionLibraries\Xbox.Game.Chat.2.Cpp.API\DesignTime\CommonConfiguration\Neutral\Include\GameChat2.h").FullName + @"(2083): note: see declaration of 'xbox::services::game_chat_2::chat_manager::set_memory_callbacks'",
				};

				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				ILogFile? log = await LogFileService.GetLogFileAsync(job.Batches[0].Steps[0].LogId!.Value);
				List<ILogEvent> events = await LogFileService.FindEventsAsync(log!);
				Assert.AreEqual(1, events.Count);
				Assert.AreEqual(2, events[0].LineCount);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(1, spans.Count);
				Assert.AreEqual(1, issue.Fingerprints.Count);
				Assert.AreEqual("Compile", issue.Fingerprints[0].Type);
				Assert.AreEqual("Compile warnings in GameChat2Impl.h", issue.Summary);
			}
		}

		[TestMethod]
		public async Task GenericErrorTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync(resolved: false);
				Assert.AreEqual(0, openIssues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Creates issue, blames submitters at CL 110, 115, 120
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "fog.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
				};

				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(1, spans.Count);
				Assert.AreEqual(1, issue.Fingerprints.Count);
				Assert.AreEqual("Compile", issue.Fingerprints[0].Type);
				Assert.AreEqual("Compile errors in fog.cpp", issue.Summary);

				IIssueSpan stream = spans[0];
				Assert.AreEqual(105, stream.LastSuccess?.Change);
				Assert.AreEqual(null, stream.NextSuccess?.Change);

				IReadOnlyList<IIssueSuspect> suspects = await IssueCollection.FindSuspectsAsync(issue);
				suspects = suspects.OrderBy(x => x.Id).ToList();
				Assert.AreEqual(3, suspects.Count);

				Assert.AreEqual(75 /*115*/, suspects[1].Change);
				Assert.AreEqual(_jerryId, suspects[1].AuthorId);
				//				Assert.AreEqual(75, Suspects[1].OriginatingChange);

				Assert.AreEqual(110, suspects[2].Change);
				Assert.AreEqual(_bobId, suspects[2].AuthorId);
				//			Assert.AreEqual(null, Suspects[2].OriginatingChange);

				Assert.AreEqual(120, suspects[0].Change);
				Assert.AreEqual(_timId, suspects[0].AuthorId);
				//				Assert.AreEqual(null, Suspects[0].OriginatingChange);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync(resolved: false);
				Assert.AreEqual(1, openIssues.Count);
			}

			// #3
			// Scenario: Job step succeeds at CL 110
			// Expected: Issue is updated to vindicate change at CL 110
			{
				IJob job = CreateJob(_mainStreamId, 110, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(spans.Count, 1);

				IIssueSpan stream = spans[0];
				Assert.AreEqual(110, stream.LastSuccess?.Change);
				Assert.AreEqual(null, stream.NextSuccess?.Change);

				IReadOnlyList<IIssueSuspect> suspects = (await IssueCollection.FindSuspectsAsync(issue)).OrderByDescending(x => x.Change).ToList();
				Assert.AreEqual(2, suspects.Count);

				Assert.AreEqual(120, suspects[0].Change);
				Assert.AreEqual(_timId, suspects[0].AuthorId);

				Assert.AreEqual(75, suspects[1].Change);
				Assert.AreEqual(_jerryId, suspects[1].AuthorId);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync(resolved: false);
				Assert.AreEqual(1, openIssues.Count);
			}

			// #4
			// Scenario: Job step succeeds at CL 125
			// Expected: Issue is updated to narrow range to 115, 120
			{
				IJob job = CreateJob(_mainStreamId, 125, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync(resolved: true);
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(spans.Count, 1);

				IIssueSpan stream = spans[0];
				Assert.AreEqual(110, stream.LastSuccess?.Change);
				Assert.AreEqual(125, stream.NextSuccess?.Change);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync(resolved: false);
				Assert.AreEqual(0, openIssues.Count);
			}

			// #5
			// Scenario: Additional error in same node at 115
			// Expected: Event is merged into existing issue
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "fog.cpp").FullName + @"(114): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
				};

				IJob job = CreateJob(_mainStreamId, 115, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync(resolved: true);
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(1, spans.Count);

				IIssueSpan span = spans[0];
				Assert.AreEqual(110, span.LastSuccess?.Change);
				Assert.AreEqual(125, span.NextSuccess?.Change);

				List<IIssueStep> steps = await IssueCollection.FindStepsAsync(span.Id);
				Assert.AreEqual(2, steps.Count);
			}

			// #5
			// Scenario: Additional error in different node at 115
			// Expected: New issue is created
			{
				IJob job = CreateJob(_mainStreamId, 115, "Test Build", _graph);
				await AddEvent(job, 0, 1, new { });
				await UpdateCompleteStep(job, 0, 1, JobStepOutcome.Failure);

				List<IIssue> resolvedIssues = await IssueCollection.FindIssuesAsync(resolved: true);
				Assert.AreEqual(1, resolvedIssues.Count);

				List<IIssue> unresolvedIssues = await IssueCollection.FindIssuesAsync(resolved: false);
				Assert.AreEqual(1, unresolvedIssues.Count);
			}
		}

		[TestMethod]
		public async Task DefaultOwnerTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Compile Test", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Creates issue, blames submitters at CL 110, 115, 120
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
				};

				_perforce.Changes[MainStreamName][110].Files.Add("/Engine/Source/Boo.cpp");
				_perforce.Changes[MainStreamName][115].Files.Add("/Engine/Source/Foo.cpp");
				_perforce.Changes[MainStreamName][120].Files.Add("/Engine/Source/Foo.cpp");

				IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.IsTrue(issues[0].Promoted);

				List<IIssueSuspect> suspects = await IssueCollection.FindSuspectsAsync(issues[0]);

				List<UserId> primarySuspects = suspects.Select(x => x.AuthorId).ToList();
				Assert.AreEqual(2, primarySuspects.Count);
				Assert.IsTrue(primarySuspects.Contains(_jerryId)); // 115
				Assert.IsTrue(primarySuspects.Contains(_timId)); // 120
			}

			// #3
			// Scenario: Job step succeeds at CL 115
			// Expected: Creates issue, blames submitter at CL 120
			{
				IJob job = CreateJob(_mainStreamId, 115, "Compile Test", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				Assert.AreEqual(_timId, issue.OwnerId);

				// Also check updating an issue doesn't clear the owner
				Assert.IsTrue(await IssueService.UpdateIssueAsync(issue.Id));
				Assert.AreEqual(_timId, issue!.OwnerId);
			}
		}

		[TestMethod]
		public async Task ManualPromotionTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Compile Test", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Creates issue, blames submitters at CL 110, 115, 120
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
				};

				_perforce.Changes[MainStreamName][110].Files.Add("/Engine/Source/Boo.cpp");
				_perforce.Changes[MainStreamName][115].Files.Add("/Engine/Source/Foo.cpp");
				_perforce.Changes[MainStreamName][120].Files.Add("/Engine/Source/Foo.cpp");

				IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph, promoteByDefault: false);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.IsFalse(issues[0].Promoted);

				await IssueService.UpdateIssueAsync(issues[0].Id, promoted: true);

				issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.IsTrue(issues[0].Promoted);
			}
		}

		[TestMethod]
		public async Task DefaultPromotionTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Compile Test", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Issue is promoted
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
				};

				_perforce.Changes[MainStreamName][110].Files.Add("/Engine/Source/Boo.cpp");
				_perforce.Changes[MainStreamName][115].Files.Add("/Engine/Source/Foo.cpp");
				_perforce.Changes[MainStreamName][120].Files.Add("/Engine/Source/Foo.cpp");

				IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph, promoteByDefault: true);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.IsTrue(issues[0].Promoted);

			}
		}

		[TestMethod]
		public async Task CallstackTest()
		{
			string[] lines1 =
			{
				@"LogWindows: Error: begin: stack for UAT",
				@"LogWindows: Error: === Critical error: ===",
				@"LogWindows: Error:",
				@"LogWindows: Error: Fatal error!",
				@"LogWindows: Error:",
				@"LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000070",
				@"LogWindows: Error:",
				@"LogWindows: Error: [Callstack] 0x00007ffdaea6afc8 UnrealEditor-Landscape.dll!ALandscape::ALandscape() []",
				@"LogWindows: Error: [Callstack] 0x00007ffdc005d375 UnrealEditor-CoreUObject.dll!StaticConstructObject_Internal() []",
				@"LogWindows: Error: [Callstack] 0x00007ffdbfe7f2af UnrealEditor-CoreUObject.dll!FLinkerLoad::CreateExport() []",
				@"LogWindows: Error: [Callstack] 0x00007ffdbfe7fb7b UnrealEditor-CoreUObject.dll!FLinkerLoad::CreateExportAndPreload() []",
				@"LogWindows: Error: [Callstack] 0x00007ffdbfea9141 UnrealEditor-CoreUObject.dll!FLinkerLoad::LoadAllObjects() []",
				@"LogWindows: Error:",
				@"LogWindows: Error: end: stack for UAT",
				@"Took 70.6962389s to run UnrealEditor-Cmd.exe, ExitCode=3",
				@"Copying crash data to d:\build\U5M+Inc\Sync\Engine\Programs\AutomationTool\Saved\Logs\Crashes\UECC-Windows-D7C3D5AD4E079F5DF8FD00B69907CD38_0000...",
				@"Editor terminated with exit code 3 while running Cook for D:\build\U5M+Inc\Sync\Samples\Games\Lyra\Lyra.uproject; see log d:\build\U5M+Inc\Sync\Engine\Programs\AutomationTool\Saved\Logs\Cook-2022.08.19-17.34.05.txt",
			};

			IJob job1 = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
			await ParseEventsAsync(job1, 0, 0, lines1);
			await UpdateCompleteStep(job1, 0, 0, JobStepOutcome.Failure);

			List<IIssue> issues1 = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues1.Count);

			IIssue issue1 = issues1[0];
			Assert.AreEqual(issue1.Fingerprints.Count, 1);
			Assert.AreEqual(issue1.Fingerprints[0].Type, "Hashed");

			// SAME ERROR BUT DIFFERENT CALLSTACK ADDRESSES

			string[] lines2 =
			{
				@"LogWindows: Error: begin: stack for UAT",
				@"LogWindows: Error: === Critical error: ===",
				@"LogWindows: Error:",
				@"LogWindows: Error: Fatal error!",
				@"LogWindows: Error:",
				@"LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000070",
				@"LogWindows: Error:",
				@"LogWindows: Error: [Callstack] 0x00007ff95973afc8 UnrealEditor-Landscape.dll!ALandscape::ALandscape() []",
				@"LogWindows: Error: [Callstack] 0x00007ff963ced375 UnrealEditor-CoreUObject.dll!StaticConstructObject_Internal() []",
				@"LogWindows: Error: [Callstack] 0x00007ff963b0f2af UnrealEditor-CoreUObject.dll!FLinkerLoad::CreateExport() []",
				@"LogWindows: Error: [Callstack] 0x00007ff963b0fb7b UnrealEditor-CoreUObject.dll!FLinkerLoad::CreateExportAndPreload() []",
				@"LogWindows: Error: [Callstack] 0x00007ff963b39141 UnrealEditor-CoreUObject.dll!FLinkerLoad::LoadAllObjects() []",
				@"LogWindows: Error:",
				@"LogWindows: Error: end: stack for UAT",
				@"Took 67.0103214s to run UnrealEditor-Cmd.exe, ExitCode=3",
				@"Copying crash data to d:\build\U5M+Inc\Sync\Engine\Programs\AutomationTool\Saved\Logs\Crashes\UECC-Windows-5F2FFCFE4EAAFE5DDD3E0EAB04336EA0_0000...",
				@"Editor terminated with exit code 3 while running Cook for D:\build\U5M+Inc\Sync\Samples\Games\Lyra\Lyra.uproject; see log d:\build\U5M+Inc\Sync\Engine\Programs\AutomationTool\Saved\Logs\Cook-2022.08.19-16.53.28.txt",
			};

			IJob job2 = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
			await ParseEventsAsync(job2, 0, 0, lines2);
			await UpdateCompleteStep(job2, 0, 0, JobStepOutcome.Failure);

			List<IIssue> issues2 = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues2.Count);
			Assert.AreEqual(issues2[0].Id, issues1[0].Id);
		}

		[TestMethod]
		public async Task CompileTypeTest()
		{
			string[] lines =
			{
				FileReference.Combine(_workspaceDir, "FOO.CPP").FullName + @"(170) : warning C6011: Dereferencing NULL pointer 'CurrentProperty'. : Lines: 159, 162, 163, 169, 170, 174, 176, 159, 162, 163, 169, 170",
				FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(170) : warning C6011: Dereferencing NULL pointer 'CurrentProperty'. : Lines: 159, 162, 163, 169, 170, 174, 176, 159, 162, 163, 169, 170"
			};

			IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);
			await UpdateCompleteStep(job, 0, 1, JobStepOutcome.Success);
			await UpdateCompleteStep(job, 0, 2, JobStepOutcome.Success);
			await UpdateCompleteStep(job, 0, 3, JobStepOutcome.Success);
			await ParseEventsAsync(job, 0, 4, lines);
			await UpdateCompleteStep(job, 0, 4, JobStepOutcome.Failure);

			List<IIssue> issues = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues.Count);

			IIssue issue = issues[0];
			Assert.AreEqual("Static analysis warnings in FOO.CPP", issue.Summary);
		}

		[TestMethod]
		public async Task CompileIssueTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Compile Test", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Creates issue, blames submitters at CL 110, 115, 120
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "FOO.CPP").FullName + @"(170) : warning C6011: Dereferencing NULL pointer 'CurrentProperty'. : Lines: 159, 162, 163, 169, 170, 174, 176, 159, 162, 163, 169, 170",
					FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(170) : warning C6011: Dereferencing NULL pointer 'CurrentProperty'. : Lines: 159, 162, 163, 169, 170, 174, 176, 159, 162, 163, 169, 170"
				};

				IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				Assert.AreEqual(1, issue.Fingerprints.Count);

				CompileIssueHandler handler = new CompileIssueHandler();

				IIssueFingerprint fingerprint = issue.Fingerprints[0];
				Assert.AreEqual(handler.Type, fingerprint.Type);
				Assert.AreEqual(1, fingerprint.Keys.Count);
				Assert.AreEqual("FOO.CPP", fingerprint.Keys.First());
			}
		}

		[TestMethod]
		public async Task MaskedEventTest()
		{
			string[] lines =
			{
				FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
				"Error executing d:\\build\\AutoSDK\\Sync\\HostWin64\\Win64\\VS2019\\14.29.30145\\bin\\HostX64\\x64\\cl.exe (tool returned code: 2)",
				"BUILD FAILED: Command failed (Result:1): C:\\Program Files (x86)\\IncrediBuild\\xgConsole.exe \"d:\\build\\++UE5\\Sync\\Engine\\Programs\\AutomationTool\\Saved\\Logs\\UAT_XGE.xml\" /Rebuild /NoLogo /ShowAgent /ShowTime /no_watchdog_thread. See logfile for details: 'xgConsole-2022.06.09-15.14.03.txt'",
				"BUILD FAILED",
			};

			IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
			await ParseEventsAsync(job, 0, 0, lines);
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

			List<IIssue> issues = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues.Count);

			IIssueFingerprint fingerprint = issues[0].Fingerprints[0];
			Assert.AreEqual("Compile", fingerprint.Type);
			Assert.AreEqual(1, fingerprint.Keys.Count);
			Assert.AreEqual("foo.cpp", fingerprint.Keys.First());
		}

		[TestMethod]
		public async Task DeprecationTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Compile Test", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Creates issue, blames submitter at CL 115 that introduced deprecation message
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "Consumer.h").FullName + @"(22): warning C4996: 'USimpleWheeledVehicleMovementComponent': PhysX is deprecated.Use the UChaosWheeledVehicleMovementComponent from the ChaosVehiclePhysics Plugin.Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.",
					FileReference.Combine(_workspaceDir, "Deprecater.h").FullName + @"(16): note: see declaration of 'USimpleWheeledVehicleMovementComponent'"
				};

				_perforce.Changes[MainStreamName][110].Files.Add("/Engine/Source/Boo.cpp");
				_perforce.Changes[MainStreamName][115].Files.Add("/Engine/Source/Deprecater.h");
				_perforce.Changes[MainStreamName][120].Files.Add("/Engine/Source/Foo.cpp");

				IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				List<IIssueSuspect> suspects = await IssueCollection.FindSuspectsAsync(issues[0]);

				List<UserId> primarySuspects = suspects.Select(x => x.AuthorId).ToList();
				Assert.AreEqual(1, primarySuspects.Count);
				Assert.AreEqual(_jerryId, primarySuspects[0]); // 115
			}
		}

		[TestMethod]
		public async Task DeclineIssueTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Compile Test", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Creates issue, blames submitters at CL 110, 115, 120
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
				};

				_perforce.Changes[MainStreamName][110].Files.Add("/Engine/Source/Boo.cpp");
				_perforce.Changes[MainStreamName][115].Files.Add("/Engine/Source/Foo.cpp");
				_perforce.Changes[MainStreamName][120].Files.Add("/Engine/Source/Foo.cpp");

				IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				List<IIssueSuspect> suspects = await IssueCollection.FindSuspectsAsync(issues[0]);

				List<UserId> primarySuspects = suspects.Select(x => x.AuthorId).ToList();
				Assert.AreEqual(2, primarySuspects.Count);
				Assert.IsTrue(primarySuspects.Contains(_jerryId)); // 115
				Assert.IsTrue(primarySuspects.Contains(_timId)); // 120
			}

			// #3
			// Scenario: Tim declines the issue
			// Expected: Only suspect is Jerry, but owner is still unassigned
			{
				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				await IssueService.UpdateIssueAsync(issues[0].Id, declinedById: _timId);

				issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				List<IIssueSuspect> suspects = await IssueCollection.FindSuspectsAsync(issues[0]);

				List<UserId> primarySuspects = suspects.Where(x => x.DeclinedAt == null).Select(x => x.AuthorId).ToList();
				Assert.AreEqual(1, primarySuspects.Count);
				Assert.AreEqual(_jerryId, primarySuspects[0]); // 115
			}
		}

		[TestMethod]
		public async Task HashedIssueTest()
		{
			IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);

			await ParseEventsAsync(job, 0, 0, new[] { "LogSomething: Warning: This is a warning from the editor" });
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

			await ParseEventsAsync(job, 0, 1, new[] { "LogSomething: Warning: This is a warning from the editor" });
			await UpdateCompleteStep(job, 0, 1, JobStepOutcome.Failure);

			List<IIssue> issues = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues.Count);

			IIssue issue = issues[0];
			Assert.AreEqual("Warnings in Update Version Files and Compile UnrealHeaderTool Win64", issue.Summary);
		}

		[TestMethod]
		public async Task HashedIssueTest2()
		{
			IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);

			await ParseEventsAsync(job, 0, 0, new[] { "LogSomething: Warning: This is a warning from the editor" });
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

			await ParseEventsAsync(job, 0, 1, new[] { "LogSomething: Warning: This is a warning from the editor2" });
			await UpdateCompleteStep(job, 0, 1, JobStepOutcome.Failure);

			List<IIssue> issues = await IssueCollection.FindIssuesAsync();
			issues.SortBy(x => x.Id);
			Assert.AreEqual(2, issues.Count);

			Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);
			Assert.AreEqual("Warnings in Compile UnrealHeaderTool Win64", issues[1].Summary);
		}

		[TestMethod]
		public async Task HashedIssueTest3()
		{
			IJob job = CreateJob(_mainStreamId, 120, "Compile Test", _graph);

			await ParseEventsAsync(job, 0, 0, new[] { "LogSomething: Warning: This is a warning from the editor", "warning: some generic thing that will be ignored" });
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

			await ParseEventsAsync(job, 0, 1, new[] { "LogSomething: Warning: This is a warning from the editor" });
			await UpdateCompleteStep(job, 0, 1, JobStepOutcome.Failure);

			List<IIssue> issues = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues.Count);

			IIssue issue = issues[0];
			Assert.AreEqual("Warnings in Update Version Files and Compile UnrealHeaderTool Win64", issue.Summary);
		}

		[TestMethod]
		public async Task SymbolIssueTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120
			// Expected: Creates issue, blames submitter at CL 115 due to file matching symbol name
			{
				string[] lines =
				{
					@"  Foo.cpp.obj : error LNK2019: unresolved external symbol ""__declspec(dllimport) private: static class UClass * __cdecl UE::FOO::BAR"" (__UE__FOO_BAR) referenced in function ""class UPhysicalMaterial * __cdecl ConstructorHelpersInternal::FindOrLoadObject<class UPhysicalMaterial>(class FString &,unsigned int)"" (??$FindOrLoadObject@VUPhysicalMaterial@@@ConstructorHelpersInternal@@YAPEAVUPhysicalMaterial@@AEAVFString@@I@Z)"
				};

				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(spans.Count, 1);
				Assert.AreEqual(issue.Fingerprints.Count, 1);
				Assert.AreEqual(issue.Fingerprints[0].Type, "Symbol");

				IIssueSpan stream = spans[0];
				Assert.AreEqual(105, stream.LastSuccess?.Change);
				Assert.AreEqual(null, stream.NextSuccess?.Change);

				List<IIssueSuspect> suspects = await IssueCollection.FindSuspectsAsync(issues[0]);

				List<UserId> primarySuspects = suspects.Select(x => x.AuthorId).ToList();
				Assert.AreEqual(1, primarySuspects.Count);
				Assert.AreEqual(_jerryId, primarySuspects[0]); // 115 = foo.cpp
			}
		}

		[TestMethod]
		public async Task SymbolIssueTest2()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120 on different platforms
			// Expected: Creates single issue
			{
				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);

				string[] lines1 =
				{
					@"Undefined symbols for architecture x86_64:",
					@"  ""Metasound::FTriggerOnThresholdOperator::DefaultThreshold"", referenced from:",
					@"      Metasound::FTriggerOnThresholdOperator::DeclareVertexInterface() in Module.MetasoundStandardNodes.cpp.o",
					@"ld: symbol(s) not found for architecture x86_64",
					@"clang: error: linker command failed with exit code 1 (use -v to see invocation)"
				};
				await ParseEventsAsync(job, 0, 0, lines1);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				// NB. This is a new step and has not succeeded before, but can still be merged with the issue above.
				string[] lines2 =
				{
					@"ld.lld: error: undefined symbol: Metasound::FTriggerOnThresholdOperator::DefaultThreshold",
					@">>> referenced by Module.MetasoundStandardNodes.cpp",
					@">>>               D:/build/++UE5/Sync/Engine/Plugins/Runtime/Metasound/Intermediate/Build/Linux/B4D820EA/UnrealEditor/Debug/MetasoundStandardNodes/Module.MetasoundStandardNodes.cpp.o:(Metasound::FTriggerOnThresholdOperator::DeclareVertexInterface())",
					@"clang++: error: linker command failed with exit code 1 (use -v to see invocation)",
				};
				await ParseEventsAsync(job, 0, 1, lines2);
				await UpdateCompleteStep(job, 0, 1, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(spans.Count, 2);
				Assert.AreEqual(issue.Fingerprints.Count, 1);
				Assert.AreEqual(issue.Fingerprints[0].Type, "Symbol");

				IIssueSpan span1 = spans[0];
				Assert.AreEqual(105, span1.LastSuccess?.Change);
				Assert.AreEqual(null, span1.NextSuccess?.Change);

				IIssueSpan span2 = spans[1];
				Assert.AreEqual(null, span2.LastSuccess?.Change);
				Assert.AreEqual(null, span2.NextSuccess?.Change);
			}
		}

		[TestMethod]
		public async Task SymbolIssueTest3()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120 on different platforms
			// Expected: Creates single issue
			{
				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);

				string[] lines =
				{
					@"  DatasmithDirectLink.cpp.obj : error LNK2019: unresolved external symbol ""enum DirectLink::ECommunicationStatus __cdecl DirectLink::ValidateCommunicationStatus(void)"" (?ValidateCommunicationStatus@DirectLink@@YA?AW4ECommunicationStatus@1@XZ) referenced in function ""public: static int __cdecl FDatasmithDirectLink::ValidateCommunicationSetup(void)"" (?ValidateCommunicationSetup@FDatasmithDirectLink@@SAHXZ)",
					@"  Engine\Binaries\Win64\UE4Editor-DatasmithExporter.dll: fatal error LNK1120: 1 unresolved externals",
				};
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(1, spans.Count);
				Assert.AreEqual(1, issue.Fingerprints.Count);
				Assert.AreEqual("Symbol", issue.Fingerprints[0].Type);

				IIssueSpan span = spans[0];
				Assert.AreEqual(105, span.LastSuccess?.Change);
				Assert.AreEqual(null, span.NextSuccess?.Change);
			}

			// #3
			// Scenario: Job step fails at 125 with link error, but does not match symbol name
			// Expected: Creates new issue
			{
				IJob job = CreateJob(_mainStreamId, 125, "Test Build", _graph);

				string[] lines =
				{
					@"  Engine\Binaries\Win64\UE4Editor-DatasmithExporter.dll: fatal error LNK1120: 1 unresolved externals",
				};
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(1, spans.Count);
				Assert.AreEqual(1, issue.Fingerprints.Count);
				Assert.AreEqual("Hashed", issue.Fingerprints[0].Type);

				IIssueSpan span = spans[0];
				Assert.AreEqual(120, span.LastSuccess?.Change);
				Assert.AreEqual(null, span.NextSuccess?.Change);
			}
		}

		[TestMethod]
		public async Task LinkerIssueTest2()
		{
			string[] lines =
			{
				@"..\Intermediate\Build\Win64\UnrealEditor\Development\AdvancedPreviewScene\Default.rc2.res : fatal error LNK1123: failure during conversion to COFF: file invalid or corrupt"
			};

			IJob job = CreateJob(_mainStreamId, 120, "Linker Test", _graph);
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);
			await ParseEventsAsync(job, 0, 0, lines);
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

			List<IIssue> issues = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues.Count);

			IIssue issue = issues[0];
			Assert.AreEqual("Errors in Update Version Files", issue.Summary);
		}

		[TestMethod]
		public async Task MaskIssueTest()
		{
			// #1
			// Scenario: Job step completes successfully at CL 105
			// Expected: No issues are created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
			}

			// #2
			// Scenario: Job step fails at CL 120 with compile & link error
			// Expected: Creates one issue for compile error
			{
				string[] lines =
				{
					FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
					@"  Foo.cpp.obj : error LNK2019: unresolved external symbol ""__declspec(dllimport) private: static class UClass * __cdecl UE::FOO::BAR"" (__UE__FOO_BAR) referenced in function ""class UPhysicalMaterial * __cdecl ConstructorHelpersInternal::FindOrLoadObject<class UPhysicalMaterial>(class FString &,unsigned int)"" (??$FindOrLoadObject@VUPhysicalMaterial@@@ConstructorHelpersInternal@@YAPEAVUPhysicalMaterial@@AEAVFString@@I@Z)"
				};

				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(1, spans.Count);
				Assert.AreEqual(1, issue.Fingerprints.Count);
				Assert.AreEqual("Compile", issue.Fingerprints[0].Type);
			}
		}

		[TestMethod]
		public async Task MissingCopyrightTest()
		{
			string[] lines =
			{
				@"WARNING: Engine\Source\Programs\UnrealBuildTool\ProjectFiles\Rider\ToolchainInfo.cs: Missing copyright boilerplate"
			};

			IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);
			await ParseEventsAsync(job, 0, 0, lines);
			await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

			List<IIssue> issues = await IssueCollection.FindIssuesAsync();
			Assert.AreEqual(1, issues.Count);

			IIssue issue = issues[0];
			List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
			Assert.AreEqual(1, spans.Count);
			Assert.AreEqual(1, issue.Fingerprints.Count);
			Assert.AreEqual("Copyright", issue.Fingerprints[0].Type);
			Assert.AreEqual("Missing copyright notice in ToolchainInfo.cs", issue.Summary);
		}

		[TestMethod]
		public async Task AddSpanToIssueTest()
		{
			// Create the first issue
			IIssue issueA;
			IIssueSpan spanA;
			{
				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);

				string[] lines =
				{
					@"  DatasmithDirectLink.cpp.obj : error LNK2019: unresolved external symbol ""enum DirectLink::ECommunicationStatus __cdecl DirectLink::ValidateCommunicationStatus(void)"" (?ValidateCommunicationStatus@DirectLink@@YA?AW4ECommunicationStatus@1@XZ) referenced in function ""public: static int __cdecl FDatasmithDirectLink::ValidateCommunicationSetup(void)"" (?ValidateCommunicationSetup@FDatasmithDirectLink@@SAHXZ)",
				};
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				issueA = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issueA.Id);
				Assert.AreEqual(spans.Count, 1);
				spanA = spans[0];
			}

			// Create the second issue
			IIssue issueB;
			IIssueSpan spanB;
			{
				string[] lines =
				{
					@"WARNING: Engine\Source\Programs\UnrealBuildTool\ProjectFiles\Rider\ToolchainInfo.cs: Missing copyright boilerplate"
				};

				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);
				await ParseEventsAsync(job, 0, 0, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				issues.RemoveAll(x => x.Id == issueA.Id);
				Assert.AreEqual(1, issues.Count);

				issueB = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issueB.Id);
				Assert.AreEqual(1, spans.Count);
				spanB = spans[0];
			}

			// Add SpanB to IssueA
			{
				await IssueService.UpdateIssueAsync(issueA.Id, addSpanIds: new List<ObjectId> { spanB.Id });

				IIssue newIssueA = (await IssueCollection.GetIssueAsync(issueA.Id))!;
				Assert.IsNull(newIssueA.VerifiedAt);
				Assert.IsNull(newIssueA.ResolvedAt);
				Assert.AreEqual(2, newIssueA.Fingerprints.Count);
				List<IIssueSpan> newSpansA = await IssueCollection.FindSpansAsync(newIssueA!.Id);
				Assert.AreEqual(2, newSpansA.Count);
				Assert.AreEqual(newIssueA.Id, newSpansA[0].IssueId);
				Assert.AreEqual(newIssueA.Id, newSpansA[1].IssueId);

				IIssue newIssueB = (await IssueCollection.GetIssueAsync(issueB.Id))!;
				Assert.IsNotNull(newIssueB.VerifiedAt);
				Assert.IsNotNull(newIssueB.ResolvedAt);
				Assert.AreEqual(0, newIssueB.Fingerprints.Count);
				List<IIssueSpan> newSpansB = await IssueCollection.FindSpansAsync(newIssueB.Id);
				Assert.AreEqual(0, newSpansB.Count);
			}

			// Add SpanA and SpanB to IssueB
			{
				await IssueService.UpdateIssueAsync(issueB.Id, addSpanIds: new List<ObjectId> { spanA.Id, spanB.Id });

				IIssue newIssueA = (await IssueCollection.GetIssueAsync(issueA.Id))!;
				Assert.IsNotNull(newIssueA.VerifiedAt);
				Assert.IsNotNull(newIssueA.ResolvedAt);
				Assert.AreEqual(0, newIssueA.Fingerprints.Count);
				List<IIssueSpan> newSpansA = await IssueCollection.FindSpansAsync(newIssueA.Id);
				Assert.AreEqual(0, newSpansA.Count);

				IIssue newIssueB = (await IssueCollection.GetIssueAsync(issueB.Id))!;
				Assert.IsNull(newIssueB.VerifiedAt);
				Assert.IsNull(newIssueB.ResolvedAt);
				Assert.AreEqual(2, newIssueB.Fingerprints.Count);
				List<IIssueSpan> newSpansB = await IssueCollection.FindSpansAsync(newIssueB!.Id);
				Assert.AreEqual(2, newSpansB.Count);
				Assert.AreEqual(newIssueB.Id, newSpansB[0].IssueId);
				Assert.AreEqual(newIssueB.Id, newSpansB[1].IssueId);
			}
		}

		[TestMethod]
		public async Task ExplicitGroupingTest()
		{
			string[] lines =
			{
				FileReference.Combine(_workspaceDir, "foo.cpp").FullName + @"(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
			};

			IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);

			// Create the first issue
			{
				await ParseEventsAsync(job, 0, 4, lines);
				await UpdateCompleteStep(job, 0, 4, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual("Compile", issues[0].Fingerprints[0].Type);
			}

			// Create the same error in a different group, check they don't merge
			{
				await ParseEventsAsync(job, 0, 5, lines);
				await UpdateCompleteStep(job, 0, 5, JobStepOutcome.Failure);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(2, issues.Count);

				issues.SortBy(x => x.Id);
				Assert.AreEqual("Compile", issues[0].Fingerprints[0].Type);
				Assert.AreEqual("Compile:StaticAnalysis", issues[1].Fingerprints[0].Type);
			}
		}

		[TestMethod]
		public async Task GauntletIssueTest()
		{
			string[] lines =
			{
				@"  Error: EngineTest.RunTests Group:HLOD (Win64 Development EditorGame) result=Failed",
				@"    # EngineTest.RunTests Group:HLOD Report",
				@"    ----------------------------------------",
				@"    ### Process Role: Editor (Win64 Development)",
				@"    ----------------------------------------",
				@"    ##### Result: Abnormal Exit: Reason=3/24 tests failed, Code=-1",
				@"    FatalErrors: 0, Ensures: 0, Errors: 8, Warnings: 20, Hash: 0",
				@"    ##### Artifacts",
				@"    Log: P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor\Saved_1\Editor\EditorOutput.log",
				@"    Commandline: d:\Build\++UE5\Sync\EngineTest\EngineTest.uproject   -gauntlet  -unattended  -stdout  -AllowStdOutLogVerbosity  -gauntlet.heartbeatperiod=30  -NoWatchdog  -FORCELOGFLUSH  -CrashForUAT  -buildmachine  -ReportExportPath=""P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor""  -ExecCmds=""Automation RunTests Group:HLOD; Quit;""  -ddc=default  -userdir=""d:\Build\++UE5\Sync/Tests\DeviceCache\Win64\LocalDevice0_UserDir""",
				@"    P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor\Saved_1\Editor",
				@"    ----------------------------------------",
				@"    ## Summary",
				@"    ### EngineTest.RunTests Group:HLOD Failed",
				@"    ### Editor: 3/24 tests failed",
				@"    See below for logs and any callstacks",
				@"    Context: Win64 Development EditorGame",
				@"    FatalErrors: 0, Ensures: 0, Errors: 8, Warnings: 20",
				@"    Result: Failed, ResultHash: 0",
				@"    21 of 24 tests passed",
				@"    ### The following tests failed:",
				@"    ##### SectionFlags: SectionFlags",
				@"    * LogAutomationController: Building static mesh SectionFlags... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Building static mesh SectionFlags... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SectionFlags_LOD_0_None' test failed, Screenshots were different!  Global Difference = 0.058361, Max Local Difference = 0.821376 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    ##### SimpleMerge: SimpleMerge",
				@"    * LogAutomationController: Building static mesh SM_TeapotHLOD... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Building static mesh SM_TeapotHLOD... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_0_None' was similar!  Global Difference = 0.000298, Max Local Difference = 0.010725 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_1_None' test failed, Screenshots were different!  Global Difference = 0.006954, Max Local Difference = 0.129438 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_2_None' test failed, Screenshots were different!  Global Difference = 0.007732, Max Local Difference = 0.127959 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_3_None' test failed, Screenshots were different!  Global Difference = 0.009140, Max Local Difference = 0.172337 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_0_BaseColor' was similar!  Global Difference = 0.000000, Max Local Difference = 0.000000 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_1_BaseColor' was similar!  Global Difference = 0.002068, Max Local Difference = 0.045858 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_2_BaseColor' was similar!  Global Difference = 0.002377, Max Local Difference = 0.045858 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_3_BaseColor' was similar!  Global Difference = 0.002647, Max Local Difference = 0.057322 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    ##### SingleLODMerge: SingleLODMerge",
				@"    * LogAutomationController: Building static mesh Pencil2... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Building static mesh Pencil2... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SingleLODMerge_LOD_0_BaseColor' test failed, Screenshots were different!  Global Difference = 0.013100, Max Local Difference = 0.131657 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]"
			};

			// #1
			// Scenario: Job with multiple GJob step fails at CL 120 with compile & link error
			// Expected: Creates one issue for compile error
			{
				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph);
				await ParseAsync(job.Batches[0].Steps[0].LogId!.Value, lines);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Failure);

				ILogFile? log = await LogFileService.GetLogFileAsync(job.Batches[0].Steps[0].LogId!.Value);
				List<ILogEvent> events = await LogFileService.FindEventsAsync(log!);
				Assert.AreEqual(1, events.Count);
				Assert.AreEqual(40, events[0].LineCount);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);

				IIssue issue = issues[0];
				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(spans.Count, 1);
				Assert.AreEqual(issue.Fingerprints.Count, 1);
				Assert.AreEqual(issue.Fingerprints[0].Type, "Gauntlet");
				Assert.AreEqual(issue.Summary, "HLOD test failures: SectionFlags, SimpleMerge and SingleLODMerge");
			}
		}

		[TestMethod]
		public async Task FixFailedTest()
		{
			int issueId;

			// #1
			// Scenario: Warning in first step
			// Expected: Default issue is created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Warning, issues[0].Severity);

				Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);

				issueId = issues[0].Id;
			}

			// #2
			// Scenario: Issue is marked fixed
			// Expected: Resolved time, owner is set
			{
				await IssueService.UpdateIssueAsync(issueId, resolvedById: _bobId);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, openIssues.Count);

				IIssue issue = (await IssueCollection.GetIssueAsync(issueId))!;
				Assert.IsNotNull(issue.ResolvedAt);
				Assert.AreEqual(_bobId, issue.OwnerId);
				Assert.AreEqual(_bobId, issue.ResolvedById);
			}

			// #3
			// Scenario: Issue recurs an hour later
			// Expected: Issue is still marked as resolved
			{
				IJob job = CreateJob(_mainStreamId, 110, "Test Build", _graph, TimeSpan.FromHours(1.0));
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, openIssues.Count);

				IIssue issue = (await IssueCollection.GetIssueAsync(issueId))!;
				Assert.IsNotNull(issue.ResolvedAt);
				Assert.AreEqual(_bobId, issue.OwnerId);
				Assert.AreEqual(_bobId, issue.ResolvedById);
			}

			// #4
			// Scenario: Issue recurs a day later at the same change
			// Expected: Issue is reopened
			{
				IJob job = CreateJob(_mainStreamId, 110, "Test Build", _graph, TimeSpan.FromHours(25.0));
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, openIssues.Count);

				IIssue issue = openIssues[0];
				Assert.AreEqual(issueId, issue.Id);
				Assert.IsNull(issue.ResolvedAt);
				Assert.AreEqual(_bobId, issue.OwnerId);
				Assert.IsNull(issue.ResolvedById);
			}

			// #5
			// Scenario: Issue is marked fixed again, at a particular changelist
			// Expected: Resolved time, owner is set
			{
				await IssueService.UpdateIssueAsync(issueId, resolvedById: _bobId, fixChange: 115);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, openIssues.Count);

				IIssue issue = (await IssueCollection.GetIssueAsync(issueId))!;
				Assert.IsNotNull(issue.ResolvedAt);
				Assert.AreEqual(_bobId, issue.OwnerId);
				Assert.AreEqual(_bobId, issue.ResolvedById);
			}

			// #6
			// Scenario: Issue fails again at a later changelist
			// Expected: Issue is reopened
			{
				IJob job = CreateJob(_mainStreamId, 120, "Test Build", _graph, TimeSpan.FromHours(25.0));
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, openIssues.Count);

				IIssue issue = openIssues[0];
				Assert.AreEqual(issueId, issue.Id);
				Assert.IsNull(issue.ResolvedAt);
				Assert.AreEqual(_bobId, issue.OwnerId);
				Assert.IsNull(issue.ResolvedById);
			}

			// #7
			// Scenario: Issue is marked fixed again, at a particular changelist
			// Expected: Resolved time, owner is set
			{
				await IssueService.UpdateIssueAsync(issueId, resolvedById: _bobId, fixChange: 125);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, openIssues.Count);

				IIssue issue = (await IssueCollection.GetIssueAsync(issueId))!;
				Assert.IsNotNull(issue.ResolvedAt);
				Assert.AreEqual(_bobId, issue.OwnerId);
				Assert.AreEqual(_bobId, issue.ResolvedById);
			}

			// #8
			// Scenario: Issue succeeds at a later changelist
			// Expected: Issue remains closed
			{
				IJob job = CreateJob(_mainStreamId, 125, "Test Build", _graph, TimeSpan.FromHours(25.0));
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, openIssues.Count);
			}

			// #9
			// Scenario: Issue fails at a later changelist
			// Expected: New issue is opened
			{
				IJob job = CreateJob(_mainStreamId, 130, "Test Build", _graph, TimeSpan.FromHours(25.0));
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> openIssues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, openIssues.Count);

				IIssue issue = openIssues[0];
				Assert.AreNotEqual(issueId, issue.Id);
			}
		}

		[TestMethod]
		public async Task AutoResolveTest()
		{
			int issueId;

			// #1
			// Scenario: Warning in first step
			// Expected: Default issue is created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Warning, issues[0].Severity);

				Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);

				issueId = issues[0].Id;
			}

			// #2
			// Scenario: Job succeeds
			// Expected: Issue is marked as resolved
			{
				IJob job = CreateJob(_mainStreamId, 115, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				IIssue? issue = await IssueCollection.GetIssueAsync(issueId);
				Assert.IsNotNull(issue!.ResolvedAt);

				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(spans.Count, 1);
			}
		}

		[TestMethod]
		public async Task QuarantineTest()
		{
			int issueId;
			DateTime lastSeenAt;

			// #1
			// Scenario: Warning in first step
			// Expected: Default issue is created
			{
				IJob job = CreateJob(_mainStreamId, 105, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Warning, issues[0].Severity);

				Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);

				issueId = issues[0].Id;
				lastSeenAt = issues[0].LastSeenAt;
			}

			// Mark issue as quarantined
			await IssueService.UpdateIssueAsync(issueId, quarantinedById: _jerryId);

			// #2
			// Scenario: Job succeeds
			// Expected: Issue is not marked resolved
			{
				IJob job = CreateJob(_mainStreamId, 115, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				IIssue? issue = await IssueCollection.GetIssueAsync(issueId);
				Assert.IsNull(issue!.ResolvedAt);

				List<IIssueSpan> spans = await IssueCollection.FindSpansAsync(issue.Id);
				Assert.AreEqual(spans.Count, 1);

			}

			// #3
			// Scenario: Job fails
			// Expected: Existing issue is updated
			{
				IJob job = CreateJob(_mainStreamId, 125, "Test Build", _graph);
				await AddEvent(job, 0, 0, new { level = nameof(LogLevel.Warning) }, EventSeverity.Warning);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Warnings);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(1, issues.Count);
				Assert.AreEqual(IssueSeverity.Warning, issues[0].Severity);

				Assert.AreEqual("Warnings in Update Version Files", issues[0].Summary);

				Assert.AreEqual(issueId, issues[0].Id);
				Assert.AreNotEqual(lastSeenAt, issues[0].LastSeenAt);
			}

			// Mark issue as not quarantined
			await IssueService.UpdateIssueAsync(issueId, quarantinedById: UserId.Empty);

			// #4
			// Scenario: Job succeeds
			// Expected: Issue is marked resolved and closed
			{
				IJob job = CreateJob(_mainStreamId, 135, "Test Build", _graph);
				await UpdateCompleteStep(job, 0, 0, JobStepOutcome.Success);

				IIssue? issue = await IssueCollection.GetIssueAsync(issueId);
				Assert.IsNotNull(issue!.ResolvedAt);

				List<IIssue> issues = await IssueCollection.FindIssuesAsync();
				Assert.AreEqual(0, issues.Count);
												
			}


		}


		private async Task ParseAsync(LogId logId, string[] lines)
		{
			await using (TestJsonLogger logger = new TestJsonLogger(LogFileService, logId))
			{
				PerforceLogger perforceLogger = new PerforceLogger(logger);
				perforceLogger.AddClientView(DirectoryReference.GetCurrentDirectory(), "//UE4/Main/...", 12345);

				using (LogParser parser = new LogParser(perforceLogger, new List<string>()))
				{
					for (int idx = 0; idx < lines.Length; idx++)
					{
						parser.WriteLine(lines[idx]);
					}
				}
			}
		}

		//		static List<IIssueSpan> GetOriginSpans(List<IIssueSpan> spans)

		static IIssueSpanSuspect MockSuspect(int change, int? originatingChange)
		{
			Mock<IIssueSpanSuspect> suspect = new Mock<IIssueSpanSuspect>(MockBehavior.Strict);
			suspect.SetupGet(x => x.Change).Returns(change);
			suspect.SetupGet(x => x.OriginatingChange).Returns(originatingChange);
			return suspect.Object;
		}

		static IIssueSpan MockSpan(StreamId streamId, params IIssueSpanSuspect[] suspects)
		{
			Mock<IIssueSpan> span = new Mock<IIssueSpan>(MockBehavior.Strict);
			span.SetupGet(x => x.StreamId).Returns(streamId);
			span.SetupGet(x => x.Suspects).Returns(suspects);
			return span.Object;
		}

		[TestMethod]
		public void FindMergeOrigins()
		{
			{
				List<IIssueSpan> spans = new List<IIssueSpan>();
				spans.Add(MockSpan(new StreamId("ue5-main"), MockSuspect(201, 1), MockSuspect(202, 2)));
				spans.Add(MockSpan(new StreamId("ue5-release-staging"), MockSuspect(101, 1), MockSuspect(102, 2)));
				spans.Add(MockSpan(new StreamId("ue5-release"), MockSuspect(1, null), MockSuspect(2, null)));

				List<IIssueSpan> results = Horde.Build.Issues.IssueService.FindMergeOriginSpans(spans);
				Assert.AreEqual(1, results.Count);
				Assert.AreEqual(new StreamId("ue5-release"), results[0].StreamId);
			}

			{
				List<IIssueSpan> spans = new List<IIssueSpan>();
				spans.Add(MockSpan(new StreamId("ue5-main"), MockSuspect(201, null), MockSuspect(202, 2)));
				spans.Add(MockSpan(new StreamId("ue5-release-staging"), MockSuspect(101, 1), MockSuspect(102, null)));
				spans.Add(MockSpan(new StreamId("ue5-release"), MockSuspect(1, null), MockSuspect(2, null)));

				List<IIssueSpan> results = Horde.Build.Issues.IssueService.FindMergeOriginSpans(spans);
				Assert.AreEqual(1, results.Count);
				Assert.AreEqual(new StreamId("ue5-release"), results[0].StreamId);
			}

			{
				List<IIssueSpan> spans = new List<IIssueSpan>();
				spans.Add(MockSpan(new StreamId("ue5-release"), MockSuspect(1, null), MockSuspect(2, null)));
				spans.Add(MockSpan(new StreamId("ue5-release-staging"), MockSuspect(101, 1), MockSuspect(102, null)));
				spans.Add(MockSpan(new StreamId("ue5-main"), MockSuspect(201, null), MockSuspect(202, 2)));

				List<IIssueSpan> results = Horde.Build.Issues.IssueService.FindMergeOriginSpans(spans);
				Assert.AreEqual(1, results.Count);
				Assert.AreEqual(new StreamId("ue5-release"), results[0].StreamId);
			}
		}
	}
}
