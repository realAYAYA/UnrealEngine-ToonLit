// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce.Managed;
using Horde.Agent.Utility;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Execution;

/// <summary>
/// Workspace materializer wrapping ManagedWorkspace and WorkspaceInfo
/// </summary>
public sealed class ManagedWorkspaceMaterializer : IWorkspaceMaterializer
{
	private readonly AgentWorkspace _agentWorkspace;
	private readonly DirectoryReference _workingDir;
	private readonly bool _useCacheFile;
	private WorkspaceInfo? _workspace;

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="agentWorkspace">Workspace configuration</param>
	/// <param name="workingDir">Where to put synced Perforce files and any cached data/metadata</param>
	/// <param name="useCacheFile">Whether to use a cache file during syncs</param>
	public ManagedWorkspaceMaterializer(
		AgentWorkspace agentWorkspace,
		DirectoryReference workingDir,
		bool useCacheFile)
	{
		_agentWorkspace = agentWorkspace;
		_workingDir = workingDir;
		_useCacheFile = useCacheFile;
	}

	/// <inheritdoc/>
	public void Dispose()
	{
		_workspace?.Dispose();
	}

	/// <inheritdoc/>
	public async Task<WorkspaceMaterializerSettings> InitializeAsync(ILogger logger, CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.InitializeAsync");

		ManagedWorkspaceOptions options = WorkspaceInfo.GetMwOptions(_agentWorkspace);
		_workspace = await WorkspaceInfo.SetupWorkspaceAsync(_agentWorkspace, _workingDir, options, logger, cancellationToken);
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
		if (_workspace == null)
		{
			throw new WorkspaceMaterializationException("Workspace not initialized");
		}

		// ManagedWorkspace store synced files in a sub-directory from the top working dir.
		DirectoryReference syncDir = DirectoryReference.Combine(_workingDir, _agentWorkspace.Identifier, "Sync");

		// Variables expected to be set for UAT/BuildGraph when Perforce is enabled (-P4 flag is set) 
		Dictionary<string, string> envVars = new()
		{
			["uebp_PORT"] = _workspace.ServerAndPort,
			["uebp_USER"] = _workspace.UserName,
			["uebp_CLIENT"] = _workspace.ClientName,
			["uebp_CLIENT_ROOT"] = $"//{_workspace.ClientName}"
		};

		// Perforce-specific variables
		envVars["P4USER"] = _workspace.UserName;
		envVars["P4CLIENT"] = _workspace.ClientName;

		return Task.FromResult(new WorkspaceMaterializerSettings(syncDir, _agentWorkspace.Identifier, _agentWorkspace.Stream, envVars, true));
	}

	/// <inheritdoc/>
	public async Task SyncAsync(int changeNum, int preflightChangeNum, SyncOptions options, CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.SyncAsync");
		scope.Span.SetTag("ChangeNum", changeNum);
		scope.Span.SetTag("RemoveUntracked", options.RemoveUntracked);

		if (_workspace == null)
		{
			throw new WorkspaceMaterializationException("Workspace not initialized");
		}

		if (changeNum == IWorkspaceMaterializer.LatestChangeNumber)
		{
			int latestChangeNum = await _workspace.GetLatestChangeAsync(cancellationToken);
			scope.Span.SetTag("LatestChangeNum", latestChangeNum);
			changeNum = latestChangeNum;
		}

		FileReference cacheFile = FileReference.Combine(_workspace.MetadataDir, "Contents.dat");
		if (_useCacheFile)
		{
			bool isSyncedDataDirty = await _workspace.UpdateLocalCacheMarkerAsync(cacheFile, changeNum, preflightChangeNum);
			scope.Span.SetTag("IsSyncedDataDirty", isSyncedDataDirty);
			if (!isSyncedDataDirty)
			{
				return;
			}
		}
		else
		{
			WorkspaceInfo.RemoveLocalCacheMarker(cacheFile);
		}

		await _workspace.SyncAsync(changeNum, preflightChangeNum, cacheFile, cancellationToken);
	}

	/// <summary>
	/// Get info for Perforce workspace
	/// </summary>
	/// <returns>Workspace info</returns>
	public WorkspaceInfo? GetWorkspaceInfo()
	{
		return _workspace;
	}

	private IScope CreateTraceSpan(string operationName)
	{
		IScope scope = GlobalTracer.Instance.BuildSpan(operationName).WithResourceName(_agentWorkspace.Identifier).StartActive();
		scope.Span.SetTag("UseHaveTable", WorkspaceInfo.ShouldUseHaveTable(_agentWorkspace.Method));
		scope.Span.SetTag("Cluster", _agentWorkspace.Cluster);
		scope.Span.SetTag("Incremental", _agentWorkspace.Incremental);
		scope.Span.SetTag("Method", _agentWorkspace.Method);
		scope.Span.SetTag("UseCacheFile", _useCacheFile);
		return scope;
	}
}