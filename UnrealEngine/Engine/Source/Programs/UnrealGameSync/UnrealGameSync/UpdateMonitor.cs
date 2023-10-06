// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using System;
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
#pragma warning disable CA2213 // warning CA2213: 'UpdateMonitor' contains field '_cancellationSource' that is of IDisposable type 'CancellationTokenSource', but it is never disposed. Change the Dispose method on 'UpdateMonitor' to call Close or Dispose on this field.
		readonly CancellationTokenSource _cancellationSource = new CancellationTokenSource();
#pragma warning restore CA2213
		readonly ILogger _logger;
		readonly IAsyncDisposer _asyncDisposer;

		public Action<UpdateType>? OnUpdateAvailable;

		public bool? RelaunchPreview
		{
			get;
			private set;
		}

		public UpdateMonitor(IPerforceSettings perforceSettings, string? watchPath, IServiceProvider serviceProvider)
		{
			_logger = serviceProvider.GetRequiredService<ILogger<UpdateMonitor>>();
			_asyncDisposer = serviceProvider.GetRequiredService<IAsyncDisposer>();

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
				_asyncDisposer.Add(_workerTask.ContinueWith(_ => _cancellationSource.Dispose(), TaskScheduler.Default));
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
			RelaunchPreview = relaunchPreview;
			IsUpdateAvailable = true;
			if(OnUpdateAvailable != null)
			{
				OnUpdateAvailable(updateType);
			}
		}
	}
}
