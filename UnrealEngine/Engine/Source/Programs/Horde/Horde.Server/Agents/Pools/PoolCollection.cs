// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Common;
using Horde.Server.Agents.Fleet;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Agents.Pools
{
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

			[BsonIgnoreIfNull]
			public AutoSdkConfig? AutoSdkConfig { get; set; }

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
			public TimeSpan? ShutdownIfDisabledGracePeriod { get; set;  }

			[BsonIgnoreIfNull]
			public ScaleResult? LastScaleResult { get; set; }

			[BsonIgnoreIfNull]
			public int? LastAgentCount { get; set; }
			
			[BsonIgnoreIfNull]
			public int? LastDesiredAgentCount { get; set; }

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
		public async Task<IPool> AddAsync(PoolId id, string name, AddPoolOptions options)
		{
			PoolDocument pool = new PoolDocument();
			pool.Id = id;
			pool.Name = name;
			pool.Condition = options.Condition;
			if (options.EnableAutoscaling != null)
			{
				pool.EnableAutoscaling = options.EnableAutoscaling.Value;
			}
			pool.MinAgents = options.MinAgents;
			pool.NumReserveAgents = options.NumReserveAgents;
			if (options.Properties != null)
			{
				pool.Properties = new Dictionary<string, string>(options.Properties);
			}

			pool.ConformInterval = options.ConformInterval;
			pool.ScaleOutCooldown = options.ScaleOutCooldown;
			pool.ScaleInCooldown = options.ScaleInCooldown;
			pool.SizeStrategy = options.SizeStrategy;
			pool.LeaseUtilizationSettings = options.LeaseUtilizationSettings;
			pool.JobQueueSettings = options.JobQueueSettings;
			pool.ComputeQueueAwsMetricSettings = options.ComputeQueueAwsMetricSettings;
			if (options.SizeStrategies != null) { pool.SizeStrategies = options.SizeStrategies; }
			if (options.FleetManagers != null) { pool.FleetManagers = options.FleetManagers; }
			
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
		public Task<IPool?> TryUpdateAsync(IPool pool, UpdatePoolOptions options)
		{
			TransactionBuilder<PoolDocument> transaction = new TransactionBuilder<PoolDocument>();
			if (options.Name != null)
			{
				transaction.Set(x => x.Name, options.Name);
			}
			if (options.Condition != null)
			{
				if (options.Condition.IsEmpty())
				{
					transaction.Unset(x => x.Condition!);
				}
				else
				{
					transaction.Set(x => x.Condition, options.Condition);
				}
			}
			if (options.EnableAutoscaling != null)
			{
				if (options.EnableAutoscaling.Value)
				{
					transaction.Unset(x => x.EnableAutoscaling);
				}
				else
				{
					transaction.Set(x => x.EnableAutoscaling, options.EnableAutoscaling.Value);
				}
			}
			if (options.MinAgents != null)
			{
				if (options.MinAgents.Value < 0)
				{
					transaction.Unset(x => x.MinAgents!);
				}
				else
				{
					transaction.Set(x => x.MinAgents, options.MinAgents.Value);
				}
			}
			if (options.NumReserveAgents != null)
			{
				if (options.NumReserveAgents.Value < 0)
				{
					transaction.Unset(x => x.NumReserveAgents!);
				}
				else
				{
					transaction.Set(x => x.NumReserveAgents, options.NumReserveAgents.Value);
				}
			}
			if (options.Workspaces != null)
			{
				transaction.Set(x => x.Workspaces, options.Workspaces);
			}
			if (options.AutoSdkConfig != null)
			{
				if (options.AutoSdkConfig == AutoSdkConfig.None)
				{
					transaction.Unset(x => x.AutoSdkConfig!);
				}
				else
				{
					transaction.Set(x => x.AutoSdkConfig, options.AutoSdkConfig);
				}
			}
			if (options.Properties != null)
			{
				transaction.UpdateDictionary(x => x.Properties, options.Properties);
			}
			if (options.ConformInterval != null)
			{
				if (options.ConformInterval.Value < TimeSpan.Zero)
				{
					transaction.Unset(x => x.ConformInterval!);
				}
				else
				{
					transaction.Set(x => x.ConformInterval, options.ConformInterval);
				}
			}
			if (options.LastScaleUpTime != null)
			{
				transaction.Set(x => x.LastScaleUpTime, options.LastScaleUpTime);
			}
			if (options.LastScaleDownTime != null)
			{
				transaction.Set(x => x.LastScaleDownTime, options.LastScaleDownTime);
			}
			if (options.ScaleOutCooldown != null)
			{
				transaction.Set(x => x.ScaleOutCooldown, options.ScaleOutCooldown);
			}
			if (options.ScaleInCooldown != null)
			{
				transaction.Set(x => x.ScaleInCooldown, options.ScaleInCooldown);
			}
			if (options.ShutdownIfDisabledGracePeriod != null)
			{
				transaction.Set(x => x.ShutdownIfDisabledGracePeriod, options.ShutdownIfDisabledGracePeriod);
			}
			if (options.LastScaleResult != null)
			{
				transaction.Set(x => x.LastScaleResult, options.LastScaleResult);
			}
			if (options.LastAgentCount != null)
			{
				transaction.Set(x => x.LastAgentCount, options.LastAgentCount);
			}
			if (options.LastDesiredAgentCount != null)
			{
				transaction.Set(x => x.LastDesiredAgentCount, options.LastDesiredAgentCount);
			}
			if (options.SizeStrategy != null)
			{
				transaction.Set(x => x.SizeStrategy, options.SizeStrategy);				
			}
			else if (options.UseDefaultStrategy != null && options.UseDefaultStrategy.Value)
			{
				transaction.Set(x => x.SizeStrategy, null);
			}
			if (options.SizeStrategies != null)
			{
				transaction.Set(x => x.SizeStrategies, options.SizeStrategies);				
			}
			if (options.FleetManagers != null)
			{
				transaction.Set(x => x.FleetManagers, options.FleetManagers);				
			}
			if (options.LeaseUtilizationSettings != null)
			{
				transaction.Set(x => x.LeaseUtilizationSettings, options.LeaseUtilizationSettings);
			}
			if (options.JobQueueSettings != null)
			{
				transaction.Set(x => x.JobQueueSettings, options.JobQueueSettings);
			}
			if (options.ComputeQueueAwsMetricSettings != null)
			{
				transaction.Set(x => x.ComputeQueueAwsMetricSettings, options.ComputeQueueAwsMetricSettings);
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
