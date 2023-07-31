// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis;
using EpicGames.Redis.Utility;
using EpicGames.Serialization;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Templates;
using Horde.Build.Perforce;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Driver;
using OpenTracing;
using OpenTracing.Util;
using StackExchange.Redis;
using TimeZoneConverter;

namespace Horde.Build.Jobs.Schedules
{
	using JobId = ObjectId<IJob>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

	/// <summary>
	/// Manipulates schedule instances
	/// </summary>
	public sealed class ScheduleService : IHostedService, IDisposable
	{
		[RedisConverter(typeof(RedisCbConverter<QueueItem>))]
		class QueueItem
		{
			[CbField("sid")]
			public StreamId StreamId { get; set; }

			[CbField("tid")]
			public TemplateRefId TemplateId { get; set; }

			public QueueItem()
			{
			}

			public QueueItem(StreamId streamId, TemplateRefId templateId)
			{
				StreamId = streamId;
				TemplateId = templateId;
			}

			public static DateTime GetTimeFromScore(double score) => DateTime.UnixEpoch + TimeSpan.FromSeconds(score);
			public static double GetScoreFromTime(DateTime time) => (time.ToUniversalTime() - DateTime.UnixEpoch).TotalSeconds;
		}

		class PerforceHistory
		{
			readonly IPerforceService _perforce;
			readonly string _clusterName;
			readonly string _streamName;
			int _maxResults;
			readonly List<ChangeDetails> _changes = new List<ChangeDetails>();
			int _nextIndex;

			public PerforceHistory(IPerforceService perforce, string clusterName, string streamName)
			{
				_perforce = perforce;
				_clusterName = clusterName;
				_streamName = streamName;
				_maxResults = 10;
			}

			async ValueTask<ChangeDetails?> GetChangeAtIndexAsync(int index)
			{
				while (index >= _changes.Count)
				{
					List<ChangeSummary> newChanges = await _perforce.GetChangesAsync(_clusterName, _streamName, null, null, _maxResults, null);

					int numResults = newChanges.Count;
					if (_changes.Count > 0)
					{
						newChanges.RemoveAll(x => x.Number >= _changes[^1].Number);
					}
					if (newChanges.Count == 0 && numResults < _maxResults)
					{
						return null;
					}
					if(newChanges.Count > 0)
					{
						_changes.AddRange((await _perforce.GetChangeDetailsAsync(_clusterName, _streamName, newChanges.ConvertAll(x => x.Number), null)).OrderByDescending(x => x.Number));
					}
					_maxResults += 10;
				}
				return _changes[index];
			}

			public async ValueTask<ChangeDetails?> GetNextChangeAsync()
			{
				ChangeDetails? details = await GetChangeAtIndexAsync(_nextIndex);
				if (details != null)
				{
					_nextIndex++;
				}
				return details;
			}

			public async ValueTask<int> GetCodeChange(int change)
			{
				int index = _changes.BinarySearch(x => -x.Number, -change);
				if (index < 0)
				{
					index = ~index;
				}

				for (; ; )
				{
					ChangeDetails? details = await GetChangeAtIndexAsync(index);
					if (details == null)
					{
						return 0;
					}
					if ((details.GetContentFlags() & ChangeContentFlags.ContainsCode) != 0)
					{
						return details.Number;
					}
					index++;
				}
			}
		}

		readonly IGraphCollection _graphs;
		readonly IPerforceService _perforce;
		readonly IJobCollection _jobCollection;
		readonly JobService _jobService;
		readonly StreamService _streamService;
		readonly ITemplateCollection _templateCollection;
		readonly TimeZoneInfo _timeZone;
		readonly IClock _clock;
		readonly ILogger _logger;
		readonly RedisService _redis;
		readonly RedisKey _baseLockKey;
		readonly RedisKey _tickLockKey; // Lock to tick the queue
		readonly RedisSortedSet<QueueItem> _queue; // Items to tick, ordered by time
		readonly ITicker _ticker;

