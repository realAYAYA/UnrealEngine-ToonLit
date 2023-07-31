// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;

namespace Horde.Build.Agents.Telemetry
{

	/// <summary>
	/// Represents one stream in one pool in one hour of telemetry
	/// </summary>
	public class UtilizationTelemetryStream
	{
		/// <summary>
		/// Stream Id
		/// </summary>
		public string StreamId { get; set; }

		/// <summary>
		/// Total time
		/// </summary>
		public double Time { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="stream">The stream</param>
		public UtilizationTelemetryStream(IStreamUtilizationTelemetry stream)
		{
			StreamId = stream.StreamId.ToString();
			Time = stream.Time;
		}
	}

	/// <summary>
	/// Representation of an hour of time
	/// </summary>
	public class UtilizationTelemetryPool
	{
		/// <summary>
		/// Pool id
		/// </summary>
		public string PoolId { get; set; }

		/// <summary>
		/// Number of agents in this pool
		/// </summary>
		public int NumAgents { get; set; }

		/// <summary>
		/// Total time spent doing admin work
		/// </summary>
		public double AdminTime { get; set; }

		/// <summary>
		/// Time spent hibernating
		/// </summary>
		public double HibernatingTime { get; }

		/// <summary>
		/// Total time agents in this pool were doing work for other pools
		/// </summary>
		public double OtherTime { get; set; }

		/// <summary>
		/// List of streams
		/// </summary>
		public List<UtilizationTelemetryStream> Streams { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="pool"></param>
		public UtilizationTelemetryPool(IPoolUtilizationTelemetry pool)
		{
			PoolId = pool.PoolId.ToString();
			NumAgents = pool.NumAgents;
			AdminTime = pool.AdminTime;
			HibernatingTime = pool.HibernatingTime;
			OtherTime = pool.OtherTime;

			Streams = pool.Streams.ConvertAll(stream => new UtilizationTelemetryStream(stream));
		}
	}

	/// <summary>
	/// Response data for a utilization request
	/// </summary>
	public class UtilizationTelemetryResponse
	{
		/// <summary>
		/// Start hour
		/// </summary>
		public DateTimeOffset StartTime { get; set; }

		/// <summary>
		/// End hour
		/// </summary>
		public DateTimeOffset FinishTime { get; set; }

		/// <summary>
		/// List of pools
		/// </summary>
		public List<UtilizationTelemetryPool> Pools { get; set; }

		/// <summary>
		/// Total admin time
		/// </summary>
		public double AdminTime { get; set; }

		/// <summary>
		/// Total hibernating time
		/// </summary>
		public double HibernatingTime { get; set; }

		/// <summary>
		/// Total agents
		/// </summary>
		public int NumAgents { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public UtilizationTelemetryResponse(IUtilizationTelemetry telemetry)
		{
			StartTime = telemetry.StartTime;
			FinishTime = telemetry.FinishTime;
			AdminTime = telemetry.AdminTime;
			HibernatingTime = telemetry.HibernatingTime;
			NumAgents = telemetry.NumAgents;

			Pools = telemetry.Pools.ConvertAll(pool => new UtilizationTelemetryPool(pool));
		}
	}
}
