// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimpleAV.h"

EAVPreset USimpleAVHelper::ConvertPreset(ESimpleAVPreset From)
{
	switch (From)
	{
	case ESimpleAVPreset::UltraLowQuality:
		return EAVPreset::UltraLowQuality;
	case ESimpleAVPreset::LowQuality:
		return EAVPreset::LowQuality;
	case ESimpleAVPreset::HighQuality:
		return EAVPreset::HighQuality;
	case ESimpleAVPreset::Lossless:
		return EAVPreset::Lossless;
	default:
		return EAVPreset::Default;
	}
}
