// Copyright Epic Games, Inc. All Rights Reserved.

#include "Part.h"
#include "Util.h"

#include "HAL/PlatformMath.h"

constexpr float FPart::CosMaxAngleSideTangent;
constexpr float FPart::CosMaxAngleSides;

FPart::FPart()
{
	Prev = nullptr;
	Next = nullptr;
	bSmooth = false;
	AvailableExpandNear = 0.0f;
	DoneExpand = 0.f;
}

FPart::FPart(const FPartConstPtr& Other)
{
	Position = Other->Position;
	DoneExpand = Other->DoneExpand;
	TangentX = Other->TangentX;
	Normal = Other->Normal;
	bSmooth = Other->bSmooth;
	InitialPosition = Other->InitialPosition;
	PathPrev = Other->PathPrev;
	PathNext = Other->PathNext;
	AvailableExpandNear = Other->AvailableExpandNear;
}

float FPart::TangentsDotProduct() const
{
	check(Prev);
	return FVector2D::DotProduct(-Prev->TangentX, TangentX);
}

float FPart::Length() const
{
	check(Next);

	return (Next->Position - Position).Size();
}

void FPart::ResetDoneExpand()
{
	DoneExpand = 0.f;
}

void FPart::ComputeTangentX()
{
	check(Next);
	TangentX = (Next->Position - Position).GetSafeNormal();
}

bool FPart::ComputeNormal()
{
	check(Prev);

	// Scale is needed to make ((p_(i+1) + k * n_(i+1)) - (p_i + k * n_i)) parallel to (p_(i+1) - p_i). Also (k) is distance between original edge and this edge after expansion with value (k).
	const float OneMinusADotC = 1.0f - TangentsDotProduct();

	if (FMath::IsNearlyZero(OneMinusADotC))
	{
		return false;
	}

	const FVector2D A = -Prev->TangentX;
	const FVector2D C = TangentX;

	Normal = A + C;
	const float NormalLength2 = Normal.SizeSquared();
	const float Scale = -FPlatformMath::Sqrt(2.0f / OneMinusADotC);

	// If previous and next edge are nearly on one line
	if (FMath::IsNearlyZero(NormalLength2, 0.0001f))
	{
		Normal = FVector2D(A.Y, -A.X) * Scale;
	}
	else
	{
		// Sign of cross product is needed to be sure that Normal is directed outside.
		Normal *= -Scale * FPlatformMath::Sign(FVector2D::CrossProduct(A, C)) / FPlatformMath::Sqrt(NormalLength2);
	}

	Normal.Normalize();

	return true;
}

void FPart::ComputeSmooth()
{
	bSmooth = TangentsDotProduct() <= CosMaxAngleSides;
}

bool FPart::ComputeNormalAndSmooth()
{
	if (!ComputeNormal())
	{
		return false;
	}

	ComputeSmooth();
	return true;
}

void FPart::ResetInitialPosition()
{
	InitialPosition = Position;
}

void FPart::ComputeInitialPosition()
{
	InitialPosition = Position - DoneExpand * Normal;
}

void FPart::DecreaseExpandsFar(const float Delta)
{
	for (auto It = AvailableExpandsFar.CreateIterator(); It; ++It)
	{
		It->Value -= Delta;

		if (It->Value < 0.f)
		{
			It.RemoveCurrent();
		}
	}
}

FVector2D FPart::Expanded(const float Value) const
{
	return Position + Normal * Value;
}
