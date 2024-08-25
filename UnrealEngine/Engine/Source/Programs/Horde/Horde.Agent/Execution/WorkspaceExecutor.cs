// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.RegularExpressions;
using EpicGames.Core;
using Horde.Agent.Utility;
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

		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				_workspace.Dispose();
				_autoSdkWorkspace?.Dispose();
			}

			base.Dispose(disposing);
		}

		public override async Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			await base.InitializeAsync(logger, cancellationToken);

			if (Batch.Change == 0)
			{
				throw new WorkspaceMaterializationException("Jobs with an empty change number are not supported");
			}

			// Setup and sync the AutoSDK workspace
			WorkspaceMaterializerSettings? autoSdkWorkspaceSettings = null;
			if (_autoSdkWorkspace != null)
			{
				using IScope _ = GlobalTracer.Instance.BuildSpan("Workspace").WithResourceName("AutoSDK").StartActive();
				// TODO: Set type of workspace materializer as scope tag.

				autoSdkWorkspaceSettings = await _autoSdkWorkspace.InitializeAsync(logger, cancellationToken);

				SyncOptions syncOptions = new();
				await _autoSdkWorkspace.SyncAsync(IWorkspaceMaterializer.LatestChangeNumber, -1, syncOptions, cancellationToken);
			}

			// Sync the regular workspace
			WorkspaceMaterializerSettings workspaceSettings;
			using (IScope scope = GlobalTracer.Instance.BuildSpan("Workspace").StartActive())
			{
				workspaceSettings = await _workspace.InitializeAsync(logger, cancellationToken);
				scope.Span.SetTag(Datadog.Trace.OpenTracing.DatadogTags.ResourceName, workspaceSettings.Identifier);

				int preflightChange = (Batch.ClonedPreflightChange != 0) ? Batch.ClonedPreflightChange : Batch.PreflightChange;
				await _workspace.SyncAsync(Batch.Change, preflightChange, new SyncOptions(), cancellationToken);

				// TODO: Purging of cache for ManagedWorkspace did happen here in WorkspaceInfo

				DeleteCachedBuildGraphManifests(workspaceSettings.DirectoryPath, logger);
			}

			// Remove all the local settings directories
			PerforceExecutor.DeleteEngineUserSettings(logger);

			// Get the temp storage directory
			if (!String.IsNullOrEmpty(Batch.TempStorageDir))
			{
				string escapedStreamName = Regex.Replace(Batch.StreamName, "[^a-zA-Z0-9_-]", "+");
				_sharedStorageDir = DirectoryReference.Combine(new DirectoryReference(Batch.TempStorageDir), escapedStreamName, $"CL {Batch.Change} - Job {JobId}");
				CopyAutomationTool(_sharedStorageDir, workspaceSettings.DirectoryPath, logger);
			}

			// Set any non-materializer specific environment variables for jobs
			_envVars["IsBuildMachine"] = "1";
			_envVars["uebp_LOCAL_ROOT"] = workspaceSettings.DirectoryPath.FullName;
			_envVars["uebp_BuildRoot_P4"] = Batch.StreamName;
			_envVars["uebp_BuildRoot_Escaped"] = Batch.StreamName.Replace('/', '+');
			_envVars["uebp_CL"] = Batch.Change.ToString();
			_envVars["uebp_CodeCL"] = Batch.CodeChange.ToString();

			WorkspaceMaterializerSettings settings = await _workspace.GetSettingsAsync(cancellationToken);
			foreach ((string key, string value) in settings.EnvironmentVariables)
			{
				_envVars[key] = value;
			}

			if (autoSdkWorkspaceSettings != null)
			{
				_envVars["UE_SDKS_ROOT"] = autoSdkWorkspaceSettings.DirectoryPath.FullName;
			}
		}

		/// <inheritdoc/>
		protected override async Task<bool> SetupAsync(JobStepInfo step, ILogger logger, CancellationToken cancellationToken)
		{
			// Loop back to JobExecutor's SetupAsync again, but with workspace and shared storage dir set
			WorkspaceMaterializerSettings settings = await _workspace.GetSettingsAsync(cancellationToken);
			DirectoryReference workspaceDir = settings.DirectoryPath;
			return await SetupAsync(step, workspaceDir, _sharedStorageDir, settings.IsPerforceWorkspace, GetLogger(settings, logger), cancellationToken);
		}

		/// <inheritdoc/>
		protected override async Task<bool> ExecuteAsync(JobStepInfo step, ILogger logger, CancellationToken cancellationToken)
		{
			// Loop back to JobExecutor's ExecuteAsync again, but with workspace and shared storage dir set
			WorkspaceMaterializerSettings settings = await _workspace.GetSettingsAsync(cancellationToken);
			DirectoryReference workspaceDir = settings.DirectoryPath;
			return await ExecuteAsync(step, workspaceDir, _sharedStorageDir, settings.IsPerforceWorkspace, GetLogger(settings, logger), cancellationToken);
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

		private ILogger GetLogger(WorkspaceMaterializerSettings settings, ILogger logger)
		{
			if (settings.IsPerforceWorkspace)
			{
				// Try resolve a PerforceLogger using assumptions about the materializer.
				// This is to remain compatible with PerforceExecutor.
				// These Perforce-specific references should ideally not exist in WorkspaceExecutor.
				WorkspaceInfo? workspaceInfo = (_workspace as ManagedWorkspaceMaterializer)?.GetWorkspaceInfo();
				WorkspaceInfo? autoSdkWorkspaceInfo = (_autoSdkWorkspace as ManagedWorkspaceMaterializer)?.GetWorkspaceInfo();

				if (workspaceInfo != null)
				{
					return PerforceExecutor.CreatePerforceLogger(logger, Batch.Change, workspaceInfo, autoSdkWorkspaceInfo);
				}
			}

			return logger;
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
			IWorkspaceMaterializer? workspaceMaterializer = null;
			IWorkspaceMaterializer? autoSdkMaterializer = null;
			try
			{
				WorkspaceMaterializerType type = GetMaterializerType(options.JobOptions.WorkspaceMaterializer, WorkspaceMaterializerType.ManagedWorkspace);

				workspaceMaterializer = _materializerFactory.CreateMaterializer(type, workspaceInfo, options);
				if (autoSdkWorkspaceInfo != null)
				{
					autoSdkMaterializer = _materializerFactory.CreateMaterializer(type, autoSdkWorkspaceInfo, options, forAutoSdk: true);
				}

				return new WorkspaceExecutor(options, workspaceMaterializer, autoSdkMaterializer, _loggerFactory.CreateLogger<WorkspaceExecutor>());
			}
			catch
			{
				autoSdkMaterializer?.Dispose();
				workspaceMaterializer?.Dispose();
				throw;
			}
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
