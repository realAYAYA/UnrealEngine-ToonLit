/*
Copyright 2019 Valve Corporation under https://opensource.org/licenses/BSD-3-Clause
This code includes modifications by Epic Games.  Modifications (c) Epic Games, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "ISteamVRInputDeviceModule.h"
#include "Engine/Engine.h"
#include "IInputDevice.h"
#include "IXRTrackingSystem.h"
#include "Interfaces/IPluginManager.h"
#include "SteamVRInputDevice.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "IVREditorModule.h"
#endif

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FSteamVRInputDeviceModule : public ISteamVRInputDeviceModule
{
	/* Creates a new instance of SteamVR Input Controller **/
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override
	{
		static FName SystemName(TEXT("SteamVR"));
		if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName))
		{
			TSharedPtr<class FSteamVRInputDevice> Device(new FSteamVRInputDevice(InMessageHandler));
#if WITH_EDITOR
			FEditorDelegates::OnActionAxisMappingsChanged.AddSP(Device.ToSharedRef(), &FSteamVRInputDevice::OnActionMappingsChanged);

			if (IVREditorModule::IsAvailable())
			{
				IVREditorModule::Get().OnVREditingModeEnter().AddSP(Device.ToSharedRef(), &FSteamVRInputDevice::OnVREditingModeEnter);
				IVREditorModule::Get().OnVREditingModeExit().AddSP(Device.ToSharedRef(), &FSteamVRInputDevice::OnVREditingModeExit);
			}
#endif
			return Device;
		}
		return nullptr;
	}

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FSteamVRInputDeviceModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);


	// Unload engine integrated controller modules if Valve's Input Plugin is present
	FModuleManager& ModuleManager = FModuleManager::Get();
	IModuleInterface* ValveInputOverride = ModuleManager.GetModule(FName("SteamVR_Input"));

	if (ValveInputOverride != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[SteamVR Input] Found Valve Input Plugin. Integrated version of SteamVR Input will not be loaded."));
		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	}
}

void FSteamVRInputDeviceModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

IMPLEMENT_MODULE(FSteamVRInputDeviceModule, SteamVRInputDevice)

PRAGMA_ENABLE_DEPRECATION_WARNINGS
