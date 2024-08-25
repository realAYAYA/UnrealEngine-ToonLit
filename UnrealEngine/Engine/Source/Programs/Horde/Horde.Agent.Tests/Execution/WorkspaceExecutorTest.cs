// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Streams;
using Horde.Agent.Execution;
using Horde.Agent.Services;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests.Execution;

[TestClass]
public sealed class WorkspaceExecutorTest : IAsyncDisposable
{
	private readonly StreamId _streamId = new StreamId("foo-main");
	private readonly JobId _jobId = JobId.Parse("65bd0655591b5d5d7d047b58");
	private readonly JobStepBatchId _batchId = new JobStepBatchId(0x1234);
	private const string AgentType = "bogusAgentType";

	private readonly ILogger<WorkspaceExecutorTest> _logger;
	private readonly FakeHordeRpcServer _server = new();
	private readonly ISession _session;
	private readonly FakeWorkspaceMaterializer _autoSdkWorkspace = new();
	private readonly FakeWorkspaceMaterializer _workspace = new();
	private readonly WorkspaceExecutor _executor;

	public WorkspaceExecutorTest()
	{
		using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
		{
			builder.SetMinimumLevel(LogLevel.Debug);
			builder.AddSimpleConsole(options => { options.SingleLine = true; });
		});

		_logger = loggerFactory.CreateLogger<WorkspaceExecutorTest>();

		_server.AddStream(_streamId, "//Foo/Main");
		_server.AddAgentType(_streamId, AgentType);
		_server.AddJob(_jobId, _streamId, 1, 0);
		_session = FakeServerSessionFactory.CreateSession(_server.GetConnection(), _server.GetGrpcChannel());

		_autoSdkWorkspace.SetFile(1, "HostWin64/Android/base.h", "base");
		_workspace.SetFile(1, "main.cpp", "main");
		_workspace.SetFile(1, "foo/bar/baz.h", "baz");

		BeginBatchResponse batch = new BeginBatchResponse { Change = 1 };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, _jobId, _batchId, batch, null!, new JobOptions());
		_executor = new(executorOptions, _workspace, null, NullLogger.Instance);
	}

	public async ValueTask DisposeAsync()
	{
		_executor.Dispose();
		await _session.DisposeAsync();
		await _server.DisposeAsync();
		_workspace.Dispose();
		_autoSdkWorkspace.Dispose();
	}

	[TestMethod]
	public async Task RegularWorkspaceAsync()
	{
		await _executor.InitializeAsync(_logger, CancellationToken.None);
		AssertWorkspaceFile(_workspace, "main.cpp", "main");
		AssertWorkspaceFile(_workspace, "foo/bar/baz.h", "baz");
		await _executor.FinalizeAsync(_logger, CancellationToken.None);
	}

	[TestMethod]
	public async Task RegularAndAutoSdkWorkspaceAsync()
	{
		BeginBatchResponse batch = new BeginBatchResponse { Change = 1 };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, _jobId, _batchId, batch, null!, new JobOptions());
		using WorkspaceExecutor executor = new(executorOptions, _workspace, _autoSdkWorkspace, NullLogger.Instance);

		await executor.InitializeAsync(_logger, CancellationToken.None);
		AssertWorkspaceFile(_autoSdkWorkspace, "HostWin64/Android/base.h", "base");
		AssertWorkspaceFile(_workspace, "main.cpp", "main");
		AssertWorkspaceFile(_workspace, "foo/bar/baz.h", "baz");
		await executor.FinalizeAsync(_logger, CancellationToken.None);
	}

	[TestMethod]
	public async Task EnvVarsAsync()
	{
		BeginBatchResponse batch = new BeginBatchResponse { Change = 1, StreamName = "//UE5/Main" };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, _jobId, _batchId, batch, null!, new JobOptions());
		using WorkspaceExecutor executor = new(executorOptions, _workspace, _autoSdkWorkspace, NullLogger.Instance);
		await executor.InitializeAsync(_logger, CancellationToken.None);

		WorkspaceMaterializerSettings settings = await _workspace.GetSettingsAsync(CancellationToken.None);
		WorkspaceMaterializerSettings autoSdkSettings = await _autoSdkWorkspace.GetSettingsAsync(CancellationToken.None);
		IReadOnlyDictionary<string, string> envVars = executor.GetEnvVars();
		Assert.AreEqual("1", envVars["IsBuildMachine"]);
		Assert.AreEqual(settings.DirectoryPath.FullName, envVars["uebp_LOCAL_ROOT"]);
		Assert.AreEqual(batch.StreamName, envVars["uebp_BuildRoot_P4"]);
		Assert.AreEqual("++UE5+Main", envVars["uebp_BuildRoot_Escaped"]);
		Assert.AreEqual("1", envVars["uebp_CL"]);
		Assert.AreEqual("0", envVars["uebp_CodeCL"]);
		Assert.AreEqual(autoSdkSettings.DirectoryPath.FullName, envVars["UE_SDKS_ROOT"]);

		await executor.FinalizeAsync(_logger, CancellationToken.None);
	}

	[TestMethod]
	public async Task JobWithPreflightAsync()
	{
		JobId preflightJobId = JobId.Parse("65bd0655591b5d5d7d047b59");

		_server.AddJob(preflightJobId, _streamId, 1, 1000);
		_workspace.SetFile(1000, "New/Feature/Foo.cs", "foo");

		BeginBatchResponse batch = new BeginBatchResponse { Change = 1, PreflightChange = 1000 };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, preflightJobId, _batchId, batch, null!, new JobOptions());
		using WorkspaceExecutor executor = new(executorOptions, _workspace, null, NullLogger.Instance);

		await executor.InitializeAsync(_logger, CancellationToken.None);
		AssertWorkspaceFile(_workspace, "main.cpp", "main");
		AssertWorkspaceFile(_workspace, "foo/bar/baz.h", "baz");
		AssertWorkspaceFile(_workspace, "New/Feature/Foo.cs", "foo");
		await executor.FinalizeAsync(_logger, CancellationToken.None);
	}

	[TestMethod]
	public async Task JobWithNoChangeAsync()
	{
		JobId noChangeJobId = JobId.Parse("65bd0655591b5d5d7d047b5a");

		_server.AddJob(noChangeJobId, _streamId, 0, 0);
		BeginBatchResponse batch = new BeginBatchResponse { };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, noChangeJobId, _batchId, batch, null!, new JobOptions());
		using WorkspaceExecutor executor = new(executorOptions, _workspace, null, NullLogger.Instance);
		await Assert.ThrowsExceptionAsync<WorkspaceMaterializationException>(() => executor.InitializeAsync(_logger, CancellationToken.None));
	}

	private static void AssertWorkspaceFile(IWorkspaceMaterializer workspace, string relativePath, string expectedContent)
	{
		DirectoryReference workspaceDir = workspace.GetSettingsAsync(CancellationToken.None).Result.DirectoryPath;
		Assert.AreEqual(expectedContent, File.ReadAllText(Path.Join(workspaceDir.FullName, relativePath)));
	}
}
