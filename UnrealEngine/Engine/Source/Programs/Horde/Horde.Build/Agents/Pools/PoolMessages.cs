// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using EpicGames.Horde.Common;
using Horde.Build.Agents.Fleet;

namespace Horde.Build.Agents.Pools
{
	/// <see cref="LeaseUtilizationSettings" />
	public class LeaseUtilizationSettingsMessage
	{
		/// <summary>
		/// Construct a public REST API representation from the internal one
		/// </summary>
		/// <param name="settings"></param>
#pragma warning disable IDE0060 // Remove unused parameter
		public LeaseUtilizationSettingsMessage(LeaseUtilizationSettings settings)
#pragma warning restore IDE0060 // Remove unused parameter
		{
		}
		
		/// <summary>
		/// Convert this public REST API object to an internal representation
		/// </summary>
		/// <returns></returns>
		public LeaseUtilizationSettings Convert()
		{
			return new LeaseUtilizationSettings();
		}
	}
	
	/// <see cref="JobQueueSettings" />
	public class JobQueueSettingsMessage
	{
		/// <summary>
		/// Factor translating queue size to additional agents to grow the pool with
		/// The result is always rounded up to nearest integer. 
		/// Example: if there are 20 jobs in queue, a factor 0.25 will result in 5 new agents being added (20 * 0.25)
		/// </summary>
		public double ScaleOutFactor { get; set; }

		/// <summary>
		/// Factor by which to shrink the pool size with when queue is empty
		/// The result is always rounded up to nearest integer.
		/// Example: when the queue size is zero, a default value of 0.9 will shrink the pool by 10% (current agent count * 0.9)
		/// </summary>
		public double ScaleInFactor { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public JobQueueSettingsMessage()
		{
		}

		/// <summary>
		/// Construct a public REST API representation from the internal one
		/// </summary>
		/// <param name="settings"></param>
		public JobQueueSettingsMessage(JobQueueSettings settings)
		{
			ScaleInFactor = settings.ScaleInFactor;
			ScaleOutFactor = settings.ScaleOutFactor;
		}
		
		/// <summary>
		/// Convert this public REST API object to an internal representation
		/// </summary>
		/// <returns></returns>
		public JobQueueSettings Convert()
		{
			return new JobQueueSettings(ScaleOutFactor, ScaleInFactor);
		}
	}
	
	/// <see cref="ComputeQueueAwsMetricSettings" />
	public class ComputeQueueAwsMetricSettingsMessage
	{
		/// <summary>
		/// Compute cluster ID to observe
		/// </summary>
		public string ComputeClusterId { get; set; } = null!;

		/// <summary>
		/// AWS CloudWatch namespace to write metrics in
		/// </summary>
		public string Namespace { get; set; } = null!;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeQueueAwsMetricSettingsMessage()
		{
		}
		
		/// <summary>
		/// Construct a public REST API representation from the internal one
		/// </summary>
		/// <param name="settings"></param>
		public ComputeQueueAwsMetricSettingsMessage(ComputeQueueAwsMetricSettings settings)
		{
			ComputeClusterId = settings.ComputeClusterId;
			Namespace = settings.Namespace;
		}
		
		/// <summary>
		/// Convert this public REST API object to an internal representation
		/// </summary>
		/// <returns></returns>
		public ComputeQueueAwsMetricSettings Convert()
		{
			return new ComputeQueueAwsMetricSettings(ComputeClusterId, Namespace);
		}
	}
	
	/// <summary>
	/// Parameters to create a new pool
	/// </summary>
	public class CreatePoolRequest
	{
		/// <summary>
		/// Name for the new pool
		/// </summary>
		[Required]
		public string Name { get; set; } = null!;

		/// <summary>
		/// Condition to satisfy for agents to be included in this pool
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Whether to enable autoscaling for this pool
		/// </summary>
		public bool? EnableAutoscaling { get; set; }

		/// <summary>
		/// Interval between conforms in hours. Set to zero to disable.
		/// </summary>
		public int? ConformInterval { get; set; }
		
