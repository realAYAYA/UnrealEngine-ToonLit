// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchyElements.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigControlHierarchy.h"
#include "Units/RigUnitContext.h"
#include "ControlRigObjectVersion.h"
#include "ControlRigGizmoLibrary.h"
#include "AnimationCoreLibrary.h"
#include "Algo/Transform.h"

////////////////////////////////////////////////////////////////////////////////
// FRigBaseElement
////////////////////////////////////////////////////////////////////////////////

FRigBaseElement::FRigBaseElement(const FRigBaseElement& InOther)
{
	*this = InOther;
}

FRigBaseElement& FRigBaseElement::operator=(const FRigBaseElement& InOther)
{
	Key = InOther.Key;
	NameString = InOther.NameString;
	Index = InOther.Index;
	SubIndex = InOther.SubIndex;
	bSelected = InOther.bSelected;
	CreatedAtInstructionIndex = InOther.CreatedAtInstructionIndex;
	TopologyVersion = InOther.TopologyVersion;
	CachedChildren.Reset();
	OwnedInstances = 1;

	RemoveAllMetadata();
	for(const FRigBaseMetadata* InOtherMd : InOther.Metadata)
	{
		FRigBaseMetadata* Md = SetupValidMetadata(InOtherMd->Name, InOtherMd->Type);
		check(Md);
		Md->SetValueData(InOtherMd->GetValueData(), InOtherMd->GetValueSize());
	}

	return *this;
}

FRigBaseElement::~FRigBaseElement()
{
	RemoveAllMetadata();
}

UScriptStruct* FRigBaseElement::GetElementStruct() const
{
	switch(GetType())
	{
		case ERigElementType::Bone:
		{
			return FRigBoneElement::StaticStruct();
		}
		case ERigElementType::Null:
		{
			return FRigNullElement::StaticStruct();
		}
		case ERigElementType::Control:
		{
			return FRigControlElement::StaticStruct();
		}
		case ERigElementType::Curve:
		{
			return FRigCurveElement::StaticStruct();
		}
		case ERigElementType::Reference:
		{
			return FRigReferenceElement::StaticStruct();
		}
		case ERigElementType::RigidBody:
		{
			return FRigRigidBodyElement::StaticStruct();
		}
		default:
		{
				break;
		}
	}
	return FRigBaseElement::StaticStruct();
}

void FRigBaseElement::Serialize(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);

	if (Ar.IsSaving() || Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Save(Ar, Hierarchy, SerializationPhase);
	}
	else if (Ar.IsLoading())
	{
		Load(Ar, Hierarchy, SerializationPhase);
	}
	else
	{
		// remove due to FPIEFixupSerializer hitting this checkNoEntry();
	}
}

void FRigBaseElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Ar << Key;

		static const UEnum* MetadataTypeEnum = StaticEnum<ERigMetadataType>();

		int32 MetadataNum = Metadata.Num();
		Ar << MetadataNum;

		for(FRigBaseMetadata* Md : Metadata)
		{
			FName MetadataName = Md->GetName();
			FName MetadataTypeName = MetadataTypeEnum->GetNameByValue((int64)Md->GetType());

			Ar << MetadataName;
			Ar << MetadataTypeName;
			Md->Serialize(Ar, false);
		}
	}
}

void FRigBaseElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		FRigElementKey LoadedKey;
	
		Ar << LoadedKey;

		ensure(LoadedKey.Type == Key.Type);
		Key = LoadedKey;

		NameString = Key.Name.ToString();

		RemoveAllMetadata();
		
		if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::HierarchyElementMetadata)
		{
			static const UEnum* MetadataTypeEnum = StaticEnum<ERigMetadataType>();

			int32 MetadataNum = 0;
			Ar << MetadataNum;

			for(int32 MetadataIndex = 0; MetadataIndex < MetadataNum; MetadataIndex++)
			{
				FName MetadataName(NAME_None);
				FName MetadataTypeName(NAME_None);
				Ar << MetadataName;
				Ar << MetadataTypeName;
				
				const ERigMetadataType MetadataType = (ERigMetadataType)MetadataTypeEnum->GetValueByName(MetadataTypeName);
				FRigBaseMetadata* Md = FRigBaseMetadata::MakeMetadata(this, MetadataName, MetadataType);
				Md->Serialize(Ar, true);
				Metadata.Add(Md);
			}

			for(int32 MetadataIndex = 0; MetadataIndex < Metadata.Num(); MetadataIndex++)
			{
				MetadataNameToIndex.Add(Metadata[MetadataIndex]->GetName(), MetadataIndex);
			}

			for(int32 MetadataIndex = 0; MetadataIndex < Metadata.Num(); MetadataIndex++)
			{
				NotifyMetadataChanged(Metadata[MetadataIndex]->GetName());
			}
		}
	}
}

