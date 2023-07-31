// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstddef>
#include <cstdint>

struct OutputScope {
    std::uint16_t lod;
    std::size_t offset;
    std::size_t size;
};

struct StrategyTestParams {
    std::uint16_t lod;
};
