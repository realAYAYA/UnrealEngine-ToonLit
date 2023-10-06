// Copyright Epic Games, Inc. All Rights Reserved.

#include "AVCoder.h"

TMap<FTypeID, TMap<FTypeID, TMap<FTypeID, TSharedPtr<void>>>> IAVCoder::Factories = TMap<FTypeID, TMap<FTypeID, TMap<FTypeID, TSharedPtr<void>>>>();
