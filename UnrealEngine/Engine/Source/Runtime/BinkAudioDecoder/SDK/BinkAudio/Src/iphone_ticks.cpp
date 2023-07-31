// Copyright Epic Games, Inc. All Rights Reserved.
#include "ticks.h"

#include <mach/mach_time.h>
#include <unistd.h>

U64 baue_ticks( void )
{
  return mach_absolute_time();
}
