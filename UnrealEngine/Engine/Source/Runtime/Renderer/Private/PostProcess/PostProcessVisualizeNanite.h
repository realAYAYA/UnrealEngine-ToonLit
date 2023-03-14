// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "Nanite/Nanite.h"

void AddVisualizeNanitePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture Output, const Nanite::FRasterResults& RasterResults);
