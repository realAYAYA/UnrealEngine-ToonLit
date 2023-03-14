// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dnatests/Defs.h"

#include "dna/StreamReader.h"
#include "dna/StreamWriter.h"

#include <pma/resources/DefaultMemoryResource.h>

class StreamWriterTest : public ::testing::Test {
    public:
        ~StreamWriterTest();

    protected:
        void SetUp() override {
            stream = pma::makeScoped<trio::MemoryStream>();
            writer = dna::StreamWriter::create(stream.get(), &memRes);
            reader = dna::StreamReader::create(stream.get(), dna::DataLayer::All, 0u, &memRes);
        }

        void TearDown() override {
            dna::StreamReader::destroy(reader);
            dna::StreamWriter::destroy(writer);
        }

    protected:
        pma::ScopedPtr<trio::MemoryStream, pma::FactoryDestroy<trio::MemoryStream> > stream;
        pma::DefaultMemoryResource memRes;
        dna::StreamWriter* writer;
        dna::StreamReader* reader;
};
