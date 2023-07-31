// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/riglogic/RigInstanceImpl.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/riglogic/RigLogicImpl.h"
#include "riglogic/riglogic/RigMetrics.h"
#include "riglogic/types/Aliases.h"
#include "riglogic/utils/Extd.h"

#include <pma/PolyAllocator.h>
#include <pma/resources/AlignedMemoryResource.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <algorithm>
#include <cassert>
#include <cstdint>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

namespace {

const float controlsMin = 0.0f;
const float controlsMax = 1.0f;

}  // namespace

RigInstance* RigInstance::create(RigLogic* rigLogic, MemoryResource* memRes) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto ptr = static_cast<RigLogicImpl*>(rigLogic);
    PolyAllocator<RigInstanceImpl> alloc{memRes};
    return alloc.newObject(ptr->getRigMetrics(), memRes);
}

void RigInstance::destroy(RigInstance* instance) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto ptr = static_cast<RigInstanceImpl*>(instance);
    PolyAllocator<RigInstanceImpl> alloc{ptr->getMemoryResource()};
    alloc.deleteObject(ptr);
}

RigInstance::~RigInstance() = default;

RigInstanceImpl::RigInstanceImpl(const RigMetrics& metrics, MemoryResource* memRes_) :
    memRes{memRes_},
    lodMaxLevel{metrics.lodCount > 0u ? static_cast<std::uint16_t>(metrics.lodCount - 1) : static_cast<std::uint16_t>(0)},
    lodLevel{},
    guiControlCount{metrics.guiControlCount},
    rawControlCount{metrics.rawControlCount},
    guiControlBuffer{guiControlCount, {}, memRes},
    inputBuffer{static_cast<std::size_t>(rawControlCount + metrics.psdCount), {}, memRes},
    jointOutputs{metrics.jointAttributeCount, {}, memRes},
    blendShapeOutputs{metrics.blendShapeCount, {}, memRes},
    animatedMapOutputs{metrics.animatedMapCount, {}, memRes} {
}

std::uint16_t RigInstanceImpl::getGUIControlCount() const {
    return guiControlCount;
}

void RigInstanceImpl::setGUIControl(std::uint16_t index, float value) {
    assert(index < guiControlBuffer.size());
    guiControlBuffer[index] = value;
}

void RigInstanceImpl::setGUIControlValues(const float* values) {
    std::copy(values, values + guiControlCount, guiControlBuffer.begin());
}

std::uint16_t RigInstanceImpl::getRawControlCount() const {
    return rawControlCount;
}

void RigInstanceImpl::setRawControl(std::uint16_t index, float value) {
    assert(index < inputBuffer.size());
    inputBuffer[index] = extd::clamp(value, controlsMin, controlsMax);
}

void RigInstanceImpl::setRawControlValues(const float* values) {
    auto clamper = [](float v) {
            return extd::clamp(v, controlsMin, controlsMax);
        };
    std::transform(values, values + rawControlCount, inputBuffer.begin(), clamper);
}

std::uint16_t RigInstanceImpl::getLOD() const {
    return lodLevel;
}

void RigInstanceImpl::setLOD(std::uint16_t level) {
    lodLevel = extd::clamp(level, static_cast<std::uint16_t>(0), lodMaxLevel);
}

ArrayView<float> RigInstanceImpl::getGUIControlValues() {
    return ArrayView<float>{guiControlBuffer};
}

ConstArrayView<float> RigInstanceImpl::getGUIControlValues() const {
    return ConstArrayView<float>{guiControlBuffer};
}

ArrayView<float> RigInstanceImpl::getInputValues() {
    return ArrayView<float>{inputBuffer};
}

ConstArrayView<float> RigInstanceImpl::getInputValues() const {
    return ConstArrayView<float>{inputBuffer};
}

ArrayView<float> RigInstanceImpl::getRawJointOutputs() {
    return ArrayView<float>{jointOutputs};
}

ConstArrayView<float> RigInstanceImpl::getRawJointOutputs() const {
    return ConstArrayView<float>{jointOutputs};
}

TransformationArrayView RigInstanceImpl::getJointOutputs() const {
    return TransformationArrayView{jointOutputs.data(), jointOutputs.size()};
}

ArrayView<float> RigInstanceImpl::getBlendShapeOutputs() {
    return ArrayView<float>{blendShapeOutputs};
}

ConstArrayView<float> RigInstanceImpl::getBlendShapeOutputs() const {
    return ConstArrayView<float>{blendShapeOutputs};
}

ArrayView<float> RigInstanceImpl::getAnimatedMapOutputs() {
    return ArrayView<float>{animatedMapOutputs};
}

ConstArrayView<float> RigInstanceImpl::getAnimatedMapOutputs() const {
    return ConstArrayView<float>{animatedMapOutputs};
}

MemoryResource* RigInstanceImpl::getMemoryResource() {
    return memRes;
}

}  // namespace rl4
