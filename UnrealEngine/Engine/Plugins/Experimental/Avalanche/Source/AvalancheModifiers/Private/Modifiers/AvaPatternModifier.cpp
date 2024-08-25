// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaPatternModifier.h"

#include "Async/Async.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"

#define LOCTEXT_NAMESPACE "AvaPatternModifier"

void UAvaPatternModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("Pattern"));
	InMetadata.SetCategory(TEXT("Geometry"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Repeats a geometry multiple times following a specific layout pattern"));
#endif
}

void UAvaPatternModifier::Apply()
{
	if ((Layout == EAvaPatternModifierLayout::Line && LineLayoutOptions.RepeatCount <= 0)
		|| (Layout == EAvaPatternModifierLayout::Grid && (GridLayoutOptions.RepeatCount.X <= 0 ||  GridLayoutOptions.RepeatCount.Y <= 0))
		|| (Layout == EAvaPatternModifierLayout::Circle && CircleLayoutOptions.RepeatCount <= 0))
	{
		Fail(LOCTEXT("InvalidRepeatCount", "Layout repeat count must be greater than 0"));
		return;
	}

	using namespace UE::Geometry;

	UDynamicMeshComponent* TargetMeshComponent = GetMeshComponent();

	if (!IsValid(TargetMeshComponent))
	{
		Fail(LOCTEXT("InvalidDynamicMeshComponent", "Invalid dynamic mesh component on modified actor"));
		return;
	}

	TargetMeshComponent->GetDynamicMesh()->EditMesh([this](FDynamicMesh3& AppendToMesh)
	{
		// copy original mesh once into tmp mesh
		FDynamicMesh3 TmpMesh = AppendToMesh;

		// Get the original bounds before clearing the mesh
		OriginalMeshBounds = GetMeshBounds();

		// clear all since we have a copy
		for (int32 TId : AppendToMesh.TriangleIndicesItr())
		{
			AppendToMesh.RemoveTriangle(TId);
		}

		FVector CenterAxis = FVector::ZeroVector;

		if (Layout == EAvaPatternModifierLayout::Line)
		{
			// edit original mesh to append tmp mesh
			FMeshIndexMappings TmpMappings;
			FDynamicMeshEditor Editor(&AppendToMesh);
			const FTransformSRT3d TransformChange = GetLineLayoutTransformChange();
			if (!LineLayoutOptions.bAccumulateTransform)
			{
				FTransform RotationAndScaleTransform;
				RotationAndScaleTransform.SetLocation(FVector::ZeroVector);
				RotationAndScaleTransform.SetRotation(TransformChange.GetRotator().Quaternion());
				RotationAndScaleTransform.SetScale3D(TransformChange.GetScale());
				FTransformSRT3d RotateScale(RotationAndScaleTransform);
				MeshTransforms::ApplyTransform(TmpMesh, RotateScale, true);
			}
			for (int32 Idx = 0; Idx < LineLayoutOptions.RepeatCount; Idx++)
			{
				if (Idx != 0)
				{
					if (LineLayoutOptions.bAccumulateTransform)
					{
						MeshTransforms::ApplyTransform(TmpMesh, TransformChange, true);
					}
					else
					{
						MeshTransforms::Translate(TmpMesh, TransformChange.GetTranslation());
					}
				}
				Editor.AppendMesh(&TmpMesh, TmpMappings);
				TmpMappings.Reset();
			}
			if (LineLayoutOptions.bCentered)
			{
				if (LineLayoutOptions.Axis == EAvaPatternModifierAxis::X)
				{
					CenterAxis.X = 1;
				}
				else if (LineLayoutOptions.Axis == EAvaPatternModifierAxis::Y)
				{
					CenterAxis.Y = 1;
				}
				else if (LineLayoutOptions.Axis == EAvaPatternModifierAxis::Z)
				{
					CenterAxis.Z = 1;
				}
			}
		}
		else if (Layout == EAvaPatternModifierLayout::Grid)
		{
			// edit original mesh to append tmp mesh
			FMeshIndexMappings TmpMappings;
			FDynamicMeshEditor Editor(&AppendToMesh);
			const FTransformSRT3d ColTransformChange = GetGridLayoutColTransformChange();
			const FTransformSRT3d RowTransformChange = GetGridLayoutRowTransformChange();
			if (!GridLayoutOptions.bAccumulateTransform)
			{
				FTransform RotationAndScaleTransform;
				RotationAndScaleTransform.SetLocation(FVector::ZeroVector);
				RotationAndScaleTransform.SetRotation(ColTransformChange.GetRotator().Quaternion());
				RotationAndScaleTransform.SetScale3D(ColTransformChange.GetScale());
				FTransformSRT3d RotateScale(RotationAndScaleTransform);
				MeshTransforms::ApplyTransform(TmpMesh, RotateScale, true);
			}
			for (int32 Row = 0; Row < GridLayoutOptions.RepeatCount.X; Row++)
			{
				for (int32 Col = 0; Col < GridLayoutOptions.RepeatCount.Y; Col++)
				{
					if (!(Row == 0 && Col == 0))
					{
						if (GridLayoutOptions.bAccumulateTransform)
						{
							MeshTransforms::ApplyTransform(TmpMesh, ColTransformChange, true);
						}
						else
						{
							MeshTransforms::Translate(TmpMesh, ColTransformChange.GetTranslation());
						}
					}
					Editor.AppendMesh(&TmpMesh, TmpMappings);
					TmpMappings.Reset();
				}
				MeshTransforms::Translate(TmpMesh, RowTransformChange.GetTranslation() - ColTransformChange.GetTranslation() * GridLayoutOptions.RepeatCount.Y);
			}
			if (GridLayoutOptions.bCentered)
			{
				if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::XY)
				{
					CenterAxis.X = 1;
					CenterAxis.Y = 1;
				}
				else if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::YZ)
				{
					CenterAxis.Y = 1;
					CenterAxis.Z = 1;
				}
				else if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::ZX)
				{
					CenterAxis.X = 1;
					CenterAxis.Z = 1;
				}
			}
		}
		else if (Layout == EAvaPatternModifierLayout::Circle)
		{
			// edit original mesh to append tmp mesh
			FMeshIndexMappings TmpMappings;
			FDynamicMeshEditor Editor(&AppendToMesh);
			if (!CircleLayoutOptions.bAccumulateTransform)
			{
				FTransform RotationAndScaleTransform;
				RotationAndScaleTransform.SetLocation(FVector::ZeroVector);
				RotationAndScaleTransform.SetRotation(CircleLayoutOptions.Rotation.Quaternion());
				RotationAndScaleTransform.SetScale3D(CircleLayoutOptions.Scale);
				FTransformSRT3d RotateScale(RotationAndScaleTransform);
				MeshTransforms::ApplyTransform(TmpMesh, RotateScale, true);
			}
			for (int32 Idx = 0; Idx < CircleLayoutOptions.RepeatCount; Idx++)
			{
				FDynamicMesh3 AppendMesh = TmpMesh;
				FTransformSRT3d TransformChange = GetCircleLayoutTransformChange(Idx);
				const FVector Translation = TransformChange.GetTranslation();

				if (CircleLayoutOptions.bAccumulateTransform)
				{
					TransformChange.SetTranslation(FVector::ZeroVector);
					MeshTransforms::ApplyTransform(TmpMesh, TransformChange, true);
				}

				MeshTransforms::Translate(AppendMesh, Translation);
				Editor.AppendMesh(&AppendMesh, TmpMappings);
				TmpMappings.Reset();
			}
		}
		// center this mesh
		if (!CenterAxis.IsZero())
		{
			const FBox BoundingBox = static_cast<FBox>(AppendToMesh.GetBounds(true));
			if (BoundingBox.IsValid)
			{
				const FVector BoundingCenter = BoundingBox.GetCenter();
				// center only on specified axis
				const FVector Translate = BoundingCenter * CenterAxis;
				MeshTransforms::Translate(AppendToMesh, -Translate);
			}
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown);

	Next();
}

