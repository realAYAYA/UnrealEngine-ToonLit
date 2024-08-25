// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/riglogic/Configuration.h"

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

template<std::uint16_t LOD>
struct TStrategyTestParams {

    static constexpr std::uint16_t lod() {
        return LOD;
    }

};

template<rl4::CalculationType CalcType>
struct TCalculationType {

    static constexpr rl4::CalculationType get() {
        return CalcType;
    }

};
