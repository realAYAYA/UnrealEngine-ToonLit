// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Utility;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Execution;

/// <summary>
/// Workspace materializer wrapping ManagedWorkspace and WorkspaceInfo
/// </summary>
public class ManagedWorkspaceMaterializer : IWorkspaceMaterializer
{
	private readonly AgentWorkspace _agentWorkspace;
	private readonly DirectoryReference _workingDir;
	private readonly bool _useSyncMarker;
	private readonly bool _useCacheFile;
	private readonly ILogger<ManagedWorkspaceMaterializer> _logger;
	private WorkspaceInfo? _workspace;
	
	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="agentWorkspace">Workspace configuration</param>
	/// <param name="workingDir">Where to put synced Perforce files and any cached data/metadata</param>
	/// <param name="useSyncMarker">Whether to use a sync marker for identifying last synced change number</param>
	/// <param name="useCacheFile">Whether to use a cache file during syncs</param>
	/// <param name="logger">Logger</param>
	public ManagedWorkspaceMaterializer(
		AgentWorkspace agentWorkspace,
		DirectoryReference workingDir,
		bool useSyncMarker,
		bool useCacheFile,
		ILogger<ManagedWorkspaceMaterializer> logger)
	{
		_agentWorkspace = agentWorkspace;
		_workingDir = workingDir;
		_useSyncMarker = useSyncMarker;
		_useCacheFile = useCacheFile;
		_logger = logger;
	}

	/// <inheritdoc/>
	public async Task<WorkspaceMaterializerSettings> InitializeAsync(CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.InitializeAsync");
		
		bool useHaveTable = WorkspaceInfo.ShouldUseHaveTable(_agentWorkspace.Method);
		_workspace = await WorkspaceInfo.SetupWorkspaceAsync(_agentWorkspace, _workingDir, useHaveTable, _logger, cancellationToken);
		return await GetSettingsAsync(cancellationToken);
	}

	/// <inheritdoc/>
	public async Task FinalizeAsync(CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.FinalizeAsync");

		if (_workspace != null)
		{
			await _workspace.CleanAsync(cancellationToken);	
		}
	}

	/// <inheritdoc/>
	public Task<WorkspaceMaterializerSettings> GetSettingsAsync(CancellationToken cancellationToken)
	{
		// ManagedWorkspace store synced files in a sub-directory from the top working dir.
		DirectoryReference syncDir = DirectoryReference.Combine(_workingDir, _agentWorkspace.Identifier, "Sync");
		return Task.FromResult(new WorkspaceMaterializerSettings(syncDir, _agentWorkspace.Identifier, _agentWorkspace.Stream));
	}

	/// <inheritdoc/>
	public async Task SyncAsync(int changeNum, SyncOptions options, CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.SyncAsync");
		scope.Span.SetTag("ChangeNum", changeNum);
		scope.Span.SetTag("RemoveUntracked", options.RemoveUntracked);

		if (_workspace == null)
		{
			throw new WorkspaceMaterializationException("Workspace not initialized");
		}

		if (!IsAlreadySynced(changeNum))
		{
			FileReference? cacheFile = _useCacheFile ? FileReference.Combine(_workspace.MetadataDir, "Contents.dat") : null;
			await _workspace.SyncAsync(changeNum, 0, cacheFile, cancellationToken);
			MarkChangeNumSynced(changeNum);
		}
	}

	private (FileReference file, string fileContent) GetSyncMarker(int changeNum)
	{
		if (_workspace == null)
		{
			throw new WorkspaceMaterializationException("Workspace not initialized");
		}

		return (FileReference.Combine(_workspace.MetadataDir, "Synced.txt"), $"Synced to CL {changeNum}");
	}
	
	/// <summary>
	/// Check if workspace is already synced to given change number
	/// </summary>
	/// <param name="changeNum">Change number to check</param>
	/// <returns>True if already synced</returns>
	private bool IsAlreadySynced(int changeNum)
	{
		if (!_useSyncMarker) return false;

		(FileReference syncFile, string syncText) = GetSyncMarker(changeNum);
		if (!FileReference.Exists(syncFile) || FileReference.ReadAllText(syncFile) != syncText)
		{
			FileReference.Delete(syncFile);
			return false;
		}

		return true;
	}
	
	/// <summary>
	/// Mark a change number as synced with a file on disk
	/// </summary>
	/// <param name="changeNum">Change number to mark</param>
	/// <returns>True if already synced</returns>
	private void MarkChangeNumSynced(int changeNum)
	{
		if (!_useSyncMarker) return;
		
		(FileReference syncFile, string syncText) = GetSyncMarker(changeNum);
		FileReference.WriteAllText(syncFile, syncText);
	}

	/// <inheritdoc/>
	public async Task UnshelveAsync(int changeNum, CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.UnshelveAsync");
		scope.Span.SetTag("ChangeNum", changeNum);
		
		if (_workspace == null)
		{
			throw new WorkspaceMaterializationException("Workspace not initialized");
		}
		
		await _workspace.UnshelveAsync(changeNum, cancellationToken);
	}

	private IScope CreateTraceSpan(string operationName)
	{
		IScope scope = GlobalTracer.Instance.BuildSpan(operationName).WithResourceName(_agentWorkspace.Identifier).StartActive();
		scope.Span.SetTag("UseHaveTable", WorkspaceInfo.ShouldUseHaveTable(_agentWorkspace.Method));
		scope.Span.SetTag("Cluster", _agentWorkspace.Cluster);
		scope.Span.SetTag("Incremental", _agentWorkspace.Incremental);
		scope.Span.SetTag("Method", _agentWorkspace.Method);
		return scope;
	}
}