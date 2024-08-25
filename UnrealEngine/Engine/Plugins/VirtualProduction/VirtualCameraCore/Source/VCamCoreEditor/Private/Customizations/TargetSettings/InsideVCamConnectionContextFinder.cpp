// Copyright Epic Games, Inc. All Rights Reserved.

#include "InsideVCamConnectionContextFinder.h"

#include "UI/VCamConnectionStructs.h"

#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

namespace UE::VCamCoreEditor::Private::ConnectionTargetContextFinding
{
	void FInsideVCamConnectionContextFinder::FindAndProcessContext(
		const TSharedRef<IPropertyHandle>& ConnectionTargetSettingsStructHandle,
		IPropertyUtilities& PropertyUtils,
		TFunctionRef<void(const FVCamConnection& Connection)> ProcessWithContext,
		TFunctionRef<void()> ProcessWithoutContext)
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = ConnectionTargetSettingsStructHandle->GetParentHandle();
		const FStructProperty* ParentStruct = CastField<FStructProperty>(ParentHandle->GetProperty());
		const bool bIsValidHandle = ParentHandle
			&& ParentStruct
			&& ParentStruct->Struct == FVCamConnection::StaticStruct();
		const TSharedPtr<IPropertyHandle> OptionalVCamConnectionParentStructHandle = bIsValidHandle
			? ParentHandle
			: nullptr;

		void* ValueData = nullptr;
		if (OptionalVCamConnectionParentStructHandle
			&& OptionalVCamConnectionParentStructHandle->IsValidHandle()
			&& OptionalVCamConnectionParentStructHandle->GetValueData(ValueData) == FPropertyAccess::Success
			&& ValueData)
		{
			const FVCamConnection* ConnectionData = static_cast<FVCamConnection*>(ValueData);
			ProcessWithContext(*ConnectionData);
		}
		else
		{
			ProcessWithoutContext();
		}
	}
}
