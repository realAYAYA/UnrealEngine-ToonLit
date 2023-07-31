// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/types/Aliases.h"

#include <terse/archives/binary/InputArchive.h>
#include <terse/archives/binary/OutputArchive.h>
#include <trio/Stream.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstdint>
#include <functional>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

class RigInstance;

class JointsEvaluator {
    protected:
        virtual ~JointsEvaluator();

    public:
        virtual void calculate(ConstArrayView<float> inputs, ArrayView<float> outputs, std::uint16_t lod) const = 0;
        virtual void calculate(ConstArrayView<float> inputs,
                               ArrayView<float> outputs,
                               std::uint16_t lod,
                               std::uint16_t jointGroupIndex) const = 0;
        virtual void load(terse::BinaryInputArchive<BoundedIOStream>& archive) = 0;
        virtual void save(terse::BinaryOutputArchive<BoundedIOStream>& archive) = 0;
};

using JointsEvaluatorPtr = std::unique_ptr<JointsEvaluator, std::function<void (JointsEvaluator*)> >;

}  // namespace rl4
