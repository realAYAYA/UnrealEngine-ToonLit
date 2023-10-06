// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Execution
{
	class WorkspaceExecutor : JobExecutor
	{
		public const string Name = "Workspace";

		private DirectoryReference? _sharedStorageDir;
		private readonly IWorkspaceMaterializer _workspace;
		private readonly IWorkspaceMaterializer? _autoSdkWorkspace;

		public WorkspaceExecutor(JobExecutorOptions options, IWorkspaceMaterializer workspace, IWorkspaceMaterializer? autoSdkWorkspace, ILogger logger)
			: base(options, logger)
		{
			_workspace = workspace;
			_autoSdkWorkspace = autoSdkWorkspace;
		}

		public override async Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			await base.InitializeAsync(logger, cancellationToken);
			
			if (_batch.Change == 0)
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
				int autoSdkChangeNumber = _batch.Change;

				SyncOptions syncOptions = new();
				await _autoSdkWorkspace.SyncAsync(autoSdkChangeNumber, syncOptions, cancellationToken);
			}
			
			// Sync the regular workspace
			WorkspaceMaterializerSettings workspaceSettings;
			using (IScope scope = GlobalTracer.Instance.BuildSpan("Workspace").StartActive())
			{
				workspaceSettings = await _workspace.InitializeAsync(cancellationToken);
				scope.Span.SetTag(Datadog.Trace.OpenTracing.DatadogTags.ResourceName, workspaceSettings.Identifier);
				
				int preflightChange = (_batch.ClonedPreflightChange != 0) ? _batch.ClonedPreflightChange : _batch.PreflightChange;
				await _workspace.SyncAsync(_batch.Change, new SyncOptions(), cancellationToken);
				
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
			if (!String.IsNullOrEmpty(_batch.TempStorageDir))
			{
				string escapedStreamName = Regex.Replace(_batch.StreamName, "[^a-zA-Z0-9_-]", "+");
				_sharedStorageDir = DirectoryReference.Combine(new DirectoryReference(_batch.TempStorageDir), escapedStreamName, $"CL {_batch.Change} - Job {_jobId}");
				CopyAutomationTool(_sharedStorageDir, workspaceSettings.DirectoryPath, logger);
			}

			// Set any non-materializer specific environment variables for jobs
			_envVars["IsBuildMachine"] = "1";
			_envVars["uebp_LOCAL_ROOT"] = workspaceSettings.DirectoryPath.FullName;
			_envVars["uebp_BuildRoot_P4"] = workspaceSettings.StreamRoot;
			_envVars["uebp_BuildRoot_Escaped"] = workspaceSettings.StreamRoot.Replace('/', '+');
			_envVars["uebp_CL"] = _batch.Change.ToString();
			_envVars["uebp_CodeCL"] = _batch.CodeChange.ToString();

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
			return await SetupAsync(step, workspaceDir, _sharedStorageDir, false, logger, cancellationToken);
		}

		/// <inheritdoc/>
		protected override async Task<bool> ExecuteAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			// Loop back to JobExecutor's ExecuteAsync again, but with workspace and shared storage dir set
			DirectoryReference workspaceDir = (await _workspace.GetSettingsAsync(cancellationToken)).DirectoryPath;
			return await ExecuteAsync(step, workspaceDir, _sharedStorageDir, false, logger, cancellationToken);
		}

		/// <inheritdoc/>
		public override async Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			DirectoryReference workspaceDir = (await _workspace.GetSettingsAsync(cancellationToken)).DirectoryPath;
			await ExecuteLeaseCleanupScriptAsync(workspaceDir, logger);
			await TerminateProcessesAsync(TerminateCondition.AfterBatch, logger);

			if (_autoSdkWorkspace != null)
			{
				await _autoSdkWorkspace.FinalizeAsync(cancellationToken);
			}
			
			await _workspace.FinalizeAsync(cancellationToken);
		}
	}

	class WorkspaceExecutorFactory : IJobExecutorFactory
	{
		private readonly IWorkspaceMaterializerFactory _materializerFactory;
		private readonly ILoggerFactory _loggerFactory;
		
		public string Name => WorkspaceExecutor.Name;

		public WorkspaceExecutorFactory(IWorkspaceMaterializerFactory materializerFactory, ILoggerFactory loggerFactory)
		{
			_materializerFactory = materializerFactory;
			_loggerFactory = loggerFactory;
		}

		public IJobExecutor CreateExecutor(AgentWorkspace workspaceInfo, AgentWorkspace? autoSdkWorkspaceInfo, JobExecutorOptions options)
		{
			WorkspaceMaterializerType type = GetMaterializerType(options.JobOptions.WorkspaceMaterializer, WorkspaceMaterializerType.ManagedWorkspace);
			IWorkspaceMaterializer workspaceMaterializer = _materializerFactory.CreateMaterializer(type, workspaceInfo, options);
			
			IWorkspaceMaterializer? autoSdkMaterializer = null;
			if (autoSdkWorkspaceInfo != null)
			{
				autoSdkMaterializer = _materializerFactory.CreateMaterializer(type, autoSdkWorkspaceInfo, options, forAutoSdk: true);
			}
			
			return new WorkspaceExecutor(options, workspaceMaterializer, autoSdkMaterializer, _loggerFactory.CreateLogger<WorkspaceExecutor>());
		}

		private static WorkspaceMaterializerType GetMaterializerType(string name, WorkspaceMaterializerType defaultValue)
		{
			if (String.IsNullOrEmpty(name))
			{
				return defaultValue;
			}
			
			if (Enum.TryParse(name, true, out WorkspaceMaterializerType enumType))
			{
				return enumType;
			}
			
			throw new ArgumentException($"Unable to find materializer type '{name}'");
		}
	}
}
