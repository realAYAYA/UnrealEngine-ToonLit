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
			[BsonId]
			public PoolId Id { get; set; }

			[BsonRequired]
			public string Name { get; set; } = null!;

			[BsonIgnoreIfNull]
			public Condition? Condition { get; set; }

			public List<AgentWorkspace> Workspaces { get; set; } = new List<AgentWorkspace>();

			[BsonIgnoreIfDefault, BsonDefaultValue(true)]
			public bool UseAutoSdk { get; set; } = true;

			public Dictionary<string, string> Properties { get; set; } = new Dictionary<string, string>();

			[BsonIgnoreIfDefault, BsonDefaultValue(true)]
			public bool EnableAutoscaling { get; set; } = true;

			[BsonIgnoreIfNull]
			public int? MinAgents { get; set; }

			[BsonIgnoreIfNull]
			public int? NumReserveAgents { get; set; }

			[BsonIgnoreIfNull]
			public TimeSpan? ConformInterval { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? LastScaleUpTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? LastScaleDownTime { get; set; }

			[BsonIgnoreIfNull]
			public TimeSpan? ScaleOutCooldown { get; set; }

			[BsonIgnoreIfNull]
			public TimeSpan? ScaleInCooldown { get; set; }

			[BsonIgnoreIfNull]
			public PoolSizeStrategy? SizeStrategy { get; set; }

			[BsonIgnoreIfNull]
			public List<PoolSizeStrategyInfo>? SizeStrategies { get; set; }

			[BsonIgnoreIfNull]
			public List<FleetManagerInfo>? FleetManagers { get; set; }

			[BsonIgnoreIfNull]
			public LeaseUtilizationSettings? LeaseUtilizationSettings { get; set; }

			[BsonIgnoreIfNull]
			public JobQueueSettings? JobQueueSettings { get; set; }

			[BsonIgnoreIfNull]
			public ComputeQueueAwsMetricSettings? ComputeQueueAwsMetricSettings { get; set; }

			[BsonIgnoreIfNull]
			public string? Revision { get; set; }

			public int UpdateIndex { get; set; }

			// Read-only wrappers
			IReadOnlyList<AgentWorkspace> IPool.Workspaces => Workspaces;
			IReadOnlyDictionary<string, string> IPoolConfig.Properties => Properties;
			IReadOnlyList<PoolSizeStrategyInfo> IPoolConfig.SizeStrategies => (IReadOnlyList<PoolSizeStrategyInfo>?)SizeStrategies ?? Array.Empty<PoolSizeStrategyInfo>();
			IReadOnlyList<FleetManagerInfo> IPoolConfig.FleetManagers => (IReadOnlyList<FleetManagerInfo>?)FleetManagers ?? Array.Empty<FleetManagerInfo>();

			public PoolDocument()
			{
			}

			public PoolDocument(IPoolConfig other, string? otherRevision)
			{
				Id = other.Id;
				Name = String.IsNullOrEmpty(other.Name)? other.Id.ToString() : other.Name;
				Condition = other.Condition;
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
				if (other.SizeStrategies != null && other.SizeStrategies.Count > 0)
				{
					SizeStrategies = new List<PoolSizeStrategyInfo>(other.SizeStrategies);
				}
				if (other.FleetManagers != null && other.FleetManagers.Count > 0)
				{
					FleetManagers = new List<FleetManagerInfo>(other.FleetManagers);
				}
				LeaseUtilizationSettings = other.LeaseUtilizationSettings;
				JobQueueSettings = other.JobQueueSettings;
				ComputeQueueAwsMetricSettings = other.ComputeQueueAwsMetricSettings;
				Revision = otherRevision;
			}

			public PoolDocument(IPool other)
				: this(other, other.Revision)
			{
				Workspaces.AddRange(other.Workspaces);
				UpdateIndex = other.UpdateIndex;
			}
		}

		/// <summary>
		/// Collection of pool documents
		/// </summary>
		readonly IMongoCollection<PoolDocument> _pools;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		public PoolCollection(MongoService mongoService)
		{
			_pools = mongoService.GetCollection<PoolDocument>("Pools");
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
			List<PoolSizeStrategyInfo>? sizeStrategies,
			List<FleetManagerInfo>? fleetManagers,
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
			if (sizeStrategies != null) { pool.SizeStrategies = sizeStrategies; }
			if (fleetManagers != null) { pool.FleetManagers = fleetManagers; }
			
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
			List<PoolSizeStrategyInfo>? newSizeStrategies,
			List<FleetManagerInfo>? newFleetManagers,
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
			if (newSizeStrategies != null)
			{
				transaction.Set(x => x.SizeStrategies, newSizeStrategies);				
			}
			if (newFleetManagers != null)
			{
				transaction.Set(x => x.FleetManagers, newFleetManagers);				
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

		/// <inheritdoc/>
		public async Task ConfigureAsync(IReadOnlyList<(PoolConfig Config, string Revision)> poolConfigs)
		{
			List<PoolDocument> pools = await _pools.Find(FilterDefinition<PoolDocument>.Empty).ToListAsync();
			Dictionary<PoolId, PoolDocument> idToPool = pools.ToDictionary(x => x.Id, x => x);

			// Make sure we don't have any duplicates in the new pool configs before setting anything
			Dictionary<PoolId, string> poolIdToRevision = new Dictionary<PoolId, string>();
			foreach ((PoolConfig poolConfig, string revision) in poolConfigs)
			{
				if (poolIdToRevision.TryGetValue(poolConfig.Id, out string? prevRevision))
				{
					throw new PoolConflictException(poolConfig.Id, prevRevision, revision);
				}
				else
				{
					poolIdToRevision.Add(poolConfig.Id, revision);
				}
			}

			// Update all the pools which are still present
			foreach ((PoolConfig poolConfig, string revision) in poolConfigs)
			{
				idToPool.TryGetValue(poolConfig.Id, out PoolDocument? currentPool);
				while (currentPool == null || currentPool.Revision != revision)
				{
					int updateIndex = currentPool?.UpdateIndex ?? 0;

					PoolDocument newPool = new PoolDocument(poolConfig, revision);
					if (currentPool != null)
					{
						newPool.Workspaces.AddRange(currentPool.Workspaces);
						newPool.UpdateIndex = currentPool.UpdateIndex;
					}

					ReplaceOneResult result = await _pools.ReplaceOneAsync(x => x.Id == poolConfig.Id && x.UpdateIndex == updateIndex, newPool, new ReplaceOptions { IsUpsert = true });
					if (result.MatchedCount > 0)
					{
						break;
					}

					currentPool = await _pools.Find(x => x.Id == poolConfig.Id).FirstOrDefaultAsync();
				}
				idToPool.Remove(poolConfig.Id);
			}

			// Remove anything that has been deleted
			foreach (PoolDocument remainingPool in idToPool.Values)
			{
				PoolDocument? remainingPoolCopy = remainingPool;
				while (remainingPoolCopy != null && !String.IsNullOrEmpty(remainingPoolCopy.Revision))
				{
					DeleteResult result = await _pools.DeleteOneAsync(x => x.Id == remainingPool.Id && x.UpdateIndex == remainingPoolCopy.UpdateIndex);
					if (result.DeletedCount > 0)
					{
						break;
					}
					remainingPoolCopy = await _pools.Find(x => x.Id == remainingPool.Id).FirstOrDefaultAsync();
				}
			}
		}
	}
}
