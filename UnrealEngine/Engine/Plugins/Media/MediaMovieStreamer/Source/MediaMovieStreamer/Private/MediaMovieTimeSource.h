// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTimeSource.h"


/**
 * Implements the a media time source that derives its time from the application's global time.
 */
class FMediaMovieTimeSource
	: public IMediaTimeSource
{
public:

	/** Virtual destructor. */
	virtual ~FMediaMovieTimeSource() { }

public:

	//~ IMediaTimeSource interface

	virtual FTimespan GetTimecode() override;
};
