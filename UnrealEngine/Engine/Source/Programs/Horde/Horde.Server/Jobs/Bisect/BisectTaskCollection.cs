// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Users;
using Horde.Server.Utilities;
using HordeCommon;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Jobs.Bisect
{
	class BisectTaskCollection : IBisectTaskCollection
	{
		class BisectTaskDoc : IBisectTask
		{
			public BisectTaskId Id { get; set; }

			[BsonElement("running"), BsonIgnoreIfNull]
			public bool? Running // Used for sparse index
			{
				get => (State == BisectTaskState.Running)? true : null;
				set { }
			}

			[BsonElement("state")]
			public BisectTaskState State { get; set; }

			[BsonElement("oid")]
			public UserId OwnerId { get; set; }

			[BsonElement("sid")]
			public StreamId StreamId { get; set; }

			[BsonElement("tid")]
			public TemplateId TemplateId { get; set; }

			[BsonElement("step")]
			public string NodeName { get; set; } = String.Empty;

			[BsonElement("out")]
			public JobStepOutcome Outcome { get; set; }

			[BsonElement("job")]
			public JobId InitialJobId { get; set; }

			[BsonElement("chg")]
			public int InitialChange { get; set; }

			[BsonElement("curJob")]
			public JobId CurrentJobId { get; set; }

			[BsonElement("curChg")]
			public int CurrentChange { get; set; }

			[BsonElement("idx")]
			public int UpdateIdx { get; set; }

			[BsonElement("tags"), BsonIgnoreIfNull]
			public List<CommitTag>? CommitTags { get; set; }
			IReadOnlyList<CommitTag>? IBisectTask.CommitTags => CommitTags;

			[BsonElement("nochg")]
			public HashSet<int> IgnoreChanges { get; set; } = new HashSet<int>();
			IReadOnlySet<int> IBisectTask.IgnoreChanges => IgnoreChanges;

			[BsonElement("nojob")]
			public HashSet<JobId> IgnoreJobs { get; set; } = new HashSet<JobId>();
			IReadOnlySet<JobId> IBisectTask.IgnoreJobs => IgnoreJobs;
		}

		readonly IMongoCollection<BisectTaskDoc> _bisectTasks;

		public BisectTaskCollection(MongoService mongoService)
		{
			List<MongoIndex<BisectTaskDoc>> indexes = new List<MongoIndex<BisectTaskDoc>>();
			indexes.Add(keys => keys.Ascending(x => x.Id).Ascending(x => x.Running).Ascending(x => x.InitialJobId), sparse: true);

			_bisectTasks = mongoService.GetCollection<BisectTaskDoc>("BisectTasks", indexes);
		}

		/// <inheritdoc/>
		public async Task<IBisectTask> CreateAsync(IJob job, string nodeName, JobStepOutcome outcome, UserId ownerId, CreateBisectTaskOptions? options, CancellationToken cancellationToken = default)
		{
			BisectTaskDoc bisectTaskDoc = new BisectTaskDoc();
			bisectTaskDoc.Id = BisectTaskId.GenerateNewId();
			bisectTaskDoc.State = BisectTaskState.Running;
			bisectTaskDoc.OwnerId = ownerId;
			bisectTaskDoc.StreamId = job.StreamId;
			bisectTaskDoc.TemplateId = job.TemplateId;
			bisectTaskDoc.NodeName = nodeName;
			bisectTaskDoc.Outcome = outcome;
			bisectTaskDoc.InitialJobId = job.Id;
			bisectTaskDoc.InitialChange = job.Change;
			bisectTaskDoc.CurrentJobId = job.Id;
			bisectTaskDoc.CurrentChange = job.Change;

			if (options != null)
			{
				if (options.CommitTags != null && options.CommitTags.Count > 0)
				{
					bisectTaskDoc.CommitTags = new List<CommitTag>(options.CommitTags);
				}
				if (options.IgnoreChanges != null)
				{
					bisectTaskDoc.IgnoreChanges.UnionWith(options.IgnoreChanges);
				}
				if (options.IgnoreJobs != null)
				{
					bisectTaskDoc.IgnoreJobs.UnionWith(options.IgnoreJobs);
				}
			}

			await _bisectTasks.InsertOneAsync(bisectTaskDoc, cancellationToken: cancellationToken);

			return bisectTaskDoc;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<IBisectTask> FindActiveAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			using (IAsyncCursor<BisectTaskDoc> cursor = await _bisectTasks.Find(x => x.State == BisectTaskState.Running).SortBy(x => x.Id).ToCursorAsync(cancellationToken))
			{
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					foreach (BisectTaskDoc bisectTaskDoc in cursor.Current)
					{
						yield return bisectTaskDoc;
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IBisectTask?> GetAsync(BisectTaskId bisectTaskId, CancellationToken cancellationToken = default)
		{
			return await _bisectTasks.Find(x => x.Id == bisectTaskId).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IBisectTask>> FindAsync(JobId? jobId = null, CancellationToken cancellationToken = default)
		{
			// Find all the bisection tasks matching the given criteria
			FilterDefinitionBuilder<BisectTaskDoc> filterBuilder = Builders<BisectTaskDoc>.Filter;

			FilterDefinition<BisectTaskDoc> filter = FilterDefinition<BisectTaskDoc>.Empty;
			filter &= filterBuilder.Eq(x => x.InitialJobId, jobId);

			List<BisectTaskDoc> steps = await _bisectTasks.Find(filter).SortByDescending(x => x.InitialChange).ToListAsync(cancellationToken);
			return steps.ConvertAll<IBisectTask>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IBisectTask?> TryUpdateAsync(IBisectTask bisectTask, UpdateBisectTaskOptions options, CancellationToken cancellationToken = default)
		{
			BisectTaskDoc bisectTaskDoc = (BisectTaskDoc)bisectTask;

			UpdateDefinition<BisectTaskDoc> update = Builders<BisectTaskDoc>.Update.Inc(x => x.UpdateIdx, 1);
			if (options.CurrentJob != null)
			{
				update = update.Set(x => x.CurrentJobId, options.CurrentJob.Value.JobId).Set(x => x.CurrentChange, options.CurrentJob.Value.Change);
			}
			if (options.State != null)
			{
				update = update.Set(x => x.State, options.State.Value);
			}
		
			if (options.IncludeChanges != null && options.IncludeChanges.Count > 0)
			{
				update = update.PullAll(x => x.IgnoreChanges, options.IncludeChanges); 
			}
			else if (options.ExcludeChanges != null && options.ExcludeChanges.Count > 0)
			{
				update = update.AddToSetEach(x => x.IgnoreChanges, options.ExcludeChanges);
			}

			if (options.IncludeJobs != null && options.IncludeJobs.Count > 0)
			{
				update = update.PullAll(x => x.IgnoreJobs, options.IncludeJobs);
			}
			else if (options.ExcludeJobs != null && options.ExcludeJobs.Count > 0)
			{
				update = update.AddToSetEach(x => x.IgnoreJobs, options.ExcludeJobs);
			}

			FilterDefinition<BisectTaskDoc> filter = Builders<BisectTaskDoc>.Filter.Expr(x => x.Id == bisectTaskDoc.Id && x.UpdateIdx == bisectTaskDoc.UpdateIdx);
			return await _bisectTasks.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<BisectTaskDoc> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}
	}
}
