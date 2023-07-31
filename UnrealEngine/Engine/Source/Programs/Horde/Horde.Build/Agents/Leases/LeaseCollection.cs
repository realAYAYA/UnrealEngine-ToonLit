// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Logs;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Agents.Leases
{
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using SessionId = ObjectId<ISession>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Collection of lease documents
	/// </summary>
	public class LeaseCollection : ILeaseCollection
	{
		/// <summary>
		/// Concrete implementation of a lease document
		/// </summary>
		class LeaseDocument : ILease
		{
			public LeaseId Id { get; set; }
			public string Name { get; set; }
			public AgentId AgentId { get; set; }
			public SessionId SessionId { get; set; }
			public StreamId? StreamId { get; set; }
			public PoolId? PoolId { get; set; }
			public LogId? LogId { get; set; }
			public DateTime StartTime { get; set; }
			public DateTime? FinishTime { get; set; }
			public byte[] Payload { get; set; }
			public LeaseOutcome Outcome { get; set; }

			[BsonIgnoreIfNull]
			public byte[]? Output { get; set; }

			ReadOnlyMemory<byte> ILease.Payload => Payload;
			ReadOnlyMemory<byte> ILease.Output => Output ?? Array.Empty<byte>();

			[BsonConstructor]
			private LeaseDocument()
			{
				Name = String.Empty;
				Payload = null!;
			}

			public LeaseDocument(LeaseId id, string name, AgentId agentId, SessionId sessionId, StreamId? streamId, PoolId? poolId, LogId? logId, DateTime startTime, byte[] payload)
			{
				Id = id;
				Name = name;
				AgentId = agentId;
				SessionId = sessionId;
				StreamId = streamId;
				PoolId = poolId;
				LogId = logId;
				StartTime = startTime;
				Payload = payload;
			}
		}
		
		readonly IMongoCollection<LeaseDocument> _leases;
		readonly MongoIndex<LeaseDocument> _finishTimeStartTimeCompoundIndex;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		public LeaseCollection(MongoService mongoService)
		{
			List<MongoIndex<LeaseDocument>> indexes = new List<MongoIndex<LeaseDocument>>();
			indexes.Add(keys => keys.Ascending(x => x.AgentId));
			indexes.Add(keys => keys.Ascending(x => x.SessionId));
			indexes.Add(keys => keys.Ascending(x => x.StartTime));
			indexes.Add(keys => keys.Ascending(x => x.FinishTime));
			indexes.Add(_finishTimeStartTimeCompoundIndex = MongoIndex.Create<LeaseDocument>(keys => keys.Ascending(x => x.FinishTime).Descending(x => x.StartTime)));

			_leases = mongoService.GetCollection<LeaseDocument>("Leases", indexes);
		}

		/// <inheritdoc/>
		public async Task<ILease> AddAsync(LeaseId id, string name, AgentId agentId, SessionId sessionId, StreamId? streamId, PoolId? poolId, LogId? logId, DateTime startTime, byte[] payload)
		{
			LeaseDocument lease = new LeaseDocument(id, name, agentId, sessionId, streamId, poolId, logId, startTime, payload);
			await _leases.ReplaceOneAsync(x => x.Id == id, lease, new ReplaceOptions { IsUpsert = true });
			return lease;
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(LeaseId leaseId)
		{
			await _leases.DeleteOneAsync(x => x.Id == leaseId);
		}

		/// <inheritdoc/>
		public async Task<ILease?> GetAsync(LeaseId leaseId)
		{
			return await _leases.Find(x => x.Id == leaseId).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<ILease>> FindLeasesAsync(AgentId? agentId, SessionId? sessionId, DateTime? minTime, DateTime? maxTime, int? index, int? count, string? indexHint = null, bool consistentRead = true)
		{
			IMongoCollection<LeaseDocument> collection = consistentRead ? _leases : _leases.WithReadPreference(ReadPreference.SecondaryPreferred);
			
			FilterDefinitionBuilder<LeaseDocument> filterBuilder = Builders<LeaseDocument>.Filter;
			FilterDefinition<LeaseDocument> filter = FilterDefinition<LeaseDocument>.Empty;
			if (agentId != null)
			{
				filter &= filterBuilder.Eq(x => x.AgentId, agentId.Value);
			}
			if (sessionId != null)
			{
				filter &= filterBuilder.Eq(x => x.SessionId, sessionId.Value);
			}
			if (minTime != null)
			{
				filter &= filterBuilder.Or(filterBuilder.Eq(x => x.FinishTime!, null), filterBuilder.Gt(x => x.FinishTime!, minTime.Value));
			}
			if (maxTime != null)
			{
				filter &= filterBuilder.Lt(x => x.StartTime, maxTime.Value);
			}

			List<LeaseDocument> results = await collection.FindWithHint(filter, indexHint, x => x.SortByDescending(x => x.StartTime).Range(index, count).ToListAsync());
			return results.ConvertAll<ILease>(x => x);
		}
		
		/// <inheritdoc/>
		public async Task<List<ILease>> FindLeasesByFinishTimeAsync(DateTime? minFinishTime, DateTime? maxFinishTime, int? index, int? count, string? indexHint, bool consistentRead)
		{
			IMongoCollection<LeaseDocument> collection = consistentRead ? _leases : _leases.WithReadPreference(ReadPreference.SecondaryPreferred);
			FilterDefinitionBuilder<LeaseDocument> filterBuilder = Builders<LeaseDocument>.Filter;
			FilterDefinition<LeaseDocument> filter = FilterDefinition<LeaseDocument>.Empty;

			if (minFinishTime == null && maxFinishTime == null)
			{
				throw new ArgumentException($"Both {nameof(minFinishTime)} and {nameof(maxFinishTime)} cannot be null");
			}

			if (minFinishTime != null)
			{
				filter &= filterBuilder.Gt(x => x.FinishTime, minFinishTime.Value);
			}
			if (maxFinishTime != null)
			{
				filter &= filterBuilder.Lt(x => x.FinishTime, maxFinishTime.Value);
			}

			List<LeaseDocument> results = await collection.FindWithHint(filter, indexHint, x => x.SortByDescending(x => x.FinishTime).Range(index, count).ToListAsync());
			return results.ConvertAll<ILease>(x => x);
		}

		/// <inheritdoc/>
		public async Task<List<ILease>> FindLeasesAsync(DateTime? minTime, DateTime? maxTime)
		{
			return await FindLeasesAsync(null, null, minTime, maxTime, null, null, _finishTimeStartTimeCompoundIndex.Name, false);
		}

		/// <inheritdoc/>
		public async Task<List<ILease>> FindActiveLeasesAsync(int? index = null, int? count = null)
		{
			List<LeaseDocument> results = await _leases.Find(x => x.FinishTime == null).Range(index, count).ToListAsync();
			return results.ConvertAll<ILease>(x => x);
		}

		/// <inheritdoc/>
		public async Task<bool> TrySetOutcomeAsync(LeaseId leaseId, DateTime finishTime, LeaseOutcome outcome, byte[]? output)
		{
			FilterDefinitionBuilder<LeaseDocument> filterBuilder = Builders<LeaseDocument>.Filter;
			FilterDefinition<LeaseDocument> filter = filterBuilder.Eq(x => x.Id, leaseId) & filterBuilder.Eq(x => x.FinishTime, null);

			UpdateDefinitionBuilder<LeaseDocument> updateBuilder = Builders<LeaseDocument>.Update;
			UpdateDefinition<LeaseDocument> update = updateBuilder.Set(x => x.FinishTime, finishTime).Set(x => x.Outcome, outcome);

			if(output != null && output.Length > 0)
			{
				update = update.Set(x => x.Output, output);
			}

			UpdateResult result = await _leases.UpdateOneAsync(filter, update);
			return result.ModifiedCount > 0;
		}
	}
}
