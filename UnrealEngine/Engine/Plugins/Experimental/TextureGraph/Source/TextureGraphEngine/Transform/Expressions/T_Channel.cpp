// Copyright Epic Games, Inc. All Rights Reserved.

#include "T_Channel.h"
#include "Job/JobArgs.h"
#include "TextureGraphEngine.h"
#include "Math/Vector.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Helper/GraphicsUtil.h"

IMPLEMENT_GLOBAL_SHADER(FSH_ChannelSplitter_Red, "/Plugin/TextureGraph/Expressions/Expression_ChannelSplitter.usf", "FSH_ChannelSplitter_Red", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_ChannelSplitter_Green, "/Plugin/TextureGraph/Expressions/Expression_ChannelSplitter.usf", "FSH_ChannelSplitter_Green", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_ChannelSplitter_Blue, "/Plugin/TextureGraph/Expressions/Expression_ChannelSplitter.usf", "FSH_ChannelSplitter_Blue", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_ChannelSplitter_Alpha, "/Plugin/TextureGraph/Expressions/Expression_ChannelSplitter.usf", "FSH_ChannelSplitter_Alpha", SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(FSH_ChannelCombiner, "/Plugin/TextureGraph/Expressions/Expression_ChannelCombiner.usf", "FSH_ChannelCombiner", SF_Pixel);