		/// <summary>
		/// Cooldown time between scale-out events in seconds
		/// </summary>
		public int? ScaleOutCooldown { get; set; }
		
		/// <summary>
		/// Cooldown time between scale-in events in seconds
		/// </summary>
		public int? ScaleInCooldown { get; set; }
		
		/// <summary>
		/// Pool sizing strategy
		/// </summary>
		public PoolSizeStrategy? SizeStrategy { get; set; }
		
		/// <summary>
		/// Settings for lease utilization pool sizing strategy (if used)
		/// </summary>
		public LeaseUtilizationSettingsMessage? LeaseUtilizationSettings { get; set; }
		
		/// <summary>
		/// Settings for job queue pool sizing strategy (if used) 
		/// </summary>
		public JobQueueSettingsMessage? JobQueueSettings { get; set; }
		
		/// <summary>
		/// Settings for compute queue pool sizing strategy for AWS metrics (if used) 
		/// </summary>
		public ComputeQueueAwsMetricSettingsMessage? ComputeQueueAwsMetricSettings { get; set; }

		/// <summary>
		/// The minimum number of agents to retain in this pool
		/// </summary>
		public int? MinAgents { get; set; }

		/// <summary>
		/// The minimum number of idle agents in this pool, if autoscaling is enabled
		/// </summary>
		public int? NumReserveAgents { get; set; }

		/// <summary>
		/// Properties for the new pool
		/// </summary>
		public Dictionary<string, string>? Properties { get; set; }
	}

	/// <summary>
	/// Response from creating a new pool
	/// </summary>
	public class CreatePoolResponse
	{
		/// <summary>
		/// Unique id for the new pool
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id for the new pool</param>
		public CreatePoolResponse(string id)
		{
			Id = id;
		}
	}

	/// <summary>
	/// Parameters to update a pool
	/// </summary>
	public class UpdatePoolRequest
	{
		/// <summary>
		/// Optional new name for the pool
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Requirements for this pool
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Whether to enable autoscaling for this pool
		/// </summary>
		public bool? EnableAutoscaling { get; set; }

		/// <summary>
		/// Interval between conforms in hours. Set to -1 to reset to the default, or 0 to disable.
		/// </summary>
		public int? ConformInterval { get; set; }
		
		/// <summary>
		/// Cooldown time between scale-out events in seconds
		/// </summary>
		public int? ScaleOutCooldown { get; set; }
		
		/// <summary>
		/// Cooldown time between scale-in events in seconds
		/// </summary>
		public int? ScaleInCooldown { get; set; }
		
		/// <summary>
		/// Pool sizing strategy
		/// </summary>
		public PoolSizeStrategy? SizeStrategy { get; set; }

		/// <summary>
		/// Set pool to use default strategy
		/// </summary>
		public bool? UseDefaultStrategy { get; set; }

		/// <summary>
		/// Settings for lease utilization pool sizing strategy (if used)
		/// </summary>
		public LeaseUtilizationSettingsMessage? LeaseUtilizationSettings { get; set; }
		
		/// <summary>
		/// Settings for job queue pool sizing strategy (if used) 
		/// </summary>
		public JobQueueSettingsMessage? JobQueueSettings { get; set; }
		
		/// <summary>
		/// Settings for compute queue AWS metric pool sizing strategy (if used) 
		/// </summary>
		public ComputeQueueAwsMetricSettingsMessage? ComputeQueueAwsMetricSettings { get; set; }

		/// <summary>
		/// The minimum number of agents to retain in this pool
		/// </summary>
		public int? MinAgents { get; set; }

		/// <summary>
		/// The minimum number of idle agents in this pool, if autoscaling is enabled
		/// </summary>
		public int? NumReserveAgents { get; set; }

		/// <summary>
		/// Properties to update for the pool. Properties set to null will be removed.
		/// </summary>
		public Dictionary<string, string?>? Properties { get; set; }
	}

