// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieScenePreAnimatedMaterialParameters.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace UE::MovieScene
{

TAutoRegisterPreAnimatedStorageID<FPreAnimatedScalarMaterialParameterStorage> FPreAnimatedScalarMaterialParameterStorage::StorageID;
TAutoRegisterPreAnimatedStorageID<FPreAnimatedVectorMaterialParameterStorage> FPreAnimatedVectorMaterialParameterStorage::StorageID;

FMaterialParameterKey::FMaterialParameterKey(UObject* InBoundMaterial, const FName& InParameterName)
	: BoundMaterial(InBoundMaterial)
	, ParameterName(InParameterName)
{}

uint32 GetTypeHash(const FMaterialParameterKey& InKey)
{
	return GetTypeHash(InKey.BoundMaterial) ^ GetTypeHash(InKey.ParameterName);
}

bool operator==(const FMaterialParameterKey& A, const FMaterialParameterKey& B)
{
	return A.BoundMaterial == B.BoundMaterial && A.ParameterName == B.ParameterName;
}

void FMaterialParameterCollectionScalarTraits::ReplaceObject(FMaterialParameterKey& InOutKey, const FObjectKey& NewObject)
{
	InOutKey.BoundMaterial = NewObject.ResolveObjectPtr();
}

float FMaterialParameterCollectionScalarTraits::CachePreAnimatedValue(UObject* InBoundMaterial, const FName& ParameterName)
{
	float ParameterValue = 0.f;
	if (UMaterialParameterCollectionInstance* MPCI = Cast<UMaterialParameterCollectionInstance>(InBoundMaterial))
	{
		MPCI->GetScalarParameterValue(ParameterName, ParameterValue);
	}
	else if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InBoundMaterial))
	{
		MaterialInterface->GetScalarParameterValue(FMaterialParameterInfo(ParameterName), ParameterValue);
	}
	return ParameterValue;
}

void FMaterialParameterCollectionScalarTraits::RestorePreAnimatedValue(const FMaterialParameterKey& InKey, float OldValue, const FRestoreStateParams& Params)
{
	UObject* BoundObject = InKey.BoundMaterial.ResolveObjectPtr();

	if (UMaterialParameterCollectionInstance* MPCI = Cast<UMaterialParameterCollectionInstance>(BoundObject))
	{
		MPCI->SetScalarParameterValue(InKey.ParameterName, OldValue);
	}
	else if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(BoundObject))
	{
		MID->SetScalarParameterValue(InKey.ParameterName, OldValue);
	}
}

void FMaterialParameterCollectionVectorTraits::ReplaceObject(FMaterialParameterKey& InOutKey, const FObjectKey& NewObject)
{
	InOutKey.BoundMaterial = NewObject.ResolveObjectPtr();
}

FLinearColor FMaterialParameterCollectionVectorTraits::CachePreAnimatedValue(UObject* InBoundMaterial, const FName& ParameterName)
{
	FLinearColor ParameterValue = FLinearColor::White;
	if (UMaterialParameterCollectionInstance* MPCI = Cast<UMaterialParameterCollectionInstance>(InBoundMaterial))
	{
		MPCI->GetVectorParameterValue(ParameterName, ParameterValue);
	}
	else if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InBoundMaterial))
	{
		MaterialInterface->GetVectorParameterValue(FMaterialParameterInfo(ParameterName), ParameterValue);
	}
	return ParameterValue;
}

void FMaterialParameterCollectionVectorTraits::RestorePreAnimatedValue(const FMaterialParameterKey& InKey, const FLinearColor& OldValue, const FRestoreStateParams& Params)
{
	UObject* BoundObject = InKey.BoundMaterial.ResolveObjectPtr();

	if (UMaterialParameterCollectionInstance* MPCI = Cast<UMaterialParameterCollectionInstance>(BoundObject))
	{
		MPCI->SetVectorParameterValue(InKey.ParameterName, OldValue);
	}
	else if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(BoundObject))
	{
		MID->SetVectorParameterValue(InKey.ParameterName, OldValue);
	}
}


} // namespace UE::MovieScene