// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Logs;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Streams;
using Horde.Build.Jobs;
using Horde.Build.Utilities;
using HordeCommon;

namespace Horde.Build.Tests.Stubs.Collections
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<ITemplateRef>;

	class JobStepRefStub : IJobStepRef
	{
		public JobStepRefId Id { get; set; }
		public string JobName { get; set; }
		public string NodeName { get; set; }
		public StreamId StreamId { get; set; }
		public TemplateRefId TemplateId { get; set; }
		public int Change { get; set; }
		public LogId? LogId { get; set; }
		public PoolId? PoolId { get; set; }
		public AgentId? AgentId { get; set; }
		public JobStepOutcome? Outcome { get; set; }
		public bool UpdateIssues { get; set; } = false;
		public int? LastSuccess { get; set; }
		public int? LastWarning { get; set; }


		public virtual float BatchWaitTime => throw new NotImplementedException();
		public virtual float BatchInitTime => throw new NotImplementedException();
		public virtual DateTime StartTimeUtc => throw new NotImplementedException();
		public virtual DateTime? FinishTimeUtc => throw new NotImplementedException();

		public JobStepRefStub(JobId jobId, SubResourceId batchId, SubResourceId stepId, string jobName, string nodeName, StreamId streamId, TemplateRefId templateId, int change, JobStepOutcome? outcome, bool updateIssues)
		{
			Id = new JobStepRefId(jobId, batchId, stepId);
			JobName = jobName;
			NodeName = nodeName;
			StreamId = streamId;
			TemplateId = templateId;
			Change = change;
			Outcome = outcome;
			UpdateIssues = updateIssues;
		}
	}

	class JobStepRefCollectionStub : IJobStepRefCollection
	{
		readonly List<IJobStepRef> _refs = new List<IJobStepRef>();

		public void Add(IJobStepRef jobStepRef)
		{
			_refs.Add(jobStepRef);
		}

		public Task<IJobStepRef?> GetNextStepForNodeAsync(StreamId streamId, TemplateRefId templateId, string nodeName, int change, JobStepOutcome? outcome = null, bool? updateIssues = null)
		{
			IJobStepRef? nextRef = null;
			foreach (IJobStepRef jobStepRef in _refs)
			{
				if (jobStepRef.StreamId == streamId && jobStepRef.TemplateId == templateId && jobStepRef.NodeName == nodeName && jobStepRef.Change > change && (outcome == null ? jobStepRef.Outcome != null : jobStepRef.Outcome == outcome) && (updateIssues != null ? jobStepRef.UpdateIssues == updateIssues : true) )
				{
					if (nextRef == null || jobStepRef.Change < nextRef.Change)
					{
						nextRef = jobStepRef;
					}
				}
			}
			return Task.FromResult(nextRef);
		}

		public Task<IJobStepRef?> GetPrevStepForNodeAsync(StreamId streamId, TemplateRefId templateId, string nodeName, int change, JobStepOutcome? outcome = null, bool? updateIssues = null)
		{
			IJobStepRef? prevRef = null;
			foreach (IJobStepRef jobStepRef in _refs)
			{
				if (jobStepRef.StreamId == streamId && jobStepRef.TemplateId == templateId && jobStepRef.NodeName == nodeName && jobStepRef.Change < change && (outcome == null ? jobStepRef.Outcome != null : jobStepRef.Outcome == outcome) && (updateIssues != null ? jobStepRef.UpdateIssues == updateIssues : true))
				{
					if (prevRef == null || jobStepRef.Change > prevRef.Change)
					{
						prevRef = jobStepRef;
					}
				}
			}
			return Task.FromResult(prevRef);
		}

		Task<List<IJobStepRef>> IJobStepRefCollection.GetStepsForNodeAsync(StreamId streamId, TemplateRefId templateId, string nodeName, int? change, bool includeFailed, int count)
		{
			throw new NotImplementedException();
		}

		Task<IJobStepRef> IJobStepRefCollection.InsertOrReplaceAsync(JobStepRefId id, string jobName, string nodeName, StreamId streamId, TemplateRefId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepOutcome? outcome, bool updateIssues, int? lastSuccess, int? lastWarning, float waitTime, float initTime, DateTime jobStartTime, DateTime startTime, DateTime? finishTime)
		{
			throw new NotImplementedException();
		}
	}
}
