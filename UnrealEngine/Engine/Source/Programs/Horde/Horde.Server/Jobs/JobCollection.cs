// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Bisect;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Acls;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Telemetry;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.IO;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Options;
using MongoDB.Driver;
using OpenTelemetry.Trace;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Wrapper around the jobs collection in a mongo DB
	/// </summary>
	public class JobCollection : IJobCollection
	{
		/// <summary>
		/// Embedded jobstep document
		/// </summary>
		[BsonIgnoreExtraElements]
		class JobStepDocument : IJobStep
		{
			[BsonRequired]
			public JobStepId Id { get; set; }

			[BsonRequired]
			public int NodeIdx { get; set; }

			[BsonRequired]
			public JobStepState State { get; set; } = JobStepState.Waiting;

			public JobStepOutcome Outcome { get; set; } = JobStepOutcome.Success;

			public JobStepError Error { get; set; } = JobStepError.None;

			[BsonIgnoreIfNull]
			public LogId? LogId { get; set; }

			[BsonIgnoreIfNull]
			public ObjectId? NotificationTriggerId { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? ReadyTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? StartTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? FinishTime { get; set; }

			[BsonIgnoreIfNull]
			public Priority? Priority { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool Retry { get; set; }

			public UserId? RetriedByUserId { get; set; }

			[BsonElement("RetryByUser")]
			public string? RetriedByUserDeprecated { get; set; }

			public bool AbortRequested { get; set; } = false;

			public UserId? AbortedByUserId { get; set; }

			[BsonElement("AbortByUser")]
			public string? AbortedByUserDeprecated { get; set; }

			[BsonIgnoreIfNull]
			public List<Report>? Reports { get; set; }
			IReadOnlyList<IReport>? IJobStep.Reports => Reports;

			[BsonIgnoreIfNull]
			public Dictionary<string, string>? Properties { get; set; }

			DateTime? IJobStep.ReadyTimeUtc => ReadyTime?.UtcDateTime;
			DateTime? IJobStep.StartTimeUtc => StartTime?.UtcDateTime;
			DateTime? IJobStep.FinishTimeUtc => FinishTime?.UtcDateTime;

			[BsonConstructor]
			private JobStepDocument()
			{
			}

			public JobStepDocument(JobStepId id, int nodeIdx)
			{
				Id = id;
				NodeIdx = nodeIdx;
			}

			public override string ToString()
			{
				StringBuilder description = new StringBuilder($"{Id}: {State}");
				if (Outcome != JobStepOutcome.Unspecified)
				{
					description.Append($" ({Outcome}");
					if (Error != JobStepError.None)
					{
						description.Append($" - {Error}");
					}
					description.Append(')');
				}
				return description.ToString();
			}
		}

		class JobStepBatchDocument : IJobStepBatch
		{
			[BsonRequired]
			public JobStepBatchId Id { get; set; }

			public LogId? LogId { get; set; }

			[BsonRequired]
			public int GroupIdx { get; set; }

			[BsonRequired]
			public JobStepBatchState State { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(JobStepBatchError.None)]
			public JobStepBatchError Error { get; set; }

			public List<JobStepDocument> Steps { get; set; } = new List<JobStepDocument>();

			[BsonIgnoreIfNull]
			public PoolId? PoolId { get; set; }

			[BsonIgnoreIfNull]
			public AgentId? AgentId { get; set; }

			[BsonIgnoreIfNull]
			public SessionId? SessionId { get; set; }

			[BsonIgnoreIfNull]
			public LeaseId? LeaseId { get; set; }

			public int SchedulePriority { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? ReadyTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? StartTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? FinishTime { get; set; }

			IReadOnlyList<IJobStep> IJobStepBatch.Steps => Steps;
			DateTime? IJobStepBatch.ReadyTimeUtc => ReadyTime?.UtcDateTime;
			DateTime? IJobStepBatch.StartTimeUtc => StartTime?.UtcDateTime;
			DateTime? IJobStepBatch.FinishTimeUtc => FinishTime?.UtcDateTime;

			[BsonConstructor]
			private JobStepBatchDocument()
			{
			}

			public JobStepBatchDocument(JobStepBatchId id, int groupIdx)
			{
				Id = id;
				GroupIdx = groupIdx;
			}

			public override string ToString()
			{
				StringBuilder description = new StringBuilder($"{Id}: {State}");
				if (Error != JobStepBatchError.None)
				{
					description.Append($" - {Error}");
				}
				description.Append($" ({Steps.Count} steps");
				return description.ToString();
			}
		}

		class ChainedJobDocument : IChainedJob
		{
			public string Target { get; set; }
			public TemplateId TemplateRefId { get; set; }
			public JobId? JobId { get; set; }
			public bool UseDefaultChangeForTemplate { get; set; }

			[BsonConstructor]
			private ChainedJobDocument()
			{
				Target = String.Empty;
			}

			public ChainedJobDocument(ChainedJobTemplateConfig trigger)
			{
				Target = trigger.Trigger;
				TemplateRefId = trigger.TemplateId;
				UseDefaultChangeForTemplate = trigger.UseDefaultChangeForTemplate;
			}
		}

		class LabelNotificationDocument
		{
			public int _labelIdx;
			public ObjectId _triggerId;
		}

		class JobDocument : IJob
		{
			[BsonRequired, BsonId]
			public JobId Id { get; set; }

			public StreamId StreamId { get; set; }
			public TemplateId TemplateId { get; set; }
			public ContentHash? TemplateHash { get; set; }
			public ContentHash GraphHash { get; set; }

			[BsonIgnoreIfNull]
			public UserId? StartedByUserId { get; set; }

			[BsonIgnoreIfNull]
			public BisectTaskId? StartedByBisectTaskId { get; set; }

			[BsonIgnoreIfNull, BsonElement("StartedByUser")]
			public string? StartedByUserDeprecated { get; set; }

			[BsonIgnoreIfNull]
			public UserId? AbortedByUserId { get; set; }

			[BsonIgnoreIfNull, BsonElement("AbortedByUser")]
			public string? AbortedByUserDeprecated { get; set; }

			[BsonRequired]
			public string Name { get; set; }

			public int Change { get; set; }
			public int CodeChange { get; set; }
			public int PreflightChange { get; set; }
			public int ClonedPreflightChange { get; set; }
			public string? PreflightDescription { get; set; }
			public Priority Priority { get; set; }

			[BsonIgnoreIfDefault]
			public bool AutoSubmit { get; set; }

			[BsonIgnoreIfNull]
			public int? AutoSubmitChange { get; set; }

			[BsonIgnoreIfNull]
			public string? AutoSubmitMessage { get; set; }

			public bool UpdateIssues { get; set; }

			public bool PromoteIssuesByDefault { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? CreateTime { get; set; }

			[BsonIgnoreIfNull]
			public JobOptions? JobOptions { get; set; }

			public List<AclClaimConfig> Claims { get; set; } = new List<AclClaimConfig>();
			IReadOnlyList<AclClaimConfig> IJob.Claims => Claims;

			[BsonIgnoreIfNull]
			public DateTime? CreateTimeUtc { get; set; }

			public int SchedulePriority { get; set; }
			public List<JobStepBatchDocument> Batches { get; set; } = new List<JobStepBatchDocument>();
			public List<Report>? Reports { get; set; }
			public List<string> Arguments { get; set; } = new List<string>();

			[BsonDictionaryOptions(DictionaryRepresentation.ArrayOfDocuments)]
			public Dictionary<string, string> Environment { get; set; } = new Dictionary<string, string>();

			public List<int> ReferencedByIssues { get; set; } = new List<int>();
			public ObjectId? NotificationTriggerId { get; set; }
			public bool ShowUgsBadges { get; set; }
			public bool ShowUgsAlerts { get; set; }
			public string? NotificationChannel { get; set; }
			public string? NotificationChannelFilter { get; set; }
			public List<LabelNotificationDocument> _labelNotifications = new List<LabelNotificationDocument>();
			public List<ChainedJobDocument> ChainedJobs { get; set; } = new List<ChainedJobDocument>();

			[BsonIgnoreIfNull]
			public List<NodeRef>? RetriedNodes { get; set; }
			public SubResourceId NextSubResourceId { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? UpdateTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? UpdateTimeUtc { get; set; }

			[BsonRequired]
			public int UpdateIndex { get; set; }

			DateTime IJob.CreateTimeUtc => CreateTimeUtc ?? CreateTime?.UtcDateTime ?? DateTime.UnixEpoch;
			DateTime IJob.UpdateTimeUtc => UpdateTimeUtc ?? UpdateTime?.UtcDateTime ?? DateTime.UnixEpoch;
			IReadOnlyList<IJobStepBatch> IJob.Batches => (IReadOnlyList<JobStepBatchDocument>)Batches;
			IReadOnlyList<IReport>? IJob.Reports => Reports;
			IReadOnlyList<string> IJob.Arguments => Arguments;
			IReadOnlyList<int> IJob.Issues => ReferencedByIssues;
			IReadOnlyDictionary<int, ObjectId> IJob.LabelIdxToTriggerId => _labelNotifications.ToDictionary(x => x._labelIdx, x => x._triggerId);
			IReadOnlyDictionary<string, string> IJob.Environment => Environment;
			IReadOnlyList<IChainedJob> IJob.ChainedJobs => ChainedJobs;

			[BsonConstructor]
			private JobDocument()
			{
				Name = null!;
				GraphHash = null!;
			}

			public JobDocument(JobId id, StreamId streamId, TemplateId templateId, ContentHash templateHash, ContentHash graphHash, string name, int change, int codeChange, CreateJobOptions options, DateTime createTimeUtc)
			{
				Id = id;
				StreamId = streamId;
				TemplateId = templateId;
				TemplateHash = templateHash;
				GraphHash = graphHash;
				Name = name;
				Change = change;
				CodeChange = codeChange;
				PreflightChange = options.PreflightChange ?? 0;
				ClonedPreflightChange = options.ClonedPreflightChange ?? 0;
				PreflightDescription = options.PreflightDescription;
				StartedByUserId = options.StartedByUserId;
				StartedByBisectTaskId = options.StartedByBisectTaskId;
				Priority = options.Priority ?? HordeCommon.Priority.Normal;
				AutoSubmit = options.AutoSubmit ?? false;
				UpdateIssues = options.UpdateIssues ?? (options.StartedByUserId == null && (options.PreflightChange == 0 || options.PreflightChange == null));
				PromoteIssuesByDefault = options.PromoteIssuesByDefault ?? false;
				Claims = options.Claims;
				JobOptions = options.JobOptions;
				CreateTimeUtc = createTimeUtc;
				ChainedJobs.AddRange(options.JobTriggers.Select(x => new ChainedJobDocument(x)));
				ShowUgsBadges = options.ShowUgsBadges;
				ShowUgsAlerts = options.ShowUgsAlerts;
				NotificationChannel = options.NotificationChannel;
				NotificationChannelFilter = options.NotificationChannelFilter;
				Arguments.AddRange(options.Arguments);

				foreach (KeyValuePair<string, string> pair in options.Environment)
				{
					Environment[pair.Key] = pair.Value;
				}

				NextSubResourceId = SubResourceId.GenerateNewId();
				UpdateTimeUtc = createTimeUtc;
			}
		}

		/// <summary>
		/// Maximum number of times a step can be retried (after the original run)
		/// </summary>
		const int MaxRetries = 2;

		readonly IMongoCollection<JobDocument> _jobs;
		readonly MongoIndex<JobDocument> _createTimeIndex;
		readonly MongoIndex<JobDocument> _updateTimeIndex;
		readonly MongoIndex<JobDocument> _streamThenTemplateThenCreationTimeIndex;
		readonly MongoIndex<JobDocument> _startedByBisectTaskIdIndex;
		readonly ITelemetrySink _telemetrySink;
		readonly IClock _clock;
		readonly Tracer _tracer;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger<JobCollection> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobCollection(MongoService mongoService, IClock clock, ITelemetrySink telemetrySink, IOptionsMonitor<GlobalConfig> globalConfig, Tracer tracer, ILogger<JobCollection> logger)
		{
			_clock = clock;
			_telemetrySink = telemetrySink;
			_globalConfig = globalConfig;
			_tracer = tracer;
			_logger = logger;

			List<MongoIndex<JobDocument>> indexes = new List<MongoIndex<JobDocument>>();
			indexes.Add(keys => keys.Ascending(x => x.StreamId));
			indexes.Add(_streamThenTemplateThenCreationTimeIndex = MongoIndex.Create<JobDocument>(keys => keys.Ascending(x => x.StreamId).Ascending(x => x.TemplateId).Descending(x => x.CreateTimeUtc)));
			indexes.Add(keys => keys.Ascending(x => x.Change));
			indexes.Add(keys => keys.Ascending(x => x.PreflightChange));
			indexes.Add(_createTimeIndex = MongoIndex.Create<JobDocument>(keys => keys.Descending(x => x.CreateTimeUtc)));
			indexes.Add(_updateTimeIndex = MongoIndex.Create<JobDocument>(keys => keys.Descending(x => x.UpdateTimeUtc)));
			indexes.Add(keys => keys.Ascending(x => x.Name));
			indexes.Add(keys => keys.Ascending(x => x.StartedByUserId));
			indexes.Add(keys => keys.Ascending(x => x.TemplateId));
			indexes.Add(keys => keys.Descending(x => x.SchedulePriority));
			indexes.Add(_startedByBisectTaskIdIndex = MongoIndex.Create<JobDocument>(keys => keys.Descending(x => x.StartedByBisectTaskId), sparse: true));
			_jobs = mongoService.GetCollection<JobDocument>("Jobs", indexes);
		}

		static Task PostLoadAsync(JobDocument job)
		{
			if (job.GraphHash == ContentHash.Empty)
			{
				job.Batches.Clear();
			}
			return Task.CompletedTask;
		}

		static JobDocument Clone(JobDocument job)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				using (BsonBinaryWriter writer = new BsonBinaryWriter(stream))
				{
					BsonSerializer.Serialize(writer, job);
				}
				return BsonSerializer.Deserialize<JobDocument>(stream.ToArray());
			}
		}

		/// <inheritdoc/>
		public async Task<IJob> AddAsync(JobId jobId, StreamId streamId, TemplateId templateRefId, ContentHash templateHash, IGraph graph, string name, int change, int codeChange, CreateJobOptions options, CancellationToken cancellationToken)
		{
			JobDocument newJob = new JobDocument(jobId, streamId, templateRefId, templateHash, graph.Id, name, change, codeChange, options, DateTime.UtcNow);
			CreateBatches(newJob, graph, _logger);

			await _jobs.InsertOneAsync(newJob, null, cancellationToken);

			if (_globalConfig.CurrentValue.TryGetStream(streamId, out StreamConfig? streamConfig) && !streamConfig.TelemetryStoreId.IsEmpty)
			{
				_telemetrySink.SendEvent(streamConfig.TelemetryStoreId, TelemetryRecordMeta.CurrentHordeInstance, new
				{
					EventName = "State.Job",
					Id = newJob.Id,
					StreamId = newJob.StreamId,
					Arguments = newJob.Arguments,
					AutoSubmit = newJob.AutoSubmit,
					Change = newJob.Change,
					CodeChange = newJob.CodeChange,
					CreateTimeUtc = newJob.CreateTimeUtc,
					GraphHash = newJob.GraphHash,
					Name = newJob.Name,
					PreflightChange = newJob.PreflightChange,
					PreflightDescription = newJob.PreflightDescription,
					Priority = newJob.Priority,
					StartedByUserId = newJob.StartedByUserId,
					TemplateId = newJob.TemplateId
				});
			}

			return newJob;
		}

		/// <inheritdoc/>
		public async Task<IJob?> GetAsync(JobId jobId, CancellationToken cancellationToken)
		{
			JobDocument? job = await _jobs.Find<JobDocument>(x => x.Id == jobId).FirstOrDefaultAsync(cancellationToken);
			if (job != null)
			{
				await PostLoadAsync(job);
			}
			return job;
		}

		/// <inheritdoc/>
		public async Task<bool> RemoveAsync(IJob job, CancellationToken cancellationToken)
		{
			DeleteResult result = await _jobs.DeleteOneAsync(x => x.Id == job.Id && x.UpdateIndex == job.UpdateIndex, null, cancellationToken);
			return result.DeletedCount > 0;
		}

		/// <inheritdoc/>
		public async Task RemoveStreamAsync(StreamId streamId, CancellationToken cancellationToken)
		{
			await _jobs.DeleteManyAsync(x => x.StreamId == streamId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IJob>> FindAsync(JobId[]? jobIds, StreamId? streamId, string? name, TemplateId[]? templates, int? minChange, int? maxChange, int? preflightChange, bool? preflightOnly, UserId? preflightStartedByUser, UserId? startedByUser, DateTimeOffset? minCreateTime, DateTimeOffset? maxCreateTime, DateTimeOffset? modifiedBefore, DateTimeOffset? modifiedAfter, JobStepBatchState? batchState, int? index, int? count, bool consistentRead, string? indexHint, bool? excludeUserJobs, CancellationToken cancellationToken)
		{
			FilterDefinitionBuilder<JobDocument> filterBuilder = Builders<JobDocument>.Filter;

			FilterDefinition<JobDocument> filter = filterBuilder.Empty;
			if (jobIds != null && jobIds.Length > 0)
			{
				filter &= filterBuilder.In(x => x.Id, jobIds);
			}
			if (streamId != null)
			{
				filter &= filterBuilder.Eq(x => x.StreamId, streamId.Value);
			}
			if (name != null)
			{
				if (name.StartsWith("$", StringComparison.InvariantCulture))
				{
					BsonRegularExpression regex = new BsonRegularExpression(name.Substring(1), "i");
					filter &= filterBuilder.Regex(x => x.Name, regex);
				}
				else
				{
					filter &= filterBuilder.Eq(x => x.Name, name);
				}
			}
			if (templates != null)
			{
				filter &= filterBuilder.In(x => x.TemplateId, templates);
			}
			if (minChange != null)
			{
				filter &= filterBuilder.Gte(x => x.Change, minChange);
			}
			if (maxChange != null)
			{
				filter &= filterBuilder.Lte(x => x.Change, maxChange);
			}
			if (preflightChange != null)
			{
				filter &= filterBuilder.Eq(x => x.PreflightChange, preflightChange);
			}
			if (preflightOnly != null && preflightOnly.Value)
			{
				filter &= filterBuilder.Ne(x => x.PreflightChange, 0);
			}
			if (excludeUserJobs != null && excludeUserJobs.Value)
			{
				filter &= filterBuilder.Eq(x => x.StartedByUserId, null);
			}
			else
			{
				if (preflightStartedByUser != null)
				{
					filter &= filterBuilder.Or(filterBuilder.Eq(x => x.PreflightChange, 0), filterBuilder.Eq(x => x.StartedByUserId, preflightStartedByUser));
				}
				if (startedByUser != null)
				{
					filter &= filterBuilder.Eq(x => x.StartedByUserId, startedByUser);
				}
			}
			if (minCreateTime != null)
			{
				filter &= filterBuilder.Gte(x => x.CreateTimeUtc!, minCreateTime.Value.UtcDateTime);
			}
			if (maxCreateTime != null)
			{
				filter &= filterBuilder.Lte(x => x.CreateTimeUtc!, maxCreateTime.Value.UtcDateTime);
			}
			if (modifiedBefore != null)
			{
				filter &= filterBuilder.Lte(x => x.UpdateTimeUtc!, modifiedBefore.Value.UtcDateTime);
			}
			if (modifiedAfter != null)
			{
				filter &= filterBuilder.Gte(x => x.UpdateTimeUtc!, modifiedAfter.Value.UtcDateTime);
			}
			if (batchState != null)
			{
				filter &= filterBuilder.ElemMatch(x => x.Batches, batch => batch.State == batchState);
			}

			List<JobDocument> results;
			using (TelemetrySpan _ = _tracer.StartActiveSpan($"{nameof(JobCollection)}.{nameof(FindAsync)}"))
			{
				IMongoCollection<JobDocument> collection = consistentRead ? _jobs : _jobs.WithReadPreference(ReadPreference.SecondaryPreferred);
				results = await collection.FindWithHintAsync(filter, indexHint, x => x.SortByDescending(x => x.CreateTimeUtc!).Range(index, count).ToListAsync());
			}
			foreach (JobDocument result in results)
			{
				await PostLoadAsync(result);
			}
			return results;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<IJob> FindBisectTaskJobsAsync(BisectTaskId bisectTaskId, bool? running, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(JobCollection)}.{nameof(FindBisectTaskJobsAsync)}");
			span.SetAttribute("TaskId", bisectTaskId.Id.ToString());

			FilterDefinitionBuilder<JobDocument> filterBuilder = Builders<JobDocument>.Filter;
			FilterDefinition<JobDocument> filter = filterBuilder.Exists(x => x.StartedByBisectTaskId);
			filter &= filterBuilder.Eq(x => x.StartedByBisectTaskId, bisectTaskId);
			List<JobDocument> results = await _jobs.FindWithHintAsync(filter, _startedByBisectTaskIdIndex.Name, x => x.SortByDescending(x => x.CreateTimeUtc!).ToListAsync(cancellationToken));
			foreach (JobDocument jobDoc in results)
			{
				if (running.HasValue && running.Value)
				{
					JobState state = jobDoc.GetState();
					if (state == JobState.Complete)
					{
						continue;
					}
				}

				await PostLoadAsync(jobDoc);
				yield return jobDoc;
			}
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IJob>> FindLatestByStreamWithTemplatesAsync(StreamId streamId, TemplateId[] templates, UserId? preflightStartedByUser, DateTimeOffset? maxCreateTime, DateTimeOffset? modifiedAfter, int? index, int? count, bool consistentRead, CancellationToken cancellationToken)
		{
			string indexHint = _streamThenTemplateThenCreationTimeIndex.Name;

			// This find call uses an index hint. Modifying the parameter passed to FindAsync can affect execution time a lot as the query planner is forced to use the specified index.
			// Casting to interface to benefit from default parameter values
			return await (this as IJobCollection).FindAsync(
				streamId: streamId, templates: templates, preflightStartedByUser: preflightStartedByUser, modifiedAfter: modifiedAfter, maxCreateTime: maxCreateTime,
				index: index, count: count, indexHint: indexHint, consistentRead: consistentRead, cancellationToken: cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IJob?> TryUpdateJobAsync(IJob inJob, IGraph graph, string? name, Priority? priority, bool? autoSubmit, int? autoSubmitChange, string? autoSubmitMessage, UserId? abortedByUserId, ObjectId? notificationTriggerId, List<Report>? reports, List<string>? arguments, KeyValuePair<int, ObjectId>? labelIdxToTriggerId, KeyValuePair<TemplateId, JobId>? jobTrigger, CancellationToken cancellationToken)
		{
			// Create the update 
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();

			// Flag for whether to update batches
			bool updateBatches = false;

			// Build the update list
			JobDocument jobDocument = Clone((JobDocument)inJob);
			if (name != null)
			{
				jobDocument.Name = name;
				updates.Add(updateBuilder.Set(x => x.Name, jobDocument.Name));
			}
			if (priority != null)
			{
				jobDocument.Priority = priority.Value;
				updates.Add(updateBuilder.Set(x => x.Priority, jobDocument.Priority));
			}
			if (autoSubmit != null)
			{
				jobDocument.AutoSubmit = autoSubmit.Value;
				updates.Add(updateBuilder.Set(x => x.AutoSubmit, jobDocument.AutoSubmit));
			}
			if (autoSubmitChange != null)
			{
				jobDocument.AutoSubmitChange = autoSubmitChange.Value;
				updates.Add(updateBuilder.Set(x => x.AutoSubmitChange, jobDocument.AutoSubmitChange));
			}
			if (autoSubmitMessage != null)
			{
				jobDocument.AutoSubmitMessage = (autoSubmitMessage.Length == 0) ? null : autoSubmitMessage;
				updates.Add(updateBuilder.SetOrUnsetNullRef(x => x.AutoSubmitMessage, jobDocument.AutoSubmitMessage));
			}
			if (abortedByUserId != null && jobDocument.AbortedByUserId == null)
			{
				jobDocument.AbortedByUserId = abortedByUserId;
				updates.Add(updateBuilder.Set(x => x.AbortedByUserId, jobDocument.AbortedByUserId));
				updateBatches = true;
			}
			if (notificationTriggerId != null)
			{
				jobDocument.NotificationTriggerId = notificationTriggerId.Value;
				updates.Add(updateBuilder.Set(x => x.NotificationTriggerId, notificationTriggerId));
			}
			if (labelIdxToTriggerId != null)
			{
				if (jobDocument._labelNotifications.Any(x => x._labelIdx == labelIdxToTriggerId.Value.Key))
				{
					throw new ArgumentException("Cannot update label trigger that already exists");
				}
				jobDocument._labelNotifications.Add(new LabelNotificationDocument { _labelIdx = labelIdxToTriggerId.Value.Key, _triggerId = labelIdxToTriggerId.Value.Value });
				updates.Add(updateBuilder.Set(x => x._labelNotifications, jobDocument._labelNotifications));
			}
			if (jobTrigger != null)
			{
				for (int idx = 0; idx < jobDocument.ChainedJobs.Count; idx++)
				{
					ChainedJobDocument jobTriggerDocument = jobDocument.ChainedJobs[idx];
					if (jobTriggerDocument.TemplateRefId == jobTrigger.Value.Key)
					{
						int localIdx = idx;
						jobTriggerDocument.JobId = jobTrigger.Value.Value;
						updates.Add(updateBuilder.Set(x => x.ChainedJobs[localIdx].JobId, jobTrigger.Value.Value));
					}
				}
			}
			if (reports != null)
			{
				jobDocument.Reports ??= new List<Report>();
				jobDocument.Reports.RemoveAll(x => reports.Any(y => y.Name == x.Name));
				jobDocument.Reports.AddRange(reports);
				updates.Add(updateBuilder.Set(x => x.Reports, jobDocument.Reports));
			}
			if (arguments != null)
			{
				HashSet<string> modifiedArguments = new HashSet<string>(jobDocument.Arguments);
				modifiedArguments.SymmetricExceptWith(arguments);

				foreach (string modifiedArgument in modifiedArguments)
				{
					if (modifiedArgument.StartsWith(IJob.TargetArgumentPrefix, StringComparison.OrdinalIgnoreCase))
					{
						updateBatches = true;
					}
				}

				jobDocument.Arguments = arguments.ToList();
				updates.Add(updateBuilder.Set(x => x.Arguments, jobDocument.Arguments));
			}

			// Update the batches
			if (updateBatches)
			{
				UpdateBatches(jobDocument, graph, updates, _logger);
			}

			// Update the new list of job steps
			return await TryUpdateAsync(jobDocument, updateBuilder.Combine(updates), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IJob?> TryUpdateBatchAsync(IJob job, IGraph graph, JobStepBatchId batchId, LogId? newLogId, JobStepBatchState? newState, JobStepBatchError? newError, CancellationToken cancellationToken)
		{
			JobDocument jobDocument = Clone((JobDocument)job);

			// Find the index of the appropriate batch
			int batchIdx = jobDocument.Batches.FindIndex(x => x.Id == batchId);
			if (batchIdx == -1)
			{
				return null;
			}

			// If we're marking the batch as complete and there are still steps to run (eg. because the agent crashed), we need to mark all the steps as complete first
			JobStepBatchDocument batch = jobDocument.Batches[batchIdx];

			// Create the update 
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();

			// Update the batch
			if (newLogId != null)
			{
				batch.LogId = newLogId.Value;
				updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].LogId, batch.LogId));
			}
			if (newState != null)
			{
				batch.State = newState.Value;
				updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].State, batch.State));

				if (batch.StartTime == null && newState >= JobStepBatchState.Starting)
				{
					batch.StartTime = _clock.UtcNow;
					updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].StartTime, batch.StartTime));
				}
				if (newState == JobStepBatchState.Complete)
				{
					batch.FinishTime = _clock.UtcNow;
					updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].FinishTime, batch.FinishTime));
				}
			}
			if (newError != null && newError.Value != batch.Error)
			{
				batch.Error = newError.Value;

				// Only allow retrying nodes that may succeed if run again
				bool allowRetrying = !IsFatalBatchError(newError.Value);

				// Update the state of the nodes
				List<NodeRef> retriedNodes = jobDocument.RetriedNodes ?? new List<NodeRef>();
				foreach (JobStepDocument step in batch.Steps)
				{
					if (step.State == JobStepState.Running)
					{
						step.State = JobStepState.Completed;
						step.Outcome = JobStepOutcome.Failure;
						step.Error = JobStepError.Incomplete;

						if (allowRetrying && CanRetryNode(jobDocument, batch.GroupIdx, step.NodeIdx))
						{
							step.Retry = true;
							retriedNodes.Add(new NodeRef(batch.GroupIdx, step.NodeIdx));
						}
					}
					else if (step.State == JobStepState.Ready || step.State == JobStepState.Waiting)
					{
						if (allowRetrying && CanRetryNode(jobDocument, batch.GroupIdx, step.NodeIdx))
						{
							retriedNodes.Add(new NodeRef(batch.GroupIdx, step.NodeIdx));
						}
						else
						{
							step.State = JobStepState.Skipped;
						}
					}
				}

				// Force an update of all batches in the job. This will fail or reschedule any nodes that can no longer be executed in this batch.
				updates.Clear();
				UpdateBatches(jobDocument, graph, updates, _logger);

				if (retriedNodes.Count > 0)
				{
					jobDocument.RetriedNodes = retriedNodes;
					updates.Add(updateBuilder.Set(x => x.RetriedNodes, jobDocument.RetriedNodes));
				}
			}

			// Update the new list of job steps
			return await TryUpdateAsync(jobDocument, updates, cancellationToken);
		}

		/// <summary>
		/// Marks a node for retry and - if it's a skipped node - all of the nodes that caused it to be skipped
		/// </summary>
		static void RetryNodesRecursive(JobDocument jobDocument, IGraph graph, int groupIdx, int nodeIdx, UserId retryByUserId, List<UpdateDefinition<JobDocument>> updates)
		{
			HashSet<NodeRef> retryNodes = new HashSet<NodeRef>();
			retryNodes.Add(new NodeRef(groupIdx, nodeIdx));

			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			for (int batchIdx = jobDocument.Batches.Count - 1; batchIdx >= 0; batchIdx--)
			{
				JobStepBatchDocument batch = jobDocument.Batches[batchIdx];
				for (int stepIdx = batch.Steps.Count - 1; stepIdx >= 0; stepIdx--)
				{
					JobStepDocument step = batch.Steps[stepIdx];

					NodeRef nodeRef = new NodeRef(batch.GroupIdx, step.NodeIdx);
					if (retryNodes.Remove(nodeRef))
					{
						if (step.State == JobStepState.Skipped)
						{
							// Add all the dependencies to be retried
							NodeRef[] dependencies = graph.GetNode(nodeRef).InputDependencies;
							retryNodes.UnionWith(dependencies);
						}
						else
						{
							// Retry this step
							int lambdaBatchIdx = batchIdx;
							int lambdaStepIdx = stepIdx;

							step.Retry = true;
							updates.Add(updateBuilder.Set(x => x.Batches[lambdaBatchIdx].Steps[lambdaStepIdx].Retry, true));

							step.RetriedByUserId = retryByUserId;
							updates.Add(updateBuilder.Set(x => x.Batches[lambdaBatchIdx].Steps[lambdaStepIdx].RetriedByUserId, step.RetriedByUserId));
						}
					}
				}
			}
		}

		/// <inheritdoc/>
		public Task<IJob?> TryUpdateStepAsync(IJob job, IGraph graph, JobStepBatchId batchId, JobStepId stepId, JobStepState newState, JobStepOutcome newOutcome, JobStepError? newError, bool? newAbortRequested, UserId? newAbortByUserId, LogId? newLogId, ObjectId? newNotificationTriggerId, UserId? newRetryByUserId, Priority? newPriority, List<Report>? newReports, Dictionary<string, string?>? newProperties, CancellationToken cancellationToken)
		{
			JobDocument jobDocument = Clone((JobDocument)job);

			// Create the update 
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();

			// Update the appropriate batch
			bool refreshBatches = false;
			bool refreshDependentJobSteps = false;
			for (int loopBatchIdx = 0; loopBatchIdx < jobDocument.Batches.Count; loopBatchIdx++)
			{
				int batchIdx = loopBatchIdx; // For lambda capture
				JobStepBatchDocument batch = jobDocument.Batches[batchIdx];
				if (batch.Id == batchId)
				{
					for (int loopStepIdx = 0; loopStepIdx < batch.Steps.Count; loopStepIdx++)
					{
						int stepIdx = loopStepIdx; // For lambda capture
						JobStepDocument step = batch.Steps[stepIdx];
						if (step.Id == stepId)
						{
							// Update the request abort status
							if (newAbortRequested != null && step.AbortRequested == false)
							{
								step.AbortRequested = newAbortRequested.Value;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].AbortRequested, step.AbortRequested));

								// If the step is pending, and not running on an agent, set to aborted
								if (step.IsPending() && step.State != JobStepState.Running)
								{
									newState = JobStepState.Aborted;
									newOutcome = JobStepOutcome.Failure;
								}

								refreshDependentJobSteps = true;
							}

							// Update the user that requested the abort
							if (newAbortByUserId != null && step.AbortedByUserId == null)
							{
								step.AbortedByUserId = newAbortByUserId;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].AbortedByUserId, step.AbortedByUserId));

								refreshDependentJobSteps = true;
							}

							// Update the state
							if (newState != JobStepState.Unspecified && step.State != newState)
							{
								if (batch.State == JobStepBatchState.Starting)
								{
									batch.State = JobStepBatchState.Running;
									updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].State, batch.State));
								}

								step.State = newState;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].State, step.State));

								if (step.State == JobStepState.Running)
								{
									step.StartTime = _clock.UtcNow;
									updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].StartTime, step.StartTime));
								}
								else if (step.State == JobStepState.Completed || step.State == JobStepState.Aborted)
								{
									step.FinishTime = _clock.UtcNow;
									updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].FinishTime, step.FinishTime));
								}

								refreshDependentJobSteps = true;
							}

							// Update the job outcome
							if (newOutcome != JobStepOutcome.Unspecified && step.Outcome != newOutcome)
							{
								step.Outcome = newOutcome;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].Outcome, step.Outcome));

								refreshDependentJobSteps = true;
							}

							// Update the job step error
							if (newError != null)
							{
								step.Error = newError.Value;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].Error, step.Error));
							}

							// Update the log id
							if (newLogId != null && step.LogId != newLogId.Value)
							{
								step.LogId = newLogId.Value;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].LogId, step.LogId));

								refreshDependentJobSteps = true;
							}

							// Update the notification trigger id
							if (newNotificationTriggerId != null && step.NotificationTriggerId == null)
							{
								step.NotificationTriggerId = newNotificationTriggerId.Value;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].NotificationTriggerId, step.NotificationTriggerId));
							}

							// Update the retry flag
							if (newRetryByUserId != null && step.RetriedByUserId == null)
							{
								RetryNodesRecursive(jobDocument, graph, batch.GroupIdx, step.NodeIdx, newRetryByUserId.Value, updates);
								refreshBatches = true;
							}

							// Update the priority
							if (newPriority != null && newPriority.Value != step.Priority)
							{
								step.Priority = newPriority.Value;
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].Priority, step.Priority));

								refreshBatches = true;
							}

							// Add any new reports
							if (newReports != null)
							{
								step.Reports ??= new List<Report>();
								step.Reports.RemoveAll(x => newReports.Any(y => y.Name == x.Name));
								step.Reports.AddRange(newReports);
								updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].Reports, step.Reports));
							}

							// Apply any property updates
							if (newProperties != null)
							{
								step.Properties ??= new Dictionary<string, string>(StringComparer.Ordinal);

								foreach (KeyValuePair<string, string?> pair in newProperties)
								{
									if (pair.Value == null)
									{
										step.Properties.Remove(pair.Key);
									}
									else
									{
										step.Properties[pair.Key] = pair.Value;
									}
								}

								if (step.Properties.Count == 0)
								{
									step.Properties = null;
									updates.Add(updateBuilder.Unset(x => x.Batches[batchIdx].Steps[stepIdx].Properties));
								}
								else
								{
									foreach (KeyValuePair<string, string?> pair in newProperties)
									{
										if (pair.Value == null)
										{
											updates.Add(updateBuilder.Unset(x => x.Batches[batchIdx].Steps[stepIdx].Properties![pair.Key]));
										}
										else
										{
											updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].Properties![pair.Key], pair.Value));
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
			}

			// Update the batches
			if (refreshBatches)
			{
				updates.Clear(); // UpdateBatches will update the entire batches list. We need to remove all the individual step updates to avoid an exception.
				UpdateBatches(jobDocument, graph, updates, _logger);
			}

			// Update the state of dependent jobsteps
			if (refreshDependentJobSteps)
			{
				RefreshDependentJobSteps(jobDocument, graph, updates, _logger);
				RefreshJobPriority(jobDocument, updates);
			}

			// Update the new list of job steps
			return TryUpdateAsync(jobDocument, updates, cancellationToken);
		}

		Task<IJob?> TryUpdateAsync(JobDocument job, List<UpdateDefinition<JobDocument>> updates, CancellationToken cancellationToken)
		{
			if (updates.Count == 0)
			{
				return Task.FromResult<IJob?>(job);
			}
			else
			{
				return TryUpdateAsync(job, Builders<JobDocument>.Update.Combine(updates), cancellationToken);
			}
		}

		async Task<IJob?> TryUpdateAsync(JobDocument job, UpdateDefinition<JobDocument> update, CancellationToken cancellationToken)
		{
			int newUpdateIndex = job.UpdateIndex + 1;
			update = update.Set(x => x.UpdateIndex, newUpdateIndex);

			DateTime newUpdateTimeUtc = DateTime.UtcNow;
			update = update.Set(x => x.UpdateTimeUtc, newUpdateTimeUtc);

			UpdateResult result = await _jobs.UpdateOneAsync<JobDocument>(x => x.Id == job.Id && x.UpdateIndex == job.UpdateIndex, update, null, cancellationToken);
			if (result.ModifiedCount > 0)
			{
				job.UpdateIndex = newUpdateIndex;
				job.UpdateTimeUtc = newUpdateTimeUtc;
				return job;
			}
			return null;
		}

		/// <inheritdoc/>
		public Task<IJob?> TryRemoveFromDispatchQueueAsync(IJob job, CancellationToken cancellationToken)
		{
			JobDocument jobDocument = Clone((JobDocument)job);

			// Create the update 
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();

			jobDocument.SchedulePriority = 0;
			updates.Add(updateBuilder.Set(x => x.SchedulePriority, jobDocument.SchedulePriority));

			// Update the new list of job steps
			return TryUpdateAsync(jobDocument, updates, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<IJob?> TryUpdateGraphAsync(IJob job, IGraph oldGraph, IGraph newGraph, CancellationToken cancellationToken)
		{
			JobDocument jobDocument = (JobDocument)job;
			if (job.GraphHash != oldGraph.Id)
			{
				throw new InvalidOperationException($"Job {job.Id} uses graph {job.GraphHash}, not {oldGraph.Id}");
			}

			// Update all the references in the job to use references within the new graph
			List<int> newNodeIndexes = new List<int>();
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				INodeGroup oldGroup = oldGraph.Groups[batch.GroupIdx];
				newNodeIndexes.Clear();

				// Find the new node indexes for this group
				int newGroupIdx = -1;
				for (int oldNodeIdx = 0; oldNodeIdx < oldGroup.Nodes.Count; oldNodeIdx++)
				{
					INode oldNode = oldGroup.Nodes[oldNodeIdx];

					NodeRef newNodeRef;
					if (!newGraph.TryFindNode(oldNode.Name, out newNodeRef))
					{
						throw new InvalidOperationException($"Node '{oldNode.Name}' exists in graph {oldGraph.Id}; does not exist in graph {newGraph.Id}");
					}

					if (newGroupIdx == -1)
					{
						newGroupIdx = newNodeRef.GroupIdx;
					}
					else if (newGroupIdx != newNodeRef.GroupIdx)
					{
						throw new InvalidOperationException($"Node '{oldNode.Name}' is in different group in graph {oldGraph.Id} than graph {newGraph.Id}");
					}

					newNodeIndexes.Add(newNodeRef.NodeIdx);
				}
				if (newGroupIdx == -1)
				{
					throw new InvalidOperationException($"Group {batch.GroupIdx} in graph {oldGraph.Id} does not have any nodes");
				}

				// Update all the steps
				batch.GroupIdx = newGroupIdx;
				INodeGroup newGroup = newGraph.Groups[newGroupIdx];

				foreach (JobStepDocument step in batch.Steps)
				{
					int oldNodeIdx = step.NodeIdx;
					int newNodeIdx = newNodeIndexes[oldNodeIdx];

					INode oldNode = oldGroup.Nodes[oldNodeIdx];
					INode newNode = newGroup.Nodes[newNodeIdx];

					if (!step.IsPending() && !NodesMatch(oldGraph, oldNode, newGraph, newNode))
					{
						throw new InvalidOperationException($"Definition for node '{oldNode.Name}' has changed.");
					}

					step.NodeIdx = newNodeIdx;
				}
			}

			// Create the update 
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();

			jobDocument.GraphHash = newGraph.Id;
			updates.Add(updateBuilder.Set(x => x.GraphHash, job.GraphHash));

			UpdateBatches(jobDocument, newGraph, updates, _logger);

			// Update the new list of job steps
			return TryUpdateAsync(jobDocument, updates, cancellationToken);
		}

		static bool NodesMatch(IGraph oldGraph, INode oldNode, IGraph newGraph, INode newNode)
		{
			IEnumerable<string> oldInputDependencies = oldNode.InputDependencies.Select(x => oldGraph.GetNode(x).Name);
			IEnumerable<string> newInputDependencies = newNode.InputDependencies.Select(x => newGraph.GetNode(x).Name);
			if (!CompareListsIgnoreOrder(oldInputDependencies, newInputDependencies))
			{
				return false;
			}

			IEnumerable<string> oldInputs = oldNode.Inputs.Select(x => oldGraph.GetNode(x.NodeRef).OutputNames[x.OutputIdx]);
			IEnumerable<string> newInputs = newNode.Inputs.Select(x => newGraph.GetNode(x.NodeRef).OutputNames[x.OutputIdx]);
			if (!CompareListsIgnoreOrder(oldInputs, newInputs))
			{
				return false;
			}

			return CompareListsIgnoreOrder(oldNode.OutputNames, newNode.OutputNames);
		}

		static bool CompareListsIgnoreOrder(IEnumerable<string> seq1, IEnumerable<string> seq2)
		{
			return new HashSet<string>(seq1, StringComparer.OrdinalIgnoreCase).SetEquals(seq2);
		}

		/// <inheritdoc/>
		public async Task AddIssueToJobAsync(JobId jobId, int issueId, CancellationToken cancellationToken)
		{
			FilterDefinition<JobDocument> jobFilter = Builders<JobDocument>.Filter.Eq(x => x.Id, jobId);
			UpdateDefinition<JobDocument> jobUpdate = Builders<JobDocument>.Update.AddToSet(x => x.ReferencedByIssues, issueId).Inc(x => x.UpdateIndex, 1).Max(x => x.UpdateTimeUtc, DateTime.UtcNow);
			await _jobs.UpdateOneAsync(jobFilter, jobUpdate, null, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IJob>> GetDispatchQueueAsync(CancellationToken cancellationToken)
		{
			List<JobDocument> newJobs = await _jobs.Find(x => x.SchedulePriority > 0).SortByDescending(x => x.SchedulePriority).ThenBy(x => x.CreateTimeUtc).ToListAsync(cancellationToken);
			foreach (JobDocument result in newJobs)
			{
				await PostLoadAsync(result);
			}
			return newJobs;
		}

		/// <inheritdoc/>
		public async Task<IJob?> SkipAllBatchesAsync(IJob? job, IGraph graph, JobStepBatchError reason, CancellationToken cancellationToken)
		{
			while (job != null)
			{
				JobDocument jobDocument = Clone((JobDocument)job);

				for (int batchIdx = 0; batchIdx < jobDocument.Batches.Count; batchIdx++)
				{
					JobStepBatchDocument batch = jobDocument.Batches[batchIdx];
					if (batch.State == JobStepBatchState.Ready || batch.State == JobStepBatchState.Waiting)
					{
						batch.State = JobStepBatchState.Complete;
						batch.Error = reason;
						batch.FinishTime = DateTimeOffset.UtcNow;

						for (int stepIdx = 0; stepIdx < batch.Steps.Count; stepIdx++)
						{
							JobStepDocument step = batch.Steps[stepIdx];
							if (step.State == JobStepState.Ready || step.State == JobStepState.Waiting)
							{
								step.State = JobStepState.Completed;
								step.Outcome = JobStepOutcome.Failure;
							}
						}
					}
				}

				List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();
				UpdateBatches(jobDocument, graph, updates, _logger);

				IJob? newJob = await TryUpdateAsync(jobDocument, updates, cancellationToken);
				if (newJob != null)
				{
					return newJob;
				}

				job = await GetAsync(job.Id, cancellationToken);
			}
			return job;
		}

		/// <inheritdoc/>
		public async Task<IJob?> SkipBatchAsync(IJob? job, JobStepBatchId batchId, IGraph graph, JobStepBatchError reason, CancellationToken cancellationToken)
		{
			while (job != null)
			{
				JobDocument jobDocument = (JobDocument)job;

				JobStepBatchDocument? batch = jobDocument.Batches.FirstOrDefault(x => x.Id == batchId);
				if (batch == null)
				{
					return job;
				}

				batch.State = JobStepBatchState.Complete;
				batch.Error = reason;
				batch.FinishTime = DateTimeOffset.UtcNow;

				foreach (JobStepDocument step in batch.Steps)
				{
					if (step.State != JobStepState.Skipped)
					{
						step.State = JobStepState.Skipped;
						step.Outcome = JobStepOutcome.Failure;
					}
				}

				List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();
				UpdateBatches(jobDocument, graph, updates, _logger);

				IJob? newJob = await TryUpdateAsync(jobDocument, updates, cancellationToken);
				if (newJob != null)
				{
					return newJob;
				}

				job = await GetAsync(job.Id, cancellationToken);
			}
			return job;
		}

		/// <inheritdoc/>
		public async Task<IJob?> TryAssignLeaseAsync(IJob job, int batchIdx, PoolId poolId, AgentId agentId, SessionId sessionId, LeaseId leaseId, LogId logId, CancellationToken cancellationToken)
		{
			// Try to update the job with this agent id
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();

			updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].PoolId, poolId));
			updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].AgentId, agentId));
			updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].SessionId, sessionId));
			updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].LeaseId, leaseId));
			updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].LogId, logId));

			// Extra logs for catching why session IDs sometimes doesn't match in prod. Resulting in PermissionDenied errors.
			if (batchIdx < job.Batches.Count && job.Batches[batchIdx].SessionId != null)
			{
				string currentSessionId = job.Batches[batchIdx].SessionId!.Value.ToString();
				_logger.LogError("Attempt to replace current session ID {CurrSessionId} with {NewSessionId} for batch {JobId}:{BatchId}", currentSessionId, sessionId.ToString(), job.Id.ToString(), job.Batches[batchIdx].Id);
				return null;
			}

			JobDocument jobDocument = Clone((JobDocument)job);
			if (await TryUpdateAsync(jobDocument, updates, cancellationToken) == null)
			{
				return null;
			}

			JobStepBatchDocument batch = jobDocument.Batches[batchIdx];
			batch.AgentId = agentId;
			batch.SessionId = sessionId;
			batch.LeaseId = leaseId;
			batch.LogId = logId;

			return jobDocument;
		}

		/// <inheritdoc/>
		public async Task<IJob?> TryCancelLeaseAsync(IJob job, int batchIdx, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Cancelling lease {LeaseId} for agent {AgentId}", job.Batches[batchIdx].LeaseId, job.Batches[batchIdx].AgentId);

			JobDocument jobDocument = Clone((JobDocument)job);

			UpdateDefinition<JobDocument> update = Builders<JobDocument>.Update.Unset(x => x.Batches[batchIdx].AgentId).Unset(x => x.Batches[batchIdx].SessionId).Unset(x => x.Batches[batchIdx].LeaseId);
			if (await TryUpdateAsync(jobDocument, update, cancellationToken) == null)
			{
				return null;
			}

			JobStepBatchDocument batch = jobDocument.Batches[batchIdx];
			batch.AgentId = null;
			batch.SessionId = null;
			batch.LeaseId = null;

			return jobDocument;
		}

		/// <inheritdoc/>
		public Task<IJob?> TryFailBatchAsync(IJob job, int batchIdx, IGraph graph, JobStepBatchError error, CancellationToken cancellationToken)
		{
			JobDocument jobDocument = Clone((JobDocument)job);
			JobStepBatchDocument batch = jobDocument.Batches[batchIdx];
			_logger.LogInformation("Failing batch {JobId}:{BatchId} with error {Error}", job.Id, batch.Id, error);

			UpdateDefinitionBuilder<JobDocument> updateBuilder = new UpdateDefinitionBuilder<JobDocument>();
			List<UpdateDefinition<JobDocument>> updates = new List<UpdateDefinition<JobDocument>>();

			if (batch.State != JobStepBatchState.Complete)
			{
				batch.State = JobStepBatchState.Complete;
				updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].State, batch.State));

				batch.Error = error;
				updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Error, batch.Error));

				batch.FinishTime = DateTimeOffset.UtcNow;
				updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].FinishTime, batch.FinishTime));
			}

			for (int stepIdx = 0; stepIdx < batch.Steps.Count; stepIdx++)
			{
				JobStepDocument step = batch.Steps[stepIdx];
				if (step.State == JobStepState.Running)
				{
					int stepIdxCopy = stepIdx;

					step.State = JobStepState.Aborted;
					updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdxCopy].State, step.State));

					step.Outcome = JobStepOutcome.Failure;
					updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdxCopy].Outcome, step.Outcome));

					step.FinishTime = _clock.UtcNow;
					updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdxCopy].FinishTime, step.FinishTime));
				}
				else if (step.State == JobStepState.Ready)
				{
					int stepIdxCopy = stepIdx;

					step.State = JobStepState.Skipped;
					updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdxCopy].State, step.State));

					step.Outcome = JobStepOutcome.Failure;
					updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdxCopy].Outcome, step.Outcome));
				}
			}

			RefreshDependentJobSteps(jobDocument, graph, updates, _logger);
			RefreshJobPriority(jobDocument, updates);

			return TryUpdateAsync(jobDocument, updateBuilder.Combine(updates), cancellationToken);
		}

		/// <summary>
		/// Populate the list of batches for a job
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="graph">The graph definition for this job</param>
		/// <param name="logger">Logger for output messages</param>
		private static void CreateBatches(JobDocument job, IGraph graph, ILogger logger)
		{
			UpdateBatches(job, graph, new List<UpdateDefinition<JobDocument>>(), logger);
		}

		/// <summary>
		/// Updates the list of batches for a job
		/// </summary>
		/// <param name="job">Job to update</param>
		/// <param name="graph">Graph definition for this job</param>
		/// <param name="updates">List of updates for the job</param>
		/// <param name="logger">Logger for updates</param>
		static void UpdateBatches(JobDocument job, IGraph graph, List<UpdateDefinition<JobDocument>> updates, ILogger logger)
		{
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;

			// Update the list of batches
			CreateOrUpdateBatches(job, graph, logger);
			updates.Add(updateBuilder.Set(x => x.Batches, job.Batches));
			updates.Add(updateBuilder.Set(x => x.NextSubResourceId, job.NextSubResourceId));

			// Update all the dependencies
			RefreshDependentJobSteps(job, graph, new List<UpdateDefinition<JobDocument>>(), logger);
			RefreshJobPriority(job, updates);
		}

		static void RemoveSteps(JobStepBatchDocument batch, Predicate<JobStepDocument> predicate, Dictionary<NodeRef, JobStepId> recycleStepIds)
		{
			for (int idx = batch.Steps.Count - 1; idx >= 0; idx--)
			{
				JobStepDocument step = batch.Steps[idx];
				if (predicate(step))
				{
					recycleStepIds[new NodeRef(batch.GroupIdx, step.NodeIdx)] = step.Id;
					batch.Steps.RemoveAt(idx);
				}
			}
		}

		static void RemoveBatches(JobDocument job, Predicate<JobStepBatchDocument> predicate, Dictionary<int, JobStepBatchId> recycleBatchIds)
		{
			for (int idx = job.Batches.Count - 1; idx >= 0; idx--)
			{
				JobStepBatchDocument batch = job.Batches[idx];
				if (predicate(batch))
				{
					recycleBatchIds[batch.GroupIdx] = batch.Id;
					job.Batches.RemoveAt(idx);
				}
			}
		}

		/// <summary>
		/// Update the jobsteps for the given node graph 
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="logger">Logger for any changes</param>
		private static void CreateOrUpdateBatches(JobDocument job, IGraph graph, ILogger logger)
		{
			// Find the priorities of each node, incorporating all the per-step overrides
			Dictionary<INode, Priority> nodePriorities = new Dictionary<INode, Priority>();
			foreach (INodeGroup group in graph.Groups)
			{
				foreach (INode node in group.Nodes)
				{
					nodePriorities[node] = node.Priority;
				}
			}
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				INodeGroup group = graph.Groups[batch.GroupIdx];
				foreach (JobStepDocument step in batch.Steps)
				{
					if (step.Priority != null)
					{
						INode node = group.Nodes[step.NodeIdx];
						nodePriorities[node] = step.Priority.Value;
					}
				}
			}

			// Remove any steps and batches that haven't started yet, saving their ids so we can re-use them if we re-add them
			Dictionary<NodeRef, JobStepId> recycleStepIds = new Dictionary<NodeRef, JobStepId>();
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				RemoveSteps(batch, x => x.State == JobStepState.Waiting || x.State == JobStepState.Ready, recycleStepIds);
			}

			// Mark any steps in failed batches as skipped
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				if (batch.State == JobStepBatchState.Complete && batch.Error != JobStepBatchError.Incomplete)
				{
					foreach (JobStepDocument step in batch.Steps)
					{
						if (step.IsPending())
						{
							step.State = JobStepState.Skipped;
						}
					}
				}
			}

			// Remove any skipped nodes whose skipped state is no longer valid
			HashSet<INode> failedNodes = new HashSet<INode>();
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				INodeGroup group = graph.Groups[batch.GroupIdx];
				foreach (JobStepDocument step in batch.Steps)
				{
					INode node = group.Nodes[step.NodeIdx];
					if (step.Retry)
					{
						failedNodes.Remove(node);
					}
					else if (batch.State == JobStepBatchState.Complete && IsFatalBatchError(batch.Error))
					{
						failedNodes.Add(node);
					}
					else if (step.State == JobStepState.Skipped)
					{
						if (node.InputDependencies.Any(x => failedNodes.Contains(graph.GetNode(x))) || !CanRetryNode(job, batch.GroupIdx, step.NodeIdx))
						{
							failedNodes.Add(node);
						}
						else
						{
							failedNodes.Remove(node);
						}
					}
					else if (step.Outcome == JobStepOutcome.Failure)
					{
						failedNodes.Add(node);
					}
					else
					{
						failedNodes.Remove(node);
					}
				}
				RemoveSteps(batch, x => x.State == JobStepState.Skipped && !failedNodes.Contains(group.Nodes[x.NodeIdx]), recycleStepIds);
			}

			// Remove any batches which are now empty
			Dictionary<int, JobStepBatchId> recycleBatchIds = new Dictionary<int, JobStepBatchId>();
			RemoveBatches(job, x => x.Steps.Count == 0 && x.LeaseId == null && x.Error == JobStepBatchError.None, recycleBatchIds);

			// Find all the targets in this job
			HashSet<string> targets = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			if (job.AbortedByUserId == null)
			{
				foreach (string argument in job.Arguments)
				{
					if (argument.StartsWith(IJob.TargetArgumentPrefix, StringComparison.OrdinalIgnoreCase))
					{
						targets.UnionWith(argument.Substring(IJob.TargetArgumentPrefix.Length).Split(';'));
					}
				}
				targets.Add(IJob.SetupNodeName);
			}

			// Add all the referenced aggregates
			HashSet<INode> newNodesToExecute = new HashSet<INode>();
			foreach (IAggregate aggregate in graph.Aggregates)
			{
				if (targets.Contains(aggregate.Name))
				{
					newNodesToExecute.UnionWith(aggregate.Nodes.Select(x => graph.GetNode(x)));
				}
			}

			// Add any individual nodes
			foreach (INode node in graph.Groups.SelectMany(x => x.Nodes))
			{
				if (targets.Contains(node.Name))
				{
					newNodesToExecute.Add(node);
				}
			}

			// Also add any dependencies of these nodes
			for (int groupIdx = graph.Groups.Count - 1; groupIdx >= 0; groupIdx--)
			{
				INodeGroup group = graph.Groups[groupIdx];
				for (int nodeIdx = group.Nodes.Count - 1; nodeIdx >= 0; nodeIdx--)
				{
					INode node = group.Nodes[nodeIdx];
					if (newNodesToExecute.Contains(node))
					{
						foreach (NodeRef dependency in node.InputDependencies)
						{
							newNodesToExecute.Add(graph.GetNode(dependency));
						}
					}
				}
			}

			// Cancel any batches which are still running but are no longer required
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				if (batch.State == JobStepBatchState.Starting && batch.Steps.Count == 0)
				{
					// This batch is still starting but hasn't executed anything yet. Don't cancel it; we can still append to it.
				}
				else if (batch.State == JobStepBatchState.Starting || batch.State == JobStepBatchState.Running)
				{
					INodeGroup group = graph.Groups[batch.GroupIdx];
					if (!batch.Steps.Any(x => newNodesToExecute.Contains(group.Nodes[x.NodeIdx])))
					{
						logger.LogInformation("Job {JobId} batch {BatchId} (lease {LeaseId}) is being cancelled; {NodeCount} nodes are not set to be executed", job.Id, batch.Id, batch.LeaseId, batch.Steps.Count);
						foreach (JobStepDocument step in batch.Steps)
						{
							logger.LogInformation("Step {JobId}:{BatchId}:{StepId} is no longer needed ({NodeName})", job.Id, batch.Id, step.Id, group.Nodes[step.NodeIdx].Name);
						}
						batch.Error = JobStepBatchError.NoLongerNeeded;
					}
				}
			}

			// Remove all the nodes which have already succeeded
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				foreach (JobStepDocument step in batch.Steps)
				{
					if ((step.State == JobStepState.Running && !step.Retry)
						|| (step.State == JobStepState.Completed && !step.Retry)
						|| (step.State == JobStepState.Aborted && !step.Retry)
						|| (step.State == JobStepState.Skipped))
					{
						newNodesToExecute.Remove(graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx]);
					}
				}
			}

			// Re-add all the nodes that have input dependencies in the same group.
			for (int groupIdx = graph.Groups.Count - 1; groupIdx >= 0; groupIdx--)
			{
				INodeGroup group = graph.Groups[groupIdx];
				for (int nodeIdx = group.Nodes.Count - 1; nodeIdx >= 0; nodeIdx--)
				{
					INode node = group.Nodes[nodeIdx];
					if (newNodesToExecute.Contains(node))
					{
						foreach (NodeRef dependency in node.InputDependencies)
						{
							if (dependency.GroupIdx == groupIdx)
							{
								newNodesToExecute.Add(group.Nodes[dependency.NodeIdx]);
							}
						}
					}
				}
			}

			// Build a list of nodes which are currently set to be executed
			HashSet<INode> existingNodesToExecute = new HashSet<INode>();
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				INodeGroup group = graph.Groups[batch.GroupIdx];
				foreach (JobStepDocument step in batch.Steps)
				{
					if (!step.Retry)
					{
						INode node = group.Nodes[step.NodeIdx];
						existingNodesToExecute.Add(node);
					}
				}
			}

			// Figure out the existing batch for each group
			JobStepBatchDocument?[] appendToBatches = new JobStepBatchDocument?[graph.Groups.Count];
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				if (batch.CanBeAppendedTo())
				{
					INodeGroup group = graph.Groups[batch.GroupIdx];
					appendToBatches[batch.GroupIdx] = batch;
				}
			}

			// Invalidate all the entries for groups where we're too late to append new entries (ie. we need to execute an earlier node that wasn't executed previously)
			for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
			{
				INodeGroup group = graph.Groups[groupIdx];
				for (int nodeIdx = 0; nodeIdx < group.Nodes.Count; nodeIdx++)
				{
					INode node = group.Nodes[nodeIdx];
					if (newNodesToExecute.Contains(node) && !existingNodesToExecute.Contains(node))
					{
						IJobStepBatch? batch = appendToBatches[groupIdx];
						if (batch != null && batch.Steps.Count > 0)
						{
							IJobStep lastStep = batch.Steps[batch.Steps.Count - 1];
							if (nodeIdx <= lastStep.NodeIdx)
							{
								appendToBatches[groupIdx] = null;
							}
						}
					}
				}
			}

			// Create all the new jobsteps
			for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
			{
				INodeGroup group = graph.Groups[groupIdx];
				for (int nodeIdx = 0; nodeIdx < group.Nodes.Count; nodeIdx++)
				{
					INode node = group.Nodes[nodeIdx];
					if (newNodesToExecute.Contains(node))
					{
						JobStepBatchDocument? batch = appendToBatches[groupIdx];
						if (batch == null)
						{
							JobStepBatchId batchId;
							if (!recycleBatchIds.Remove(groupIdx, out batchId))
							{
								job.NextSubResourceId = job.NextSubResourceId.Next();
								batchId = new JobStepBatchId(job.NextSubResourceId);
							}

							batch = new JobStepBatchDocument(batchId, groupIdx);
							job.Batches.Add(batch);

							appendToBatches[groupIdx] = batch;
						}

						// Don't re-add nodes that have already executed in this batch. If we were missing a dependency, we would have already nulled out the entry in appendToBatches above; anything else
						// is already valid.
						if (batch.Steps.Count == 0 || nodeIdx > batch.Steps[^1].NodeIdx)
						{
							JobStepId stepId;
							if (!recycleStepIds.Remove(new NodeRef(groupIdx, nodeIdx), out stepId))
							{
								job.NextSubResourceId = job.NextSubResourceId.Next();
								stepId = new JobStepId(job.NextSubResourceId);
							}

							JobStepDocument step = new JobStepDocument(stepId, nodeIdx);
							batch.Steps.Add(step);
						}
					}
				}
			}

			// Find the priority of each node, propagating dependencies from dependent nodes
			for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
			{
				INodeGroup group = graph.Groups[groupIdx];
				for (int nodeIdx = 0; nodeIdx < group.Nodes.Count; nodeIdx++)
				{
					INode node = group.Nodes[nodeIdx];
					Priority nodePriority = nodePriorities[node];

					foreach (NodeRef dependencyRef in node.OrderDependencies)
					{
						INode dependency = graph.Groups[dependencyRef.GroupIdx].Nodes[dependencyRef.NodeIdx];
						if (nodePriorities[node] > nodePriority)
						{
							nodePriorities[dependency] = nodePriority;
						}
					}
				}
			}
			foreach (JobStepBatchDocument batch in job.Batches)
			{
				if (batch.Steps.Count > 0)
				{
					INodeGroup group = graph.Groups[batch.GroupIdx];
					Priority nodePriority = batch.Steps.Max(x => nodePriorities[group.Nodes[x.NodeIdx]]);
					batch.SchedulePriority = ((int)job.Priority * 10) + (int)nodePriority + 1; // Reserve '0' for none.
				}
			}

			// Check we're not running a node which doesn't allow retries more than once
			Dictionary<INode, int> nodeExecutionCount = new Dictionary<INode, int>();
			foreach (IJobStepBatch batch in job.Batches)
			{
				INodeGroup group = graph.Groups[batch.GroupIdx];
				foreach (IJobStep step in batch.Steps)
				{
					INode node = group.Nodes[step.NodeIdx];

					int count;
					nodeExecutionCount.TryGetValue(node, out count);

					if (!node.AllowRetry && count > 0)
					{
						throw new RetryNotAllowedException(node.Name);
					}

					nodeExecutionCount[node] = count + 1;
				}
			}
		}

		/// <summary>
		/// Test whether the nodes in a batch can still be run
		/// </summary>
		static bool IsFatalBatchError(JobStepBatchError error)
		{
			return error != JobStepBatchError.None && error != JobStepBatchError.Incomplete;
		}

		/// <summary>
		/// Tests whether a node can be retried again
		/// </summary>
		static bool CanRetryNode(JobDocument job, int groupIdx, int nodeIdx)
		{
			return job.RetriedNodes == null || job.RetriedNodes.Count(x => x.GroupIdx == groupIdx && x.NodeIdx == nodeIdx) < MaxRetries;
		}

		/// <summary>
		/// Gets the scheduling priority of this job
		/// </summary>
		/// <param name="job">Job to consider</param>
		public static int GetSchedulePriority(IJob job)
		{
			int newSchedulePriority = 0;
			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.State == JobStepBatchState.Ready)
				{
					newSchedulePriority = Math.Max(batch.SchedulePriority, newSchedulePriority);
				}
			}
			return newSchedulePriority;
		}

		/// <summary>
		/// Update the state of any jobsteps that are dependent on other jobsteps (eg. transition them from waiting to ready based on other steps completing)
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="updates">List of updates to the job</param>
		/// <param name="logger">Logger instance</param>
		static void RefreshDependentJobSteps(JobDocument job, IGraph graph, List<UpdateDefinition<JobDocument>> updates, ILogger logger)
		{
			// Update the batches
			UpdateDefinitionBuilder<JobDocument> updateBuilder = Builders<JobDocument>.Update;
			if (job.Batches != null)
			{
				Dictionary<INode, IJobStep> stepForNode = new Dictionary<INode, IJobStep>();
				for (int loopBatchIdx = 0; loopBatchIdx < job.Batches.Count; loopBatchIdx++)
				{
					int batchIdx = loopBatchIdx; // For lambda capture
					JobStepBatchDocument batch = job.Batches[batchIdx];

					for (int loopStepIdx = 0; loopStepIdx < batch.Steps.Count; loopStepIdx++)
					{
						int stepIdx = loopStepIdx; // For lambda capture
						JobStepDocument step = batch.Steps[stepIdx];

						JobStepState newState = step.State;
						JobStepOutcome newOutcome = step.Outcome;

						INode node = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx];
						if (newState == JobStepState.Waiting)
						{
							List<IJobStep> steps = GetDependentSteps(graph, node, stepForNode);
							if (steps.Any(x => x.AbortRequested || x.IsFailedOrSkipped()))
							{
								newState = JobStepState.Skipped;
								newOutcome = JobStepOutcome.Failure;
							}
							else if (!steps.Any(x => !x.AbortRequested && x.IsPending()))
							{
								logger.LogDebug("Transitioning job {JobId}, batch {BatchId}, step {StepId} to ready state ({Dependencies})", job.Id, batch.Id, step.Id, String.Join(", ", steps.Select(x => x.Id.ToString())));
								newState = JobStepState.Ready;
							}
						}

						if (newState != step.State)
						{
							step.State = newState;
							updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].State, newState));
						}

						if (newOutcome != step.Outcome)
						{
							step.Outcome = newOutcome;
							updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].Steps[stepIdx].Outcome, newOutcome));
						}

						stepForNode[node] = step;
					}

					if (batch.State == JobStepBatchState.Waiting || batch.State == JobStepBatchState.Ready)
					{
						DateTime? newReadyTime;
						JobStepBatchState newState = GetBatchState(job, graph, batch, stepForNode, out newReadyTime);
						if (batch.State != newState)
						{
							batch.State = newState;
							updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].State, batch.State));
						}
						if (batch.ReadyTime != newReadyTime)
						{
							batch.ReadyTime = newReadyTime;
							updates.Add(updateBuilder.Set(x => x.Batches[batchIdx].ReadyTime, batch.ReadyTime));
						}
					}
				}
			}
		}

		/// <summary>
		/// Updates the schedule priority of a job
		/// </summary>
		/// <param name="job"></param>
		/// <param name="updates"></param>
		static void RefreshJobPriority(JobDocument job, List<UpdateDefinition<JobDocument>> updates)
		{
			// Update the weighted priority for the job
			int newSchedulePriority = GetSchedulePriority(job);
			if (job.SchedulePriority != newSchedulePriority)
			{
				job.SchedulePriority = newSchedulePriority;
				updates.Add(Builders<JobDocument>.Update.Set(x => x.SchedulePriority, newSchedulePriority));
			}
		}

		/// <summary>
		/// Gets the steps that a node depends on
		/// </summary>
		/// <param name="graph">The graph for this job</param>
		/// <param name="node">The node to test</param>
		/// <param name="stepForNode">Map of node to step</param>
		/// <returns></returns>
		static List<IJobStep> GetDependentSteps(IGraph graph, INode node, Dictionary<INode, IJobStep> stepForNode)
		{
			List<IJobStep> steps = new List<IJobStep>();
			foreach (NodeRef orderDependencyRef in node.OrderDependencies)
			{
				IJobStep? step;
				if (stepForNode.TryGetValue(graph.GetNode(orderDependencyRef), out step))
				{
					steps.Add(step);
				}
			}
			return steps;
		}

		/// <summary>
		/// Gets the new state for a batch
		/// </summary>
		/// <param name="job">The job being executed</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="batch">List of nodes in the job</param>
		/// <param name="stepForNode">Array mapping each index to the appropriate step for that node</param>
		/// <param name="outReadyTimeUtc">Receives the time at which the batch was ready to execute</param>
		/// <returns>True if the batch is ready, false otherwise</returns>
		static JobStepBatchState GetBatchState(IJob job, IGraph graph, IJobStepBatch batch, Dictionary<INode, IJobStep> stepForNode, out DateTime? outReadyTimeUtc)
		{
			// Check if the batch is already complete
			if (batch.Steps.All(x => x.State == JobStepState.Skipped || x.State == JobStepState.Completed || x.State == JobStepState.Aborted))
			{
				outReadyTimeUtc = batch.ReadyTimeUtc;
				return JobStepBatchState.Complete;
			}

			// Get the dependencies for this batch to start. Some steps may be "after" dependencies that are optional parts of the graph.
			List<INode> nodeDependencies = batch.GetStartDependencies(graph.Groups).ToList();

			// Check if we're still waiting on anything
			DateTime readyTimeUtc = job.CreateTimeUtc;
			foreach (INode nodeDependency in nodeDependencies)
			{
				IJobStep? stepDependency;
				if (stepForNode.TryGetValue(nodeDependency, out stepDependency))
				{
					if (stepDependency.State != JobStepState.Completed && stepDependency.State != JobStepState.Skipped && stepDependency.State != JobStepState.Aborted)
					{
						outReadyTimeUtc = null;
						return JobStepBatchState.Waiting;
					}

					if (stepDependency.FinishTimeUtc != null && stepDependency.FinishTimeUtc.Value > readyTimeUtc)
					{
						readyTimeUtc = stepDependency.FinishTimeUtc.Value;
					}
				}
			}

			// Otherwise return the ready state
			outReadyTimeUtc = readyTimeUtc;
			return JobStepBatchState.Ready;
		}

		/// <inheritdoc/>
		public async Task UpgradeDocumentsAsync()
		{
			IAsyncCursor<JobDocument> cursor = await _jobs.Find(Builders<JobDocument>.Filter.Eq(x => x.UpdateTimeUtc, null)).ToCursorAsync();

			int numUpdated = 0;
			while (await cursor.MoveNextAsync())
			{
				foreach (JobDocument document in cursor.Current)
				{
					UpdateDefinition<JobDocument> update = Builders<JobDocument>.Update.Set(x => x.CreateTimeUtc, ((IJob)document).CreateTimeUtc).Set(x => x.UpdateTimeUtc, ((IJob)document).UpdateTimeUtc);
					await _jobs.UpdateOneAsync(Builders<JobDocument>.Filter.Eq(x => x.Id, document.Id), update);
					numUpdated++;
				}
				_logger.LogInformation("Updated {NumDocuments} documents", numUpdated);
			}
		}
	}
}
