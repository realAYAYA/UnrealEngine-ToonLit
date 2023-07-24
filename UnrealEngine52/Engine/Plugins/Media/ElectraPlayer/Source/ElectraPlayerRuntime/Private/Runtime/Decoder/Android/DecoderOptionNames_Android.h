// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

namespace Electra
{

namespace Android
{

//!< (boolean) if set to true the decoder will be reconfigured with the then-current-surface when the application resumes after being suspended.
const TCHAR* const OptionKey_Decoder_ReconfigureSurfaceOnWakeup = TEXT("android:update_surface_on_wakeup");	

//!< (boolean) if set to true the decoder must be destroyed and recreated instead of trying to set a new surface when the application resumes after being suspended.
const TCHAR* const OptionKey_Decoder_ForceNewDecoderOnWakeup = TEXT("android:force_new_decoder_on_wakeup");	

//! (boolean) if set to true skips decoding replay AUs on application resume and waits for the next IDR frame.
//! Requires OptionKey_Decoder_ReconfigureSurfaceOnWakeup to be set to true.
const TCHAR* const OptionKey_Decoder_SkipUntilIDROnSurfaceChange = TEXT("android:skip_until_idr");						

}

} // namespace Electra