bool FRigBaseElement::RemoveMetadata(const FName& InName)
{
	if(const int32* MetadataIndexPtr = MetadataNameToIndex.Find(InName))
	{
		const int32 MetadataIndex = *MetadataIndexPtr;
		FRigBaseMetadata::DestroyMetadata(&Metadata[MetadataIndex]);
		MetadataNameToIndex.Remove(InName);
		Metadata.RemoveAt(MetadataIndex);
		for(TPair<FName, int32>& Pair : MetadataNameToIndex)
		{
			if(Pair.Value > MetadataIndex)
			{
				Pair.Value--;
			}
		}
		NotifyMetadataChanged(InName);
		return true;
	}
	return false;
}

bool FRigBaseElement::RemoveAllMetadata()
{
	if(!Metadata.IsEmpty())
	{
		TArray<FName> Names;
		Names.Reserve(Metadata.Num());
		for(FRigBaseMetadata* Md : Metadata)
		{
			Names.Add(Md->GetName());
			FRigBaseMetadata::DestroyMetadata(&Md);
		}
		Metadata.Reset();
		MetadataNameToIndex.Reset();
		for(const FName& Name: Names)
		{
			NotifyMetadataChanged(Name);
		}
		return true;
	}
	return false;
}

void FRigBaseElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy)
{
	// remember all previous names
	TArray<FName> RemainingNames;
	Algo::Transform(Metadata, RemainingNames, [](const FRigBaseMetadata* Md) -> FName
	{
		return Md->GetName();
	});

	// copy over all metadata. this also takes care of potential type changes
	for(const FRigBaseMetadata* InOtherMd : InOther->Metadata)
	{
		FRigBaseMetadata* Md = SetupValidMetadata(InOtherMd->Name, InOtherMd->Type);
		check(Md);
		Md->SetValueData(InOtherMd->GetValueData(), InOtherMd->GetValueSize());
		RemainingNames.Remove(InOtherMd->Name);
	}

	// remove all remaining metadata
	for(const FName& NameToRemove : RemainingNames)
	{
		RemoveMetadata(NameToRemove);
	}

	// rebuild the name map
	MetadataNameToIndex.Reset();
	for(int32 MetadataIndex = 0; MetadataIndex < Metadata.Num(); MetadataIndex++)
	{
		MetadataNameToIndex.Add(Metadata[MetadataIndex]->GetName(), MetadataIndex);
	}
}

FRigBaseMetadata* FRigBaseElement::SetupValidMetadata(const FName& InName, ERigMetadataType InType)
{
	if(const int32* MetadataIndexPtr = MetadataNameToIndex.Find(InName))
	{
		const int32 MetadataIndex = *MetadataIndexPtr;
		if(Metadata[MetadataIndex]->GetType() == InType)
		{
			return Metadata[MetadataIndex];
		}

		FRigBaseMetadata::DestroyMetadata(&Metadata[MetadataIndex]);
		Metadata[MetadataIndex] = FRigBaseMetadata::MakeMetadata(this, InName, InType);
		NotifyMetadataChanged(InName);
		return Metadata[MetadataIndex];
	}

	FRigBaseMetadata* Md = FRigBaseMetadata::MakeMetadata(this, InName, InType);
	const int32 MetadataIndex = Metadata.Add(Md);
	MetadataNameToIndex.Add(InName, MetadataIndex);
	NotifyMetadataChanged(InName);
	return Md;
}

void FRigBaseElement::NotifyMetadataChanged(const FName& InName)
{
	MetadataVersion++;
	if(MetadataChangedDelegate.IsBound())
	{
		MetadataChangedDelegate.Execute(GetKey(), InName);
	}
}

void FRigBaseElement::NotifyMetadataTagChanged(const FName& InTag, bool bAdded)
{
	if(MetadataTagChangedDelegate.IsBound())
	{
		MetadataTagChangedDelegate.Execute(GetKey(), InTag, bAdded);
	}
}

////////////////////////////////////////////////////////////////////////////////
// FRigComputedTransform
////////////////////////////////////////////////////////////////////////////////

void FRigComputedTransform::Save(FArchive& Ar)
{
	Ar << Transform;
	Ar << bDirty;
}

void FRigComputedTransform::Load(FArchive& Ar)
{
	// load and save are identical
	Save(Ar);
}

////////////////////////////////////////////////////////////////////////////////
// FRigLocalAndGlobalTransform
////////////////////////////////////////////////////////////////////////////////

void FRigLocalAndGlobalTransform::Save(FArchive& Ar)
{
	Local.Save(Ar);
	Global.Save(Ar);
}

void FRigLocalAndGlobalTransform::Load(FArchive& Ar)
{
	Local.Load(Ar);
	Global.Load(Ar);
}

////////////////////////////////////////////////////////////////////////////////
// FRigCurrentAndInitialTransform
////////////////////////////////////////////////////////////////////////////////

void FRigCurrentAndInitialTransform::Save(FArchive& Ar)
{
	Current.Save(Ar);
	Initial.Save(Ar);
}

