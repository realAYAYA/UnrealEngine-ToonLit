// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchyDefines.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigHierarchyDefines)

#if WITH_EDITOR
#include "RigVMPythonUtils.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// FRigControlLimitEnabled
////////////////////////////////////////////////////////////////////////////////

void FRigControlLimitEnabled::Serialize(FArchive& Ar)
{
	Ar << bMinimum;
	Ar << bMaximum;
}

bool FRigControlLimitEnabled::GetForValueType(ERigControlValueType InValueType) const
{
	if(InValueType == ERigControlValueType::Minimum)
	{
		return bMinimum;
	}
	return bMaximum;
}

void FRigControlLimitEnabled::SetForValueType(ERigControlValueType InValueType, bool InValue)
{
	if(InValueType == ERigControlValueType::Minimum)
	{
		bMinimum = InValue;
	}
	else
	{
		bMaximum = InValue;
	}
}

////////////////////////////////////////////////////////////////////////////////
// FRigElementKey
////////////////////////////////////////////////////////////////////////////////

void FRigElementKey::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving() || Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Save(Ar);
	}
	else if (Ar.IsLoading())
	{
		Load(Ar);
	}
	else
	{
		// remove due to FPIEFixupSerializer hitting this checkNoEntry();
	}
}

void FRigElementKey::Save(FArchive& Ar)
{
	static const UEnum* ElementTypeEnum = StaticEnum<ERigElementType>();

	FName TypeName = ElementTypeEnum->GetNameByValue((int64)Type);
	Ar << TypeName;
	Ar << Name;
}

void FRigElementKey::Load(FArchive& Ar)
{
	static const UEnum* ElementTypeEnum = StaticEnum<ERigElementType>();

	FName TypeName;
	Ar << TypeName;

	const int64 TypeValue = ElementTypeEnum->GetValueByName(TypeName);
	Type = (ERigElementType)TypeValue;

	Ar << Name;
}

