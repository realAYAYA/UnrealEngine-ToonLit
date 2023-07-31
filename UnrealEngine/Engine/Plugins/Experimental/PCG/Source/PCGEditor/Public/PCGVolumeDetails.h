// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGComponentDetails.h"

class FPCGVolumeDetails : public FPCGComponentDetails
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	/** ~Begin FPCGComponentDetails interface */
	virtual void GatherPCGComponentsFromSelection(const TArray<TWeakObjectPtr<UObject>>& InObjectSelected) override;
	virtual bool AddDefaultProperties() const override { return false; }
	/** ~End FPCGComponentDetails interface */
};