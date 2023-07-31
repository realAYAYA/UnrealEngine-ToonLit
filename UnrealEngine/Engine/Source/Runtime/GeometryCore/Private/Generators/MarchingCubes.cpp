// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generators/MarchingCubes.h"

using namespace UE::Geometry;

constexpr int FMarchingCubes::EdgeIndices[12][2];
constexpr int FMarchingCubes::EdgeTable[256];
constexpr int FMarchingCubes::TriTable[256][16];