// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigGizmoLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigGizmoLibrary)

#define LOCTEXT_NAMESPACE "ControlRigGizmoLibrary"

UControlRigShapeLibrary::UControlRigShapeLibrary()
{
#if WITH_EDITOR
	XRayMaterial = LoadObject<UMaterial>(nullptr, TEXT("/ControlRig/Controls/ControlRigXRayMaterial.ControlRigXRayMaterial"));
#endif
}

#if WITH_EDITOR

// UObject interface
void UControlRigShapeLibrary::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property->GetName() == TEXT("ShapeName"))
	{
		FProperty* MemberProperty = PropertyChangedEvent.PropertyChain.GetHead()->GetValue();
		if (MemberProperty->GetName() == TEXT("DefaultShape"))
		{
			DefaultShape.ShapeName = FControlRigShapeDefinition().ShapeName;
			GetUpdatedNameList(true);
		}
		else if(MemberProperty->GetName() == TEXT("Shapes"))
		{
			if (Shapes.Num() == 0)
			{
				return;
			}

			int32 ShapeIndexEdited = PropertyChangedEvent.GetArrayIndex(TEXT("Shapes"));
			if (Shapes.IsValidIndex(ShapeIndexEdited))
			{
				TArray<FName> Names;
				Names.Add(DefaultShape.ShapeName);
				for (int32 ShapeIndex = 0; ShapeIndex < Shapes.Num(); ShapeIndex++)
				{
					if (ShapeIndex != ShapeIndexEdited)
					{
						Names.Add(Shapes[ShapeIndex].ShapeName);
					}
				}

				FName DesiredName = Shapes[ShapeIndexEdited].ShapeName;
				FString Name = DesiredName.ToString();
				int32 Suffix = 0;
				while (Names.Contains(*Name))
				{
					Suffix++;
					Name = FString::Printf(TEXT("%s_%d"), *DesiredName.ToString(), Suffix);
				}
				Shapes[ShapeIndexEdited].ShapeName = *Name;
			}
			GetUpdatedNameList(true);
		}
	}
	else if (PropertyChangedEvent.Property->GetName() == TEXT("Shapes"))
	{
		TArray<FName> Names;
		Names.Add(DefaultShape.ShapeName);
		for (int32 ShapeIndex = 0; ShapeIndex < Shapes.Num(); ShapeIndex++)
		{
			FName DesiredName = Shapes[ShapeIndex].ShapeName;
			FString Name = DesiredName.ToString();
			int32 Suffix = 0;
			while (Names.Contains(*Name))
			{
				Suffix++;
				Name = FString::Printf(TEXT("%s_%d"), *DesiredName.ToString(), Suffix);
			}
			Shapes[ShapeIndex].ShapeName = *Name;

			Names.Add(Shapes[ShapeIndex].ShapeName);
		}
		GetUpdatedNameList(true);
	}
}

#endif

const FControlRigShapeDefinition* UControlRigShapeLibrary::GetShapeByName(const FName& InName, bool bUseDefaultIfNotFound) const
{
	UControlRigShapeLibrary* MutableThis = (UControlRigShapeLibrary*)this;
	if (InName == MutableThis->DefaultShape.ShapeName)
	{
		MutableThis->DefaultShape.Library = MutableThis;
		return &MutableThis->DefaultShape;
	}

	for (int32 ShapeIndex = 0; ShapeIndex < MutableThis->Shapes.Num(); ShapeIndex++)
	{
		if (MutableThis->Shapes[ShapeIndex].ShapeName == InName)
		{
			MutableThis->Shapes[ShapeIndex].Library = MutableThis;
			return &MutableThis->Shapes[ShapeIndex];
		}
	}

	if (bUseDefaultIfNotFound)
	{
		MutableThis->DefaultShape.Library = MutableThis;
		return &DefaultShape;
	}

	return nullptr;
}

const FControlRigShapeDefinition* UControlRigShapeLibrary::GetShapeByName(const FName& InName, const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& InShapeLibraries)
{
	const FString InString = InName.ToString();
	FString Left, Right;
	FName RightName;
	if(!InString.Split(TEXT("."), &Left, &Right))
	{
		Left = FString();
		Right = InString;
		RightName = InName;
	}
	else
	{
		RightName = *Right;
	}

	for(const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary : InShapeLibraries)
	{
		if(!ShapeLibrary.IsValid())
		{
			continue;
		}

		if(ShapeLibrary->GetName().Equals(Left) || Left.IsEmpty())
		{
			if(const FControlRigShapeDefinition* Shape = ShapeLibrary->GetShapeByName(RightName))
			{
				return Shape;
			}
		}
	}

	return nullptr;
}

const TArray<FName> UControlRigShapeLibrary::GetUpdatedNameList(bool bReset)
{
	if (bReset)
	{
		NameList.Reset();
	}

	if (NameList.Num() != Shapes.Num())
	{
		NameList.Reset();
		for (const FControlRigShapeDefinition& Shape : Shapes)
		{
			NameList.Add(Shape.ShapeName);
		}
		NameList.Sort(FNameLexicalLess());
	}

	return NameList;
}

#undef LOCTEXT_NAMESPACE

