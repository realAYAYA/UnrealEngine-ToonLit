// Copyright Epic Games, Inc. All Rights Reserved.

#pragma warning disable CS1591

namespace HordeCommon.Rpc.Messages.Telemetry
{
	partial class AgentMetadataEvent
	{
		public string EventName { get; } = "AgentMetadata";
	}

	partial class AgentCpuMetricsEvent
	{
		public string EventName { get; } = "CpuUsage";
	}

	partial class AgentMemoryMetricsEvent
	{
		public string EventName { get; } = "MemoryUsage";
	}
}

