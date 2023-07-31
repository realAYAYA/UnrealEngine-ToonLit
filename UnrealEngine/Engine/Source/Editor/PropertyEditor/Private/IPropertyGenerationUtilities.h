// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** Defines internal utilities for generating property layouts. */
class IPropertyGenerationUtilities
{
public:
	virtual ~IPropertyGenerationUtilities() { }

	/** Gets the instance type customization map for the details implementation providing this utilities object. */
	virtual const FCustomPropertyTypeLayoutMap& GetInstancedPropertyTypeLayoutMap() const = 0;

	/** Rebuilds the details tree nodes for the details implementation providing this utilities object. */
	virtual void RebuildTreeNodes() = 0;
};