		/// <summary>
		/// Constructor
		/// </summary>
		public ScheduleService(RedisService redis, IGraphCollection graphs, IPerforceService perforce, IJobCollection jobCollection, JobService jobService, StreamService streamService, ITemplateCollection templateCollection, MongoService mongoService, IClock clock, IOptionsMonitor<ServerSettings> settings, ILogger<ScheduleService> logger)
		{
			_graphs = graphs;
			_perforce = perforce;
			_jobCollection = jobCollection;
			_jobService = jobService;
			_streamService = streamService;
			_templateCollection = templateCollection;
			_clock = clock;

			string? scheduleTimeZone = settings.CurrentValue.ScheduleTimeZone;
			_timeZone = (scheduleTimeZone == null) ? TimeZoneInfo.Local : TZConvert.GetTimeZoneInfo(scheduleTimeZone);

			_logger = logger;

			_redis = redis;
			_baseLockKey = "scheduler/locks";
			_tickLockKey = _baseLockKey.Append("/tick");
			_queue = new RedisSortedSet<QueueItem>(redis.GetDatabase(), "scheduler/queue");
			if (mongoService.ReadOnlyMode)
			{
				_ticker = new NullTicker();
			}
			else
			{
				_ticker = clock.AddTicker<ScheduleService>(TimeSpan.FromMinutes(1.0), TickAsync, logger);
			}
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_ticker.Dispose();
		}

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			DateTime utcNow = _clock.UtcNow;

			// Update the current queue
			await using (RedisLock sharedLock = new (_redis.GetDatabase(), _tickLockKey))
			{
				if (await sharedLock.AcquireAsync(TimeSpan.FromMinutes(1.0), false))
				{
					await UpdateQueueAsync(utcNow);
				}
			}

			// Keep updating schedules
			while (!cancellationToken.IsCancellationRequested)
			{
				// Get the item with the lowest score (ie. the one that hasn't been updated in the longest time)
				QueueItem? item = await PopQueueItemAsync();
				if (item == null)
				{
					break;
				}

				// Acquire the lock for this schedule and update it
				await using (RedisLock sharedLock = new RedisLock<QueueItem>(_redis.GetDatabase(), _baseLockKey, item))
				{
					if (await sharedLock.AcquireAsync(TimeSpan.FromMinutes(1.0)))
					{
						try
						{
							await TriggerAsync(item.StreamId, item.TemplateId, utcNow, cancellationToken);
						}
						catch (OperationCanceledException)
						{
							throw;
						}
						catch (Exception ex)
						{
							_logger.LogError(ex, "Error while updating schedule for {StreamId}/{TemplateId}", item.StreamId, item.TemplateId);
						}
					}
				}
			}
		}

		async Task<QueueItem?> PopQueueItemAsync()
		{
			for (; ; )
			{
				QueueItem[] items = await _queue.RangeByRankAsync(0, 0);
				if (items.Length == 0)
				{
					return null;
				}
				if (await _queue.RemoveAsync(items[0]))
				{
					return items[0];
				}
			}
		}

		internal async Task ResetAsync()
		{
			IDatabase redis = _redis.GetDatabase();
			await redis.KeyDeleteAsync(_queue.Key);
			await redis.KeyDeleteAsync(_tickLockKey);
		}

		internal async Task TickForTestingAsync()
		{
			await UpdateQueueAsync(_clock.UtcNow);
			await TickAsync(CancellationToken.None);
		}

		/// <summary>
		/// Get the current set of streams and ensure there's an entry for each item
		/// </summary>
		public async Task UpdateQueueAsync(DateTime utcNow)
		{
			List<SortedSetEntry<QueueItem>> queueItems = new List<SortedSetEntry<QueueItem>>();

			List<IStream> streams = await _streamService.GetStreamsAsync();
			foreach (IStream stream in streams)
			{
				foreach ((TemplateRefId templateId, TemplateRef templateRef) in stream.Templates)
				{
					if (templateRef.Schedule != null)
					{
						DateTime? nextTriggerTimeUtc = templateRef.Schedule.GetNextTriggerTimeUtc(_timeZone);
						if (nextTriggerTimeUtc != null)
						{
							if (utcNow > nextTriggerTimeUtc.Value)
							{
								double score = QueueItem.GetScoreFromTime(nextTriggerTimeUtc.Value);
								queueItems.Add(new SortedSetEntry<QueueItem>(new QueueItem(stream.Id, templateId), score));

								await _streamService.UpdateScheduleTriggerAsync(stream, templateId, utcNow);
							}
						}
					}
				}
			}

			await _queue.AddAsync(queueItems.ToArray());
		}

