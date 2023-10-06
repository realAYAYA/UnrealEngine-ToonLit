// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/blendshapes/BlendShapesFactory.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/utils/Extd.h"
#include "riglogic/blendshapes/BlendShapesImpl.h"
#include "riglogic/blendshapes/BlendShapesImplOutputInstance.h"
#include "riglogic/blendshapes/BlendShapesNull.h"
#include "riglogic/blendshapes/BlendShapesOutputInstance.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigMetrics.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstdint>
#include <utility>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace rl4 {

static BlendShapesOutputInstance::Factory createBlendShapesOutputInstanceFactory(const Configuration&  /*unused*/,
                                                                                 std::uint16_t blendShapeCount) {
    return [ = ](MemoryResource* memRes) {
               return UniqueInstance<BlendShapesImplOutputInstance, BlendShapesOutputInstance>::with(memRes).create(
                   blendShapeCount,
                   memRes);
    };
}

BlendShapes::Pointer BlendShapesFactory::create(const Configuration& config, const RigMetrics& metrics, MemoryResource* memRes) {
    if (!config.loadBlendShapes) {
        return UniqueInstance<BlendShapesNull, BlendShapes>::with(memRes).create();
    }
    auto instanceFactory = createBlendShapesOutputInstanceFactory(config, metrics.blendShapeCount);
    auto moduleFactory = UniqueInstance<BlendShapesImpl, BlendShapes>::with(memRes);
    return moduleFactory.create(Vector<std::uint16_t>{memRes},
                                Vector<std::uint16_t>{memRes},
                                Vector<std::uint16_t>{memRes},
                                instanceFactory);
}

BlendShapes::Pointer BlendShapesFactory::create(const Configuration& config,
                                                const dna::BehaviorReader* reader,
                                                MemoryResource* memRes) {
    if (!config.loadBlendShapes) {
        return UniqueInstance<BlendShapesNull, BlendShapes>::with(memRes).create();
    }

    Vector<std::uint16_t> lods{memRes};
    Vector<std::uint16_t> inputIndices{memRes};
    Vector<std::uint16_t> outputIndices{memRes};

    extd::copy(reader->getBlendShapeChannelLODs(), lods);
    extd::copy(reader->getBlendShapeChannelInputIndices(), inputIndices);
    extd::copy(reader->getBlendShapeChannelOutputIndices(), outputIndices);

    auto instanceFactory = createBlendShapesOutputInstanceFactory(config, reader->getBlendShapeChannelCount());
    auto moduleFactory = UniqueInstance<BlendShapesImpl, BlendShapes>::with(memRes);
    return moduleFactory.create(std::move(lods), std::move(inputIndices), std::move(outputIndices), instanceFactory);
}

}  // namespace rl4
