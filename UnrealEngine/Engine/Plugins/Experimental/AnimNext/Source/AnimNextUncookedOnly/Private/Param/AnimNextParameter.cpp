// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameter.h"
#include "UObject/Package.h"

#if WITH_EDITORONLY_DATA

UE::AnimNext::UncookedOnly::FOnParameterModified& UAnimNextParameter::OnModified()
{
	return ModifiedDelegate;
}

void UAnimNextParameter::SetType(const FAnimNextParamType& InType, bool bSetupUndoRedo)
{
	ensure(InType.IsValid());

	if(bSetupUndoRedo)
	{
		Modify();
	}

	Type = InType;

	ModifiedDelegate.Broadcast(this);
}

void UAnimNextParameter::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	ModifiedDelegate.Broadcast(this);
}

bool UAnimNextParameter::IsAsset() const
{
	// Parameters are considered assets to allow using the asset logic for save dialogs, etc.
	// Also, they return true even if pending kill, in order to show up as deleted in these dialogs.
	return IsPackageExternal() && !GetPackage()->HasAnyFlags(RF_Transient) && !HasAnyFlags(RF_Transient | RF_ClassDefaultObject);
}

#endif // #if WITH_EDITORONLY_DATA