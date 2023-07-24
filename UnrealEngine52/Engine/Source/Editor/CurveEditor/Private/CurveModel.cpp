// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveModel.h"

#include "Containers/Array.h"
#include "CurveDataAbstraction.h"
#include "Curves/KeyHandle.h"
#include "HAL/PlatformCrt.h"

void FCurveModel::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, const FKeyAttributes& InKeyAttributes, EPropertyChangeType::Type ChangeType)
{
	TArray<FKeyAttributes> ExpandedAttributes;
	ExpandedAttributes.Reserve(InKeys.Num());

	for (FKeyHandle Handle : InKeys)
	{
		ExpandedAttributes.Add(InKeyAttributes);
	}

	SetKeyAttributes(InKeys, ExpandedAttributes);
	Modify();
}

TOptional<FKeyHandle> FCurveModel::AddKey(const FKeyPosition& NewKeyPosition, const FKeyAttributes& InAttributes)
{
	Modify();

	TOptional<FKeyHandle> Handle;

	TArrayView<TOptional<FKeyHandle>> Handles = MakeArrayView(&Handle, 1);
	AddKeys(TArrayView<const FKeyPosition>(&NewKeyPosition, 1), TArrayView<const FKeyAttributes>(&InAttributes, 1), &Handles);
	return Handle;
}
