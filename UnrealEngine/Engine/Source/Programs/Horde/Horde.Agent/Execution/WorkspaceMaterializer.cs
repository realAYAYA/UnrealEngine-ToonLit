// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Execution;

/// <summary>
/// Exception for workspace materializer
/// </summary>
public class WorkspaceMaterializationException : Exception
{
	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="message"></param>
	public WorkspaceMaterializationException(string? message) : base(message)
	{
	}

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="message"></param>
	/// <param name="innerException"></param>
	public WorkspaceMaterializationException(string? message, Exception? innerException) : base(message, innerException)
	{
	}
}

/// <summary>
/// Settings returned by SetupAsync
/// </summary>
public class WorkspaceMaterializerSettings
{
	/// <summary>
	/// Path to local file system directory where files from changelist are materialized
	/// </summary>
	public DirectoryReference DirectoryPath { get; }

	/// <summary>
	/// Identifier for this workspace
	/// </summary>
	public string Identifier { get; }

	/// <summary>
	/// Stream path inside Perforce
	/// </summary>
	public string StreamRoot { get; }

	/// <summary>
	/// Environment variables expected to be set for applications executing inside the workspace
	/// Mostly intended for Perforce-specific variables when <see cref="IsPerforceWorkspace" /> is set to true
	/// </summary>
	public IReadOnlyDictionary<string, string> EnvironmentVariables { get; }

	/// <summary>
	/// Whether the materialized workspace is a true Perforce workspace
	/// This flag is provided as a stop-gap solution to allow replacing ManagedWorkspace with WorkspaceMaterializer.
	/// It's *highly* recommended to set this to false for any new implementations of IWorkspaceMaterializer.
	/// </summary>
	public bool IsPerforceWorkspace { get; }

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="directoryPath"></param>
	/// <param name="identifier"></param>
	/// <param name="streamRoot"></param>
	/// <param name="envVars"></param>
	/// <param name="isPerforceWorkspace"></param>
	public WorkspaceMaterializerSettings(DirectoryReference directoryPath, string identifier, string streamRoot, IReadOnlyDictionary<string, string> envVars, bool isPerforceWorkspace)
	{
		DirectoryPath = directoryPath;
		Identifier = identifier;
		StreamRoot = streamRoot;
		EnvironmentVariables = envVars;
		IsPerforceWorkspace = isPerforceWorkspace;
	}
}

/// <summary>
/// Options passed to SyncAsync
/// </summary>
public class SyncOptions
{
	/// <summary>
	/// Remove any files not referenced by changelist
	/// </summary>
	public bool RemoveUntracked { get; set; }

	/// <summary>
	/// If true, skip syncing actual file data and instead create empty placeholder files.
	/// Used for testing.
	/// </summary>
	public bool FakeSync { get; set; }
}

/// <summary>
/// Interface for materializing a file tree to the local file system.
/// One instance roughly equals one Perforce stream.
/// </summary>
public interface IWorkspaceMaterializer : IDisposable
{
	/// <summary>
	/// Placeholder for resolving the latest available change number of stream during sync
	/// </summary>
	public const int LatestChangeNumber = -2;

	/// <summary>
	/// Prepare file system for syncing
	/// </summary>
	/// <param name="logger">Logger for output</param>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <returns>Async task</returns>
	public Task<WorkspaceMaterializerSettings> InitializeAsync(ILogger logger, CancellationToken cancellationToken);

	/// <summary>
	/// Finalize and clean file system
	/// </summary>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <returns>Async task</returns>
	public Task FinalizeAsync(CancellationToken cancellationToken);

	/// <summary>
	/// Get settings for workspace
	/// </summary>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <returns>Settings for workspace materializer</returns>
	public Task<WorkspaceMaterializerSettings> GetSettingsAsync(CancellationToken cancellationToken);

	/// <summary>
	/// Materialize (or sync) a Perforce stream at a given change number
	/// Once method has completed, file tree is available on disk.
	/// </summary>
	/// <param name="changeNum">Change number to materialize</param>
	/// <param name="preflightChangeNum">Preflight change number to add</param>
	/// <param name="options">Additional options</param>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <exception cref="Horde.Agent.Execution.WorkspaceMaterializationException">Thrown if syncing fails</exception>
	/// <returns>Async task</returns>
	public Task SyncAsync(int changeNum, int preflightChangeNum, SyncOptions options, CancellationToken cancellationToken);
}

enum WorkspaceMaterializerType
{
	ManagedWorkspace,
}

/// <summary>
/// Factory for creating new workspace materializers
/// </summary>
interface IWorkspaceMaterializerFactory
{
	/// <summary>
	/// Creates a new workspace materializer instance
	/// </summary>
	/// <param name="type">Type of materializer to instantiate</param>
	/// <param name="workspaceInfo">Agent workspace</param>
	/// <param name="options">Job options</param>
	/// <param name="forAutoSdk">Whether intended for AutoSDK materialization</param>
	/// <returns>A new workspace materializer instance</returns>
	IWorkspaceMaterializer CreateMaterializer(WorkspaceMaterializerType type, AgentWorkspace workspaceInfo, JobExecutorOptions options, bool forAutoSdk = false);
}

class WorkspaceMaterializerFactory : IWorkspaceMaterializerFactory
{
	/// <inheritdoc/>
	public IWorkspaceMaterializer CreateMaterializer(WorkspaceMaterializerType type, AgentWorkspace workspaceInfo, JobExecutorOptions options, bool forAutoSdk)
	{
		switch (type)
		{
			case WorkspaceMaterializerType.ManagedWorkspace:
				return forAutoSdk
					? new ManagedWorkspaceMaterializer(workspaceInfo, options.Session.WorkingDir, true)
					: new ManagedWorkspaceMaterializer(workspaceInfo, options.Session.WorkingDir, false);

			default:
				throw new Exception("Unhandled materializer option: " + type);
		}
	}
}
