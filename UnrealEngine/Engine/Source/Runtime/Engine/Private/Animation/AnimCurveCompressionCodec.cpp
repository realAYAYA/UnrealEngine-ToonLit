// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimCurveCompressionCodec.h"
#include "UObject/FortniteReleaseBranchCustomObjectVersion.h"
#include "UObject/Package.h"
#include "Interfaces/ITargetPlatform.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimCurveCompressionCodec)

UAnimCurveCompressionCodec::UAnimCurveCompressionCodec(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimCurveCompressionCodec::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFortniteReleaseBranchCustomObjectVersion::GUID);
#if !WITH_EDITOR
	if (Ar.CustomVer(FFortniteReleaseBranchCustomObjectVersion::GUID) >= FFortniteReleaseBranchCustomObjectVersion::SerializeAnimCurveCompressionCodecGuidOnCook)
#endif // !WITH_EDITOR
	{
		Ar << InstanceGuid;
	}
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

void UAnimCurveCompressionCodec::PopulateDDCKey(FArchive& Ar)
{
	Ar << InstanceGuid;
}
#endif

