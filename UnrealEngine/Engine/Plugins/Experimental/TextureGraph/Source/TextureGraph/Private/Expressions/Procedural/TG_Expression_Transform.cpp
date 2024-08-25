// Copyright Epic Games, Inc. All Rights Reserved.
#include "Expressions/Procedural/TG_Expression_Transform.h"
#include "FxMat/MaterialManager.h"
#include "Transform/Expressions/T_Transform.h"

void UTG_Expression_Transform::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);


	auto DesiredDescriptor = Output.Descriptor;
	FVector2f spacing = {0, 0};

	if (!Input)
	{
		DesiredDescriptor.Width = DesiredDescriptor.Height = EResolution::Resolution1024;
	}
	else
	{
		if (DesiredDescriptor.Width == EResolution::Auto)
		{
			DesiredDescriptor.Width = EResolution::Resolution1024; 			
		}
		if (DesiredDescriptor.Height == EResolution::Auto)
		{
			DesiredDescriptor.Height = EResolution::Resolution1024;
		}
	}
	FVector2f OutResolution = { float(DesiredDescriptor.Width), float( DesiredDescriptor.Height) };
	FVector2f OutCoverage = Coverage;
	FVector2f OutOffset = Offset;

	T_Transform::TransformParameter XformParam{
		.Coverage = OutCoverage,
		.Translation = OutOffset,
		.Pivot = Pivot,
		.RotationXY = Rotation * (PI / 180.0f),
		.Scale = Repeat
	};

	T_Transform::CellParameter CellParam{
		.Zoom = Zoom,
		.StretchToFit = StretchToFit,
		.Spacing = spacing,
		.Stagger = {(StaggerOffset * StaggerHorizontally), (StaggerOffset * (1 - StaggerHorizontally))},
		.Stride = Stride,
	};

	T_Transform::ColorParameter ColorParam{
		.FillColor = FillColor,
		.WrapFilterMode = WrapMode,
		.MirrorX = MirrorX,
		.MirrorY = MirrorY,
		.ShowDebugGrid = ShowDebugGrid
	};

	Output = T_Transform::Create(InContext->Cycle, DesiredDescriptor, Input,
		XformParam, CellParam, ColorParam, InContext->TargetId);
}

