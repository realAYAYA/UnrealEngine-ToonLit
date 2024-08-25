// Copyright Epic Games, Inc. All Rights Reserved.

#include "CEClonerEffectorShared.h"

#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraSystem.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

void UE::ClonerEffector::SetBillboardComponentSprite(const AActor* InActor, const FString& InTexturePath)
{
	if (!InActor)
	{
		return;
	}

	UBillboardComponent* BillboardComponent = InActor->FindComponentByClass<UBillboardComponent>();

	if (!BillboardComponent)
	{
		return;
	}

	UTexture2D* SpriteTexture = FindObject<UTexture2D>(nullptr, *InTexturePath);

	if (!SpriteTexture)
	{
		SpriteTexture = LoadObject<UTexture2D>(nullptr, *InTexturePath);
	}

	if (SpriteTexture && SpriteTexture != BillboardComponent->Sprite)
	{
		BillboardComponent->SetSprite(SpriteTexture);
	}
}

void UE::ClonerEffector::SetBillboardComponentVisibility(const AActor* InActor, bool bInVisibility)
{
	if (!InActor)
	{
		return;
	}

	UBillboardComponent* BillboardComponent = InActor->FindComponentByClass<UBillboardComponent>();

	if (!BillboardComponent)
	{
		return;
	}

	BillboardComponent->SetVisibility(bInVisibility, false);
}

void FCEClonerEffectorChannelData::Write(UNiagaraDataChannelWriter* InWriter) const
{
	if (!InWriter)
	{
		return;
	}

	/** General */
	InWriter->WriteInt(EasingName, Identifier, static_cast<int32>(Easing));
	InWriter->WriteInt(ModeName, Identifier, static_cast<int32>(Mode));
	InWriter->WriteInt(TypeName, Identifier, static_cast<int32>(Type));
	InWriter->WriteFloat(MagnitudeName, Identifier, Magnitude);
	InWriter->WriteVector(InnerExtentName, Identifier, InnerExtent);
	InWriter->WriteVector(OuterExtentName, Identifier, OuterExtent);
	InWriter->WriteVector(LocationDeltaName, Identifier, LocationDelta);
	InWriter->WriteQuat(RotationDeltaName, Identifier, RotationDelta);
	InWriter->WriteVector(ScaleDeltaName, Identifier, ScaleDelta);
	InWriter->WritePosition(LocationName, Identifier, Location);
	InWriter->WriteQuat(RotationName, Identifier, Rotation);
	InWriter->WriteVector(ScaleName, Identifier, Scale);
	InWriter->WriteFloat(FrequencyName, Identifier, Frequency);
	InWriter->WriteVector(PanName, Identifier, Pan);

	/** Forces */
	InWriter->WriteFloat(OrientationForceRateName, Identifier, OrientationForceRate);
	InWriter->WriteVector(OrientationForceMinName, Identifier, OrientationForceMin);
	InWriter->WriteVector(OrientationForceMaxName, Identifier, OrientationForceMax);
	InWriter->WriteFloat(VortexForceAmountName, Identifier, VortexForceAmount);
	InWriter->WriteVector(VortexForceAxisName, Identifier, VortexForceAxis);
	InWriter->WriteFloat(CurlNoiseForceStrengthName, Identifier, CurlNoiseForceStrength);
	InWriter->WriteFloat(CurlNoiseForceFrequencyName, Identifier, CurlNoiseForceFrequency);
	InWriter->WriteFloat(AttractionForceStrengthName, Identifier, AttractionForceStrength);
	InWriter->WriteFloat(AttractionForceFalloffName, Identifier, AttractionForceFalloff);
	InWriter->WriteVector(GravityForceAccelerationName, Identifier, GravityForceAcceleration);
}

