// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/ICursor.h"
#include "Math/IntVector.h"
//#include "Microsoft/AllowMicrosoftPlatformTypes.h"
//#include "agile.h"
//#include "Microsoft/HideMicrosoftPlatformTypes.h"


ref class FHoloLensCursorMouseEventObj sealed
{
public:
    FHoloLensCursorMouseEventObj();

    void OnMouseMoved(Windows::Devices::Input::MouseDevice ^sender, Windows::Devices::Input::MouseEventArgs ^args);

    Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>^ GetMouseMovedHandler();
};


class FHoloLensCursor : public ICursor
{
public:

    FHoloLensCursor();

	virtual ~FHoloLensCursor();

	virtual FVector2D GetPosition() const override;

	virtual void SetPosition( const int32 X, const int32 Y ) override;

	virtual void SetType( const EMouseCursor::Type InNewCursor ) override;

	virtual EMouseCursor::Type GetType() const override
	{
		return CurrentCursor;
	}

	virtual void GetSize( int32& Width, int32& Height ) const override;

	virtual void Show( bool bShow ) override;

	virtual void Lock( const RECT* const Bounds ) override;

    void UpdatePosition(const FVector2D& NewPosition);

    bool IsUsingRawMouseNoCursor() { return bUsingRawMouseNoCursor; }

    void ProcessDeferredActions();

	void OnRawMouseMove(const FIntVector& MouseDelta);

	/**
	* Allows overriding the shape of a particular cursor.
	*/
	void SetTypeShape(EMouseCursor::Type InCursorType, void* CursorHandle);

private:

	void SetUseRawMouse(bool bUse);

	Windows::UI::Core::CoreCursor^ GetDefaultCursorForType(EMouseCursor::Type InCursorType);

    EMouseCursor::Type                                CurrentCursor = (EMouseCursor::Type) - 1;
    FVector2D                                         CursorPosition;
    bool                                              bUsingRawMouseNoCursor;
    bool                                              bDeferredCursorTypeChange;
	TArray<FIntVector>                                DeferredMoveEvents;
    
    /** Cursors */
    Platform::Array<Windows::UI::Core::CoreCursor^>^  Cursors;
    FHoloLensCursorMouseEventObj^                          MouseEventObj;
    Windows::Foundation::EventRegistrationToken       MouseEventRegistrationToken;
};