	/// <summary>
	/// Parameters to update a pool
	/// </summary>
	public class BatchUpdatePoolRequest
	{
		/// <summary>
		///  ID of the pool to update
		/// </summary>
		[Required]
		public string Id { get; set; } = null!;

		/// <summary>
		/// Optional new name for the pool
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Properties to update for the pool. Properties set to null will be removed.
		/// </summary>
		public Dictionary<string, string?>? Properties { get; set; }
	}

	/// <summary>
	/// Response describing a pool
	/// </summary>
	public class GetPoolResponse
	{
		/// <summary>
		/// Unique id of the pool
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Name of the pool
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Condition for agents to be auto-added to the pool
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Whether to enable autoscaling for this pool
		/// </summary>
		public bool EnableAutoscaling { get; set; }
		
		/// <summary>
		/// Frequency to run conforms, in hours.
		/// </summary>
		public int? ConformInterval { get; set; }

		/// <summary>
		/// Cooldown time between scale-out events in seconds
		/// </summary>
		public int? ScaleOutCooldown { get; set; }
		
		/// <summary>
		/// Cooldown time between scale-in events in seconds
		/// </summary>
		public int? ScaleInCooldown { get; set; }
		
		/// <summary>
		/// Pool sizing strategy to be used for this pool
		/// </summary>
		public PoolSizeStrategy? SizeStrategy { get; set; }
		
		/// <summary>
		/// Settings for lease utilization pool sizing strategy (if used)
		/// </summary>
		public LeaseUtilizationSettingsMessage? LeaseUtilizationSettings { get; set; }
		
		/// <summary>
		/// Settings for job queue pool sizing strategy (if used) 
		/// </summary>
		public JobQueueSettingsMessage? JobQueueSettings { get; set; }

		/// <summary>
		/// Settings for compute queue pool sizing strategy for AWS metrics (if used) 
		/// </summary>
		public ComputeQueueAwsMetricSettingsMessage? ComputeQueueAwsMetricSettings { get; set; }

		/// <summary>
		/// The minimum nunmber of agents to retain in this pool
		/// </summary>
		public int? MinAgents { get; set; }

		/// <summary>
		/// The minimum number of idle agents in this pool, if autoscaling is enabled
		/// </summary>
		public int? NumReserveAgents { get; set; }

		/// <summary>
		/// List of workspaces that this agent contains
		/// </summary>
		public List<GetAgentWorkspaceResponse> Workspaces { get; set; }

		/// <summary>
		/// Arbitrary properties for this pool.
		/// </summary>
		public IReadOnlyDictionary<string, string> Properties { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="pool">The pool to construct from</param>
		public GetPoolResponse(IPool pool)
		{
			Id = pool.Id.ToString();
			Name = pool.Name;
			Condition = pool.Condition;
			EnableAutoscaling = pool.EnableAutoscaling;
			ConformInterval = pool.ConformInterval == null ? null : (int)pool.ConformInterval.Value.TotalHours;
			ScaleOutCooldown = pool.ScaleOutCooldown == null ? null : (int)pool.ScaleOutCooldown.Value.TotalSeconds;
			ScaleInCooldown = pool.ScaleInCooldown == null ? null : (int)pool.ScaleInCooldown.Value.TotalSeconds;
			SizeStrategy = pool.SizeStrategy;
			LeaseUtilizationSettings = pool.LeaseUtilizationSettings == null ? null : new LeaseUtilizationSettingsMessage(pool.LeaseUtilizationSettings);
			JobQueueSettings = pool.JobQueueSettings == null ? null : new JobQueueSettingsMessage(pool.JobQueueSettings);
			ComputeQueueAwsMetricSettings = pool.ComputeQueueAwsMetricSettings == null ? null : new ComputeQueueAwsMetricSettingsMessage(pool.ComputeQueueAwsMetricSettings);
			MinAgents = pool.MinAgents;
			NumReserveAgents = pool.NumReserveAgents;
			Workspaces = pool.Workspaces.Select(x => new GetAgentWorkspaceResponse(x)).ToList();
			Properties = pool.Properties;
		}
	}
}
