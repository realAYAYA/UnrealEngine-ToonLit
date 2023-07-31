// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dnatests/Defs.h"

#include "dna/StreamReader.h"


struct LODParameters {
    std::uint16_t maxLOD;
    std::uint16_t currentLOD;
};

class StreamReadWriteIntegrationTest : public ::testing::TestWithParam<LODParameters> {
    public:
        ~StreamReadWriteIntegrationTest();

    protected:
        void SetUp() override {
            params = GetParam();
        }

    protected:
        LODParameters params;
};
