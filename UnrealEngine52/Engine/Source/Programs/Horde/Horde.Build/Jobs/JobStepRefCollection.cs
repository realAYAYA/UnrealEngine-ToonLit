// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Logs;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Telemetry;
using Horde.Build.Utilities;
using HordeCommon;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Jobs
{
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;
	using TemplateId = StringId<ITemplateRef>;

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

			DateTime IJobStepRef.StartTimeUtc => StartTimeUtc ?? StartTime?.UtcDateTime ?? default;
			DateTime? IJobStepRef.FinishTimeUtc => FinishTimeUtc ?? FinishTime?.UtcDateTime;
			string IJobStepRef.NodeName => Name;
			bool IJobStepRef.UpdateIssues => UpdateIssues ?? false;

			public JobStepRef(JobStepRefId id, string jobName, string nodeName, StreamId streamId, TemplateId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepOutcome? outcome, bool updateIssues, int? lastSuccess, int? lastWarning, float batchWaitTime, float batchInitTime, DateTime jobStartTimeUtc, DateTime startTimeUtc, DateTime? finishTimeUtc)
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

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		/// <param name="telemetrySink">Telemetry sink</param>
		public JobStepRefCollection(MongoService mongoService, ITelemetrySink telemetrySink)
		{
			_jobStepRefs = mongoService.GetCollection<JobStepRef>("JobStepRefs", keys => keys.Ascending(x => x.StreamId).Ascending(x => x.TemplateId).Ascending(x => x.Name).Descending(x => x.Change));
			_telemetrySink = telemetrySink;
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef> InsertOrReplaceAsync(JobStepRefId id, string jobName, string stepName, StreamId streamId, TemplateId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepOutcome? outcome, bool updateIssues, int? lastSuccess, int? lastWarning, float waitTime, float initTime, DateTime jobStartTimeUtc, DateTime startTimeUtc, DateTime? finishTimeUtc)
		{
			JobStepRef newJobStepRef = new JobStepRef(id, jobName, stepName, streamId, templateId, change, logId, poolId, agentId, outcome, updateIssues, lastSuccess, lastWarning, waitTime, initTime, jobStartTimeUtc, startTimeUtc, finishTimeUtc);
			await _jobStepRefs.ReplaceOneAsync(Builders<JobStepRef>.Filter.Eq(x => x.Id, newJobStepRef.Id), newJobStepRef, new ReplaceOptions { IsUpsert = true });

			if (_telemetrySink.Enabled)
			{
				_telemetrySink.SendEvent("State.JobStepRef", new
				{
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
					Outcome = outcome,
					PoolId = poolId,
					StartTime = startTimeUtc,
					StreamId = streamId,
					TemplateId = templateId,
					UpdateIssues = updateIssues
				});
			}

			return newJobStepRef;
		}

		/// <inheritdoc/>
		public async Task<List<IJobStepRef>> GetStepsForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int? change, bool includeFailed, int count)
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

			List<JobStepRef> steps = await _jobStepRefs.Find(filter).SortByDescending(x => x.Change).ThenByDescending(x => x.StartTimeUtc).Limit(count).ToListAsync();
			return steps.ConvertAll<IJobStepRef>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef?> GetPrevStepForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int change, JobStepOutcome? outcome = null, bool? updateIssues = null)
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