void FCEClonerEffectorChannelData::Read(const UNiagaraDataChannelReader* InReader)
{
	if (!InReader)
	{
		return;
	}

	/** Effector */
	bool bIsValid;
	Easing = static_cast<ECEClonerEasing>(InReader->ReadInt(EasingName, Identifier, bIsValid));
	Mode = static_cast<ECEClonerEffectorMode>(InReader->ReadInt(ModeName, Identifier, bIsValid));
	Type = static_cast<ECEClonerEffectorType>(InReader->ReadInt(TypeName, Identifier, bIsValid));
	Magnitude = InReader->ReadFloat(MagnitudeName, Identifier, bIsValid);
	InnerExtent = InReader->ReadVector(InnerExtentName, Identifier, bIsValid);
	OuterExtent = InReader->ReadVector(OuterExtentName, Identifier, bIsValid);
	LocationDelta = InReader->ReadVector(LocationDeltaName, Identifier, bIsValid);
	RotationDelta = InReader->ReadQuat(RotationDeltaName, Identifier, bIsValid);
	ScaleDelta = InReader->ReadVector(ScaleDeltaName, Identifier, bIsValid);
	Location = InReader->ReadPosition(LocationName, Identifier, bIsValid);
	Rotation = InReader->ReadQuat(RotationName, Identifier, bIsValid);
	Scale = InReader->ReadVector(ScaleName, Identifier, bIsValid);
	Frequency = InReader->ReadFloat(FrequencyName, Identifier, bIsValid);
	Pan = InReader->ReadVector(PanName, Identifier, bIsValid);

	/** Forces */
	OrientationForceRate = InReader->ReadFloat(OrientationForceRateName, Identifier, bIsValid);
	OrientationForceMin = InReader->ReadVector(OrientationForceMinName, Identifier, bIsValid);
	OrientationForceMax = InReader->ReadVector(OrientationForceMaxName, Identifier, bIsValid);
	VortexForceAmount = InReader->ReadFloat(VortexForceAmountName, Identifier, bIsValid);
	VortexForceAxis = InReader->ReadVector(VortexForceAxisName, Identifier, bIsValid);
	CurlNoiseForceStrength = InReader->ReadFloat(CurlNoiseForceStrengthName, Identifier, bIsValid);
	CurlNoiseForceFrequency = InReader->ReadFloat(CurlNoiseForceFrequencyName, Identifier, bIsValid);
	AttractionForceStrength = InReader->ReadFloat(AttractionForceStrengthName, Identifier, bIsValid);
	AttractionForceFalloff = InReader->ReadFloat(AttractionForceFalloffName, Identifier, bIsValid);
	GravityForceAcceleration = InReader->ReadVector(GravityForceAccelerationName, Identifier, bIsValid);
}

FCEClonerEffectorDataInterfaces::FCEClonerEffectorDataInterfaces(const UNiagaraSystem* InSystem)
{
	check(InSystem);
	const FNiagaraUserRedirectionParameterStore& UserParameterStore = InSystem->GetExposedParameters();

	static const FNiagaraVariable EffectorIndexDIVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayInt32::StaticClass()), FCEClonerEffectorDataInterfaces::IndexName);
	UNiagaraDataInterfaceArrayInt32* IndexArrayDI = CastChecked<UNiagaraDataInterfaceArrayInt32>(UserParameterStore.GetDataInterface(EffectorIndexDIVar));
	DataInterfaces.Add(EffectorIndexDIVar.GetName(), IndexArrayDI);
}

void FCEClonerEffectorDataInterfaces::Clear() const
{
	if (UNiagaraDataInterfaceArrayInt32* IndexArray = GetIndexArray())
	{
		IndexArray->GetArrayReference().Empty();
	}
}

void FCEClonerEffectorDataInterfaces::CopyTo(FCEClonerEffectorDataInterfaces& InOther) const
{
	for (const TPair<FName, TObjectPtr<UNiagaraDataInterface>>& DataInterfacePair : DataInterfaces)
	{
		const TObjectPtr<UNiagaraDataInterface> DataInterface = DataInterfacePair.Value;
		const TObjectPtr<UNiagaraDataInterface>* OtherDataInterface = InOther.DataInterfaces.Find(DataInterfacePair.Key);

		if (!OtherDataInterface || !DataInterface)
		{
			continue;
		}

		DataInterface->CopyTo(*OtherDataInterface);
	}
}

void FCEClonerEffectorDataInterfaces::Resize(int32 InSize) const
{
	if (UNiagaraDataInterfaceArrayInt32* IndexArray = GetIndexArray())
	{
		IndexArray->GetArrayReference().SetNum(InSize);
	}
}

void FCEClonerEffectorDataInterfaces::Remove(int32 InIndex) const
{
	UNiagaraDataInterfaceArrayInt32* IndexArray = GetIndexArray();
	if (IndexArray && IndexArray->GetArrayReference().IsValidIndex(InIndex))
	{
		IndexArray->GetArrayReference().RemoveAt(InIndex);
	}
}

bool FCEClonerEffectorDataInterfaces::IsValid() const
{
	for (const TPair<FName, TObjectPtr<UNiagaraDataInterface>>& DataInterfacePair : DataInterfaces)
	{
		if (!DataInterfacePair.Value.Get())
		{
			return false;
		}
	}

	return !DataInterfaces.IsEmpty();
}

int32 FCEClonerEffectorDataInterfaces::Num() const
{
	UNiagaraDataInterfaceArrayInt32* IndexDI = GetIndexArray();

	const int32 IndexSize = IndexDI->GetArrayReference().Num();

	return IndexSize;
}

UNiagaraDataInterfaceArrayInt32* FCEClonerEffectorDataInterfaces::GetIndexArray() const
{
	if (const TObjectPtr<UNiagaraDataInterface>* Array = DataInterfaces.Find(FCEClonerEffectorDataInterfaces::IndexName))
	{
		return Cast<UNiagaraDataInterfaceArrayInt32>(Array->Get());
	}

	return nullptr;
}
