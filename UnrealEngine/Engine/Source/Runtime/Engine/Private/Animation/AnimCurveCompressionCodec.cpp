// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimCurveCompressionCodec.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimCurveCompressionCodec)

UAnimCurveCompressionCodec::UAnimCurveCompressionCodec(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA
void UAnimCurveCompressionCodec::PostInitProperties()
{
	Super::PostInitProperties();

	if (!IsTemplate())
	{
		InstanceGuid = FGuid::NewGuid();
	}
}

void UAnimCurveCompressionCodec::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	InstanceGuid = FGuid::NewGuid();
}

void UAnimCurveCompressionCodec::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!Ar.IsCooking() && !(Ar.IsLoading() && GetOutermost()->bIsCookedForEditor))
	{
		Ar << InstanceGuid;
	}
}

void UAnimCurveCompressionCodec::PopulateDDCKey(FArchive& Ar)
{
	Ar << InstanceGuid;
}
#endif