#if WITH_EDITOR
void UAvaPatternModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName LayoutName = GET_MEMBER_NAME_CHECKED(UAvaPatternModifier, Layout);
	static const FName LineLayoutOptionsName = GET_MEMBER_NAME_CHECKED(UAvaPatternModifier, LineLayoutOptions);
	static const FName GridLayoutOptionsName = GET_MEMBER_NAME_CHECKED(UAvaPatternModifier, GridLayoutOptions);
	static const FName CircleLayoutOptionsName = GET_MEMBER_NAME_CHECKED(UAvaPatternModifier, CircleLayoutOptions);

	if (MemberName == LayoutName)
	{
		OnLayoutChanged();
	}
	else if (MemberName == LineLayoutOptionsName)
	{
		OnLineLayoutOptionsChanged();
	}
	else if (MemberName == GridLayoutOptionsName)
	{
		OnGridLayoutOptionsChanged();
	}
	else if (MemberName == CircleLayoutOptionsName)
	{
		OnCircleLayoutOptionsChanged();
	}
}
#endif

void UAvaPatternModifier::SetLayout(EAvaPatternModifierLayout InLayout)
{
	if (Layout == InLayout)
	{
		return;
	}

	Layout = InLayout;
	OnLayoutChanged();
}

void UAvaPatternModifier::SetLineLayoutOptions(const FAvaPatternModifierLineLayoutOptions& InOptions)
{
	LineLayoutOptions = InOptions;
	OnLineLayoutOptionsChanged();
}

