// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Electra {

bool PlatformEarlyStartup();
bool PlatformMemorySetup();
bool PlatformShutdown();

class IVideoDecoderResourceDelegate;
TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PlatformCreateVideoDecoderResourceDelegate(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& AdapterDelegate);

}; // namespace Electra

