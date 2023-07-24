// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Types/ISlateMetaData.h"

class FDriverUniqueTagMetaData
	: public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FDriverUniqueTagMetaData, ISlateMetaData)

	FDriverUniqueTagMetaData()
	{ }
};