void UAvaPatternModifier::SetGridLayoutOptions(const FAvaPatternModifierGridLayoutOptions& InOptions)
{
	GridLayoutOptions = InOptions;
	OnGridLayoutOptionsChanged();
}

void UAvaPatternModifier::SetCircleLayoutOptions(const FAvaPatternModifierCircleLayoutOptions& InOptions)
{
	CircleLayoutOptions = InOptions;
	OnCircleLayoutOptionsChanged();
}

void UAvaPatternModifier::OnLayoutChanged()
{
	MarkModifierDirty();
}

void UAvaPatternModifier::OnLineLayoutOptionsChanged()
{
	if (Layout == EAvaPatternModifierLayout::Line)
	{
		MarkModifierDirty();
	}
}

void UAvaPatternModifier::OnGridLayoutOptionsChanged()
{
	if (Layout == EAvaPatternModifierLayout::Grid)
	{
		MarkModifierDirty();
	}
}

void UAvaPatternModifier::OnCircleLayoutOptionsChanged()
{
	if (Layout == EAvaPatternModifierLayout::Circle)
	{
		MarkModifierDirty();
	}
}

UE::Geometry::FTransformSRT3d UAvaPatternModifier::GetLineLayoutTransformChange() const
{
	const FVector Axis = (LineLayoutOptions.bAxisInverted ? -1 : 1 ) *
		(LineLayoutOptions.Axis == EAvaPatternModifierAxis::X ? FVector::XAxisVector : (LineLayoutOptions.Axis == EAvaPatternModifierAxis::Y ? FVector::YAxisVector : FVector::ZAxisVector));
	const FVector Size3D = OriginalMeshBounds.GetSize();
	const FVector Translation = Axis * Size3D + Axis * LineLayoutOptions.Spacing;
	FTransform TransformChange;
	TransformChange.SetRotation(LineLayoutOptions.Rotation.Quaternion());
	TransformChange.SetLocation(Translation);
	TransformChange.SetScale3D(LineLayoutOptions.Scale);
	return UE::Geometry::FTransformSRT3d(TransformChange);
}

