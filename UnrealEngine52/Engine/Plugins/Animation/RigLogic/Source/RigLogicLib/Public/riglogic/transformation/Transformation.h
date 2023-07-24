// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/types/Aliases.h"

#include <cassert>
#include <cstddef>

namespace rl4 {

class Transformation {
    public:
        explicit Transformation(const float* chunk_) : chunk{chunk_} {
        }

        static std::size_t size() {
            return 9ul;
        }

        const Transformation& operator*() const {
            return *this;
        }

        Transformation& operator++() {
            chunk += size();
            return *this;
        }

        bool operator==(const Transformation& rhs) const {
            return (chunk == rhs.chunk);
        }

        bool operator!=(const Transformation& rhs) const {
            return !(*this == rhs);
        }

        Vector3 getTranslation() const {
            return {chunk[0ul], chunk[1ul], chunk[2ul]};
        }

        Vector3 getRotation() const {
            return {chunk[3ul], chunk[4ul], chunk[5ul]};
        }

        Vector3 getScale() const {
            return {chunk[6ul], chunk[7ul], chunk[8ul]};
        }

    private:
        const float* chunk;
};

class TransformationArrayView {
    public:
        TransformationArrayView(const float* values_, std::size_t count_) :
            values{values_},
            count{count_} {
        }

        std::size_t size() const {
            assert(count % Transformation::size() == 0ul);
            return count / Transformation::size();
        }

        Transformation operator[](std::size_t index) const {
            return Transformation{values + (index * Transformation::size())};
        }

        Transformation begin() const {
            return Transformation{values};
        }

        Transformation end() const {
            return Transformation{values + count};
        }

    private:
        const float* values;
        std::size_t count;
};

}  // namespace rl4
