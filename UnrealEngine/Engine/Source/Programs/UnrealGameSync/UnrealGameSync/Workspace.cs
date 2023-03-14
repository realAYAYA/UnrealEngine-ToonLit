// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	public sealed class Workspace : IDisposable
	{
		public IPerforceSettings PerforceSettings { get; }
		public UserWorkspaceState State { get; }
		public ProjectInfo Project { get; }
		public SynchronizationContext SynchronizationContext { get; }
		ILogger _logger;

		bool Syncing => _currentUpdate != null;

		WorkspaceUpdate? _currentUpdate;

		public event Action<WorkspaceUpdateContext, WorkspaceUpdateResult, string>? OnUpdateComplete;

		IAsyncDisposer _asyncDisposer;

		public Workspace(IPerforceSettings inPerfoceSettings, ProjectInfo inProject, UserWorkspaceState inState, ConfigFile projectConfigFile, IReadOnlyList<string>? projectStreamFilter, ILogger logger, IServiceProvider serviceProvider)
		{
			PerforceSettings = inPerfoceSettings;
			Project = inProject;
			State = inState;
			this.SynchronizationContext = SynchronizationContext.Current!;
			this._logger = logger;
			this._asyncDisposer = serviceProvider.GetRequiredService<IAsyncDisposer>();

			this.ProjectConfigFile = projectConfigFile;
			this.ProjectStreamFilter = projectStreamFilter;
		}

		public void Dispose()
		{
			CancelUpdate();
			if (_prevUpdateTask != null)
			{
				_asyncDisposer.Add(_prevUpdateTask);
			}
		}

		public ConfigFile ProjectConfigFile
		{
			get; private set;
		}

		public IReadOnlyList<string>? ProjectStreamFilter
		{
			get; private set;
		}

		CancellationTokenSource? _prevCancellationSource;
		Task _prevUpdateTask = Task.CompletedTask;

		public void Update(WorkspaceUpdateContext context)
		{
			CancelUpdate();

			Task prevUpdateTaskCopy = _prevUpdateTask;

			WorkspaceUpdate update = new WorkspaceUpdate(context);
			_currentUpdate = update;

			CancellationTokenSource cancellationSource = new CancellationTokenSource();
			_prevCancellationSource = cancellationSource;
			_prevUpdateTask = Task.Run(() => UpdateWorkspaceMini(update, prevUpdateTaskCopy, cancellationSource.Token));
		}

		public void CancelUpdate()
		{
			// Cancel the current task. We actually terminate the operation asynchronously, but we can signal the cancellation and 
			// send a cancelled event, then wait for the heavy lifting to finish in the new update task.
			if (_prevCancellationSource != null)
			{
				CancellationTokenSource prevCancellationSourceCopy = _prevCancellationSource;
				prevCancellationSourceCopy.Cancel();
				_prevUpdateTask = _prevUpdateTask.ContinueWith(task => prevCancellationSourceCopy.Dispose());
				_prevCancellationSource = null;
			}
			if(_currentUpdate != null)
			{
				CompleteUpdate(_currentUpdate, WorkspaceUpdateResult.Canceled, "Cancelled");
			}
		}

		async Task UpdateWorkspaceMini(WorkspaceUpdate update, Task prevUpdateTask, CancellationToken cancellationToken)
		{
			if (prevUpdateTask != null)
			{
				await prevUpdateTask;
			}

			WorkspaceUpdateContext context = update.Context;
			context.ProjectConfigFile = ProjectConfigFile;
			context.ProjectStreamFilter = ProjectStreamFilter;

			string statusMessage;
			WorkspaceUpdateResult result = WorkspaceUpdateResult.FailedToSync;

			try
			{
				(result, statusMessage) = await update.ExecuteAsync(PerforceSettings, Project, State, _logger, cancellationToken);
				if (result != WorkspaceUpdateResult.Success)
				{
					_logger.LogError("{Message}", statusMessage);
				}
			}
			catch (OperationCanceledException)
			{
				statusMessage = "Canceled.";
				_logger.LogError("Canceled.");
			}
			catch (Exception ex)
			{
				statusMessage = "Failed with exception - " + ex.ToString();
				_logger.LogError(ex, "Failed with exception");
			}

			ProjectConfigFile = context.ProjectConfigFile;
			ProjectStreamFilter = context.ProjectStreamFilter;

			SynchronizationContext.Post(x => CompleteUpdate(update, result, statusMessage), null);
		}

		void CompleteUpdate(WorkspaceUpdate update, WorkspaceUpdateResult result, string statusMessage)
		{
			if (_currentUpdate == update)
			{
				WorkspaceUpdateContext context = update.Context;

				State.SetLastSyncState(result, context, statusMessage);
				State.Save(_logger);

				OnUpdateComplete?.Invoke(context, result, statusMessage);
				_currentUpdate = null;
			}
		}

		public Dictionary<string, string> GetVariables(BuildConfig editorConfig, int? overrideChange = null, int? overrideCodeChange = null)
		{
			FileReference editorReceiptFile = ConfigUtils.GetEditorReceiptFile(Project, ProjectConfigFile, editorConfig);

			TargetReceipt? editorReceipt;
			if (!ConfigUtils.TryReadEditorReceipt(Project, editorReceiptFile, out editorReceipt))
			{
				editorReceipt = ConfigUtils.CreateDefaultEditorReceipt(Project, ProjectConfigFile, editorConfig);
			}

			Dictionary<string, string> variables = ConfigUtils.GetWorkspaceVariables(Project, overrideChange ?? State.CurrentChangeNumber, overrideCodeChange ?? State.CurrentCodeChangeNumber, editorReceipt, ProjectConfigFile);
			return variables;
		}

		public bool IsBusy()
		{
			return Syncing;
		}

		public int CurrentChangeNumber => State.CurrentChangeNumber;

		public int PendingChangeNumber => _currentUpdate?.Context?.ChangeNumber ?? CurrentChangeNumber;

		public string ClientName
		{
			get { return PerforceSettings.ClientName!; }
		}

		public Tuple<string, float> CurrentProgress => _currentUpdate?.CurrentProgress ?? new Tuple<string, float>("", 0.0f);
	}
}
