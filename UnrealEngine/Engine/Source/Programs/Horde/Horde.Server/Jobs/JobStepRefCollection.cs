// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Telemetry;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Options;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Collection of JobStepRef documents
	/// </summary>
	public class JobStepRefCollection : IJobStepRefCollection
	{
		class JobStepRef : IJobStepRef
		{
			[BsonId]
			public JobStepRefId Id { get; set; }

			public string JobName { get; set; } = "Unknown";

			public string Name { get; set; }

			public StreamId StreamId { get; set; }
			public TemplateId TemplateId { get; set; }
			public int Change { get; set; }
			public LogId? LogId { get; set; }
			public PoolId? PoolId { get; set; }
			public AgentId? AgentId { get; set; }
			public JobStepState? State { get; set; }
			public JobStepOutcome? Outcome { get; set; }

			[BsonIgnoreIfNull]
			public int? LastSuccess { get; }

			[BsonIgnoreIfNull]
			public int? LastWarning { get; }

			public float BatchWaitTime { get; set; }
			public float BatchInitTime { get; set; }

			public DateTime JobStartTimeUtc { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? StartTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? StartTimeUtc { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? FinishTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? FinishTimeUtc { get; set; }

			[BsonIgnoreIfNull]
			public bool? UpdateIssues { get; set; }

			[BsonIgnoreIfNull]
			public List<int>? IssueIds { get; set; }

			DateTime IJobStepRef.StartTimeUtc => StartTimeUtc ?? StartTime?.UtcDateTime ?? default;
			DateTime? IJobStepRef.FinishTimeUtc => FinishTimeUtc ?? FinishTime?.UtcDateTime;
			string IJobStepRef.NodeName => Name;
			bool IJobStepRef.UpdateIssues => UpdateIssues ?? false;
			IReadOnlyList<int>? IJobStepRef.IssueIds => IssueIds;

			public JobStepRef(JobStepRefId id, string jobName, string nodeName, StreamId streamId, TemplateId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepState? state, JobStepOutcome? outcome, bool updateIssues, int? lastSuccess, int? lastWarning, float batchWaitTime, float batchInitTime, DateTime jobStartTimeUtc, DateTime startTimeUtc, DateTime? finishTimeUtc)
			{
				Id = id;
				JobName = jobName;
				Name = nodeName;
				StreamId = streamId;
				TemplateId = templateId;
				Change = change;
				LogId = logId;
				PoolId = poolId;
				AgentId = agentId;
				State = state;
				Outcome = outcome;
				UpdateIssues = updateIssues;
				LastSuccess = lastSuccess;
				LastWarning = lastWarning;
				BatchWaitTime = batchWaitTime;
				BatchInitTime = batchInitTime;
				JobStartTimeUtc = jobStartTimeUtc;
				StartTimeUtc = startTimeUtc;
				FinishTimeUtc = finishTimeUtc;
			}
		}

		readonly IMongoCollection<JobStepRef> _jobStepRefs;
		readonly ITelemetrySink _telemetrySink;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobStepRefCollection(MongoService mongoService, ITelemetrySink telemetrySink, IOptionsMonitor<GlobalConfig> globalConfig)
		{
			List<MongoIndex<JobStepRef>> indexes = new List<MongoIndex<JobStepRef>>();
			indexes.Add(keys => keys.Ascending(x => x.StreamId).Ascending(x => x.TemplateId).Ascending(x => x.Name).Descending(x => x.Change));

			_jobStepRefs = mongoService.GetCollection<JobStepRef>("JobStepRefs", indexes);
			_telemetrySink = telemetrySink;
			_globalConfig = globalConfig;
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef> InsertOrReplaceAsync(JobStepRefId id, string jobName, string stepName, StreamId streamId, TemplateId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepState? state, JobStepOutcome? outcome, bool updateIssues, int? lastSuccess, int? lastWarning, float waitTime, float initTime, DateTime jobStartTimeUtc, DateTime startTimeUtc, DateTime? finishTimeUtc)
		{
			JobStepRef newJobStepRef = new JobStepRef(id, jobName, stepName, streamId, templateId, change, logId, poolId, agentId, state, outcome, updateIssues, lastSuccess, lastWarning, waitTime, initTime, jobStartTimeUtc, startTimeUtc, finishTimeUtc);
			await _jobStepRefs.ReplaceOneAsync(Builders<JobStepRef>.Filter.Eq(x => x.Id, newJobStepRef.Id), newJobStepRef, new ReplaceOptions { IsUpsert = true });

			if (_globalConfig.CurrentValue.TryGetStream(streamId, out StreamConfig? streamConfig) && !streamConfig.TelemetryStoreId.IsEmpty)
			{
				_telemetrySink.SendEvent(streamConfig.TelemetryStoreId, TelemetryRecordMeta.CurrentHordeInstance, new
				{
					EventName = "State.JobStepRef",
					Id = id.ToString(),
					JobId = id.JobId.ToString(),
					BatchId = id.BatchId.ToString(),
					StepId = id.StepId.ToString(),
					AgentId = agentId,
					BatchInitTime = initTime,
					BatchWaitTime = waitTime,
					Change = change,
					FinishTime = finishTimeUtc,
					JobName = jobName,
					JobStartTime = jobStartTimeUtc,
					StepName = stepName,
					State = state,
					Outcome = outcome,
					PoolId = poolId,
					StartTime = startTimeUtc,
					StreamId = streamId,
					TemplateId = templateId,
					UpdateIssues = updateIssues,
					Duration = (finishTimeUtc != null) ? (double?)(finishTimeUtc.Value - startTimeUtc).TotalSeconds : null
				});
			}

			return newJobStepRef;
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef?> UpdateAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId, List<int>? issueIds)
		{
			UpdateDefinitionBuilder<JobStepRef> updateBuilder = Builders<JobStepRef>.Update;
			List<UpdateDefinition<JobStepRef>> updates = new List<UpdateDefinition<JobStepRef>>();

			if (issueIds != null)
			{
				updates.Add(updateBuilder.Set(x => x.IssueIds, issueIds));
			}

			if (updates.Count == 0)
			{
				return await FindAsync(jobId, batchId, stepId);
			}

			JobStepRefId id = new JobStepRefId(jobId, batchId, stepId);
			return await _jobStepRefs.FindOneAndUpdateAsync(x => x.Id.Equals(id), updateBuilder.Combine(updates));
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef?> FindAsync(JobId jobId, JobStepBatchId batchId, JobStepId stepId)
		{
			JobStepRefId id = new JobStepRefId(jobId, batchId, stepId);
			return await _jobStepRefs.Find(x => x.Id.Equals(id)).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<IJobStepRef>> FindAsync(JobStepRefId[] ids, CancellationToken cancellationToken)
		{
			FilterDefinitionBuilder<JobStepRef> filterBuilder = Builders<JobStepRef>.Filter;
			FilterDefinition<JobStepRef> filter = FilterDefinition<JobStepRef>.Empty;
			filter &= filterBuilder.In(x => x.Id, ids);

			List<JobStepRef> steps = await _jobStepRefs.Find(filter).SortByDescending(x => x.Change).ToListAsync(cancellationToken);
			return steps.ConvertAll<IJobStepRef>(x => x);
		}

		/// <inheritdoc/>
		public async Task<List<IJobStepRef>> GetStepsForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int? change, bool includeFailed, int maxCount, CancellationToken cancellationToken)
		{
			// Find all the steps matching the given criteria
			FilterDefinitionBuilder<JobStepRef> filterBuilder = Builders<JobStepRef>.Filter;

			FilterDefinition<JobStepRef> filter = FilterDefinition<JobStepRef>.Empty;
			filter &= filterBuilder.Eq(x => x.StreamId, streamId);
			filter &= filterBuilder.Eq(x => x.TemplateId, templateId);
			filter &= filterBuilder.Eq(x => x.Name, nodeName);
			if (change != null)
			{
				filter &= filterBuilder.Lte(x => x.Change, change.Value);
			}
			if (!includeFailed)
			{
				filter &= filterBuilder.Ne(x => x.Outcome, JobStepOutcome.Failure);
			}

			List<JobStepRef> steps = await _jobStepRefs.Find(filter).SortByDescending(x => x.Change).ThenByDescending(x => x.StartTimeUtc).Limit(maxCount).ToListAsync(cancellationToken);
			return steps.ConvertAll<IJobStepRef>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef?> GetPrevStepForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int change, JobStepOutcome? outcome = null, bool? updateIssues = null, IEnumerable<JobId>? excludeJobIds = null)
		{
			FilterDefinitionBuilder<JobStepRef> filterBuilder = Builders<JobStepRef>.Filter;

			FilterDefinition<JobStepRef> filter = FilterDefinition<JobStepRef>.Empty;
			filter &= filterBuilder.Eq(x => x.StreamId, streamId);
			filter &= filterBuilder.Eq(x => x.TemplateId, templateId);
			filter &= filterBuilder.Eq(x => x.Name, nodeName);
			filter &= filterBuilder.Lt(x => x.Change, change);

			if (outcome != null)
			{
				filter &= filterBuilder.Eq(x => x.Outcome, outcome);
			}
			else
			{
				filter &= filterBuilder.Ne(x => x.Outcome, null);
			}

			if (updateIssues != null)
			{
				filter &= filterBuilder.Ne(x => x.UpdateIssues, false);
			}

			if (excludeJobIds != null && excludeJobIds.Any())
			{
				filter &= filterBuilder.Nin(x => x.Id.JobId, excludeJobIds);
			}

			return await _jobStepRefs.Find(filter).SortByDescending(x => x.Change).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef?> GetNextStepForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int change, JobStepOutcome? outcome = null, bool? updateIssues = null)
		{
			FilterDefinitionBuilder<JobStepRef> filterBuilder = Builders<JobStepRef>.Filter;

			FilterDefinition<JobStepRef> filter = FilterDefinition<JobStepRef>.Empty;
			filter &= filterBuilder.Eq(x => x.StreamId, streamId);
			filter &= filterBuilder.Eq(x => x.TemplateId, templateId);
			filter &= filterBuilder.Eq(x => x.Name, nodeName);
			filter &= filterBuilder.Gt(x => x.Change, change);

			if (outcome != null)
			{
				filter &= filterBuilder.Eq(x => x.Outcome, outcome);
			}
			else
			{
				filter &= filterBuilder.Ne(x => x.Outcome, null);
			}

			if (updateIssues != null)
			{
				filter &= filterBuilder.Ne(x => x.UpdateIssues, false);
			}

			return await _jobStepRefs.Find(filter).SortBy(x => x.Change).FirstOrDefaultAsync();
		}
	}
}