FString FRigElementKey::ToPythonString() const
{
#if WITH_EDITOR
	return FString::Printf(TEXT("unreal.RigElementKey(type=%s, name='%s')"),
		*RigVMPythonUtils::EnumValueToPythonString<ERigElementType>((int64)Type),
		*Name.ToString());
#else
	return FString();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// FRigElementKeyCollection
////////////////////////////////////////////////////////////////////////////////

FRigElementKeyCollection FRigElementKeyCollection::MakeFromChildren(
	URigHierarchy* InHierarchy,
	const FRigElementKey& InParentKey,
	bool bRecursive,
	bool bIncludeParent,
	uint8 InElementTypes)
{
	check(InHierarchy);

	FRigElementKeyCollection Collection;

	int32 Index = InHierarchy->GetIndex(InParentKey);
	if (Index == INDEX_NONE)
	{
		return Collection;
	}

	if (bIncludeParent)
	{
		Collection.AddUnique(InParentKey);
	}

	TArray<FRigElementKey> ParentKeys;
	ParentKeys.Add(InParentKey);

	bool bAddBones = (InElementTypes & (uint8)ERigElementType::Bone) == (uint8)ERigElementType::Bone;
	bool bAddControls = (InElementTypes & (uint8)ERigElementType::Control) == (uint8)ERigElementType::Control;
	bool bAddNulls = (InElementTypes & (uint8)ERigElementType::Null) == (uint8)ERigElementType::Null;
	bool bAddCurves = (InElementTypes & (uint8)ERigElementType::Curve) == (uint8)ERigElementType::Curve;

	for (int32 ParentIndex = 0; ParentIndex < ParentKeys.Num(); ParentIndex++)
	{
		const FRigElementKey ParentKey = ParentKeys[ParentIndex];
		TArray<FRigElementKey> Children = InHierarchy->GetChildren(ParentKey);
		for(const FRigElementKey& Child : Children)
		{
			if((InElementTypes & (uint8)Child.Type) == (uint8)Child.Type)
			{
				const int32 PreviousSize = Collection.Num();
				if(PreviousSize == Collection.AddUnique(Child))
				{
					if(bRecursive)
					{
						ParentKeys.Add(Child);
					}
				}
			}
		}
	}

	return Collection;
}

FRigElementKeyCollection FRigElementKeyCollection::MakeFromName(
	URigHierarchy* InHierarchy,
	const FName& InPartialName,
	uint8 InElementTypes
)
{
	if (InPartialName.IsNone())
	{
		return MakeFromCompleteHierarchy(InHierarchy, InElementTypes);
	}

	check(InHierarchy);

	constexpr bool bTraverse = true;

	const FString PartialNameString = InPartialName.ToString();
	
	return InHierarchy->GetKeysByPredicate([PartialNameString, InElementTypes](const FRigBaseElement& InElement) -> bool
	{
		return InElement.IsTypeOf(static_cast<ERigElementType>(InElementTypes)) &&
			   InElement.GetNameString().Contains(PartialNameString);
	}, bTraverse);
}

FRigElementKeyCollection FRigElementKeyCollection::MakeFromChain(
	URigHierarchy* InHierarchy,
	const FRigElementKey& InFirstItem,
	const FRigElementKey& InLastItem,
	bool bReverse
)
{
	check(InHierarchy);

	FRigElementKeyCollection Collection;

	int32 FirstIndex = InHierarchy->GetIndex(InFirstItem);
	int32 LastIndex = InHierarchy->GetIndex(InLastItem);

	if (FirstIndex == INDEX_NONE || LastIndex == INDEX_NONE)
	{
		return Collection;
	}

	FRigElementKey LastKey = InLastItem;
	while (LastKey.IsValid() && LastKey != InFirstItem)
	{
		Collection.Keys.Add(LastKey);
		LastKey = InHierarchy->GetFirstParent(LastKey);
	}

	if (LastKey != InFirstItem)
	{
		Collection.Reset();
	}
	else
	{
		Collection.AddUnique(InFirstItem);
	}

	if (!bReverse)
	{
		Algo::Reverse(Collection.Keys);
	}

	return Collection;
}

FRigElementKeyCollection FRigElementKeyCollection::MakeFromCompleteHierarchy(
	URigHierarchy* InHierarchy,
	uint8 InElementTypes
)
{
	check(InHierarchy);

	FRigElementKeyCollection Collection(InHierarchy->GetAllKeys(true));
	return Collection.FilterByType(InElementTypes);
}

FRigElementKeyCollection FRigElementKeyCollection::MakeUnion(const FRigElementKeyCollection& A, const FRigElementKeyCollection& B, bool bAllowDuplicates)
{
	FRigElementKeyCollection Collection;
	for (const FRigElementKey& Key : A)
	{
		Collection.Add(Key);
	}
	for (const FRigElementKey& Key : B)
	{
		if(bAllowDuplicates)
		{
			Collection.Add(Key);
		}
		else
		{
			Collection.AddUnique(Key);
		}
	}
	return Collection;
}

FRigElementKeyCollection FRigElementKeyCollection::MakeIntersection(const FRigElementKeyCollection& A, const FRigElementKeyCollection& B)
{
	FRigElementKeyCollection Collection;
	for (const FRigElementKey& Key : A)
	{
		if (B.Contains(Key))
		{
			Collection.Add(Key);
		}
	}
	return Collection;
}

FRigElementKeyCollection FRigElementKeyCollection::MakeDifference(const FRigElementKeyCollection& A, const FRigElementKeyCollection& B)
{
	FRigElementKeyCollection Collection;
	for (const FRigElementKey& Key : A)
	{
		if (!B.Contains(Key))
		{
			Collection.Add(Key);
		}
	}
	return Collection;
}

FRigElementKeyCollection FRigElementKeyCollection::MakeReversed(const FRigElementKeyCollection& InCollection)
{
	FRigElementKeyCollection Reversed = InCollection;
	Algo::Reverse(Reversed.Keys);
	return Reversed;
}

FRigElementKeyCollection FRigElementKeyCollection::FilterByType(uint8 InElementTypes) const
{
	FRigElementKeyCollection Collection;
	for (const FRigElementKey& Key : *this)
	{
		if ((InElementTypes & (uint8)Key.Type) == (uint8)Key.Type)
		{
			Collection.Add(Key);
		}
	}
	return Collection;
}

FRigElementKeyCollection FRigElementKeyCollection::FilterByName(const FName& InPartialName) const
{
	FString SearchToken = InPartialName.ToString();

	FRigElementKeyCollection Collection;
	for (const FRigElementKey& Key : *this)
	{
		if (Key.Name == InPartialName)
		{
			Collection.Add(Key);
		}
		else if (Key.Name.ToString().Contains(SearchToken, ESearchCase::CaseSensitive, ESearchDir::FromStart))
		{
			Collection.Add(Key);
		}
	}
	return Collection;
}

////////////////////////////////////////////////////////////////////////////////
// FRigMirrorSettings
////////////////////////////////////////////////////////////////////////////////

FTransform FRigMirrorSettings::MirrorTransform(const FTransform& InTransform) const
{
	FTransform Transform = InTransform;
	FQuat Quat = Transform.GetRotation();

	Transform.SetLocation(MirrorVector(Transform.GetLocation()));

	switch (AxisToFlip)
	{
	case EAxis::X:
		{
			FVector Y = MirrorVector(Quat.GetAxisY());
			FVector Z = MirrorVector(Quat.GetAxisZ());
			FMatrix Rotation = FRotationMatrix::MakeFromYZ(Y, Z);
			Transform.SetRotation(FQuat(Rotation));
			break;
		}
	case EAxis::Y:
		{
			FVector X = MirrorVector(Quat.GetAxisX());
			FVector Z = MirrorVector(Quat.GetAxisZ());
			FMatrix Rotation = FRotationMatrix::MakeFromXZ(X, Z);
			Transform.SetRotation(FQuat(Rotation));
			break;
		}
	default:
		{
			FVector X = MirrorVector(Quat.GetAxisX());
			FVector Y = MirrorVector(Quat.GetAxisY());
			FMatrix Rotation = FRotationMatrix::MakeFromXY(X, Y);
			Transform.SetRotation(FQuat(Rotation));
			break;
		}
	}

	return Transform;
}

FVector FRigMirrorSettings::MirrorVector(const FVector& InVector) const
{
	FVector Axis = FVector::ZeroVector;
	Axis.SetComponentForAxis(MirrorAxis, 1.f);
	return InVector.MirrorByVector(Axis);
}

FArchive& operator<<(FArchive& Ar, FRigControlValue& Value)
{
	Ar <<  Value.FloatStorage.Float00;
	Ar <<  Value.FloatStorage.Float01;
	Ar <<  Value.FloatStorage.Float02;
	Ar <<  Value.FloatStorage.Float03;
	Ar <<  Value.FloatStorage.Float10;
	Ar <<  Value.FloatStorage.Float11;
	Ar <<  Value.FloatStorage.Float12;
	Ar <<  Value.FloatStorage.Float13;
	Ar <<  Value.FloatStorage.Float20;
	Ar <<  Value.FloatStorage.Float21;
	Ar <<  Value.FloatStorage.Float22;
	Ar <<  Value.FloatStorage.Float23;
	Ar <<  Value.FloatStorage.Float30;
	Ar <<  Value.FloatStorage.Float31;
	Ar <<  Value.FloatStorage.Float32;
	Ar <<  Value.FloatStorage.Float33;
	Ar <<  Value.FloatStorage.Float00_2;
	Ar <<  Value.FloatStorage.Float01_2;
	Ar <<  Value.FloatStorage.Float02_2;
	Ar <<  Value.FloatStorage.Float03_2;
	Ar <<  Value.FloatStorage.Float10_2;
	Ar <<  Value.FloatStorage.Float11_2;
	Ar <<  Value.FloatStorage.Float12_2;
	Ar <<  Value.FloatStorage.Float13_2;
	Ar <<  Value.FloatStorage.Float20_2;
	Ar <<  Value.FloatStorage.Float21_2;
	Ar <<  Value.FloatStorage.Float22_2;
	Ar <<  Value.FloatStorage.Float23_2;
	Ar <<  Value.FloatStorage.Float30_2;
	Ar <<  Value.FloatStorage.Float31_2;
	Ar <<  Value.FloatStorage.Float32_2;
	Ar <<  Value.FloatStorage.Float33_2;

	return Ar;
}

