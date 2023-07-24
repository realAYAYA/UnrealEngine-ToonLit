// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Services;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Execution
{
	class WorkspaceExecutor : JobExecutor
	{
		public const string Name = "Workspace";

		private DirectoryReference? _sharedStorageDir;
		private readonly IWorkspaceMaterializer? _autoSdkWorkspace;
		private readonly IWorkspaceMaterializer _workspace;

		public WorkspaceExecutor(ISession session, string jobId, string batchId, string agentTypeName, IWorkspaceMaterializer? autoSdkWorkspace, IWorkspaceMaterializer workspace, IHttpClientFactory httpClientFactory, ILogger logger)
			: base(session, jobId, batchId, agentTypeName, httpClientFactory, logger)
		{
			_autoSdkWorkspace = autoSdkWorkspace;
			_workspace = workspace;
		}

		public override async Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			await base.InitializeAsync(logger, cancellationToken);
			
			if (_job.Change == 0)
			{
				throw new WorkspaceMaterializationException("Jobs with an empty change number are not supported");
			}

			// Setup and sync the AutoSDK workspace
			WorkspaceMaterializerSettings? autoSdkWorkspaceSettings = null;
			if (_autoSdkWorkspace != null)
			{
				using IScope _ = GlobalTracer.Instance.BuildSpan("Workspace").WithResourceName("AutoSDK").StartActive();
				// TODO: Set type of workspace materializer as scope tag.

				autoSdkWorkspaceSettings = await _autoSdkWorkspace.InitializeAsync(cancellationToken);

				// Match change for AutoSDK and actual job change
				int autoSdkChangeNumber = _job.Change;

				SyncOptions syncOptions = new();
				await _autoSdkWorkspace.SyncAsync(autoSdkChangeNumber, syncOptions, cancellationToken);
			}
			
			// Sync the regular workspace
			WorkspaceMaterializerSettings workspaceSettings;
			using (IScope scope = GlobalTracer.Instance.BuildSpan("Workspace").StartActive())
			{
				workspaceSettings = await _workspace.InitializeAsync(cancellationToken);
				scope.Span.SetTag(Datadog.Trace.OpenTracing.DatadogTags.ResourceName, workspaceSettings.Identifier);
				
				int preflightChange = (_job.ClonedPreflightChange != 0) ? _job.ClonedPreflightChange : _job.PreflightChange;
				await _workspace.SyncAsync(_job.Change, new SyncOptions(), cancellationToken);
				
				// TODO: Purging of cache for ManagedWorkspace did happen here in WorkspaceInfo
				
				// Any shelved CL to apply on top of already synced files
				if (preflightChange > 0)
				{
					await _workspace.UnshelveAsync(preflightChange, cancellationToken);
				}

				DeleteCachedBuildGraphManifests(workspaceSettings.DirectoryPath, logger);
			}

			// Remove all the local settings directories
			PerforceExecutor.DeleteEngineUserSettings(logger);

			// Get the temp storage directory
			if (!String.IsNullOrEmpty(_agentType!.TempStorageDir))
			{
				string escapedStreamName = Regex.Replace(_stream!.Name, "[^a-zA-Z0-9_-]", "+");
				_sharedStorageDir = DirectoryReference.Combine(new DirectoryReference(_agentType!.TempStorageDir), escapedStreamName, $"CL {_job!.Change} - Job {_jobId}");
				CopyAutomationTool(_sharedStorageDir, workspaceSettings.DirectoryPath, logger);
			}

			// Set any non-materializer specific environment variables for jobs
			_envVars["IsBuildMachine"] = "1";
			_envVars["uebp_LOCAL_ROOT"] = workspaceSettings.DirectoryPath.FullName;
			_envVars["uebp_BuildRoot_P4"] = workspaceSettings.StreamRoot;
			_envVars["uebp_BuildRoot_Escaped"] = workspaceSettings.StreamRoot.Replace('/', '+');
			_envVars["uebp_CL"] = _job!.Change.ToString();
			_envVars["uebp_CodeCL"] = _job!.CodeChange.ToString();

			if (autoSdkWorkspaceSettings != null)
			{
				_envVars["UE_SDKS_ROOT"] = autoSdkWorkspaceSettings.DirectoryPath.FullName;
			}
		}
		
		/// <inheritdoc/>
		protected override async Task<bool> SetupAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			// Loop back to JobExecutor's SetupAsync again, but with workspace and shared storage dir set
			DirectoryReference workspaceDir = (await _workspace.GetSettingsAsync(cancellationToken)).DirectoryPath;
			return await SetupAsync(step, workspaceDir, _sharedStorageDir, logger, cancellationToken);
		}

		/// <inheritdoc/>
		protected override async Task<bool> ExecuteAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			// Loop back to JobExecutor's ExecuteAsync again, but with workspace and shared storage dir set
			DirectoryReference workspaceDir = (await _workspace.GetSettingsAsync(cancellationToken)).DirectoryPath;
			return await ExecuteAsync(step, workspaceDir, _sharedStorageDir, logger, cancellationToken);
		}

		/// <inheritdoc/>
		public override async Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			if (_autoSdkWorkspace != null)
			{
				await _autoSdkWorkspace.FinalizeAsync(cancellationToken);
			}
			
			await _workspace.FinalizeAsync(cancellationToken);
		}
	}

	class WorkspaceExecutorFactory : JobExecutorFactory
	{
		readonly IHttpClientFactory _httpClientFactory;

		public override string Name => WorkspaceExecutor.Name;

		public WorkspaceExecutorFactory(IHttpClientFactory httpClientFactory)
		{
			_httpClientFactory = httpClientFactory;
		}

		public override JobExecutor CreateExecutor(ISession session, ExecuteJobTask executeJobTask, BeginBatchResponse beginBatchResponse)
		{
			throw new NotImplementedException("WorkspaceExecutor cannot be instantiated as there are no IWorkspaceMaterializer implementations yet");
		}
	}
}
