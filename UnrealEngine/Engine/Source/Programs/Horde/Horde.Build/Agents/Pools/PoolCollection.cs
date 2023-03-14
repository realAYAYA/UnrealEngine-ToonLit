// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Common;
using Horde.Build.Agents.Fleet;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Agents.Pools
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Collection of pool documents
	/// </summary>
	public class PoolCollection : IPoolCollection
	{
		class PoolDocument : IPool
		{
			// Properties
			[BsonId]
			public PoolId Id { get; set; }
			[BsonRequired]
			public string Name { get; set; } = null!;
			[BsonIgnoreIfNull]
			public Condition? Condition { get; set; }
			public List<AgentWorkspace> Workspaces { get; set; } = new List<AgentWorkspace>();
			public bool UseAutoSdk { get; set; } = true;
			public Dictionary<string, string> Properties { get; set; } = new Dictionary<string, string>();
			public bool EnableAutoscaling { get; set; } = true;
			public int? MinAgents { get; set; }
			public int? NumReserveAgents { get; set; }
			public TimeSpan? ConformInterval { get; set; }
			public DateTime? LastScaleUpTime { get; set; }
			public DateTime? LastScaleDownTime { get; set; }
			public TimeSpan? ScaleOutCooldown { get; set; }
			public TimeSpan? ScaleInCooldown { get; set; }
			public PoolSizeStrategy? SizeStrategy { get; set; }
			PoolSizeStrategy IPool.SizeStrategy => SizeStrategy ?? defaultStrategy!.Value;
			public LeaseUtilizationSettings? LeaseUtilizationSettings { get; set; }
			public JobQueueSettings? JobQueueSettings { get; set; }
			public ComputeQueueAwsMetricSettings? ComputeQueueAwsMetricSettings { get; set; }
			public int UpdateIndex { get; set; }

			// Read-only wrappers
			IReadOnlyList<AgentWorkspace> IPool.Workspaces => Workspaces;
			IReadOnlyDictionary<string, string> IPool.Properties => Properties;			

			public PoolDocument()
			{
			}

			public PoolDocument(IPool other)
			{
				Id = other.Id;
				Name = other.Name;
				Condition = other.Condition;
				Workspaces.AddRange(other.Workspaces);
				UseAutoSdk = other.UseAutoSdk;
				Properties = new Dictionary<string, string>(other.Properties);
				EnableAutoscaling = other.EnableAutoscaling;
				MinAgents = other.MinAgents;
				NumReserveAgents = other.NumReserveAgents;
				ConformInterval = other.ConformInterval;
				LastScaleUpTime = other.LastScaleUpTime;
				LastScaleDownTime = other.LastScaleDownTime;
				ScaleOutCooldown = other.ScaleOutCooldown;
				ScaleInCooldown = other.ScaleInCooldown;
				SizeStrategy = other.SizeStrategy;
				LeaseUtilizationSettings = other.LeaseUtilizationSettings;
				JobQueueSettings = other.JobQueueSettings;
				ComputeQueueAwsMetricSettings = other.ComputeQueueAwsMetricSettings;
				UpdateIndex = other.UpdateIndex;
			}
		}

		/// <summary>
		/// Collection of pool documents
		/// </summary>
		readonly IMongoCollection<PoolDocument> _pools;

		static PoolSizeStrategy? defaultStrategy;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		/// <param name="serverSettings">The server settings</param>
		public PoolCollection(MongoService mongoService, IOptions<ServerSettings> serverSettings)
		{
			_pools = mongoService.GetCollection<PoolDocument>("Pools");
			defaultStrategy = serverSettings.Value.DefaultAgentPoolSizeStrategy;
		}

		/// <inheritdoc/>
		public async Task<IPool> AddAsync(
			PoolId id,
			string name,
			Condition? condition,
			bool? enableAutoscaling,
			int? minAgents,
			int? numReserveAgents,
			TimeSpan? conformInterval,
			TimeSpan? scaleOutCooldown,
			TimeSpan? scaleInCooldown,
			PoolSizeStrategy? sizeStrategy,
			LeaseUtilizationSettings? leaseUtilizationSettings,
			JobQueueSettings? jobQueueSettings,
			ComputeQueueAwsMetricSettings? computeQueueAwsMetricSettings,
			IEnumerable<KeyValuePair<string, string>>? properties)
		{
			PoolDocument pool = new PoolDocument();
			pool.Id = id;
			pool.Name = name;
			pool.Condition = condition;
			if (enableAutoscaling != null)
			{
				pool.EnableAutoscaling = enableAutoscaling.Value;
			}
			pool.MinAgents = minAgents;
			pool.NumReserveAgents = numReserveAgents;
			if (properties != null)
			{
				pool.Properties = new Dictionary<string, string>(properties);
			}

			pool.ConformInterval = conformInterval;
			pool.ScaleOutCooldown = scaleOutCooldown;
			pool.ScaleInCooldown = scaleInCooldown;
			pool.SizeStrategy = sizeStrategy;
			pool.LeaseUtilizationSettings = leaseUtilizationSettings;
			pool.JobQueueSettings = jobQueueSettings;
			pool.ComputeQueueAwsMetricSettings = computeQueueAwsMetricSettings;
			await _pools.InsertOneAsync(pool);
			return pool;
		}

		/// <inheritdoc/>
		public async Task<List<IPool>> GetAsync()
		{
			List<PoolDocument> results = await _pools.Find(FilterDefinition<PoolDocument>.Empty).ToListAsync();
			return results.ConvertAll<IPool>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IPool?> GetAsync(PoolId id)
		{
			return await _pools.Find(x => x.Id == id).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<PoolId>> GetPoolIdsAsync()
		{
			ProjectionDefinition<PoolDocument, BsonDocument> projection = Builders<PoolDocument>.Projection.Include(x => x.Id);
			List<BsonDocument> results = await _pools.Find(FilterDefinition<PoolDocument>.Empty).Project(projection).ToListAsync();
			return results.ConvertAll(x => new PoolId(x.GetElement("_id").Value.AsString));
		}

		/// <inheritdoc/>
		public async Task<bool> DeleteAsync(PoolId id)
		{
			FilterDefinition<PoolDocument> filter = Builders<PoolDocument>.Filter.Eq(x => x.Id, id);
			DeleteResult result = await _pools.DeleteOneAsync(filter);
			return result.DeletedCount > 0;
		}

		/// <summary>
		/// Attempts to update a pool, upgrading it to the latest document schema if necessary
		/// </summary>
		/// <param name="poolToUpdate">Interface for the document to update</param>
		/// <param name="transaction">The transaction to execute</param>
		/// <returns>New pool definition, or null on failure.</returns>
		async Task<IPool?> TryUpdateAsync(IPool poolToUpdate, TransactionBuilder<PoolDocument> transaction)
		{
			if (transaction.IsEmpty)
			{
				return poolToUpdate;
			}

			transaction.Set(x => x.UpdateIndex, poolToUpdate.UpdateIndex + 1);

			PoolDocument? mutablePool = poolToUpdate as PoolDocument;
			if (mutablePool != null)
			{
				UpdateResult result = await _pools.UpdateOneAsync(x => x.Id == poolToUpdate.Id && x.UpdateIndex == poolToUpdate.UpdateIndex, transaction.ToUpdateDefinition());
				if (result.ModifiedCount == 0)
				{
					return null;
				}

				transaction.ApplyTo(mutablePool);
			}
			else
			{
				mutablePool = new PoolDocument(poolToUpdate);
				transaction.ApplyTo(mutablePool);

				ReplaceOneResult result = await _pools.ReplaceOneAsync<PoolDocument>(x => x.Id == poolToUpdate.Id && x.UpdateIndex == poolToUpdate.UpdateIndex, mutablePool);
				if (result.ModifiedCount == 0)
				{
					return null;
				}
			}
			return mutablePool;
		}

		/// <inheritdoc/>
		public Task<IPool?> TryUpdateAsync(
			IPool pool,
			string? newName,
			Condition? newCondition,
			bool? newEnableAutoscaling,
			int? newMinAgents,
			int? newNumReserveAgents,
			List<AgentWorkspace>? newWorkspaces,
			bool? newUseAutoSdk,
			Dictionary<string, string?>? newProperties,
			TimeSpan? conformInterval,
			DateTime? lastScaleUpTime,
			DateTime? lastScaleDownTime,
			TimeSpan? scaleOutCooldown,
			TimeSpan? scaleInCooldown, 
			PoolSizeStrategy? sizeStrategy,
			LeaseUtilizationSettings? leaseUtilizationSettings,
			JobQueueSettings? jobQueueSettings, 
			ComputeQueueAwsMetricSettings? computeQueueAwsMetricSettings, 
			bool? useDefaultStrategy)
		{
			TransactionBuilder<PoolDocument> transaction = new TransactionBuilder<PoolDocument>();
			if (newName != null)
			{
				transaction.Set(x => x.Name, newName);
			}
			if (newCondition != null)
			{
				if (newCondition.IsEmpty())
				{
					transaction.Unset(x => x.Condition!);
				}
				else
				{
					transaction.Set(x => x.Condition, newCondition);
				}
			}
			if (newEnableAutoscaling != null)
			{
				if (newEnableAutoscaling.Value)
				{
					transaction.Unset(x => x.EnableAutoscaling);
				}
				else
				{
					transaction.Set(x => x.EnableAutoscaling, newEnableAutoscaling.Value);
				}
			}
			if (newMinAgents != null)
			{
				if (newMinAgents.Value < 0)
				{
					transaction.Unset(x => x.MinAgents!);
				}
				else
				{
					transaction.Set(x => x.MinAgents, newMinAgents.Value);
				}
			}
			if (newNumReserveAgents != null)
			{
				if (newNumReserveAgents.Value < 0)
				{
					transaction.Unset(x => x.NumReserveAgents!);
				}
				else
				{
					transaction.Set(x => x.NumReserveAgents, newNumReserveAgents.Value);
				}
			}
			if (newWorkspaces != null)
			{
				transaction.Set(x => x.Workspaces, newWorkspaces);
			}
			if (newUseAutoSdk != null)
			{
				transaction.Set(x => x.UseAutoSdk, newUseAutoSdk.Value);
			}
			if (newProperties != null)
			{
				transaction.UpdateDictionary(x => x.Properties, newProperties);
			}
			if (conformInterval != null)
			{
				if (conformInterval.Value < TimeSpan.Zero)
				{
					transaction.Unset(x => x.ConformInterval!);
				}
				else
				{
					transaction.Set(x => x.ConformInterval, conformInterval);
				}
			}
			if (lastScaleUpTime != null)
			{
				transaction.Set(x => x.LastScaleUpTime, lastScaleUpTime);
			}
			if (lastScaleDownTime != null)
			{
				transaction.Set(x => x.LastScaleDownTime, lastScaleDownTime);
			}
			if (scaleOutCooldown != null)
			{
				transaction.Set(x => x.ScaleOutCooldown, scaleOutCooldown);
			}
			if (scaleInCooldown != null)
			{
				transaction.Set(x => x.ScaleInCooldown, scaleInCooldown);
			}
			if (sizeStrategy != null)
			{
				transaction.Set(x => x.SizeStrategy, sizeStrategy);				
			}
			else if (useDefaultStrategy != null && useDefaultStrategy.Value)
			{
				transaction.Set(x => x.SizeStrategy, null);
			}

			if (leaseUtilizationSettings != null)
			{
				transaction.Set(x => x.LeaseUtilizationSettings, leaseUtilizationSettings);
			}
			if (jobQueueSettings != null)
			{
				transaction.Set(x => x.JobQueueSettings, jobQueueSettings);
			}
			if (computeQueueAwsMetricSettings != null)
			{
				transaction.Set(x => x.ComputeQueueAwsMetricSettings, computeQueueAwsMetricSettings);
			}
			return TryUpdateAsync(pool, transaction);
		}
	}
}
