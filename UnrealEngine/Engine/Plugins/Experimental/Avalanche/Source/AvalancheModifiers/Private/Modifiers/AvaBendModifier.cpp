// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaBendModifier.h"

#include "Async/Async.h"
#include "Components/DynamicMeshComponent.h"
#include "SpaceDeformerOps/BendMeshOp.h"

#define LOCTEXT_NAMESPACE "AvaBendModifier"

void UAvaBendModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("Bend"));
	InMetadata.SetCategory(TEXT("Geometry"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Bend the current geometry shape with a transition between two sides"));
#endif
	InMetadata.AddDependency(TEXT("Subdivide"));
}

void UAvaBendModifier::Apply()
{
	UDynamicMeshComponent* const DynMeshComp = GetMeshComponent();
	if (!IsValid(DynMeshComp))
	{
		Fail(LOCTEXT("InvalidDynamicMeshComponent", "Invalid dynamic mesh component on modified actor"));
		return;
	}
	
	// no need to bend here!
	if (Angle == KINDA_SMALL_NUMBER)
	{
		Next();
		return;
	}

	const FTransform BendFrame(BendRotation, BendPosition);
	if (!BendFrame.IsValid())
	{
		Next();
		return;
	}
	
	using namespace UE::Geometry;

	// update extent
	DynMeshComp->ProcessMesh([this](const FDynamicMesh3& ProcessMesh)
	{
		const FBox MeshBounds = static_cast<FBox>(ProcessMesh.GetBounds(true));
		// this is the extent value used for maximum extent, meaning the bend will be done over the entire geometry along its X axis
		ModifiedMeshMaxExtent = MeshBounds.GetSize().Z / 2.0;
	});
			
	constexpr float LowerExtent = 10.0f;
	const float BendExtent = ModifiedMeshMaxExtent * Extent;
			
	const FFrame3d BendFrameAsFrame = FFrame3d(BendFrame);

	DynMeshComp->GetDynamicMesh()->EditMesh([this, BendFrameAsFrame, BendExtent, LowerExtent](FDynamicMesh3& EditMesh) 
	{
		const TSharedPtr<FDynamicMesh3> TmpMeshPtr = MakeShared<FDynamicMesh3>();
		*TmpMeshPtr = MoveTemp(EditMesh);
			    
		FBendMeshOp BendOperation;
		BendOperation.OriginalMesh = TmpMeshPtr;
		BendOperation.GizmoFrame = BendFrameAsFrame;
		BendOperation.LowerBoundsInterval = (bSymmetricExtents) ? -BendExtent : -LowerExtent;
		BendOperation.UpperBoundsInterval = BendExtent;
		BendOperation.BendDegrees = Angle;
		BendOperation.bLockBottom = !bBidirectional;
		BendOperation.CalculateResult(nullptr);
		TUniquePtr<FDynamicMesh3> NewResultMesh = BendOperation.ExtractResult();				
		FDynamicMesh3* NewResultMeshPtr = NewResultMesh.Release();
		EditMesh = MoveTemp(*NewResultMeshPtr);

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	Next();
}

#if WITH_EDITOR
void UAvaBendModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	
	static const FName BendPositionName = GET_MEMBER_NAME_CHECKED(UAvaBendModifier, BendPosition);
	static const FName BendRotationName = GET_MEMBER_NAME_CHECKED(UAvaBendModifier, BendRotation);
	static const FName AngleName = GET_MEMBER_NAME_CHECKED(UAvaBendModifier, Angle);	
	static const FName ExtentName = GET_MEMBER_NAME_CHECKED(UAvaBendModifier, Extent);
	static const FName SymmetricExtentsName = GET_MEMBER_NAME_CHECKED(UAvaBendModifier, bSymmetricExtents);
	static const FName BidirectionalName = GET_MEMBER_NAME_CHECKED(UAvaBendModifier, bBidirectional);

	if (MemberName == BendPositionName ||
		MemberName == BendRotationName)
	{
		OnBendTransformChanged();
	}
	else if (MemberName == AngleName ||
		MemberName == ExtentName ||
		MemberName == SymmetricExtentsName ||
		MemberName == BidirectionalName)
	{
		OnBendOptionChanged();
	}
}
#endif

void UAvaBendModifier::SetAngle(float InAngle)
{
	if (Angle == InAngle)
	{
		return;
	}

	Angle = InAngle;
	OnBendOptionChanged();
}

void UAvaBendModifier::SetExtent(float InExtent)
{
	if (Extent == InExtent)
	{
		return;
	}

	Extent = InExtent;
	OnBendOptionChanged();
}

void UAvaBendModifier::SetBendPosition(const FVector& InBendPosition)
{
	if (BendPosition == InBendPosition)
	{
		return;
	}

	BendPosition = InBendPosition;
	OnBendTransformChanged();
}

void UAvaBendModifier::SetBendRotation(const FRotator& InBendRotation)
{
	if (BendRotation == InBendRotation)
	{
		return;
	}

	BendRotation = InBendRotation;
	OnBendTransformChanged();
}

void UAvaBendModifier::SetSymmetricExtents(bool bInSymmetricExtents)
{
	if (bSymmetricExtents == bInSymmetricExtents)
	{
		return;
	}

	bSymmetricExtents = bInSymmetricExtents;
	OnBendOptionChanged();
}

void UAvaBendModifier::SetBidirectional(bool bInBidirectional)
{
	if (bBidirectional == bInBidirectional)
	{
		return;
	}

	bBidirectional = bInBidirectional;
	OnBendOptionChanged();
}

void UAvaBendModifier::OnBendTransformChanged()
{
	MarkModifierDirty();
}

void UAvaBendModifier::OnBendOptionChanged()
{
	MarkModifierDirty();
}

#undef LOCTEXT_NAMESPACE
