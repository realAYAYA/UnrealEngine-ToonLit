// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Cluster synchronization messages
 */
namespace DisplayClusterClusterSyncStrings
{
	constexpr static auto ProtocolName = "ClusterSync";
	
	constexpr static auto TypeRequest  = "request";
	constexpr static auto TypeResponse = "response";

	constexpr static auto ArgumentsDefaultCategory = "CS";
	constexpr static auto ArgumentsJsonEvents      = "CS_JE";
	constexpr static auto ArgumentsBinaryEvents    = "CS_BE";

	namespace WaitForGameStart
	{
		constexpr static auto Name = "WaitForGameStart";
	}

	namespace WaitForFrameStart
	{
		constexpr static auto Name = "WaitForFrameStart";
	}

	namespace WaitForFrameEnd
	{
		constexpr static auto Name = "WaitForFrameEnd";
	}

	namespace GetTimeData
	{
		constexpr static auto Name = "GetTimeData";
		constexpr static auto ArgDeltaTime        = "DeltaTime";
		constexpr static auto ArgGameTime         = "GameTime";
		constexpr static auto ArgIsFrameTimeValid = "IsFrameTimeValid";
		constexpr static auto ArgFrameTime        = "FrameTime";
	}

	namespace GetObjectsData
	{
		constexpr static auto Name = "GetObjectsData";
		constexpr static auto ArgSyncGroup = "SyncGroup";
	}

	namespace GetEventsData
	{
		constexpr static auto Name = "GetEventsData";
	}

	namespace GetNativeInputData
	{
		constexpr static auto Name = "GetNativeInputData";
	}
};
