// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/IScreenReaderBuilder.h"

class FSlateScreenReaderBuilder : public IScreenReaderBuilder
{
public:
	FSlateScreenReaderBuilder() {}
	virtual ~FSlateScreenReaderBuilder() {}

	virtual TSharedRef<FScreenReaderBase> Create(const IScreenReaderBuilder::FArgs& InArgs) override;

};