void FRigCurrentAndInitialTransform::Load(FArchive& Ar)
{
	Current.Load(Ar);
	Initial.Load(Ar);
}

////////////////////////////////////////////////////////////////////////////////
// FRigPreferredEulerAngles
////////////////////////////////////////////////////////////////////////////////

void FRigPreferredEulerAngles::Save(FArchive& Ar)
{
	static const UEnum* RotationOrderEnum = StaticEnum<EEulerRotationOrder>();
	FName RotationOrderName = RotationOrderEnum->GetNameByValue((int64)RotationOrder);
	Ar << RotationOrderName;
	Ar << Current;
	Ar << Initial;
}

void FRigPreferredEulerAngles::Load(FArchive& Ar)
{
	static const UEnum* RotationOrderEnum = StaticEnum<EEulerRotationOrder>();
	FName RotationOrderName;
	Ar << RotationOrderName;
	RotationOrder = (EEulerRotationOrder)RotationOrderEnum->GetValueByName(RotationOrderName);
	Ar << Current;
	Ar << Initial;
}

void FRigPreferredEulerAngles::Reset()
{
	RotationOrder = DefaultRotationOrder;
	Initial = Current = FVector::ZeroVector;
}

FRotator FRigPreferredEulerAngles::GetRotator(bool bInitial) const
{
	return FRotator::MakeFromEuler(GetAngles(bInitial, DefaultRotationOrder));
}

FRotator FRigPreferredEulerAngles::SetRotator(const FRotator& InValue, bool bInitial, bool bFixEulerFlips)
{
	if(RotationOrder == DefaultRotationOrder)
	{
		if(bFixEulerFlips)
		{
			const FRotator CurrentValue = GetRotator(bInitial);
			
			//Find Diff of the rotation from current and just add that instead of setting so we can go over/under -180
			FRotator CurrentWinding;
			FRotator CurrentRotRemainder;
			CurrentValue.GetWindingAndRemainder(CurrentWinding, CurrentRotRemainder);

			FRotator DeltaRot = InValue - CurrentRotRemainder;
			DeltaRot.Normalize();
			const FRotator FixedValue = CurrentValue + DeltaRot;

			SetAngles(FixedValue.Euler(), bInitial, DefaultRotationOrder);
			return FixedValue;
		}
	}
	SetAngles(InValue.Euler(), bInitial, DefaultRotationOrder);
	return InValue;
}

FVector FRigPreferredEulerAngles::GetAngles(bool bInitial, EEulerRotationOrder InRotationOrder) const
{
	if(RotationOrder == InRotationOrder)
	{
		return Get(bInitial);
	}
	return AnimationCore::ChangeEulerRotationOrder(Get(bInitial), RotationOrder, InRotationOrder);
}

void FRigPreferredEulerAngles::SetAngles(const FVector& InValue, bool bInitial, EEulerRotationOrder InRotationOrder)
{
	FVector Value = InValue;
	if(RotationOrder != InRotationOrder)
	{
		Value = AnimationCore::ChangeEulerRotationOrder(Value, InRotationOrder, RotationOrder);
	}
	Get(bInitial) = Value;
}

////////////////////////////////////////////////////////////////////////////////
// FRigElementHandle
////////////////////////////////////////////////////////////////////////////////

FRigElementHandle::FRigElementHandle(URigHierarchy* InHierarchy, const FRigElementKey& InKey)
: Hierarchy(InHierarchy)
, Key(InKey)
{
}

FRigElementHandle::FRigElementHandle(URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
: Hierarchy(InHierarchy)
, Key(InElement->GetKey())
{
}

const FRigBaseElement* FRigElementHandle::Get() const
{
	if(Hierarchy.IsValid())
	{
		return Hierarchy->Find(Key);
	}
	return nullptr;
}

FRigBaseElement* FRigElementHandle::Get()
{
	if(Hierarchy.IsValid())
	{
		return Hierarchy->Find(Key);
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// FRigTransformElement
////////////////////////////////////////////////////////////////////////////////

void FRigTransformElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Pose.Save(Ar);
	}
}

void FRigTransformElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Pose.Load(Ar);
	}
}

void FRigTransformElement::CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights)
{
	Super::CopyPose(InOther, bCurrent, bInitial, bWeights);

	if(FRigTransformElement* Other = Cast<FRigTransformElement>(InOther))
	{
		if(bCurrent)
		{
			Pose.Current = Other->Pose.Current;
		}
		if(bInitial)
		{
			Pose.Initial = Other->Pose.Initial;
		}
	}
}

void FRigTransformElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther,
	URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);
	
	const FRigTransformElement* SourceTransform = CastChecked<FRigTransformElement>(InOther);
	Pose = SourceTransform->Pose;

	ElementsToDirty.Reset();
	ElementsToDirty.Reserve(SourceTransform->ElementsToDirty.Num());
	
	for(int32 ElementToDirtyIndex = 0; ElementToDirtyIndex < SourceTransform->ElementsToDirty.Num(); ElementToDirtyIndex++)
	{
		const FElementToDirty& Source = SourceTransform->ElementsToDirty[ElementToDirtyIndex];
		FRigTransformElement* TargetTransform = CastChecked<FRigTransformElement>(InHierarchy->Get(Source.Element->Index));
		const FElementToDirty Target(TargetTransform, Source.HierarchyDistance);
		ElementsToDirty.Add(Target);
		check(ElementsToDirty[ElementToDirtyIndex].Element->GetKey() == Source.Element->GetKey());
	}
}

////////////////////////////////////////////////////////////////////////////////
// FRigSingleParentElement
////////////////////////////////////////////////////////////////////////////////

void FRigSingleParentElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::InterElementData)
	{
		FRigElementKey ParentKey;
		if(ParentElement)
		{
			ParentKey = ParentElement->GetKey();
		}
		Ar << ParentKey;
	}
}

void FRigSingleParentElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::InterElementData)
	{
		FRigElementKey ParentKey;
		Ar << ParentKey;

		if(ParentKey.IsValid())
		{
			ParentElement = Hierarchy->FindChecked<FRigTransformElement>(ParentKey);
		}
	}
}

void FRigSingleParentElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther,
	URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);

	const FRigSingleParentElement* Source = CastChecked<FRigSingleParentElement>(InOther); 
	if(Source->ParentElement)
	{
		ParentElement = CastChecked<FRigTransformElement>(InHierarchy->Get(Source->ParentElement->Index));
		check(ParentElement->GetKey() == Source->ParentElement->GetKey());
	}
	else
	{
		ParentElement = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////
// FRigMultiParentElement
////////////////////////////////////////////////////////////////////////////////

void FRigMultiParentElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		int32 NumParents = ParentConstraints.Num();
		Ar << NumParents;
	}
	else if(SerializationPhase == ESerializationPhase::InterElementData)
	{
		for(int32 ParentIndex = 0; ParentIndex < ParentConstraints.Num(); ParentIndex++)
		{
			FRigElementKey ParentKey;
			if(ParentConstraints[ParentIndex].ParentElement)
			{
				ParentKey = ParentConstraints[ParentIndex].ParentElement->GetKey();
			}

			Ar << ParentKey;
			Ar << ParentConstraints[ParentIndex].InitialWeight;
			Ar << ParentConstraints[ParentIndex].Weight;
		}
	}
}

void FRigMultiParentElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::RemovedMultiParentParentCache)
		{
			FRigCurrentAndInitialTransform Parent;
			Parent.Load(Ar);
		}

		int32 NumParents = 0;
		Ar << NumParents;

		ParentConstraints.SetNum(NumParents);
	}
	else if(SerializationPhase == ESerializationPhase::InterElementData)
	{
		for(int32 ParentIndex = 0; ParentIndex < ParentConstraints.Num(); ParentIndex++)
		{
			FRigElementKey ParentKey;
			Ar << ParentKey;
			ensure(ParentKey.IsValid());

			ParentConstraints[ParentIndex].ParentElement = Hierarchy->FindChecked<FRigTransformElement>(ParentKey);
			ParentConstraints[ParentIndex].Cache.bDirty = true;

			if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::RigHierarchyMultiParentConstraints)
			{
				Ar << ParentConstraints[ParentIndex].InitialWeight;
				Ar << ParentConstraints[ParentIndex].Weight;
			}
			else
			{
				float InitialWeight = 0.f;
				Ar << InitialWeight;
				ParentConstraints[ParentIndex].InitialWeight = FRigElementWeight(InitialWeight);

				float Weight = 0.f;
				Ar << Weight;
				ParentConstraints[ParentIndex].Weight = FRigElementWeight(Weight);
			}

			IndexLookup.Add(ParentKey, ParentIndex);
		}
	}
}

void FRigMultiParentElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther,
                                      URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);
	
	const FRigMultiParentElement* Source = CastChecked<FRigMultiParentElement>(InOther);
	ParentConstraints.Reset();
	ParentConstraints.Reserve(Source->ParentConstraints.Num());
	IndexLookup.Reset();
	IndexLookup.Reserve(Source->IndexLookup.Num());

	for(int32 ParentIndex = 0; ParentIndex < Source->ParentConstraints.Num(); ParentIndex++)
	{
		FRigElementParentConstraint ParentConstraint = Source->ParentConstraints[ParentIndex];
		const FRigTransformElement* SourceParentElement = ParentConstraint.ParentElement;
		ParentConstraint.ParentElement = CastChecked<FRigTransformElement>(InHierarchy->Get(SourceParentElement->Index));
		ParentConstraints.Add(ParentConstraint);
		check(ParentConstraints[ParentIndex].ParentElement->GetKey() == SourceParentElement->GetKey());
		IndexLookup.Add(ParentConstraint.ParentElement->GetKey(), ParentIndex);
	}
}

