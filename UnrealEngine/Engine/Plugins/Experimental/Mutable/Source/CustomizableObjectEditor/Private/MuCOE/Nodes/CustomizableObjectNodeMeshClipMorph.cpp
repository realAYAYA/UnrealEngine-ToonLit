// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"

#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Serialization/Archive.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeMeshClipMorph::UCustomizableObjectNodeMeshClipMorph()
	: Super()
{
	StartOffset = FVector::ZeroVector;
	bLocalStartOffset = true;
	B = 0.f;
	Radius = 8.f;
	Radius2 = 4.f;
	RotationAngle = 0.f;
	Exponent = 1.f;

	Origin = FVector::ZeroVector;
	Normal = -FVector::UpVector;
	MaxEffectRadius = -1.f;

	bUpdateViewportWidget = true;
}


FVector UCustomizableObjectNodeMeshClipMorph::GetOriginWithOffset() const
{
	FVector NewOrigin;

	if (bLocalStartOffset)
	{
		FVector XAxis, YAxis, ZAxis;
		FindLocalAxes(XAxis, YAxis, ZAxis);

		NewOrigin = Origin + StartOffset.X * XAxis + StartOffset.Y * YAxis + StartOffset.Z * ZAxis;
	}
	else
	{
		NewOrigin = Origin + StartOffset;
	}

	return NewOrigin;
}

void UCustomizableObjectNodeMeshClipMorph::FindLocalAxes(FVector& XAxis, FVector& YAxis, FVector& ZAxis) const
{
	YAxis = FVector(0.f, 1.f, 0.f);

	if (FMath::Abs(FVector::DotProduct(Normal, YAxis)) > 0.95f)
	{
		YAxis = FVector(0.f, 0.f, 1.f);
	}

	XAxis = FVector::CrossProduct(Normal, YAxis);
	XAxis = XAxis.RotateAngleAxis(RotationAngle, Normal);
	YAxis = FVector::CrossProduct(Normal, XAxis);
	ZAxis = Normal;

	XAxis.Normalize();
	YAxis.Normalize();
}


void UCustomizableObjectNodeMeshClipMorph::ChangeStartOffsetTransform()
{
	// Local Offset
	FVector XAxis, YAxis, ZAxis;
	FindLocalAxes(XAxis, YAxis, ZAxis);

	if (bLocalStartOffset)
	{
		StartOffset = FVector(FVector::DotProduct(StartOffset, XAxis), FVector::DotProduct(StartOffset, YAxis),
							   FVector::DotProduct(StartOffset, ZAxis));
	}
	else
	{
		StartOffset = StartOffset.X * XAxis + StartOffset.Y * YAxis + StartOffset.Z * ZAxis;
	}
}


void UCustomizableObjectNodeMeshClipMorph::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	const FName PropertyName = Helper_GetPropertyName(PropertyChangedEvent);

	if (PropertyName == "bLocalStartOffset")
	{
		ChangeStartOffsetTransform();
	}

	bUpdateViewportWidget = true;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UCustomizableObjectNodeMeshClipMorph::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PostLoadToCustomVersion
		&& bLocalStartOffset)
	{
		// Previous Offset
		FVector Tangent, Binormal;
		Origin.FindBestAxisVectors(Tangent, Binormal);
		FVector OldOffset = StartOffset.X * Tangent + StartOffset.Y * Binormal + StartOffset.Z * Normal;

		// Local Offset
		FVector XAxis, YAxis, ZAxis;
		FindLocalAxes(XAxis, YAxis, ZAxis);

		StartOffset = FVector(FVector::DotProduct(OldOffset, XAxis), FVector::DotProduct(OldOffset, YAxis),
							  FVector::DotProduct(OldOffset, ZAxis));
	}
}


void UCustomizableObjectNodeMeshClipMorph::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName("Material"));
}


FText UCustomizableObjectNodeMeshClipMorph::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Clip_Morph_Mesh", "Clip Morph Mesh");
}


FLinearColor UCustomizableObjectNodeMeshClipMorph::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


void UCustomizableObjectNodeMeshClipMorph::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin == OutputPin())
	{
		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
		}
	}
}



FText UCustomizableObjectNodeMeshClipMorph::GetTooltipText() const
{
	return LOCTEXT("Clip_Mesh_Morph_Tooltip", "Defines a cutting plane on a bone to cut tagged Materials that go past it, while morphing the mesh after the cut to blend in more naturally.\nIt only cuts and morphs mesh that receives some influence of that bone or other descendant bones.");
}


#undef LOCTEXT_NAMESPACE
