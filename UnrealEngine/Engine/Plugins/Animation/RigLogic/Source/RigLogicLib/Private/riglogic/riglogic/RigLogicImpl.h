// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/animatedmaps/AnimatedMaps.h"
#include "riglogic/blendshapes/BlendShapes.h"
#include "riglogic/controls/Controls.h"
#include "riglogic/joints/Joints.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigLogic.h"
#include "riglogic/riglogic/RigMetrics.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <functional>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

class RigLogicImpl : public RigLogic {
    private:
        using ControlsPtr = std::unique_ptr<Controls, std::function<void (Controls*)> >;
        using JointsPtr = std::unique_ptr<Joints, std::function<void (Joints*)> >;
        using BlendShapesPtr = std::unique_ptr<BlendShapes, std::function<void (BlendShapes*)> >;
        using AnimatedMapsPtr = std::unique_ptr<AnimatedMaps, std::function<void (AnimatedMaps*)> >;

    public:
        RigLogicImpl(Configuration config_,
                     RigMetrics metrics_,
                     ControlsPtr controls_,
                     JointsPtr joints_,
                     BlendShapesPtr blendShapes_,
                     AnimatedMapsPtr animatedMaps_,
                     MemoryResource* memRes_);

        void dump(BoundedIOStream* destination) const override;
        const Configuration& getConfiguration() const;
        const RigMetrics& getRigMetrics() const;
        std::uint16_t getLODCount() const override;
        ConstArrayView<float> getRawNeutralJointValues() const override;
        TransformationArrayView getNeutralJointValues() const override;
        std::uint16_t getJointGroupCount() const override;
        ConstArrayView<std::uint16_t> getJointVariableAttributeIndices(std::uint16_t lod) const override;
        void mapGUIToRawControls(RigInstance* instance) const override;
        void calculateControls(RigInstance* instance) const override;
        void calculateJoints(RigInstance* instance) const override;
        void calculateJoints(RigInstance* instance, std::uint16_t jointGroupIndex) const override;
        void calculateBlendShapes(RigInstance* instance) const override;
        void calculateAnimatedMaps(RigInstance* instance) const override;
        void calculate(RigInstance* instance) const override;

        MemoryResource* getMemoryResource();

    private:
        MemoryResource* memRes;
        ControlsPtr controls;
        JointsPtr joints;
        BlendShapesPtr blendShapes;
        AnimatedMapsPtr animatedMaps;
        Configuration config;
        RigMetrics metrics;

};

}  // namespace rl4
