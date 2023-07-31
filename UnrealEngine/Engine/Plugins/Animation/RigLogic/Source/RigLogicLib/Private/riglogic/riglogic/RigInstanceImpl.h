// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/riglogic/RigInstance.h"
#include "riglogic/riglogic/RigMetrics.h"

#include <cstdint>

namespace rl4 {

class RigInstanceImpl : public RigInstance {
    public:
        RigInstanceImpl(const RigMetrics& metrics, MemoryResource* memRes_);

        std::uint16_t getGUIControlCount() const override;
        void setGUIControl(std::uint16_t index, float value) override;
        void setGUIControlValues(const float* values) override;

        std::uint16_t getRawControlCount() const override;
        void setRawControl(std::uint16_t index, float value) override;
        void setRawControlValues(const float* values) override;

        std::uint16_t getLOD() const override;
        void setLOD(std::uint16_t level) override;

        ArrayView<float> getGUIControlValues();
        ConstArrayView<float> getGUIControlValues() const;
        ArrayView<float> getInputValues();
        ConstArrayView<float> getInputValues() const;
        ArrayView<float> getRawJointOutputs();
        ConstArrayView<float> getRawJointOutputs() const override;
        TransformationArrayView getJointOutputs() const override;
        ArrayView<float> getBlendShapeOutputs();
        ConstArrayView<float> getBlendShapeOutputs() const override;
        ArrayView<float> getAnimatedMapOutputs();
        ConstArrayView<float> getAnimatedMapOutputs() const override;

        MemoryResource* getMemoryResource();

    private:
        MemoryResource* memRes;

        std::uint16_t lodMaxLevel;
        std::uint16_t lodLevel;
        std::uint16_t guiControlCount;
        std::uint16_t rawControlCount;

        Vector<float> guiControlBuffer;
        Vector<float> inputBuffer;
        AlignedVector<float> jointOutputs;
        Vector<float> blendShapeOutputs;
        Vector<float> animatedMapOutputs;

};

}  // namespace rl4
