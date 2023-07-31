// Copyright Epic Games, Inc. All Rights Reserved.

#include "dna/stream/StreamWriterImpl.h"

#include "dna/TypeDefs.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <tuple>
#include <utility>

namespace dna {

StreamWriter::~StreamWriter() = default;

StreamWriter* StreamWriter::create(BoundedIOStream* stream, MemoryResource* memRes) {
    PolyAllocator<StreamWriterImpl> alloc{memRes};
    return alloc.newObject(stream, memRes);
}

void StreamWriter::destroy(StreamWriter* instance) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto writer = static_cast<StreamWriterImpl*>(instance);
    PolyAllocator<StreamWriterImpl> alloc{writer->getMemoryResource()};
    alloc.deleteObject(writer);
}

StreamWriterImpl::StreamWriterImpl(BoundedIOStream* stream_, MemoryResource* memRes_) :
    BaseImpl{memRes_},
    WriterImpl{memRes_},
    stream{stream_},
    binaryOutputArchive{stream_} {
}

void StreamWriterImpl::write() {
    stream->open();
    binaryOutputArchive << dna;
    stream->close();
}

}  // namespace dna
