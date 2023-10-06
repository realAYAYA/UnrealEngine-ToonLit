// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Execution;
using Horde.Agent.Services;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests.Execution;

[TestClass]
public sealed class WorkspaceExecutorTest : IDisposable
{
	private const string StreamId = "foo-main";
	private const string JobId = "job1";
	private const string AgentType = "bogusAgentType";
	
	private readonly ILogger<WorkspaceExecutorTest> _logger;
	private readonly FakeHordeRpcServer _server = new();
	private readonly ISession _session;
	private readonly FakeWorkspaceMaterializer _autoSdkWorkspace = new ();
	private readonly FakeWorkspaceMaterializer _workspace = new ();
	private readonly WorkspaceExecutor _executor;
	
	public WorkspaceExecutorTest()
	{
		using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
		{
			builder.SetMinimumLevel(LogLevel.Debug);
			builder.AddSimpleConsole(options => { options.SingleLine = true; });
		});

		_logger = loggerFactory.CreateLogger<WorkspaceExecutorTest>();
		
		_server.AddStream(StreamId, "//Foo/Main");
		_server.AddAgentType(StreamId, AgentType);
		_server.AddJob(JobId, StreamId, 1, 0);
		_session = FakeServerSessionFactory.CreateSession(_server.GetConnection());
		
		_autoSdkWorkspace.SetFile(1, "HostWin64/Android/base.h", "base");
		_workspace.SetFile(1, "main.cpp", "main");
		_workspace.SetFile(1, "foo/bar/baz.h", "baz");

		BeginBatchResponse batch = new BeginBatchResponse { Change = 1 };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, JobId, "batch1", batch, default, "", null!, new JobOptions());
		_executor = new (executorOptions, _workspace, null, NullLogger.Instance);
	}

	public void Dispose()
	{
		_server.DisposeAsync().AsTask().Wait();
		_session.DisposeAsync().AsTask().Wait();
	}
	
	[TestMethod]
	public async Task RegularWorkspace()
	{
		await _executor.InitializeAsync(_logger, CancellationToken.None);
		AssertWorkspaceFile(_workspace, "main.cpp", "main");
		AssertWorkspaceFile(_workspace, "foo/bar/baz.h", "baz");
		await _executor.FinalizeAsync(_logger, CancellationToken.None);
	}
	
	[TestMethod]
	public async Task RegularAndAutoSdkWorkspace()
	{
		BeginBatchResponse batch = new BeginBatchResponse { Change = 1 };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, JobId, "batch1", batch, default, "", null!, new JobOptions());
		WorkspaceExecutor executor = new (executorOptions, _workspace, _autoSdkWorkspace, NullLogger.Instance);

		await executor.InitializeAsync(_logger, CancellationToken.None);
		AssertWorkspaceFile(_autoSdkWorkspace, "HostWin64/Android/base.h", "base");
		AssertWorkspaceFile(_workspace, "main.cpp", "main");
		AssertWorkspaceFile(_workspace, "foo/bar/baz.h", "baz");
		await executor.FinalizeAsync(_logger, CancellationToken.None);
	}
	
	[TestMethod]
	public async Task EnvVars()
	{
		BeginBatchResponse batch = new BeginBatchResponse { Change = 1 };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, JobId, "batch1", batch, default, "", null!, new JobOptions());
		WorkspaceExecutor executor = new (executorOptions, _workspace, _autoSdkWorkspace, NullLogger.Instance);
		await executor.InitializeAsync(_logger, CancellationToken.None);

		WorkspaceMaterializerSettings settings = await _workspace.GetSettingsAsync(CancellationToken.None);
		WorkspaceMaterializerSettings autoSdkSettings = await _autoSdkWorkspace.GetSettingsAsync(CancellationToken.None);
		IReadOnlyDictionary<string, string> envVars = executor.GetEnvVars();
		Assert.AreEqual("1", envVars["IsBuildMachine"]);
		Assert.AreEqual(settings.DirectoryPath.FullName, envVars["uebp_LOCAL_ROOT"]);
		Assert.AreEqual(settings.StreamRoot, envVars["uebp_BuildRoot_P4"]);
		Assert.AreEqual(settings.StreamRoot, envVars["uebp_BuildRoot_Escaped"]);
		Assert.AreEqual("1", envVars["uebp_CL"]);
		Assert.AreEqual("0", envVars["uebp_CodeCL"]);
		Assert.AreEqual(autoSdkSettings.DirectoryPath.FullName, envVars["UE_SDKS_ROOT"]);
		
		await executor.FinalizeAsync(_logger, CancellationToken.None);
	}

	[TestMethod]
	public async Task JobWithPreflight()
	{
		_server.AddJob("jobPreflight", StreamId, 1, 1000);		
		_workspace.SetFile(1000, "New/Feature/Foo.cs", "foo");

		BeginBatchResponse batch = new BeginBatchResponse { Change = 1, PreflightChange = 1000 };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, "jobPreflight", "batch1", batch, default, "", null!, new JobOptions());
		WorkspaceExecutor executor = new (executorOptions, _workspace, null, NullLogger.Instance);
		
		await executor.InitializeAsync(_logger, CancellationToken.None);
		AssertWorkspaceFile(_workspace, "main.cpp", "main");
		AssertWorkspaceFile(_workspace, "foo/bar/baz.h", "baz");
		AssertWorkspaceFile(_workspace, "New/Feature/Foo.cs", "foo");
		await executor.FinalizeAsync(_logger, CancellationToken.None);
	}
	
	[TestMethod]
	public async Task JobWithNoChange()
	{
		_server.AddJob("jobNoChange", StreamId, 0, 0);
		BeginBatchResponse batch = new BeginBatchResponse { };
		JobExecutorOptions executorOptions = new JobExecutorOptions(_session, null!, "jobNoChange", "batch1", batch, default, "", null!, new JobOptions());
		WorkspaceExecutor executor = new (executorOptions, _workspace, null, NullLogger.Instance);
		await Assert.ThrowsExceptionAsync<WorkspaceMaterializationException>(() => executor.InitializeAsync(_logger, CancellationToken.None));
	}

	private static void AssertWorkspaceFile(IWorkspaceMaterializer workspace, string relativePath, string expectedContent)
	{
		DirectoryReference workspaceDir = workspace.GetSettingsAsync(CancellationToken.None).Result.DirectoryPath;
		Assert.AreEqual(expectedContent, File.ReadAllText(Path.Join(workspaceDir.FullName, relativePath)));
	}
}
