// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dnatests/Defs.h"

#include "dna/BinaryStreamReader.h"
#include "dna/BinaryStreamWriter.h"

#include <pma/resources/DefaultMemoryResource.h>

class BinaryStreamWriterTest : public ::testing::Test {
    public:
        ~BinaryStreamWriterTest();

    protected:
        void SetUp() override {
            stream = pma::makeScoped<trio::MemoryStream>();
            writer = dna::BinaryStreamWriter::create(stream.get(), &memRes);
            reader = dna::BinaryStreamReader::create(stream.get(),
                                                     dna::DataLayer::All,
                                                     dna::UnknownLayerPolicy::Preserve,
                                                     0u,
                                                     &memRes);
        }

        void TearDown() override {
            dna::BinaryStreamReader::destroy(reader);
            dna::BinaryStreamWriter::destroy(writer);
        }

    protected:
        pma::ScopedPtr<trio::MemoryStream, pma::FactoryDestroy<trio::MemoryStream> > stream;
        pma::DefaultMemoryResource memRes;
        dna::BinaryStreamWriter* writer;
        dna::BinaryStreamReader* reader;
};
