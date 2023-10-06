// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
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
	/// Constructor
	/// </summary>
	/// <param name="directoryPath"></param>
	/// <param name="identifier"></param>
	/// <param name="streamRoot"></param>
	public WorkspaceMaterializerSettings(DirectoryReference directoryPath, string identifier, string streamRoot)
	{
		DirectoryPath = directoryPath;
		Identifier = identifier;
		StreamRoot = streamRoot;
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
public interface IWorkspaceMaterializer
{
	/// <summary>
	/// Prepare file system for syncing
	/// </summary>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <returns>Async task</returns>
	public Task<WorkspaceMaterializerSettings> InitializeAsync(CancellationToken cancellationToken);
	
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
	/// <param name="options">Additional options</param>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <exception cref="Horde.Agent.Execution.WorkspaceMaterializationException">Thrown if syncing fails</exception>
	/// <returns>Async task</returns>
	public Task SyncAsync(int changeNum, SyncOptions options, CancellationToken cancellationToken);
	
	/// <summary>
	/// Materializes all files from a shelved changelist
	/// Any existing files will be clobbered. Usually run after a sync has been performed.
	/// </summary>
	/// <exception cref="Horde.Agent.Execution.WorkspaceMaterializationException">Thrown if unshelving fails</exception>
	/// <returns>Async task</returns>
	public Task UnshelveAsync(int changeNum, CancellationToken cancellationToken);
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
	private readonly ILoggerFactory _loggerFactory;
	
	public WorkspaceMaterializerFactory(ILoggerFactory loggerFactory)
	{
		_loggerFactory = loggerFactory;
	}
	
	/// <inheritdoc/>
	public IWorkspaceMaterializer CreateMaterializer(WorkspaceMaterializerType type, AgentWorkspace workspaceInfo, JobExecutorOptions options, bool forAutoSdk)
	{
		switch (type)
		{
			case WorkspaceMaterializerType.ManagedWorkspace:
				return forAutoSdk
					? new ManagedWorkspaceMaterializer(workspaceInfo, options.Session.WorkingDir, true, true, _loggerFactory.CreateLogger<ManagedWorkspaceMaterializer>())
					: new ManagedWorkspaceMaterializer(workspaceInfo, options.Session.WorkingDir, false, false, _loggerFactory.CreateLogger<ManagedWorkspaceMaterializer>());

			default:
				throw new Exception("Unhandled materializer option: " + type);
		};
	}
}
