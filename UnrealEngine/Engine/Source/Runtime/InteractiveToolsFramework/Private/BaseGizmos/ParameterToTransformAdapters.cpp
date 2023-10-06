// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/ParameterToTransformAdapters.h"
#include "BaseGizmos/GizmoMath.h"


void UGizmoAxisTranslationParameterSource::SetParameter(float NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	double UseDelta = LastChange.GetChangeDelta();

	// check for any constraints on the delta value
	double SnappedDelta = 0;
	if (AxisDeltaConstraintFunction(UseDelta, SnappedDelta))
	{
		UseDelta = SnappedDelta;
	}

	// construct translation as delta from initial position
	FVector Translation = UseDelta * CurTranslationAxis;

	// translate the initial transform
	FTransform NewTransform = InitialTransform;
	NewTransform.AddToTranslation(Translation);

	// apply position constraint
	FVector SnappedPos;
	if (PositionConstraintFunction(NewTransform.GetTranslation(), SnappedPos))
	{
		FVector SnappedLinePos = GizmoMath::ProjectPointOntoLine(SnappedPos, CurTranslationOrigin, CurTranslationAxis);
		NewTransform.SetTranslation(SnappedLinePos);
	}

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoAxisTranslationParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoFloatParameterChange(Parameter);

	InitialTransform = TransformSource->GetTransform();
	CurTranslationAxis = AxisSource->GetDirection();
	CurTranslationOrigin = AxisSource->GetOrigin();
}

void UGizmoAxisTranslationParameterSource::EndModify()
{
}







void UGizmoPlaneTranslationParameterSource::SetParameter(const FVector2D& NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	// construct translation as delta from initial position
	FVector2D Delta = LastChange.GetChangeDelta();
	double UseDeltaX = Delta.X;
	double UseDeltaY = Delta.Y;

	// check for any constraints on the delta value
	double SnappedDeltaX = 0, SnappedDeltaY = 0;
	if (AxisXDeltaConstraintFunction(UseDeltaX, SnappedDeltaX))
	{
		UseDeltaX = SnappedDeltaX;
	}
	if (AxisYDeltaConstraintFunction(UseDeltaY, SnappedDeltaY))
	{
		UseDeltaY = SnappedDeltaY;
	}

	FVector Translation = UseDeltaX*CurTranslationAxisX + UseDeltaY*CurTranslationAxisY;

	// apply translation to initial transform
	FTransform NewTransform = InitialTransform;
	NewTransform.AddToTranslation(Translation);

	// apply position constraint
	FVector SnappedPos;
	if (PositionConstraintFunction(NewTransform.GetTranslation(), SnappedPos))
	{
		FVector PlanePos = GizmoMath::ProjectPointOntoPlane(SnappedPos, CurTranslationOrigin, CurTranslationNormal);
		NewTransform.SetTranslation(PlanePos);
	}

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoPlaneTranslationParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoVec2ParameterChange(Parameter);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurTranslationOrigin = AxisSource->GetOrigin();
	AxisSource->GetAxisFrame(CurTranslationNormal, CurTranslationAxisX, CurTranslationAxisY);
}

void UGizmoPlaneTranslationParameterSource::EndModify()
{
}



void UGizmoAxisRotationParameterSource::SetParameter(float NewValue)
{
	Angle = NewValue;
	LastChange.CurrentValue = NewValue;

	double AngleDelta = LastChange.GetChangeDelta();
	double SnappedDelta;
	if (AngleDeltaConstraintFunction(AngleDelta, SnappedDelta))
	{
		AngleDelta = SnappedDelta;
	}

	// construct rotation as delta from initial position
	FQuat DeltaRotation(CurRotationAxis, AngleDelta);
	DeltaRotation = RotationConstraintFunction(DeltaRotation);

	// rotate the vector from the rotation origin to the transform origin, 
	// to get the translation of the origin produced by the rotation
	FVector DeltaPosition = InitialTransform.GetLocation() - CurRotationOrigin;
	DeltaPosition = DeltaRotation * DeltaPosition;
	FVector NewLocation = CurRotationOrigin + DeltaPosition;

	// rotate the initial transform by the rotation
	FQuat NewRotation = DeltaRotation * InitialTransform.GetRotation();

	// construct new transform
	FTransform NewTransform = InitialTransform;
	NewTransform.SetLocation(NewLocation);
	NewTransform.SetRotation(NewRotation);
	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}