void FRigMultiParentElement::CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights)
{
	Super::CopyPose(InOther, bCurrent, bInitial, bWeights);

	if(bWeights)
	{
		FRigMultiParentElement* Source = Cast<FRigMultiParentElement>(InOther);
		if(ensure(Source))
		{
			if(ensure(ParentConstraints.Num() == Source->ParentConstraints.Num()))
			{
				for(int32 ParentIndex = 0; ParentIndex < ParentConstraints.Num(); ParentIndex++)
				{
					ParentConstraints[ParentIndex].CopyPose(Source->ParentConstraints[ParentIndex], bCurrent, bInitial);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// FRigBoneElement
////////////////////////////////////////////////////////////////////////////////

void FRigBoneElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		static const UEnum* BoneTypeEnum = StaticEnum<ERigBoneType>();
		FName TypeName = BoneTypeEnum->GetNameByValue((int64)BoneType);
		Ar << TypeName;
	}
}

void FRigBoneElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		static const UEnum* BoneTypeEnum = StaticEnum<ERigBoneType>();
		FName TypeName;
		Ar << TypeName;
		BoneType = (ERigBoneType)BoneTypeEnum->GetValueByName(TypeName);
	}
}

void FRigBoneElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);
	
	const FRigBoneElement* Source = CastChecked<FRigBoneElement>(InOther);
	BoneType = Source->BoneType;
}

////////////////////////////////////////////////////////////////////////////////
// FRigControlSettings
////////////////////////////////////////////////////////////////////////////////

FRigControlSettings::FRigControlSettings()
: AnimationType(ERigControlAnimationType::AnimationControl)
, ControlType(ERigControlType::EulerTransform)
, DisplayName(NAME_None)
, PrimaryAxis(ERigControlAxis::X)
, bIsCurve(false)
, LimitEnabled()
, bDrawLimits(true)
, MinimumValue()
, MaximumValue()
, bShapeVisible(true)
, ShapeVisibility(ERigControlVisibility::UserDefined)
, ShapeName(NAME_None)
, ShapeColor(FLinearColor::Red)
, bIsTransientControl(false)
, ControlEnum(nullptr)
, Customization()
, bGroupWithParentControl(false)
{
	// rely on the default provided by the shape definition
	ShapeName = FControlRigShapeDefinition().ShapeName; 
}

void FRigControlSettings::Save(FArchive& Ar)
{
	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);

	static const UEnum* AnimationTypeEnum = StaticEnum<ERigControlAnimationType>();
	static const UEnum* ControlTypeEnum = StaticEnum<ERigControlType>();
	static const UEnum* ShapeVisibilityEnum = StaticEnum<ERigControlVisibility>();
	static const UEnum* ControlAxisEnum = StaticEnum<ERigControlAxis>();

	FName AnimationTypeName = AnimationTypeEnum->GetNameByValue((int64)AnimationType);
	FName ControlTypeName = ControlTypeEnum->GetNameByValue((int64)ControlType);
	FName ShapeVisibilityName = ShapeVisibilityEnum->GetNameByValue((int64)ShapeVisibility);
	FName PrimaryAxisName = ControlAxisEnum->GetNameByValue((int64)PrimaryAxis);

	FString ControlEnumPathName;
	if(ControlEnum)
	{
		ControlEnumPathName = ControlEnum->GetPathName();
	}

	Ar << AnimationTypeName;
	Ar << ControlTypeName;
	Ar << DisplayName;
	Ar << PrimaryAxisName;
	Ar << bIsCurve;
	Ar << LimitEnabled;
	Ar << bDrawLimits;
	Ar << MinimumValue;
	Ar << MaximumValue;
	Ar << bShapeVisible;
	Ar << ShapeVisibilityName;
	Ar << ShapeName;
	Ar << ShapeColor;
	Ar << bIsTransientControl;
	Ar << ControlEnumPathName;
	Ar << Customization.AvailableSpaces;
	Ar << DrivenControls;
	Ar << bGroupWithParentControl;
}

