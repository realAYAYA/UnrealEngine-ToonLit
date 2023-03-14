// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/riglogic/RigLogicImpl.h"

#include "riglogic/animatedmaps/AnimatedMapsFactory.h"
#include "riglogic/blendshapes/BlendShapesFactory.h"
#include "riglogic/controls/ControlsFactory.h"
#include "riglogic/joints/JointsFactory.h"
#include "riglogic/riglogic/RigInstanceImpl.h"
#include "riglogic/riglogic/RigMetrics.h"

#include <dna/layers/BehaviorReader.h>
#include <pma/PolyAllocator.h>
#include <pma/resources/AlignedMemoryResource.h>
#include <terse/archives/binary/InputArchive.h>
#include <terse/archives/binary/OutputArchive.h>
#include <trio/Stream.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstddef>
#include <utility>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

static RigInstanceImpl* castInstance(RigInstance* instance) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    return static_cast<RigInstanceImpl*>(instance);
}

RigLogic::~RigLogic() = default;

RigLogic* RigLogic::create(const dna::BehaviorReader* reader, CalculationType calculationType, MemoryResource* memRes) {
    RigMetrics metrics{};
    metrics.lodCount = reader->getLODCount();
    metrics.guiControlCount = reader->getGUIControlCount();
    metrics.rawControlCount = reader->getRawControlCount();
    metrics.psdCount = reader->getPSDCount();
    metrics.jointAttributeCount = reader->getJointRowCount();
    metrics.blendShapeCount = reader->getBlendShapeChannelCount();
    metrics.animatedMapCount = reader->getAnimatedMapCount();

    Configuration config{calculationType};
    auto controls = ControlsFactory::create(reader, memRes);
    auto joints = JointsFactory::create(config, reader, memRes);
    auto blendShapes = BlendShapesFactory::create(reader, memRes);
    auto animatedMaps = AnimatedMapsFactory::create(reader, memRes);

    PolyAllocator<RigLogicImpl> alloc{memRes};
    return alloc.newObject(config,
                           metrics,
                           std::move(controls),
                           std::move(joints),
                           std::move(blendShapes),
                           std::move(animatedMaps),
                           memRes);
}

RigLogic* RigLogic::restore(BoundedIOStream* source, MemoryResource* memRes) {
    PolyAllocator<RigLogicImpl> alloc{memRes};

    terse::BinaryInputArchive<BoundedIOStream> archive{source};

    Configuration config;
    archive >> config;

    RigMetrics metrics;
    archive >> metrics;

    auto controls = ControlsFactory::create(memRes);
    auto joints = JointsFactory::create(config, metrics, memRes);
    auto blendShapes = BlendShapesFactory::create(memRes);
    auto animatedMaps = AnimatedMapsFactory::create(memRes);

    archive >> *controls >> *joints >> *blendShapes >> *animatedMaps;
    return alloc.newObject(config,
                           metrics,
                           std::move(controls),
                           std::move(joints),
                           std::move(blendShapes),
                           std::move(animatedMaps),
                           memRes);
}

void RigLogic::destroy(RigLogic* instance) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto ptr = static_cast<RigLogicImpl*>(instance);
    PolyAllocator<RigLogicImpl> alloc{ptr->getMemoryResource()};
    alloc.deleteObject(ptr);
}

RigLogicImpl::RigLogicImpl(Configuration config_,
                           RigMetrics metrics_,
                           ControlsPtr controls_,
                           JointsPtr joints_,
                           BlendShapesPtr blendShapes_,
                           AnimatedMapsPtr animatedMaps_,
                           MemoryResource* memRes_) :
    memRes{memRes_},
    controls{std::move(controls_)},
    joints{std::move(joints_)},
    blendShapes{std::move(blendShapes_)},
    animatedMaps{std::move(animatedMaps_)},
    config{config_},
    metrics{metrics_} {
}

void RigLogicImpl::dump(BoundedIOStream* destination) const {
    terse::BinaryOutputArchive<BoundedIOStream> archive{destination};
    archive << config << metrics << *controls << *joints << *blendShapes << *animatedMaps;
}

const Configuration& RigLogicImpl::getConfiguration() const {
    return config;
}

const RigMetrics& RigLogicImpl::getRigMetrics() const {
    return metrics;
}

std::uint16_t RigLogicImpl::getLODCount() const {
    return metrics.lodCount;
}

ConstArrayView<float> RigLogicImpl::getRawNeutralJointValues() const {
    return joints->getRawNeutralValues();
}

TransformationArrayView RigLogicImpl::getNeutralJointValues() const {
    return joints->getNeutralValues();
}

std::uint16_t RigLogicImpl::getJointGroupCount() const {
    return joints->getJointGroupCount();
}

ConstArrayView<std::uint16_t> RigLogicImpl::getJointVariableAttributeIndices(std::uint16_t lod) const {
    return joints->getVariableAttributeIndices(lod);
}

void RigLogicImpl::mapGUIToRawControls(RigInstance* instance) const {
    auto pRigInstance = castInstance(instance);
    controls->mapGUIToRaw(pRigInstance->getGUIControlValues(), pRigInstance->getInputValues());
}

void RigLogicImpl::calculateControls(RigInstance* instance) const {
    auto pRigInstance = castInstance(instance);
    controls->calculate(pRigInstance->getInputValues());
}

void RigLogicImpl::calculateJoints(RigInstance* instance) const {
    auto pRigInstance = castInstance(instance);
    joints->calculate(pRigInstance->getInputValues(), pRigInstance->getRawJointOutputs(), pRigInstance->getLOD());
}

void RigLogicImpl::calculateJoints(RigInstance* instance, std::uint16_t jointGroupIndex) const {
    auto pRigInstance = castInstance(instance);
    joints->calculate(pRigInstance->getInputValues(), pRigInstance->getRawJointOutputs(), pRigInstance->getLOD(),
                      jointGroupIndex);
}

void RigLogicImpl::calculateBlendShapes(RigInstance* instance) const {
    auto pRigInstance = castInstance(instance);
    blendShapes->calculate(pRigInstance->getInputValues(), pRigInstance->getBlendShapeOutputs(), pRigInstance->getLOD());
}

void RigLogicImpl::calculateAnimatedMaps(RigInstance* instance) const {
    auto pRigInstance = castInstance(instance);
    animatedMaps->calculate(pRigInstance->getInputValues(), pRigInstance->getAnimatedMapOutputs(), pRigInstance->getLOD());
}

void RigLogicImpl::calculate(RigInstance* instance) const {
    calculateControls(instance);
    calculateJoints(instance);
    calculateBlendShapes(instance);
    calculateAnimatedMaps(instance);
}

MemoryResource* RigLogicImpl::getMemoryResource() {
    return memRes;
}

}  // namespace rl4
