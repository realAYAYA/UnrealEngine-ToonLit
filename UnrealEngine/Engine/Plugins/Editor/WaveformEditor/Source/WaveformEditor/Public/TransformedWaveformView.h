// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "STransformedWaveformViewPanel.h"
#include "WaveformEditorSequenceDataProvider.h"

struct FTransformedWaveformView
{
	TSharedPtr<STransformedWaveformViewPanel> ViewWidget;
	TSharedPtr<FWaveformEditorSequenceDataProvider> DataProvider;

	bool IsValid()
	{
		return ViewWidget.IsValid() && DataProvider.IsValid();
	}
};