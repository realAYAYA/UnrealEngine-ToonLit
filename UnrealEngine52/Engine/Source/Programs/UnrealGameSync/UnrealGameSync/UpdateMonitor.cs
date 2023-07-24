// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	enum UpdateType
	{
		Background,
		UserInitiated,
	}

	class UpdateMonitor : IDisposable
	{
		Task? _workerTask;
		CancellationTokenSource _cancellationSource = new CancellationTokenSource();
		ILogger _logger;
		IAsyncDisposer _asyncDisposer;

		public Action<UpdateType>? OnUpdateAvailable;

		public bool? RelaunchPreview
		{
			get;
			private set;
		}

		public UpdateMonitor(IPerforceSettings perforceSettings, string? watchPath, IServiceProvider serviceProvider)
		{
			this._logger = serviceProvider.GetRequiredService<ILogger<UpdateMonitor>>();
			this._asyncDisposer = serviceProvider.GetRequiredService<IAsyncDisposer>();

			if(watchPath != null)
			{
				_logger.LogInformation("Watching for updates on {WatchPath}", watchPath);
				_workerTask = Task.Run(() => PollForUpdatesAsync(perforceSettings, watchPath, _cancellationSource.Token));
			}
		}

		public void Dispose()
		{
			OnUpdateAvailable = null;

			if (_workerTask != null)
			{
				_cancellationSource.Cancel();
				_asyncDisposer.Add(_workerTask.ContinueWith(_ => _cancellationSource.Dispose()));
				_workerTask = null;
			}
		}

		public bool IsUpdateAvailable
		{
			get;
			private set;
		}

		async Task PollForUpdatesAsync(IPerforceSettings perforceSettings, string watchPath, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				await Task.Delay(TimeSpan.FromMinutes(5.0), cancellationToken);

				IPerforceConnection? perforce = null;
				try
				{
					perforce = await PerforceConnection.CreateAsync(perforceSettings, _logger);

					PerforceResponseList<ChangesRecord> changes = await perforce.TryGetChangesAsync(ChangesOptions.None, -1, ChangeStatus.Submitted, watchPath, cancellationToken);
					if (changes.Succeeded && changes.Data.Count > 0)
					{
						TriggerUpdate(UpdateType.Background, null);
					}
				}
				catch (PerforceException ex)
				{
					_logger.LogInformation(ex, "Perforce exception while attempting to poll for updates.");
				}
				catch (Exception ex)
				{
					_logger.LogWarning(ex, "Exception while attempting to poll for updates.");
					Program.CaptureException(ex);
				}
				finally
				{
					perforce?.Dispose();
				}
			}
		}

		public void TriggerUpdate(UpdateType updateType, bool? relaunchPreview)
		{
			this.RelaunchPreview = relaunchPreview;
			IsUpdateAvailable = true;
			if(OnUpdateAvailable != null)
			{
				OnUpdateAvailable(updateType);
			}
		}
	}
}
