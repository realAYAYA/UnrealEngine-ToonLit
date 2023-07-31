// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreBinarySampleBase.h"

#include "AjaMediaPrivate.h"

/*
 * Implements a pool for AJA binary sample objects. 
 */

class FAjaMediaBinarySamplePool : public TMediaObjectPool<FMediaIOCoreBinarySampleBase> { };
