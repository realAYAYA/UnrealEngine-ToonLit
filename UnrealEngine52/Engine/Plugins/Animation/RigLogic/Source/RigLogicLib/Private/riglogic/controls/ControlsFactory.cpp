// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/ControlsFactory.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/controls/Controls.h"
#include "riglogic/controls/instances/StandardControlsInputInstance.h"
#include "riglogic/psdmatrix/PSDMatrix.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigMetrics.h"
#include "riglogic/utils/Extd.h"

#include <cstdint>

namespace rl4 {

static ControlsInputInstance::Factory createInstanceFactory(const Configuration&  /*unused*/,
                                                            std::uint16_t guiControlCount,
                                                            std::uint16_t rawControlCount,
                                                            std::uint16_t psdControlCount,
                                                            std::uint16_t mlControlCount) {
    return [ = ](MemoryResource* memRes) {
               auto factory = UniqueInstance<StandardControlsInputInstance, ControlsInputInstance>::with(memRes);
               return factory.create(guiControlCount, rawControlCount, psdControlCount, mlControlCount, memRes);
    };
}

Controls::Pointer ControlsFactory::create(const Configuration& config, const RigMetrics& metrics, MemoryResource* memRes) {
    auto instanceFactory = createInstanceFactory(config,
                                                 metrics.guiControlCount,
                                                 metrics.rawControlCount,
                                                 metrics.psdControlCount,
                                                 metrics.mlControlCount);
    return UniqueInstance<Controls>::with(memRes).create(ConditionalTable{memRes}, PSDMatrix{memRes}, instanceFactory);
}

Controls::Pointer ControlsFactory::create(const Configuration& config, const dna::Reader* reader, MemoryResource* memRes) {
    Vector<std::uint16_t> inputIndices{memRes};
    Vector<std::uint16_t> outputIndices{memRes};
    Vector<float> fromValues{memRes};
    Vector<float> toValues{memRes};
    Vector<float> slopeValues{memRes};
    Vector<float> cutValues{memRes};
    extd::copy(reader->getGUIToRawInputIndices(), inputIndices);
    extd::copy(reader->getGUIToRawOutputIndices(), outputIndices);
    extd::copy(reader->getGUIToRawFromValues(), fromValues);
    extd::copy(reader->getGUIToRawToValues(), toValues);
    extd::copy(reader->getGUIToRawSlopeValues(), slopeValues);
    extd::copy(reader->getGUIToRawCutValues(), cutValues);
    // DNAs may contain these parameters in reverse order
    // i.e. the `from` value is actually larger than the `to` value
    assert(fromValues.size() == toValues.size());
    for (std::size_t i = 0ul; i < fromValues.size(); ++i) {
        if (fromValues[i] > toValues[i]) {
            std::swap(fromValues[i], toValues[i]);
        }
    }

    ConditionalTable conditionals{std::move(inputIndices),
                                  std::move(outputIndices),
                                  std::move(fromValues),
                                  std::move(toValues),
                                  std::move(slopeValues),
                                  std::move(cutValues),
                                  reader->getGUIControlCount(),
                                  reader->getRawControlCount(),
                                  memRes};

    Vector<std::uint16_t> psdRows{memRes};
    Vector<std::uint16_t> psdColumns{memRes};
    Vector<float> psdValues{memRes};
    extd::copy(reader->getPSDRowIndices(), psdRows);
    extd::copy(reader->getPSDColumnIndices(), psdColumns);
    extd::copy(reader->getPSDValues(), psdValues);
    PSDMatrix psds{reader->getPSDCount(),
                   std::move(psdRows),
                   std::move(psdColumns),
                   std::move(psdValues)};

    auto instanceFactory = createInstanceFactory(config,
                                                 conditionals.getInputCount(),
                                                 conditionals.getOutputCount(),
                                                 psds.getDistinctPSDCount(),
                                                 reader->getMLControlCount());

    return UniqueInstance<Controls>::with(memRes).create(std::move(conditionals), std::move(psds), instanceFactory);
}

}  // namespace rl4
