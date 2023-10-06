// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectChooser_Asset.h"

UObject* FAssetChooser::ChooseObject(FChooserEvaluationContext& Context) const
{
	return Asset;
}