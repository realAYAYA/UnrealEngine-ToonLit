// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Common;
using Horde.Server.Agents.Fleet;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Metadata for configuring and picking a pool sizing strategy
	/// </summary>
	public class PoolSizeStrategyInfo
	{
		/// <summary>
		/// Strategy implementation to use
		/// </summary>
		public PoolSizeStrategy Type { get; set; }

		/// <summary>
		/// Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Configuration for the strategy, serialized as JSON
		/// </summary>
		public string Config { get; set; } = "";

		/// <summary>
		/// Integer to add after pool size has been calculated. Can also be negative.
		/// </summary>
		public int ExtraAgentCount { get; set; } = 0;

		/// <summary>
		/// Empty constructor for JSON serialization
		/// </summary>
		public PoolSizeStrategyInfo()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public PoolSizeStrategyInfo(PoolSizeStrategy type, Condition? condition, string? config, int extraAgentCount = 0)
		{
			config = String.IsNullOrWhiteSpace(config) ? "{}" : config;

			// Try deserializing to ensure the config is valid JSON
			// Config can be null due to JSON serializer calling the constructor
			JsonSerializer.Deserialize<dynamic>(config);

			Type = type;
			Condition = condition;
			Config = config;
			ExtraAgentCount = extraAgentCount;
		}
	}

	/// <summary>
	/// Metadata for configuring and picking a fleet manager
	/// </summary>
	public class FleetManagerInfo
	{
		/// <summary>
		/// Fleet manager type implementation to use
		/// </summary>
		public FleetManagerType Type { get; set; }

		/// <summary>
		/// Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Configuration for the strategy, serialized as JSON
		/// </summary>
		public string? Config { get; set; }

		/// <summary>
		/// Empty constructor for BSON/JSON serialization
		/// </summary>
		public FleetManagerInfo()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		[JsonConstructor]
		public FleetManagerInfo(FleetManagerType type, Condition? condition, string? config)
		{
			config = String.IsNullOrWhiteSpace(config) ? "{}" : config;
			JsonSerializer.Deserialize<dynamic>(config);

			Type = type;
			Condition = condition;
			Config = config;
		}
	}

	/// <summary>
	/// Pool of machines
	/// </summary>
	public interface IPool : IPoolConfig
	{
		/// <summary>
		/// Last time the pool was (auto) scaled up
		/// </summary>
		public DateTime? LastScaleUpTime { get; }

		/// <summary>
		/// Last time the pool was (auto) scaled down
		/// </summary>
		public DateTime? LastScaleDownTime { get; }

		/// <summary>
		/// Last known agent count
		/// </summary>
		public int? LastAgentCount { get; }

		/// <summary>
		/// Last known desired agent count
		/// </summary>
		public int? LastDesiredAgentCount { get; }

		/// <summary>
		/// Last result from scaling the pool
		/// </summary>
		public ScaleResult? LastScaleResult { get; }

		/// <summary>
		/// Updates a pool
		/// </summary>
		/// <param name="options">Options for the update</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IPool?> TryUpdateAsync(UpdatePoolOptions options, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Settings for updating a pool state
	/// </summary>
	/// <param name="LastScaleUpTime"> New time for last (auto) scale up</param>
	/// <param name="LastScaleDownTime"> New time for last (auto) scale down</param>
	/// <param name="LastScaleResult"> Result from last scaling in/out attempt</param>
	/// <param name="LastAgentCount"> Last calculated agent count</param>
	/// <param name="LastDesiredAgentCount"> Last calculated desired agent count</param>
	public record UpdatePoolOptions(DateTime? LastScaleUpTime = null, DateTime? LastScaleDownTime = null, ScaleResult? LastScaleResult = null, int? LastAgentCount = null, int? LastDesiredAgentCount = null);
}
