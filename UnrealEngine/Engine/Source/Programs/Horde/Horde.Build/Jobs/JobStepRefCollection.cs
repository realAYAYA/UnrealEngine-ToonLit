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
using Horde.Build.Utilities;
using HordeCommon;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Jobs
{
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;

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
			public TemplateRefId TemplateId { get; set; }
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

			[BsonIgnoreIfNull]
			public DateTimeOffset? StartTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? StartTimeUtc { get; set; }

			[BsonIgnoreIfNull]
			public DateTimeOffset? FinishTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? FinishTimeUtc { get; set; }

			DateTime IJobStepRef.StartTimeUtc => StartTimeUtc ?? StartTime?.UtcDateTime ?? default;
			DateTime? IJobStepRef.FinishTimeUtc => FinishTimeUtc ?? FinishTime?.UtcDateTime;
			string IJobStepRef.NodeName => Name;

			public JobStepRef(JobStepRefId id, string jobName, string nodeName, StreamId streamId, TemplateRefId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepOutcome? outcome, int? lastSuccess, int? lastWarning, float batchWaitTime, float batchInitTime, DateTime startTimeUtc, DateTime? finishTimeUtc)
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
				LastSuccess = lastSuccess;
				LastWarning = lastWarning;
				BatchWaitTime = batchWaitTime;
				BatchInitTime = batchInitTime;
				StartTime = startTimeUtc;
				StartTimeUtc = startTimeUtc;
				FinishTime = finishTimeUtc;
				FinishTimeUtc = finishTimeUtc;
			}
		}

		readonly IMongoCollection<JobStepRef> _jobStepRefs;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		public JobStepRefCollection(MongoService mongoService)
		{
			_jobStepRefs = mongoService.GetCollection<JobStepRef>("JobStepRefs", keys => keys.Ascending(x => x.StreamId).Ascending(x => x.TemplateId).Ascending(x => x.Name).Descending(x => x.Change));
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef> InsertOrReplaceAsync(JobStepRefId id, string jobName, string stepName, StreamId streamId, TemplateRefId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepOutcome? outcome, int? lastSuccess, int? lastWarning, float waitTime, float initTime, DateTime startTimeUtc, DateTime? finishTimeUtc)
		{
			JobStepRef newJobStepRef = new JobStepRef(id, jobName, stepName, streamId, templateId, change, logId, poolId, agentId, outcome, lastSuccess, lastWarning, waitTime, initTime, startTimeUtc, finishTimeUtc);
			await _jobStepRefs.ReplaceOneAsync(Builders<JobStepRef>.Filter.Eq(x => x.Id, newJobStepRef.Id), newJobStepRef, new ReplaceOptions { IsUpsert = true });
			return newJobStepRef;
		}

		/// <inheritdoc/>
		public async Task<List<IJobStepRef>> GetStepsForNodeAsync(StreamId streamId, TemplateRefId templateId, string nodeName, int? change, bool includeFailed, int count)
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

			List<JobStepRef> steps = await _jobStepRefs.Find(filter).SortByDescending(x => x.Change).ThenByDescending(x => x.StartTime).Limit(count).ToListAsync();
			return steps.ConvertAll<IJobStepRef>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef?> GetPrevStepForNodeAsync(StreamId streamId, TemplateRefId templateId, string nodeName, int change)
		{
			return await _jobStepRefs.Find(x => x.StreamId == streamId && x.TemplateId == templateId && x.Name == nodeName && x.Change < change && x.Outcome != null).SortByDescending(x => x.Change).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<IJobStepRef?> GetNextStepForNodeAsync(StreamId streamId, TemplateRefId templateId, string nodeName, int change)
		{
			return await _jobStepRefs.Find(x => x.StreamId == streamId && x.TemplateId == templateId && x.Name == nodeName && x.Change > change && x.Outcome != null).SortBy(x => x.Change).FirstOrDefaultAsync();
		}
	}
}
