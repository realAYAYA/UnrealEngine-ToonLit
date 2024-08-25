// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Layout/Geometry.h"
#include "Misc/FrameRate.h"

//TODO: Move to UE::Sequencer namespace

/**
 * Utility for converting time units to slate pixel units and vice versa
 */
struct FTimeToPixel
{
public:

	FTimeToPixel( const FGeometry& AllottedGeometry, const TRange<double>& InLocalViewRange, const FFrameRate& InTickResolution )
		: FTimeToPixel(AllottedGeometry.GetLocalSize().X, InLocalViewRange, InTickResolution)
	{}

	FTimeToPixel( float WidthPx, const TRange<double>& InLocalViewRange, const FFrameRate& InTickResolution )
		: ViewRangeStartSeconds( InLocalViewRange.GetLowerBoundValue() )
		, TickResolution( InTickResolution )
	{
		ensureMsgf(WidthPx >= 0, TEXT("Got negative pixel width!"));
		const double VisibleWidth = InLocalViewRange.Size<double>();

		const float MaxPixelsPerSecond = 1000.f;
		PixelsPerSecond = VisibleWidth > 0 ? static_cast<float>(WidthPx / VisibleWidth) : MaxPixelsPerSecond;
	}

	/**
	 * Converts a time to a pixel point relative to the geometry of a widget (passed into the constructor)
	 *
	 * @param Time	The time to convert
	 * @return The pixel equivalent of the time
	 */
	float SecondsToPixel( double Time ) const
	{
		return static_cast<float>((Time - ViewRangeStartSeconds) * PixelsPerSecond);
	}

	/**
	 * Converts a time delta to a pixel delta
	 *
	 * @param TimeDelta  The time delta to convert
	 * @return           The pixel equivalent of the delta time
	 */
	float SecondsDeltaToPixel( double TimeDelta ) const
	{
		return static_cast<float>(TimeDelta * PixelsPerSecond);
	}

	/**
	 * Converts a pixel value to time 
	 *
	 * @param PixelX The x value of a pixel coordinate relative to the geometry that was passed into the constructor.
	 * @return The time where the pixel is located
	 */
	double PixelToSeconds( float PixelX ) const
	{
		return (PixelX/PixelsPerSecond) + ViewRangeStartSeconds;
	}

	/**
	 * Converts a frame time to a pixel point relative to the geometry of a widget (passed into the constructor)
	 *
	 * @param Time The time to convert
	 * @return The pixel equivalent of the frame time
	 */
	float FrameToPixel( const FFrameTime& Time ) const
	{
		return static_cast<float>((Time / TickResolution - ViewRangeStartSeconds) * PixelsPerSecond);
	}

	/**
	 * Converts a frame delta value to pixel delta
	 *
	 * @param TimeDelta   The time delta to convert
	 * @return            The pixel equivalent of the delta time
	 */
	float FrameDeltaToPixel( const FFrameTime& TimeDelta) const
	{
		return static_cast<float>((TimeDelta / TickResolution) * PixelsPerSecond);
	}

	/**
	 * Converts a pixel value to frame time 
	 *
	 * @param PixelX The x value of a pixel coordinate relative to the geometry that was passed into the constructor.
	 * @return The frame time where the pixel is located
	 */
	FFrameTime PixelToFrame( float PixelX ) const
	{
		return ( PixelX / PixelsPerSecond + ViewRangeStartSeconds ) * TickResolution;
	}

	/**
	 * Converts a pixel delta value to delta frame time 
	 *
	 * @param PixelDelta The delta value in pixel space
	 * @return The equivalent delta frame time
	 */
	FFrameTime PixelDeltaToFrame( float PixelDelta ) const
	{
		return ( PixelDelta / PixelsPerSecond ) * TickResolution;
	}

	/**
	 * Converts a pixel delta value to delta seconds time 
	 *
	 * @param PixelDelta The delta value in pixel space
	 * @return The equivalent delta time in seconds
	 */
	double PixelDeltaToSeconds( float PixelDelta ) const
	{
		return ( PixelDelta / PixelsPerSecond );
	}

	/**
	 * Retrieve the tick resolution of the current sequence
	 */
	FFrameRate GetTickResolution() const
	{
		return TickResolution;
	}

	/**
	 * Make this converter relative to the specified time (ie, such that pixel 0 == FrameAmount)
	 */
	FTimeToPixel RelativeTo(const FFrameTime& FrameZero) const
	{
		FTimeToPixel Copy = *this;
		Copy.ViewRangeStartSeconds = FrameZero / TickResolution;
		return Copy;
	}

private:
	FTimeToPixel(){}

	/** time range of the sequencer in seconds */
	double ViewRangeStartSeconds;
	/** The tick resolution of the current timeline */
	FFrameRate TickResolution;
	/** The number of pixels in the view range */
	float PixelsPerSecond;

};
