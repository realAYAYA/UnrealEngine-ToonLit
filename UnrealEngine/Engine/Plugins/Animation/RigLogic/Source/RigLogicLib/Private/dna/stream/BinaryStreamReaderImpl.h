// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/BinaryStreamReader.h"
#include "dna/ReaderImpl.h"
#include "dna/TypeDefs.h"
#include "dna/stream/FilteredBinaryInputArchive.h"
#include "dna/stream/StreamReaderStatus.h"

#include <status/Provider.h>

namespace dna {

class BinaryStreamReaderImpl : public ReaderImpl<BinaryStreamReader>, private StreamReaderStatus {
    public:
        BinaryStreamReaderImpl(BoundedIOStream* stream_,
                               DataLayer layer_,
                               UnknownLayerPolicy policy_,
                               std::uint16_t maxLOD_,
                               std::uint16_t minLOD_,
                               MemoryResource* memRes_);
        BinaryStreamReaderImpl(BoundedIOStream* stream_, DataLayer layer_, UnknownLayerPolicy policy_,
                               ConstArrayView<std::uint16_t> lods, MemoryResource* memRes_);

        void unload(DataLayer layer) override;
        void read() override;
        bool isLODConstrained() const;

    private:
        BoundedIOStream* stream;
        FilteredBinaryInputArchive archive;
        bool lodConstrained;
};

}  // namespace dna
