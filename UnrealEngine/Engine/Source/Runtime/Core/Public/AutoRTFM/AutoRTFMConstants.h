// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstddef>
#include <cstdint>

// FIXME: This is going to need support for cross-compilation. So, we will possibly have different offsets for
// different platforms, and both the compiler and the runtime will have to make choices about which to use.

// WARNING: Any change in these constants will require a re-patch and re-build of LLVM!

namespace AutoRTFM
{
namespace Constants
{

inline constexpr size_t LogLineBytes = 3;
inline constexpr size_t LineBytes = static_cast<size_t>(1) << LogLineBytes;
inline constexpr size_t LineTableSize = 33554432;

inline constexpr size_t Offset_Context_CurrentTransaction = 0;
inline constexpr size_t Offset_Context_LineTable = 192;
inline constexpr size_t Offset_Context_Status = 152;

inline constexpr size_t LogSize_LineEntry = 5;
inline constexpr size_t Size_LineEntry = static_cast<size_t>(1) << LogSize_LineEntry;
inline constexpr size_t Offset_LineEntry_LogicalLine = 0;
inline constexpr size_t Offset_LineEntry_ActiveLine = 16;
inline constexpr size_t Offset_LineEntry_LoggingTransaction = 24;
inline constexpr size_t Offset_LineEntry_AccessMask = 14;

inline constexpr uint32_t Context_Status_OnTrack = 1;

} // namespace Constants
} // namespace AutoRTFM