UE::Geometry::FTransformSRT3d UAvaPatternModifier::GetGridLayoutColTransformChange() const
{
	FVector ColAxis;
	if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::XY)
	{
		ColAxis = FVector(0, 1, 0);
	}
	else if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::YZ)
	{
		ColAxis = FVector(0, 0, 1);
	}
	else if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::ZX)
	{
		ColAxis = FVector(1, 0, 0);
	}
	ColAxis *= (GridLayoutOptions.AxisInverted.bY ? -1 : 1);
	const FVector Size3D = OriginalMeshBounds.GetSize();
	const FVector Translation = ColAxis * Size3D + ColAxis * GridLayoutOptions.Spacing.Y;
	FTransform TransformChange;
	TransformChange.SetRotation(GridLayoutOptions.Rotation.Quaternion());
	TransformChange.SetLocation(Translation);
	TransformChange.SetScale3D(GridLayoutOptions.Scale);
	return UE::Geometry::FTransformSRT3d(TransformChange);
}

UE::Geometry::FTransformSRT3d UAvaPatternModifier::GetGridLayoutRowTransformChange() const
{
	FVector RowAxis;
	if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::XY)
	{
		RowAxis = FVector(1, 0, 0);
	}
	else if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::YZ)
	{
		RowAxis = FVector(0, 1, 0);
	}
	else if (GridLayoutOptions.Plane == EAvaPatternModifierPlane::ZX)
	{
		RowAxis = FVector(0, 0, 1);
	}
	RowAxis *= (GridLayoutOptions.AxisInverted.bX ? -1 : 1);
	const FVector Size3D = OriginalMeshBounds.GetSize();
	const FVector Translation = RowAxis * Size3D + RowAxis * GridLayoutOptions.Spacing.X;
	FTransform TransformChange;
	TransformChange.SetRotation(GridLayoutOptions.Rotation.Quaternion());
	TransformChange.SetLocation(Translation);
	TransformChange.SetScale3D(GridLayoutOptions.Scale);
	return UE::Geometry::FTransformSRT3d(TransformChange);
}

UE::Geometry::FTransformSRT3d UAvaPatternModifier::GetCircleLayoutTransformChange(int32 Idx) const
{
	const int32 Count = CircleLayoutOptions.RepeatCount;
	const float Radius = CircleLayoutOptions.Radius;
	const float StartAngle = FMath::DegreesToRadians(CircleLayoutOptions.StartAngle);
	const float FullAngle = FMath::DegreesToRadians(CircleLayoutOptions.FullAngle);
	const float AngleStep = FullAngle / Count;

	const float ZeroAngle = StartAngle;
	const float ZeroX = Radius * FMath::Cos(ZeroAngle);
	const float ZeroY = Radius * FMath::Sin(ZeroAngle);

	const float CurrentAngle = StartAngle + Idx * AngleStep;
	const float X = Radius * FMath::Cos(CurrentAngle);
	const float Y = Radius * FMath::Sin(CurrentAngle);

	const bool bCenterPlane = CircleLayoutOptions.bCentered;

	FVector Translation = FVector::ZeroVector;
	if (CircleLayoutOptions.Plane == EAvaPatternModifierPlane::XY)
	{
		const FVector TranslationZero = bCenterPlane ? FVector::ZeroVector : FVector(ZeroX, ZeroY, 0);
		Translation = FVector(X, Y, 0) - TranslationZero;
	}
	else if (CircleLayoutOptions.Plane == EAvaPatternModifierPlane::YZ)
	{
		const FVector TranslationZero = bCenterPlane ? FVector::ZeroVector : FVector(0, ZeroX, ZeroY);
		Translation = FVector(0, X, Y) - TranslationZero;
	}
	else if (CircleLayoutOptions.Plane == EAvaPatternModifierPlane::ZX)
	{
		const FVector TranslationZero = bCenterPlane ? FVector::ZeroVector : FVector(ZeroX, 0, ZeroY);
		Translation = FVector(X, 0, Y) - TranslationZero;
	}
	FTransform TransformChange;
	TransformChange.SetLocation(Translation);
	TransformChange.SetRotation(CircleLayoutOptions.Rotation.Quaternion());
	TransformChange.SetScale3D(CircleLayoutOptions.Scale);
	return UE::Geometry::FTransformSRT3d(TransformChange);
}

#undef LOCTEXT_NAMESPACE