void FRigControlSettings::Load(FArchive& Ar)
{
	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);

	static const UEnum* AnimationTypeEnum = StaticEnum<ERigControlAnimationType>();
	static const UEnum* ControlTypeEnum = StaticEnum<ERigControlType>();
	static const UEnum* ShapeVisibilityEnum = StaticEnum<ERigControlVisibility>();
	static const UEnum* ControlAxisEnum = StaticEnum<ERigControlAxis>();

	FName AnimationTypeName, ControlTypeName, ShapeVisibilityName, PrimaryAxisName;
	FString ControlEnumPathName;

	bool bLimitTranslation_DEPRECATED = false;
	bool bLimitRotation_DEPRECATED = false;
	bool bLimitScale_DEPRECATED = false;
	bool bAnimatableDeprecated = false;
	bool bShapeEnabledDeprecated = false;

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::ControlAnimationType)
	{
		Ar << AnimationTypeName;
	}
	Ar << ControlTypeName;
	Ar << DisplayName;
	Ar << PrimaryAxisName;
	Ar << bIsCurve;
	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::ControlAnimationType)
	{
		Ar << bAnimatableDeprecated;
	}
	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::PerChannelLimits)
	{
		Ar << bLimitTranslation_DEPRECATED;
		Ar << bLimitRotation_DEPRECATED;
		Ar << bLimitScale_DEPRECATED;
	}
	else
	{
		Ar << LimitEnabled;
	}
	Ar << bDrawLimits;

	FTransform MinimumTransform, MaximumTransform;
	if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::StorageMinMaxValuesAsFloatStorage)
	{
		Ar << MinimumValue;
		Ar << MaximumValue;
	}
	else
	{
		Ar << MinimumTransform;
		Ar << MaximumTransform;
	}

	ControlType = (ERigControlType)ControlTypeEnum->GetValueByName(ControlTypeName);
	
	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::ControlAnimationType)
	{
		Ar << bShapeEnabledDeprecated;
		SetAnimationTypeFromDeprecatedData(bAnimatableDeprecated, bShapeEnabledDeprecated);
		AnimationTypeName = AnimationTypeEnum->GetNameByValue((int64)AnimationType);
	}
	
	Ar << bShapeVisible;
	
	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::ControlAnimationType)
	{
		ShapeVisibilityName = ShapeVisibilityEnum->GetNameByValue((int64)ERigControlVisibility::UserDefined);
	}
	else
	{
		Ar << ShapeVisibilityName;
	}
	Ar << ShapeName;

	if(Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::RenameGizmoToShape)
	{
		if(ShapeName == FRigControl().GizmoName)
		{
			ShapeName = FControlRigShapeDefinition().ShapeName; 
		}
	}
	
	Ar << ShapeColor;
	Ar << bIsTransientControl;
	Ar << ControlEnumPathName;

	AnimationType = (ERigControlAnimationType)AnimationTypeEnum->GetValueByName(AnimationTypeName);
	PrimaryAxis = (ERigControlAxis)ControlAxisEnum->GetValueByName(PrimaryAxisName);
	ShapeVisibility = (ERigControlVisibility)ShapeVisibilityEnum->GetValueByName(ShapeVisibilityName);

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::StorageMinMaxValuesAsFloatStorage)
	{
		MinimumValue.SetFromTransform(MinimumTransform, ControlType, PrimaryAxis);
		MaximumValue.SetFromTransform(MaximumTransform, ControlType, PrimaryAxis);
	}

	ControlEnum = nullptr;
	if(!ControlEnumPathName.IsEmpty())
	{
		if (IsInGameThread())
		{
			ControlEnum = LoadObject<UEnum>(nullptr, *ControlEnumPathName);
		}
		else
		{			
			ControlEnum = FindObject<UEnum>(nullptr, *ControlEnumPathName);
		}
	}

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::RigHierarchyControlSpaceFavorites)
	{
		Ar << Customization.AvailableSpaces;
	}
	else
	{
		Customization.AvailableSpaces.Reset();
	}

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::ControlAnimationType)
	{
		Ar << DrivenControls;
	}
	else
	{
		DrivenControls.Reset();
	}

	PreviouslyDrivenControls.Reset();

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::PerChannelLimits)
	{
		SetupLimitArrayForType(bLimitTranslation_DEPRECATED, bLimitRotation_DEPRECATED, bLimitScale_DEPRECATED);
	}

	if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::ControlAnimationType)
	{
		Ar << bGroupWithParentControl;
	}
	else
	{
		bGroupWithParentControl = IsAnimatable() && (
			ControlType == ERigControlType::Bool ||
			ControlType == ERigControlType::Float ||
			ControlType == ERigControlType::Integer ||
			ControlType == ERigControlType::Vector2D
		);
	}
}

uint32 GetTypeHash(const FRigControlSettings& Settings)
{
	uint32 Hash = GetTypeHash(Settings.ControlType);
	Hash = HashCombine(Hash, GetTypeHash(Settings.AnimationType));
	Hash = HashCombine(Hash, GetTypeHash(Settings.DisplayName));
	Hash = HashCombine(Hash, GetTypeHash(Settings.PrimaryAxis));
	Hash = HashCombine(Hash, GetTypeHash(Settings.bIsCurve));
	Hash = HashCombine(Hash, GetTypeHash(Settings.bDrawLimits));
	Hash = HashCombine(Hash, GetTypeHash(Settings.bShapeVisible));
	Hash = HashCombine(Hash, GetTypeHash(Settings.ShapeVisibility));
	Hash = HashCombine(Hash, GetTypeHash(Settings.ShapeName));
	Hash = HashCombine(Hash, GetTypeHash(Settings.ShapeColor));
	Hash = HashCombine(Hash, GetTypeHash(Settings.ControlEnum));
	Hash = HashCombine(Hash, GetTypeHash(Settings.DrivenControls));
	Hash = HashCombine(Hash, GetTypeHash(Settings.bGroupWithParentControl));
	return Hash;
}

