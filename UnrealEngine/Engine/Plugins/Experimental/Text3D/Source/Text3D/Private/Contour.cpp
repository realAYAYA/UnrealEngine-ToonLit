// Copyright Epic Games, Inc. All Rights Reserved.


#include "Contour.h"
#include "Part.h"


FContour::FContour()
{
}

FContour::~FContour()
{
	for (FPartPtr Part : *this)
	{
		Part->Prev.Reset();
		Part->Next.Reset();
	}
}

void FContour::SetNeighbours()
{
	for (int32 Index = 0; Index < Num(); Index++)
	{
		FPartPtr Point = (*this)[Index];

		Point->Prev = (*this)[(Index + Num() - 1) % Num()];
		Point->Next = (*this)[(Index + 1) % Num()];
	}
}

void FContour::CopyFrom(const FContour& Other)
{
	for (const FPartConstPtr OtherPoint : Other)
	{
		Add(MakeShared<FPart>(OtherPoint));
	}

	SetNeighbours();
}