void UGizmoAxisRotationParameterSource::BeginModify()
{
	check(AxisSource != nullptr);

	LastChange = FGizmoFloatParameterChange(Angle);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurRotationAxis = AxisSource->GetDirection();
	CurRotationOrigin = AxisSource->GetOrigin();
}

void UGizmoAxisRotationParameterSource::EndModify()
{
}




void UGizmoUniformScaleParameterSource::SetParameter(const FVector2D& NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	// Convert 2D parameter delta to a 1D uniform scale change
	// This possibly be exposed as a TFunction to allow customization?
	double SignedDelta = LastChange.GetChangeDelta().X + LastChange.GetChangeDelta().Y;
	SignedDelta *= ScaleMultiplier;
	SignedDelta += 1.0;

	FTransform NewTransform = InitialTransform;
	FVector StartScale = InitialTransform.GetScale3D();
	FVector NewScale = SignedDelta * StartScale;
	NewTransform.SetScale3D(NewScale);

	// apply position constraint
	//FVector SnappedPos;
	//if (PositionConstraintFunction(NewTransform.GetScale(), SnappedPos))
	//{
	//	FVector PlanePos = GizmoMath::ProjectPointOntoPlane(SnappedPos, CurTranslationOrigin, CurTranslationNormal);
	//	NewTransform.SetTranslation(PlanePos);
	//}

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoUniformScaleParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoVec2ParameterChange(Parameter);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurScaleOrigin = AxisSource->GetOrigin();
	// note: currently not used!
	AxisSource->GetAxisFrame(CurScaleNormal, CurScaleAxisX, CurScaleAxisY);
}

void UGizmoUniformScaleParameterSource::EndModify()
{
}





void UGizmoAxisScaleParameterSource::SetParameter(float NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	// construct translation as delta from initial position
	float ScaleDelta = LastChange.GetChangeDelta() * ScaleMultiplier;

	// translate the initial transform
	FTransform NewTransform = InitialTransform;
	FVector StartScale = InitialTransform.GetScale3D();
	FVector NewScale = StartScale + ScaleDelta * CurScaleAxis;

	if (bClampToZero)
	{
		NewScale = FVector::Max(FVector::ZeroVector, NewScale);
	}

	NewTransform.SetScale3D(NewScale);


	// apply position constraint
	//FVector SnappedPos;
	//if (ScaleConstraintFunction(NewTransform.GetTranslation(), SnappedPos))
	//{
	//	FVector SnappedLinePos = GizmoMath::ProjectPointOntoLine(SnappedPos, CurTranslationOrigin, CurTranslationAxis);
	//	NewTransform.SetTranslation(SnappedLinePos);
	//}

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoAxisScaleParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoFloatParameterChange(Parameter);

	InitialTransform = TransformSource->GetTransform();
		
	CurScaleAxis = AxisSource->GetDirection();
	CurScaleOrigin = AxisSource->GetOrigin();
}

void UGizmoAxisScaleParameterSource::EndModify()
{
}





void UGizmoPlaneScaleParameterSource::SetParameter(const FVector2D& NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	// construct Scale as delta from initial position
	FVector2D Delta = LastChange.GetChangeDelta() * ScaleMultiplier;

	if (bUseEqualScaling)
	{
		Delta = FVector2D(Delta.X + Delta.Y);
	}

	FTransform NewTransform = InitialTransform;
	FVector StartScale = InitialTransform.GetScale3D();
	FVector NewScale = StartScale + Delta.X*CurScaleAxisX + Delta.Y*CurScaleAxisY;

	if (bClampToZero)
	{
		NewScale = FVector::Max(NewScale, FVector::ZeroVector);
	}

	NewTransform.SetScale3D(NewScale);

	// apply position constraint
	//FVector SnappedPos;
	//if (PositionConstraintFunction(NewTransform.GetScale(), SnappedPos))
	//{
	//	FVector PlanePos = GizmoMath::ProjectPointOntoPlane(SnappedPos, CurTranslationOrigin, CurTranslationNormal);
	//	NewTransform.SetTranslation(PlanePos);
	//}

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoPlaneScaleParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoVec2ParameterChange(Parameter);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurScaleOrigin = AxisSource->GetOrigin();
	AxisSource->GetAxisFrame(CurScaleNormal, CurScaleAxisX, CurScaleAxisY);
}

void UGizmoPlaneScaleParameterSource::EndModify()
{
}