////////////////////////////////////////////////////////////////////////////////
// FRigControlElement
////////////////////////////////////////////////////////////////////////////////

bool FRigControlSettings::operator==(const FRigControlSettings& InOther) const
{
	if(AnimationType != InOther.AnimationType)
	{
		return false;
	}
	if(ControlType != InOther.ControlType)
	{
		return false;
	}
	if(DisplayName != InOther.DisplayName)
	{
		return false;
	}
	if(PrimaryAxis != InOther.PrimaryAxis)
	{
		return false;
	}
	if(bIsCurve != InOther.bIsCurve)
	{
		return false;
	}
	if(LimitEnabled != InOther.LimitEnabled)
	{
		return false;
	}
	if(bDrawLimits != InOther.bDrawLimits)
	{
		return false;
	}
	if(bShapeVisible != InOther.bShapeVisible)
	{
		return false;
	}
	if(ShapeVisibility != InOther.ShapeVisibility)
	{
		return false;
	}
	if(ShapeName != InOther.ShapeName)
	{
		return false;
	}
	if(bIsTransientControl != InOther.bIsTransientControl)
	{
		return false;
	}
	if( ControlEnum != InOther. ControlEnum)
	{
		return false;
	}
	if(!ShapeColor.Equals(InOther.ShapeColor, 0.001))
	{
		return false;
	}
	if(Customization.AvailableSpaces != InOther.Customization.AvailableSpaces)
	{
		return false;
	}
	if(DrivenControls != InOther.DrivenControls)
	{
		return false;
	}
	if(bGroupWithParentControl != InOther.bGroupWithParentControl)
	{
		return false;
	}
	
	const FTransform MinimumTransform = MinimumValue.GetAsTransform(ControlType, PrimaryAxis);
	const FTransform OtherMinimumTransform = InOther.MinimumValue.GetAsTransform(ControlType, PrimaryAxis);
	if(!MinimumTransform.Equals(OtherMinimumTransform, 0.001))
	{
		return false;
	}

	const FTransform MaximumTransform = MaximumValue.GetAsTransform(ControlType, PrimaryAxis);
	const FTransform OtherMaximumTransform = InOther.MaximumValue.GetAsTransform(ControlType, PrimaryAxis);
	if(!MaximumTransform.Equals(OtherMaximumTransform, 0.001))
	{
		return false;
	}

	return true;
}

void FRigControlSettings::SetupLimitArrayForType(bool bLimitTranslation, bool bLimitRotation, bool bLimitScale)
{
	switch(ControlType)
	{
		case ERigControlType::Integer:
		case ERigControlType::Float:
		{
			LimitEnabled.SetNum(1);
			LimitEnabled[0].Set(bLimitTranslation);
			break;
		}
		case ERigControlType::Vector2D:
		{
			LimitEnabled.SetNum(2);
			LimitEnabled[0] = LimitEnabled[1].Set(bLimitTranslation);
			break;
		}
		case ERigControlType::Position:
		{
			LimitEnabled.SetNum(3);
			LimitEnabled[0] = LimitEnabled[1] = LimitEnabled[2].Set(bLimitTranslation);
			break;
		}
		case ERigControlType::Scale:
		{
			LimitEnabled.SetNum(3);
			LimitEnabled[0] = LimitEnabled[1] = LimitEnabled[2].Set(bLimitScale);
			break;
		}
		case ERigControlType::Rotator:
		{
			LimitEnabled.SetNum(3);
			LimitEnabled[0] = LimitEnabled[1] = LimitEnabled[2].Set(bLimitRotation);
			break;
		}
		case ERigControlType::TransformNoScale:
		{
			LimitEnabled.SetNum(6);
			LimitEnabled[0] = LimitEnabled[1] = LimitEnabled[2].Set(bLimitTranslation);
			LimitEnabled[3] = LimitEnabled[4] = LimitEnabled[5].Set(bLimitRotation);
			break;
		}
		case ERigControlType::EulerTransform:
		case ERigControlType::Transform:
		{
			LimitEnabled.SetNum(9);
			LimitEnabled[0] = LimitEnabled[1] = LimitEnabled[2].Set(bLimitTranslation);
			LimitEnabled[3] = LimitEnabled[4] = LimitEnabled[5].Set(bLimitRotation);
			LimitEnabled[6] = LimitEnabled[7] = LimitEnabled[8].Set(bLimitScale);
			break;
		}
		case ERigControlType::Bool:
		default:
		{
			LimitEnabled.Reset();
			break;
		}
	}
}

void FRigControlElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Settings.Save(Ar);
		Offset.Save(Ar);
		Shape.Save(Ar);
		PreferredEulerAngles.Save(Ar);
	}
}

void FRigControlElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Settings.Load(Ar);
		Offset.Load(Ar);
		Shape.Load(Ar);

		if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::PreferredEulerAnglesForControls)
		{
			PreferredEulerAngles.Load(Ar);
		}
		else
		{
			PreferredEulerAngles.Reset();
		}
	}
}

void FRigControlElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);
	
	const FRigControlElement* Source = CastChecked<FRigControlElement>(InOther);
	Settings = Source->Settings;
	Offset = Source->Offset;
	Shape = Source->Shape;
	PreferredEulerAngles = Source->PreferredEulerAngles;
}

void FRigControlElement::CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights)
{
	Super::CopyPose(InOther, bCurrent, bInitial, bWeights);
	
	if(FRigControlElement* Other = Cast<FRigControlElement>(InOther))
	{
		if(bCurrent)
		{
			Offset.Current = Other->Offset.Current;
			Shape.Current = Other->Shape.Current;
			PreferredEulerAngles.SetAngles(Other->PreferredEulerAngles.GetAngles(false), false);
		}
		if(bInitial)
		{
			Offset.Initial = Other->Offset.Initial;
			Shape.Initial = Other->Shape.Initial;
			PreferredEulerAngles.SetAngles(Other->PreferredEulerAngles.GetAngles(true), true);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// FRigCurveElement
////////////////////////////////////////////////////////////////////////////////

void FRigCurveElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Ar << bIsValueSet;
		Ar << Value;
	}
}

void FRigCurveElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		if (Ar.CustomVer(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::CurveElementValueStateFlag)
		{
			Ar << bIsValueSet;
		}
		else
		{
			bIsValueSet = true;
		}
		Ar << Value;
	}
}

void FRigCurveElement::CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights)
{
	Super::CopyPose(InOther, bCurrent, bInitial, bWeights);
	
	if(const FRigCurveElement* Other = Cast<FRigCurveElement>(InOther))
	{
		bIsValueSet = Other->bIsValueSet;
		Value = Other->Value;
	}
}

void FRigCurveElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);
	
	if(const FRigCurveElement* Other = CastChecked<FRigCurveElement>(InOther))
	{
		bIsValueSet = Other->bIsValueSet;
		Value = Other->Value;
	}
}

////////////////////////////////////////////////////////////////////////////////
// FRigRigidBodySettings
////////////////////////////////////////////////////////////////////////////////

FRigRigidBodySettings::FRigRigidBodySettings()
	: Mass(1.f)
{
}

void FRigRigidBodySettings::Save(FArchive& Ar)
{
	Ar << Mass;
}

void FRigRigidBodySettings::Load(FArchive& Ar)
{
	Ar << Mass;
}

////////////////////////////////////////////////////////////////////////////////
// FRigRigidBodyElement
////////////////////////////////////////////////////////////////////////////////

void FRigRigidBodyElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Settings.Save(Ar);
	}
}

void FRigRigidBodyElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);

	if(SerializationPhase == ESerializationPhase::StaticData)
	{
		Settings.Load(Ar);
	}
}

void FRigRigidBodyElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther,
	URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);
	
	const FRigRigidBodyElement* Source = CastChecked<FRigRigidBodyElement>(InOther);
	Settings = Source->Settings;
}

////////////////////////////////////////////////////////////////////////////////
// FRigReferenceElement
////////////////////////////////////////////////////////////////////////////////

void FRigReferenceElement::Save(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Save(Ar, Hierarchy, SerializationPhase);
}

void FRigReferenceElement::Load(FArchive& Ar, URigHierarchy* Hierarchy, ESerializationPhase SerializationPhase)
{
	Super::Load(Ar, Hierarchy, SerializationPhase);
}

void FRigReferenceElement::CopyFrom(URigHierarchy* InHierarchy, FRigBaseElement* InOther, URigHierarchy* InOtherHierarchy)
{
	Super::CopyFrom(InHierarchy, InOther, InOtherHierarchy);
	
	const FRigReferenceElement* Source = CastChecked<FRigReferenceElement>(InOther);
	GetWorldTransformDelegate = Source->GetWorldTransformDelegate;
}

FTransform FRigReferenceElement::GetReferenceWorldTransform(const FRigUnitContext* InContext, bool bInitial) const
{
	if(GetWorldTransformDelegate.IsBound())
	{
		return GetWorldTransformDelegate.Execute(InContext, GetKey(), bInitial);
	}
	return FTransform::Identity;
}

void FRigReferenceElement::CopyPose(FRigBaseElement* InOther, bool bCurrent, bool bInitial, bool bWeights)
{
	Super::CopyPose(InOther, bCurrent, bInitial, bWeights);
	
	if(FRigReferenceElement* Other = Cast<FRigReferenceElement>(InOther))
	{
		if(Other->GetWorldTransformDelegate.IsBound())
		{
			GetWorldTransformDelegate = Other->GetWorldTransformDelegate;
		}
	}
}
