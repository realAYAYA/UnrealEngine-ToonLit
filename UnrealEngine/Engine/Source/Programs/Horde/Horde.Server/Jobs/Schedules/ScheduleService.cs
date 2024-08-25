// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using EpicGames.Redis;
using EpicGames.Redis.Utility;
using EpicGames.Serialization;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;
using StackExchange.Redis;

namespace Horde.Server.Jobs.Schedules
{
	/// <summary>
	/// Manipulates schedule instances
	/// </summary>
	public sealed class ScheduleService : IHostedService, IAsyncDisposable
	{
		[RedisConverter(typeof(RedisCbConverter<QueueItem>))]
		class QueueItem
		{
			[CbField("sid")]
			public StreamId StreamId { get; set; }

			[CbField("tid")]
			public TemplateId TemplateId { get; set; }

			public QueueItem()
			{
			}

			public QueueItem(StreamId streamId, TemplateId templateId)
			{
				StreamId = streamId;
				TemplateId = templateId;
			}

			public static DateTime GetTimeFromScore(double score) => DateTime.UnixEpoch + TimeSpan.FromSeconds(score);
			public static double GetScoreFromTime(DateTime time) => (time.ToUniversalTime() - DateTime.UnixEpoch).TotalSeconds;
		}

		readonly IGraphCollection _graphs;
		readonly ICommitService _commitService;
		readonly IJobCollection _jobCollection;
		readonly IDowntimeService _downtimeService;
		readonly JobService _jobService;
		readonly IStreamCollection _streamCollection;
		readonly ITemplateCollection _templateCollection;
		readonly IClock _clock;
		readonly Tracer _tracer;
		readonly ILogger _logger;
		readonly RedisService _redis;
		static readonly RedisKey s_baseLockKey = "scheduler/locks";
		static readonly RedisKey s_tickLockKey = s_baseLockKey.Append("/tick"); // Lock to tick the queue
		static readonly RedisSortedSetKey<QueueItem> s_queueKey = "scheduler/queue"; // Items to tick, ordered by time
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ITicker _ticker;

