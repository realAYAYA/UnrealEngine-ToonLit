// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/animatedmaps/AnimatedMaps.h"
#include "riglogic/animatedmaps/AnimatedMapsOutputInstance.h"
#include "riglogic/blendshapes/BlendShapes.h"
#include "riglogic/blendshapes/BlendShapesOutputInstance.h"
#include "riglogic/controls/Controls.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/joints/Joints.h"
#include "riglogic/joints/JointsOutputInstance.h"
#include "riglogic/ml/MachineLearnedBehavior.h"
#include "riglogic/ml/MachineLearnedBehaviorOutputInstance.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigLogic.h"
#include "riglogic/riglogic/RigMetrics.h"

namespace rl4 {

class RigLogicImpl : public RigLogic {
    public:
        RigLogicImpl(Configuration config_,
                     RigMetrics::Pointer metrics_,
                     Controls::Pointer controls_,
                     MachineLearnedBehavior::Pointer machineLearnedBehavior_,
                     Joints::Pointer joints_,
                     BlendShapes::Pointer blendShapes_,
                     AnimatedMaps::Pointer animatedMaps_,
                     MemoryResource* memRes_);

        void dump(BoundedIOStream* destination) const override;
        const Configuration& getConfiguration() const;
        const RigMetrics& getRigMetrics() const;
        std::uint16_t getLODCount() const override;
        ConstArrayView<float> getRawNeutralJointValues() const override;
        TransformationArrayView getNeutralJointValues() const override;
        ConstArrayView<std::uint16_t> getJointVariableAttributeIndices(std::uint16_t lod) const override;
        std::uint16_t getJointGroupCount() const override;
        std::uint16_t getNeuralNetworkCount() const override;
        std::uint16_t getMeshCount() const override;
        std::uint16_t getMeshRegionCount(std::uint16_t meshIndex) const override;
        ConstArrayView<std::uint16_t> getNeuralNetworkIndices(std::uint16_t meshIndex, std::uint16_t regionIndex) const override;

        ControlsInputInstance::Pointer createControlsInstance(MemoryResource* instanceMemRes) const;
        MachineLearnedBehaviorOutputInstance::Pointer createMachineLearnedBehaviorInstance(MemoryResource* instanceMemRes) const;
        JointsOutputInstance::Pointer createJointsInstance(MemoryResource* instanceMemRes) const;
        BlendShapesOutputInstance::Pointer createBlendShapesInstance(MemoryResource* instanceMemRes) const;
        AnimatedMapsOutputInstance::Pointer createAnimatedMapsInstance(MemoryResource* instanceMemRes) const;

        void mapGUIToRawControls(RigInstance* instance) const override;
        void mapRawToGUIControls(RigInstance* instance) const override;
        void calculateControls(RigInstance* instance) const override;
        void calculateMachineLearnedBehaviorControls(RigInstance* instance) const override;
        void calculateMachineLearnedBehaviorControls(RigInstance* instance, std::uint16_t neuralNetIndex) const override;
        void calculateJoints(RigInstance* instance) const override;
        void calculateJoints(RigInstance* instance, std::uint16_t jointGroupIndex) const override;
        void calculateBlendShapes(RigInstance* instance) const override;
        void calculateAnimatedMaps(RigInstance* instance) const override;
        void calculate(RigInstance* instance) const override;

        MemoryResource* getMemoryResource();

    private:
        MemoryResource* memRes;
        Configuration config;
        RigMetrics::Pointer metrics;
        Controls::Pointer controls;
        MachineLearnedBehavior::Pointer machineLearnedBehavior;
        Joints::Pointer joints;
        BlendShapes::Pointer blendShapes;
        AnimatedMaps::Pointer animatedMaps;

};

}  // namespace rl4
