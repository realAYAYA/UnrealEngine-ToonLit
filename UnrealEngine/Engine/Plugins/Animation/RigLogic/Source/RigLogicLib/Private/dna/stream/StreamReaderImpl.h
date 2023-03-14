// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/Reader.h"
#include "dna/ReaderImpl.h"
#include "dna/StreamReader.h"
#include "dna/TypeDefs.h"
#include "dna/stream/FilteredInputArchive.h"

#include <status/Provider.h>

namespace dna {

class StreamReaderImpl : public ReaderImpl<StreamReader> {
    public:
        StreamReaderImpl(BoundedIOStream* stream_,
                         DataLayer layer_,
                         std::uint16_t maxLOD_,
                         std::uint16_t minLOD_,
                         MemoryResource* memRes_);
        StreamReaderImpl(BoundedIOStream* stream_, DataLayer layer_, ConstArrayView<std::uint16_t> lods, MemoryResource* memRes_);

        void unload(DataLayer layer) override;
        void read() override;
        bool isLODConstrained() const;

    private:
        static sc::StatusProvider status;

        BoundedIOStream* stream;
        FilteredInputArchive dnaInputArchive;
        bool lodConstrained;
};

}  // namespace dna