		/// <summary>
		/// Constructor
		/// </summary>
		public ScheduleService(RedisService redis, IGraphCollection graphs, ICommitService commitService, IJobCollection jobCollection, JobService jobService, IDowntimeService downtimeService, IStreamCollection streamCollection, ITemplateCollection templateCollection, MongoService mongoService, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, Tracer tracer, ILogger<ScheduleService> logger)
		{
			_graphs = graphs;
			_commitService = commitService;
			_jobCollection = jobCollection;
			_jobService = jobService;
			_downtimeService = downtimeService;
			_streamCollection = streamCollection;
			_templateCollection = templateCollection;
			_clock = clock;
			_globalConfig = globalConfig;
			_tracer = tracer;
			_logger = logger;

			_redis = redis;
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
		public async ValueTask DisposeAsync()
		{
			await _ticker.DisposeAsync();
		}

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			DateTime utcNow = _clock.UtcNow;

			// Don't start any new jobs during scheduled downtime
			if (_downtimeService.IsDowntimeActive)
			{
				return;
			}

			// Update the current queue
			await using (RedisLock sharedLock = new(_redis.GetDatabase(), s_tickLockKey))
			{
				if (await sharedLock.AcquireAsync(TimeSpan.FromMinutes(1.0), false))
				{
					await UpdateQueueAsync(utcNow, cancellationToken);
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
				await using (RedisLock sharedLock = new RedisLock<QueueItem>(_redis.GetDatabase(), s_baseLockKey, item))
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
			IDatabaseAsync target = _redis.GetDatabase();
			for (; ; )
			{
				QueueItem[] items = await target.SortedSetRangeByRankAsync(s_queueKey, 0, 0);
				if (items.Length == 0)
				{
					return null;
				}
				if (await target.SortedSetRemoveAsync(s_queueKey, items[0]))
				{
					return items[0];
				}
			}
		}

		internal async Task ResetAsync()
		{
			IDatabase redis = _redis.GetDatabase();
			await redis.KeyDeleteAsync(s_queueKey);
			await redis.KeyDeleteAsync(s_tickLockKey);
		}

		internal async Task TickForTestingAsync()
		{
			await UpdateQueueAsync(_clock.UtcNow, CancellationToken.None);
			await TickAsync(CancellationToken.None);
		}

		/// <summary>
		/// Get the current set of streams and ensure there's an entry for each item
		/// </summary>
		public async Task UpdateQueueAsync(DateTime utcNow, CancellationToken cancellationToken)
		{
			List<SortedSetEntry<QueueItem>> queueItems = new List<SortedSetEntry<QueueItem>>();

			GlobalConfig globalConfig = _globalConfig.CurrentValue;
			foreach (StreamConfig streamConfig in globalConfig.Streams)
			{
				IStream stream = await _streamCollection.GetAsync(streamConfig, cancellationToken);
				foreach ((TemplateId templateId, ITemplateRef templateRef) in stream.Templates)
				{
					if (templateRef.Schedule != null)
					{
						DateTime? nextTriggerTimeUtc = templateRef.Schedule.GetNextTriggerTimeUtc(_clock.TimeZone);
						if (nextTriggerTimeUtc != null)
						{
							if (utcNow > nextTriggerTimeUtc.Value)
							{
								double score = QueueItem.GetScoreFromTime(nextTriggerTimeUtc.Value);
								queueItems.Add(new SortedSetEntry<QueueItem>(new QueueItem(stream.Id, templateId), score));

								await _streamCollection.UpdateScheduleTriggerAsync(stream, templateId, utcNow, cancellationToken: cancellationToken);
							}
						}
					}
				}
			}

			await _redis.GetDatabase().SortedSetAddAsync(s_queueKey, queueItems.ToArray());
		}

		/// <summary>
		/// Trigger a schedule to run
		/// </summary>
		/// <param name="streamId">Stream for the schedule</param>
		/// <param name="templateId"></param>
		/// <param name="utcNow"></param>
		/// <param name="cancellationToken"></param>
		/// <returns>Async task</returns>
		internal async Task<bool> TriggerAsync(StreamId streamId, TemplateId templateId, DateTime utcNow, CancellationToken cancellationToken)
		{
			GlobalConfig globalConfig = _globalConfig.CurrentValue;
			if (!globalConfig.TryGetStream(streamId, out StreamConfig? streamConfig))
			{
				return false;
			}

			IStream? stream = await _streamCollection.GetAsync(streamConfig, cancellationToken);
			if (stream == null || !stream.Templates.TryGetValue(templateId, out ITemplateRef? templateRef))
			{
				return false;
			}

			ITemplateSchedule? schedule = templateRef.Schedule;
			if (schedule == null || !schedule.Config.Enabled)
			{
				return false;
			}

			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(ScheduleService)}.{nameof(TriggerAsync)}");
			span.SetAttribute("streamId", stream.Id);
			span.SetAttribute("templateId", templateId);

			Stopwatch stopwatch = Stopwatch.StartNew();
			_logger.LogInformation("Updating schedule for {StreamId} template {TemplateId}", stream.Id, templateId);

			// Get a list of jobs that we need to remove
			List<JobId> removeJobIds = new List<JobId>();
			foreach (JobId activeJobId in schedule.ActiveJobs)
			{
				IJob? job = await _jobService.GetJobAsync(activeJobId, cancellationToken);
				if (job == null || job.Batches.All(x => x.State == JobStepBatchState.Complete))
				{
					_logger.LogInformation("Removing active job {JobId}", activeJobId);
					removeJobIds.Add(activeJobId);
				}
			}
			await _streamCollection.UpdateScheduleTriggerAsync(stream, templateId, removeJobs: removeJobIds, cancellationToken: cancellationToken);

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
		private async Task TriggerAsync(IStream stream, TemplateId templateId, ITemplateRef templateRef, ITemplateSchedule schedule, int numActiveJobs, DateTime utcNow, CancellationToken cancellationToken)
		{
			// Check we're not already at the maximum number of allowed jobs
			if (schedule.Config.MaxActive != 0 && numActiveJobs >= schedule.Config.MaxActive)
			{
				_logger.LogInformation("Skipping trigger of {StreamId} template {TemplateId} - already have maximum number of jobs running ({NumJobs})", stream.Id, templateId, schedule.Config.MaxActive);
				foreach (JobId jobId in schedule.ActiveJobs)
				{
					_logger.LogInformation("Active job for {StreamId} template {TemplateId}: {JobId}", stream.Id, templateId, jobId);
				}
				return;
			}

			// Minimum changelist number, inclusive
			int minChangeNumber = schedule.LastTriggerChange;
			if (minChangeNumber > 0 && !schedule.Config.RequireSubmittedChange)
			{
				minChangeNumber--;
			}

			// Maximum changelist number, exclusive
			int? maxChangeNumber = null;

			// Get the maximum number of changes to trigger
			int maxNewChanges = 1;
			if (schedule.Config.MaxChanges != 0)
			{
				maxNewChanges = schedule.Config.MaxChanges;
			}
			if (schedule.Config.MaxActive != 0)
			{
				maxNewChanges = Math.Min(maxNewChanges, schedule.Config.MaxActive - numActiveJobs);
			}

			// Create a timer to limit the amount we look back through P4 history
			Stopwatch timer = Stopwatch.StartNew();

			// Create a file filter
			FileFilter? fileFilter = null;
			if (schedule.Config.Files != null)
			{
				fileFilter = new FileFilter(schedule.Config.Files);
			}

			// Cache the Perforce history as we're iterating through changes to improve query performance
			ICommitCollection commits = _commitService.GetCollection(stream.Config);
			IAsyncEnumerable<ICommit> commitEnumerable = commits.FindAsync(minChangeNumber, null, null, schedule.Config.GetAllCommitTags(), cancellationToken);
			await using IAsyncEnumerator<ICommit> commitEnumerator = commitEnumerable.GetAsyncEnumerator(cancellationToken);

			// Start as many jobs as possible
			List<(int Change, int CodeChange)> triggerChanges = new List<(int, int)>();
			while (triggerChanges.Count < maxNewChanges)
			{
				cancellationToken.ThrowIfCancellationRequested();

				// Get the next valid change
				int change;
				ICommit? commit;

				if (schedule.Config.Gate != null)
				{
					change = await GetNextChangeForGateAsync(stream.Id, templateId, schedule.Config.Gate, minChangeNumber, maxChangeNumber, cancellationToken);
					commit = await commits.FindAsync(change, change, 1, null, cancellationToken).FirstOrDefaultAsync(cancellationToken); // May be a change in a different stream
				}
				else if (await commitEnumerator.MoveNextAsync(cancellationToken))
				{
					commit = commitEnumerator.Current;
					change = commit.Number;
				}
				else
				{
					commit = null;
					change = 0;
				}

				// Quit if we didn't find anything
				if (change <= 0)
				{
					break;
				}
				if (change < minChangeNumber)
				{
					break;
				}
				if (change == minChangeNumber && (schedule.Config.RequireSubmittedChange || triggerChanges.Count > 0))
				{
					break;
				}

				// Adjust the changelist for the desired filter
				if (commit == null || await ShouldBuildChangeAsync(commit, schedule.Config.GetAllCommitTags(), fileFilter, cancellationToken))
				{
					int codeChange = change;

					ICommit? lastCodeCommit = await commits.GetLastCodeChangeAsync(change, cancellationToken);
					if (lastCodeCommit != null)
					{
						codeChange = lastCodeCommit.Number;
					}
					else
					{
						_logger.LogWarning("Unable to find code change for CL {Change}", change);
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
				if (maxChangeNumber < minChangeNumber)
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
			ITemplate template = await _templateCollection.GetOrAddAsync(templateRef.Config);

			// Register the graph for it
			IGraph graph = await _graphs.AddAsync(template, stream.Config.InitialAgentType, cancellationToken);

			// We may need to submit a new change for any new jobs. This only makes sense if there's one change.
			if (template.SubmitNewChange != null)
			{
				int newChange = await commits.CreateNewAsync(template, cancellationToken);
				ICommit? newCodeChange = await commits.GetLastCodeChangeAsync(newChange, cancellationToken);
				triggerChanges = new List<(int, int)> { (newChange, newCodeChange?.Number ?? newChange) };
			}

			// Try to start all the new jobs
			_logger.LogInformation("Starting {NumJobs} new jobs for {StreamId} template {TemplateId} (active: {NumActive}, max new: {MaxNewJobs})", triggerChanges.Count, stream.Id, templateId, numActiveJobs, maxNewChanges);
			foreach ((int change, int codeChange) in triggerChanges.OrderBy(x => x.Change))
			{
				cancellationToken.ThrowIfCancellationRequested();

				CreateJobOptions options = new CreateJobOptions(templateRef.Config);
				options.Priority = template.Priority;
				options.Arguments.AddRange(template.GetDefaultArguments(true));

				IJob newJob = await _jobService.CreateJobAsync(null, stream.Config, templateId, template.Hash, graph, template.Name, change, codeChange, options, cancellationToken);
				_logger.LogInformation("Started new job for {StreamId} template {TemplateId} at CL {Change} (Code CL {CodeChange}): {JobId}", stream.Id, templateId, change, codeChange, newJob.Id);
				await _streamCollection.UpdateScheduleTriggerAsync(stream, templateId, utcNow, change, new List<JobId> { newJob.Id }, new List<JobId>(), cancellationToken);
			}
		}

		/// <summary>
		/// Tests whether a schedule should build a particular change, based on its requested change filters
		/// </summary>
		/// <param name="commit">The commit details</param>
		/// <param name="filterTags">Filter for the tags to trigger a build</param>
		/// <param name="fileFilter">Filter for the files to trigger a build</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		private async ValueTask<bool> ShouldBuildChangeAsync(ICommit commit, List<CommitTag>? filterTags, FileFilter? fileFilter, CancellationToken cancellationToken)
		{
			if (Regex.IsMatch(commit.Description, @"^\s*#\s*skipci", RegexOptions.Multiline))
			{
				return false;
			}
			if (filterTags != null && filterTags.Count > 0)
			{
				IReadOnlyList<CommitTag> commitTags = await commit.GetTagsAsync(cancellationToken);
				if (!commitTags.Any(x => filterTags.Contains(x)))
				{
					_logger.LogDebug("Not building change {Change} ({ChangeTags}) due to filter tags ({FilterTags})", commit.Number, String.Join(", ", commitTags.Select(x => x.ToString())), String.Join(", ", filterTags.Select(x => x.ToString())));
					return false;
				}
			}
			if (fileFilter != null)
			{
				if (!await commit.MatchesFilterAsync(fileFilter, cancellationToken))
				{
					_logger.LogDebug("Not building change {Change} due to file filter", commit.Number);
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Gets the next change to build for a schedule on a gate
		/// </summary>
		/// <returns></returns>
		private async Task<int> GetNextChangeForGateAsync(StreamId streamId, TemplateId templateRefId, ScheduleGateConfig gate, int? minChange, int? maxChange, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				cancellationToken.ThrowIfCancellationRequested();

				IReadOnlyList<IJob> jobs = await _jobCollection.FindAsync(streamId: streamId, templates: new[] { gate.TemplateId }, minChange: minChange, maxChange: maxChange, count: 1, cancellationToken: cancellationToken);
				if (jobs.Count == 0)
				{
					return 0;
				}

				IJob job = jobs[0];

				IGraph? graph = await _graphs.GetAsync(job.GraphHash, cancellationToken);
				if (graph != null)
				{
					(JobStepState, JobStepOutcome)? state = job.GetTargetState(graph, gate.Target);
					if (state != null && state.Value.Item1 == JobStepState.Completed)
					{
						JobStepOutcome outcome = state.Value.Item2;
						if (outcome == JobStepOutcome.Success || outcome == JobStepOutcome.Warnings)
						{
							return job.Change;
						}
						_logger.LogInformation("Skipping trigger of {StreamName} template {TemplateId} - last {OtherTemplateRefId} job ({JobId}) ended with errors", streamId, templateRefId, gate.TemplateId, job.Id);
					}
				}

				maxChange = job.Change - 1;
			}
		}
	}
}
