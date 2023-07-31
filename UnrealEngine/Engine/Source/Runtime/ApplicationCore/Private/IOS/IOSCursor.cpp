// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSCursor.h"
#include "IOS/IOSApplication.h"

FIOSCursor::FIOSCursor() 
	: CurrentType(EMouseCursor::None)
	, CurrentPosition(FVector2D::ZeroVector)
	, CursorClipRect()
	, bShow(false)
{	
}

void FIOSCursor::SetPosition( const int32 X, const int32 Y )
{
	FVector2D NewPosition(X, Y);
	UpdateCursorClipping(NewPosition);
	
	CurrentPosition = NewPosition;
}

void FIOSCursor::SetType( const EMouseCursor::Type InNewCursor )
{
	CurrentType = InNewCursor;
}

void FIOSCursor::GetSize(int32& Width, int32& Height) const
{
	Width = 32;
	Height = 32;
}

void FIOSCursor::Show(bool bInShow)
{
	bShow = bInShow;
}

void FIOSCursor::Lock(const RECT* const Bounds)
{
	if (Bounds == NULL)
	{
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

		// The PS4 cursor should never leave the screen
		CursorClipRect.Min = FIntPoint::ZeroValue;
		CursorClipRect.Max.X = DisplayMetrics.PrimaryDisplayWidth - 1;
		CursorClipRect.Max.Y = DisplayMetrics.PrimaryDisplayHeight - 1;
	}
	else
	{
		CursorClipRect.Min.X = Bounds->left;
		CursorClipRect.Min.Y = Bounds->top;
		CursorClipRect.Max.X = Bounds->right - 1;
		CursorClipRect.Max.Y = Bounds->bottom - 1;
	}

	FVector2D Position = GetPosition();
	if (UpdateCursorClipping(Position))
	{
		SetPosition(FMath::TruncToInt(Position.X), FMath::TruncToInt(Position.Y));
	}
}

bool FIOSCursor::UpdateCursorClipping(FVector2D& CursorPosition)
{
	bool bAdjusted = false;

	if (CursorClipRect.Area() > 0)
	{
		if (CursorPosition.X < CursorClipRect.Min.X)
		{
			CursorPosition.X = CursorClipRect.Min.X;
			bAdjusted = true;
		}
		else if (CursorPosition.X > CursorClipRect.Max.X)
		{
			CursorPosition.X = CursorClipRect.Max.X;
			bAdjusted = true;
		}

		if (CursorPosition.Y < CursorClipRect.Min.Y)
		{
			CursorPosition.Y = CursorClipRect.Min.Y;
			bAdjusted = true;
		}
		else if (CursorPosition.Y > CursorClipRect.Max.Y)
		{
			CursorPosition.Y = CursorClipRect.Max.Y;
			bAdjusted = true;
		}
	}

	return bAdjusted;
}
