// Copyright Epic Games, Inc. All Rights Reserved.

#include "dna/stream/BinaryStreamReaderImpl.h"

#include "dna/TypeDefs.h"
#include "dna/types/Limits.h"

#include <status/Provider.h>
#include <trio/utils/StreamScope.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstddef>
#include <limits>
#include <tuple>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace dna {

BinaryStreamReader::~BinaryStreamReader() = default;

BinaryStreamReader* BinaryStreamReader::create(BoundedIOStream* stream,
                                               DataLayer layer,
                                               UnknownLayerPolicy policy,
                                               std::uint16_t maxLOD,
                                               MemoryResource* memRes) {
    PolyAllocator<BinaryStreamReaderImpl> alloc{memRes};
    return alloc.newObject(stream, layer, policy, maxLOD, LODLimits::min(), memRes);
}

BinaryStreamReader* BinaryStreamReader::create(BoundedIOStream* stream,
                                               DataLayer layer,
                                               UnknownLayerPolicy policy,
                                               std::uint16_t maxLOD,
                                               std::uint16_t minLOD,
                                               MemoryResource* memRes) {
    PolyAllocator<BinaryStreamReaderImpl> alloc{memRes};
    return alloc.newObject(stream, layer, policy, maxLOD, minLOD, memRes);
}

BinaryStreamReader* BinaryStreamReader::create(BoundedIOStream* stream,
                                               DataLayer layer,
                                               UnknownLayerPolicy policy,
                                               std::uint16_t* lods,
                                               std::uint16_t lodCount,
                                               MemoryResource* memRes) {
    PolyAllocator<BinaryStreamReaderImpl> alloc{memRes};
    return alloc.newObject(stream, layer, policy, ConstArrayView<std::uint16_t>{lods, lodCount}, memRes);
}

void BinaryStreamReader::destroy(BinaryStreamReader* instance) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto reader = static_cast<BinaryStreamReaderImpl*>(instance);
    PolyAllocator<BinaryStreamReaderImpl> alloc{reader->getMemoryResource()};
    alloc.deleteObject(reader);
}

BinaryStreamReaderImpl::BinaryStreamReaderImpl(BoundedIOStream* stream_,
                                               DataLayer layer_,
                                               UnknownLayerPolicy policy_,
                                               std::uint16_t maxLOD_,
                                               std::uint16_t minLOD_,
                                               MemoryResource* memRes_) :
    BaseImpl{policy_, UpgradeFormatPolicy::Disallowed, memRes_},
    ReaderImpl{memRes_},
    stream{stream_},
    archive{stream_, layer_, maxLOD_, minLOD_, memRes_},
    lodConstrained{(maxLOD_ != LODLimits::max()) || (minLOD_ != LODLimits::min())} {
}

BinaryStreamReaderImpl::BinaryStreamReaderImpl(BoundedIOStream* stream_,
                                               DataLayer layer_,
                                               UnknownLayerPolicy policy_,
                                               ConstArrayView<std::uint16_t> lods_,
                                               MemoryResource* memRes_) :
    BaseImpl{policy_, UpgradeFormatPolicy::Disallowed, memRes_},
    ReaderImpl{memRes_},
    stream{stream_},
    archive{stream_, layer_, lods_, memRes_},
    lodConstrained{true} {
}

bool BinaryStreamReaderImpl::isLODConstrained() const {
    return lodConstrained;
}

void BinaryStreamReaderImpl::unload(DataLayer layer) {
    if ((layer == DataLayer::All) ||
        (layer == DataLayer::Descriptor)) {
        dna = DNA{dna.layers.unknownPolicy, dna.layers.upgradePolicy, memRes};
    } else if (layer == DataLayer::MachineLearnedBehavior) {
        dna.unloadMachineLearnedBehavior();
    } else if ((layer == DataLayer::Geometry) || (layer == DataLayer::GeometryWithoutBlendShapes)) {
        dna.unloadGeometry();
    } else if (layer == DataLayer::Behavior) {
        dna.unloadBehavior();
    } else if (layer == DataLayer::Definition) {
        dna.unloadMachineLearnedBehavior();
        dna.unloadGeometry();
        dna.unloadBehavior();
        dna.unloadDefinition();
    }
}

void BinaryStreamReaderImpl::read() {
    // Due to possible usage of custom stream implementations, the status actually must be cleared at this point
    // as external streams do not have access to the status reset API
    status.reset();

    trio::StreamScope scope{stream};
    if (!sc::Status::isOk()) {
        return;
    }

    archive >> dna;
    if (!sc::Status::isOk()) {
        return;
    }

    if (!archive.isOk()) {
        status.set(InvalidDataError);
        return;
    }

    if (!dna.signature.matches()) {
        status.set(SignatureMismatchError, dna.signature.value.expected.data(), dna.signature.value.got.data());
        return;
    }
    if (!dna.version.supported()) {
        status.set(VersionMismatchError, dna.version.generation, dna.version.version);
        return;
    }
}

}  // namespace dna
