// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonValue.h"

struct GLTFEXPORTER_API IGLTFJsonObject : IGLTFJsonValue
{
	virtual void WriteValue(IGLTFJsonWriter& Writer) const override final;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const = 0;
};
