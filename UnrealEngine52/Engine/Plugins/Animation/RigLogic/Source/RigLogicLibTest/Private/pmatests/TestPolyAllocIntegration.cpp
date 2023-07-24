// Copyright Epic Games, Inc. All Rights Reserved.

#include "pmatests/Defs.h"

#include "pma/TypeDefs.h"

TEST(PolyAllocIntegrationTest, InstantiateTypes) {
    pma::String<char> str;
    pma::Vector<int> vec;
    pma::Matrix<int> mat;
    pma::Set<int> set;
    pma::Map<int, int> map;
    pma::UnorderedSet<int> uset;
    pma::UnorderedMap<int, int> umap;
    ASSERT_TRUE(true);
}
