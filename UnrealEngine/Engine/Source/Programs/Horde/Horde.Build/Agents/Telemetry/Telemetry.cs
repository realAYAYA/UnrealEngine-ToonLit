// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Horde.Build.Agents.Pools;
using Horde.Build.Streams;
using Horde.Build.Utilities;

namespace Horde.Build.Agents.Telemetry
{
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Information about the utilization of a pool
	/// </summary>
	public interface IUtilizationTelemetry
	{
		/// <summary>
		/// Start time for the bucket
		/// </summary>
		public DateTime StartTime { get; }

		/// <summary>
		/// Finish time for this bucket
		/// </summary>
		public DateTime FinishTime { get; }

		/// <summary>
		/// Number of agents in total
		/// </summary>
		public int NumAgents { get; }

		/// <summary>
		/// Breakdown of utilization by pool
		/// </summary>
		public IReadOnlyList<IPoolUtilizationTelemetry> Pools { get; }

		/// <summary>
		/// Amount of time that agents were hibernating
		/// </summary>
		public double HibernatingTime { get; }

		/// <summary>
		/// Total time spent running administrative tasks (conform, etc...)
		/// </summary>
		public double AdminTime { get; }
	}

	/// <summary>
	/// Concrete implementation of <see cref="IUtilizationTelemetry"/>
	/// </summary>
	public sealed class NewUtilizationTelemetry : IUtilizationTelemetry
	{
		/// <inheritdoc/>
		public DateTime StartTime { get; set; }

		/// <inheritdoc/>
		public DateTime FinishTime { get; set; }

		/// <inheritdoc/>
		public int NumAgents { get; set; }

		/// <inheritdoc/>
		List<NewPoolUtilizationTelemetry> Pools { get; set; } = new List<NewPoolUtilizationTelemetry>();
		IReadOnlyList<IPoolUtilizationTelemetry> IUtilizationTelemetry.Pools => Pools;

		Dictionary<PoolId, NewPoolUtilizationTelemetry> PoolsLookup { get; set; } = new Dictionary<PoolId, NewPoolUtilizationTelemetry>();

		/// <inheritdoc/>
		public double HibernatingTime { get; set; }

		/// <inheritdoc/>
		public double AdminTime { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="startTime"></param>
		/// <param name="finishTime"></param>
		public NewUtilizationTelemetry(DateTime startTime, DateTime finishTime)
		{
			StartTime = startTime;
			FinishTime = finishTime;
		}

		/// <summary>
		/// Adds a new pool
		/// </summary>
		/// <param name="poolId">The pool id</param>
		/// <returns>Telemetry for the given pool</returns>
		public NewPoolUtilizationTelemetry FindOrAddPool(PoolId poolId)
		{
			NewPoolUtilizationTelemetry? pool;
			if (!PoolsLookup.TryGetValue(poolId, out pool))
			{
				pool = new NewPoolUtilizationTelemetry(poolId);
				Pools.Add(pool);
				PoolsLookup.Add(poolId, pool);
			}
			return pool;
		}
	}

	/// <summary>
	/// Information about the utilization of a pool
	/// </summary>
	public interface IPoolUtilizationTelemetry
	{
		/// <summary>
		/// Pool containing the work to execute
		/// </summary>
		public PoolId PoolId { get; }

		/// <summary>
		/// Number of agents in this pool
		/// </summary>
		public int NumAgents { get; }

		/// <summary>
		/// The stream executing work. If this is null, the time accounts for the machine executing work in another pool.
		/// </summary>
		public IReadOnlyList<IStreamUtilizationTelemetry> Streams { get; }

		/// <summary>
		/// Amount of time spent running 
		/// </summary>
		public double AdminTime { get; }

		/// <summary>
		/// Amount of time that agents were hibernating
		/// </summary>
		public double HibernatingTime { get; }

		/// <summary>
		/// Amount of time spent by agents in this pool servicing other pools
		/// </summary>
		public double OtherTime { get; }
	}

	/// <summary>
	/// Concrete implementation of <see cref="IPoolUtilizationTelemetry"/>
	/// </summary>
	public sealed class NewPoolUtilizationTelemetry : IPoolUtilizationTelemetry
	{
		/// <inheritdoc/>
		public PoolId PoolId { get; set; }

		/// <inheritdoc/>
		public int NumAgents { get; set; }

		/// <inheritdoc/>
		List<NewPoolUtilizationTelemetryStream> Streams { get; set; } = new List<NewPoolUtilizationTelemetryStream>();
		IReadOnlyList<IStreamUtilizationTelemetry> IPoolUtilizationTelemetry.Streams => Streams;

		Dictionary<StreamId, NewPoolUtilizationTelemetryStream> StreamLookup { get; set; } = new Dictionary<StreamId, NewPoolUtilizationTelemetryStream>();

		/// <inheritdoc/>
		public double AdminTime { get; set; }

		/// <inheritdoc/>
		public double HibernatingTime { get; set; }

		/// <inheritdoc/>
		public double OtherTime { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="poolId"></param>
		public NewPoolUtilizationTelemetry(PoolId poolId)
		{
			PoolId = poolId;
		}

		/// <summary>
		/// Adds a stream to the object
		/// </summary>
		/// <param name="streamId"></param>
		public NewPoolUtilizationTelemetryStream FindOrAddStream(StreamId streamId)
		{
			NewPoolUtilizationTelemetryStream? stream;
			if (!StreamLookup.TryGetValue(streamId, out stream))
			{
				stream = new NewPoolUtilizationTelemetryStream(streamId);
				Streams.Add(stream);
				StreamLookup.Add(streamId, stream);
			}
			return stream;
		}
	}

	/// <summary>
	/// Utilization of a pool for a particular stream
	/// </summary>
	public interface IStreamUtilizationTelemetry
	{
		/// <summary>
		/// The stream id
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// Number of machine hours spent executing work for this stream
		/// </summary>
		public double Time { get; }
	}

	/// <summary>
	/// Concrete implementation of <see cref="IStreamUtilizationTelemetry"/>
	/// </summary>
	public sealed class NewPoolUtilizationTelemetryStream : IStreamUtilizationTelemetry
	{
		/// <inheritdoc/>
		public StreamId StreamId { get; set; }

		/// <inheritdoc/>
		public double Time { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="streamId"></param>
		public NewPoolUtilizationTelemetryStream(StreamId streamId)
		{
			StreamId = streamId;
		}
	}
}
