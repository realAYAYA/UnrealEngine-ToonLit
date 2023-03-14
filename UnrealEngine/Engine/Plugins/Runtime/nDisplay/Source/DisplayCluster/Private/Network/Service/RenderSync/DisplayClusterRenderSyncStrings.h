// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Rendering synchronization protocol strings
 */
namespace DisplayClusterRenderSyncStrings
{
	constexpr static auto ProtocolName = "RenderSync";

	constexpr static auto TypeRequest  = "request";
	constexpr static auto TypeResponse = "response";

	constexpr static auto ArgumentsDefaultCategory = "RS";

	namespace WaitForSwapSync
	{
		constexpr static auto Name = "WaitForSwapSync";
	};
};