		/// <summary>
		/// Trigger a schedule to run
		/// </summary>
		/// <param name="streamId">Stream for the schedule</param>
		/// <param name="templateId"></param>
		/// <param name="utcNow"></param>
		/// <param name="cancellationToken"></param>
		/// <returns>Async task</returns>
		internal async Task<bool> TriggerAsync(StreamId streamId, TemplateRefId templateId, DateTime utcNow, CancellationToken cancellationToken)
		{
			IStream? stream = await _streamService.GetStreamAsync(streamId);
			if (stream == null || !stream.Templates.TryGetValue(templateId, out TemplateRef? templateRef))
			{
				return false;
			}

			Schedule? schedule = templateRef.Schedule;
			if (schedule == null)
			{
				return false;
			}

			using IScope scope = GlobalTracer.Instance.BuildSpan("ScheduleService.TriggerAsync").StartActive();
			scope.Span.SetTag("StreamId", stream.Id);
			scope.Span.SetTag("TemplateId", templateId);

			Stopwatch stopwatch = Stopwatch.StartNew();
			_logger.LogInformation("Updating schedule for {StreamId} template {TemplateId}", stream.Id, templateId);

			// Get a list of jobs that we need to remove
			List<JobId> removeJobIds = new List<JobId>();
			foreach (JobId activeJobId in schedule.ActiveJobs)
			{
				IJob? job = await _jobService.GetJobAsync(activeJobId);
				if (job == null || job.Batches.All(x => x.State == JobStepBatchState.Complete))
				{
					_logger.LogInformation("Removing active job {JobId}", activeJobId);
					removeJobIds.Add(activeJobId);
				}
			}
			await _streamService.UpdateScheduleTriggerAsync(stream, templateId, removeJobs: removeJobIds);

			// If the stream is paused, bail out
			if (stream.IsPaused(utcNow))
			{
				_logger.LogDebug("Skipping schedule update for stream {StreamId}. It has been paused until {PausedUntil} with comment '{PauseComment}'.", stream.Id, stream.PausedUntil, stream.PauseComment);
				return false;
			}

			// Trigger this schedule
			try
			{
				await TriggerAsync(stream, templateId, templateRef, schedule, schedule.ActiveJobs.Count - removeJobIds.Count, utcNow, cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Failed to start schedule {StreamId}/{TemplateId}", stream.Id, templateId);
			}

			// Print some timing info
			stopwatch.Stop();
			_logger.LogInformation("Updated schedule for {StreamId} template {TemplateId} in {TimeSeconds}ms", stream.Id, templateId, (long)stopwatch.Elapsed.TotalMilliseconds);
			return true;
		}

		/// <summary>
		/// Trigger a schedule to run
		/// </summary>
		/// <param name="stream">Stream for the schedule</param>
		/// <param name="templateId"></param>
		/// <param name="templateRef"></param>
		/// <param name="schedule"></param>
		/// <param name="numActiveJobs"></param>
		/// <param name="utcNow">The current time</param>
		/// <param name="cancellationToken"></param>
		/// <returns>Async task</returns>
		private async Task TriggerAsync(IStream stream, TemplateRefId templateId, TemplateRef templateRef, Schedule schedule, int numActiveJobs, DateTime utcNow, CancellationToken cancellationToken)
		{
			// Check we're not already at the maximum number of allowed jobs
			if (schedule.MaxActive != 0 && numActiveJobs >= schedule.MaxActive)
			{
				_logger.LogInformation("Skipping trigger of {StreamId} template {TemplateId} - already have maximum number of jobs running ({NumJobs})", stream.Id, templateId, schedule.MaxActive);
				foreach (JobId jobId in schedule.ActiveJobs)
				{
					_logger.LogInformation("Active job for {StreamId} template {TemplateId}: {JobId}", stream.Id, templateId, jobId);
				}
				return;
			}

			// Minimum changelist number, inclusive
			int minChangeNumber = schedule.LastTriggerChange;
			if (minChangeNumber > 0 && !schedule.RequireSubmittedChange)
			{
				minChangeNumber--;
			}

			// Maximum changelist number, exclusive
			int? maxChangeNumber = null;

			// Get the maximum number of changes to trigger
			int maxNewChanges = 1;
			if (schedule.MaxChanges != 0)
			{
				maxNewChanges = schedule.MaxChanges;
			}
			if (schedule.MaxActive != 0)
			{
				maxNewChanges = Math.Min(maxNewChanges, schedule.MaxActive - numActiveJobs);
			}

			// Create a timer to limit the amount we look back through P4 history
			Stopwatch timer = Stopwatch.StartNew();
			ChangeContentFlags? filterFlags = schedule.GetFilterFlags();

			// Create a file filter
			FileFilter? fileFilter = null;
			if (schedule.Files != null)
			{
				fileFilter = new FileFilter(schedule.Files);
			}

			// Cache the Perforce history as we're iterating through changes to improve query performance
			PerforceHistory history = new PerforceHistory(_perforce, stream.ClusterName, stream.Name);
			
			// Start as many jobs as possible
			List<(int Change, int CodeChange)> triggerChanges = new List<(int, int)>();
			while (triggerChanges.Count < maxNewChanges)
			{
				cancellationToken.ThrowIfCancellationRequested();

				// Get the next valid change
				ChangeDetails? changeDetails = null;
				if (schedule.Gate != null)
				{
					changeDetails = await GetNextChangeForGateAsync(stream, templateId, schedule.Gate, minChangeNumber, maxChangeNumber, cancellationToken);
				}
				else
				{
					changeDetails = await history.GetNextChangeAsync();
				}

				// Quit if we didn't find anything
				if (changeDetails == null)
				{
					break;
				}
				if (changeDetails.Number < minChangeNumber)
				{
					break;
				}
				if (changeDetails.Number == minChangeNumber && (schedule.RequireSubmittedChange || triggerChanges.Count > 0))
				{
					break;
				}

				// Adjust the changelist for the desired filter
				int change = changeDetails.Number;
				if (ShouldBuildChange(changeDetails, filterFlags, fileFilter))
				{
					int codeChange = await history.GetCodeChange(change);
					if (codeChange == -1)
					{
						_logger.LogWarning("Unable to find code change for CL {Change}", change);
						codeChange = change;
					}
					triggerChanges.Add((change, codeChange));
				}

				// Check we haven't exceeded the time limit
				if (timer.Elapsed > TimeSpan.FromMinutes(2.0))
				{
					_logger.LogError("Querying for changes to trigger for {StreamId} template {TemplateId} has taken {Time}. Aborting.", stream.Id, templateId, timer.Elapsed);
					break;
				}
				
				// Update the remaining range of changes to check for
				maxChangeNumber = change - 1;
				if(maxChangeNumber < minChangeNumber)
				{
					break;
				}
			}

			// Early out if there's nothing to do
			if (triggerChanges.Count == 0)
			{
				_logger.LogInformation("Skipping trigger of {StreamName} template {TemplateId} - no candidate changes after CL {LastTriggerChange}", stream.Id, templateId, schedule.LastTriggerChange);
				return;
			}

			// Get the matching template
			ITemplate? template = await _templateCollection.GetAsync(templateRef.Hash);
			if (template == null)
			{
				_logger.LogWarning("Unable to find template '{TemplateHash}' for '{TemplateId}'", templateRef.Hash, templateId);
				return;
			}

			// Register the graph for it
			IGraph graph = await _graphs.AddAsync(template);

			// We may need to submit a new change for any new jobs. This only makes sense if there's one change.
			if (template.SubmitNewChange != null)
			{
				int newChange = await _perforce.CreateNewChangeForTemplateAsync(stream, template);
				int newCodeChange = await _perforce.GetCodeChangeAsync(stream.ClusterName, stream.Name, newChange);
				triggerChanges = new List<(int, int)> { (newChange, newCodeChange) };
			}

			// Try to start all the new jobs
			_logger.LogInformation("Starting {NumJobs} new jobs for {StreamId} template {TemplateId} (active: {NumActive}, max new: {MaxNewJobs})", triggerChanges.Count, stream.Id, templateId, numActiveJobs, maxNewChanges);
			foreach ((int change, int codeChange) in triggerChanges.OrderBy(x => x.Change))
			{
				cancellationToken.ThrowIfCancellationRequested();
				List<string> defaultArguments = template.GetDefaultArguments();
				IJob newJob = await _jobService.CreateJobAsync(null, stream, templateId, template.Id, graph, template.Name, change, codeChange, null, null, null, null, template.Priority, null, null, template.PromoteIssuesByDefault, templateRef.ChainedJobs, templateRef.ShowUgsBadges, templateRef.ShowUgsAlerts, templateRef.NotificationChannel, templateRef.NotificationChannelFilter, defaultArguments);
				_logger.LogInformation("Started new job for {StreamId} template {TemplateId} at CL {Change} (Code CL {CodeChange}): {JobId}", stream.Id, templateId, change, codeChange, newJob.Id);
				await _streamService.UpdateScheduleTriggerAsync(stream, templateId, utcNow, change, new List<JobId> { newJob.Id }, new List<JobId>());
			}
		}

		/// <summary>
		/// Tests whether a schedule should build a particular change, based on its requested change filters
		/// </summary>
		/// <param name="details">The change details</param>
		/// <param name="filterFlags"></param>
		/// <param name="fileFilter">Filter for the files to trigger a build</param>
		/// <returns></returns>
		private bool ShouldBuildChange(ChangeDetails details, ChangeContentFlags? filterFlags, FileFilter? fileFilter)
		{
			if (Regex.IsMatch(details.Description, @"^\s*#\s*skipci", RegexOptions.Multiline))
			{
				return false;
			}
			if (filterFlags != null && filterFlags.Value != 0)
			{
				if ((details.GetContentFlags() & filterFlags.Value) == 0)
				{
					_logger.LogDebug("Not building change {Change} ({ChangeFlags}) due to filter flags ({FilterFlags})", details.Number, details.GetContentFlags().ToString(), filterFlags.Value.ToString());
					return false;
				}
			}
			if (fileFilter != null)
			{
				if (!details.Files.Any(x => fileFilter.Matches(x.Path)))
				{
					_logger.LogDebug("Not building change {Change} due to file filter", details.Number);
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Gets the next change to build for a schedule on a gate
		/// </summary>
		/// <returns></returns>
		private async Task<ChangeDetails?> GetNextChangeForGateAsync(IStream stream, TemplateRefId templateRefId, ScheduleGate gate, int? minChange, int? maxChange, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				cancellationToken.ThrowIfCancellationRequested();

				List<IJob> jobs = await _jobCollection.FindAsync(streamId: stream.Id, templates: new[] { gate.TemplateRefId }, minChange: minChange, maxChange: maxChange, count: 1);
				if (jobs.Count == 0)
				{
					return null;
				}

				IJob job = jobs[0];

				IGraph? graph = await _graphs.GetAsync(job.GraphHash);
				if (graph != null)
				{
					(JobStepState, JobStepOutcome)? state = job.GetTargetState(graph, gate.Target);
					if (state != null && state.Value.Item1 == JobStepState.Completed)
					{
						JobStepOutcome outcome = state.Value.Item2;
						if (outcome == JobStepOutcome.Success || outcome == JobStepOutcome.Warnings)
						{
							return await _perforce.GetChangeDetailsAsync(stream.ClusterName, stream.Name, job.Change);
						}
						_logger.LogInformation("Skipping trigger of {StreamName} template {TemplateId} - last {OtherTemplateRefId} job ({JobId}) ended with errors", stream.Id, templateRefId, gate.TemplateRefId, job.Id);
					}
				}

				maxChange = job.Change - 1;
			}
		}
	}
}
