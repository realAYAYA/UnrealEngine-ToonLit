// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/DNA.h"
#include "dna/StreamWriter.h"
#include "dna/WriterImpl.h"

#include <terse/archives/binary/OutputArchive.h>

namespace dna {

class StreamWriterImpl : public WriterImpl<StreamWriter> {
    public:
        StreamWriterImpl(BoundedIOStream* stream_, MemoryResource* memRes_);

        void write() override;

    private:
        BoundedIOStream* stream;
        terse::BinaryOutputArchive<BoundedIOStream> binaryOutputArchive;

};

}  // namespace dna
