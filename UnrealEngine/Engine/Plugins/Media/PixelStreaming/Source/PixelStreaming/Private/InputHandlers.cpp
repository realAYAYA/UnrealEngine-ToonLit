// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputHandlers.h"
#include "IPixelStreamingModule.h"
#include "Framework/Application/SlateApplication.h"

namespace UE::PixelStreaming
{
        FInputHandlers::FInputHandlers(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
        {
            // This is imperative for editor streaming as when a modal is open or we've hit a BP breakpoint, the engine tick loop will not run, so instead we rely on this delegate to tick for us
		    FSlateApplication::Get().OnPreTick().AddRaw(this, &FInputHandlers::Tick);
        }
    
		void FInputHandlers::Tick(float DeltaTime)
        {
            ForEachInputHandler([DeltaTime](IInputDevice* InputHandler) {
                InputHandler->Tick(DeltaTime);
            });
        }

		void FInputHandlers::SendControllerEvents()
        {
            ForEachInputHandler([](IInputDevice* InputHandler) {
                InputHandler->SendControllerEvents();
            });
        }

		void FInputHandlers::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
        {
            ForEachInputHandler([&MessageHandler = InMessageHandler](IInputDevice* InputHandler) {
                InputHandler->SetMessageHandler(MessageHandler);
            });
        }

		bool FInputHandlers::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
        {
            ForEachInputHandler([InWorld, Cmd, &Ar](IInputDevice* InputHandler) {
                InputHandler->Exec(InWorld, Cmd, Ar);
            });
            return true;
        }

		void FInputHandlers::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) 
        {
            ForEachInputHandler([&ControllerId, &ChannelType, &Value](IInputDevice* InputHandler) {
                InputHandler->SetChannelValue(ControllerId, ChannelType, Value);
            });
        }

		void FInputHandlers::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
        {
            ForEachInputHandler([&ControllerId, &Values](IInputDevice* InputHandler) {
                InputHandler->SetChannelValues(ControllerId, Values);
            });
        }

		void FInputHandlers::ForEachInputHandler(TFunction<void(IInputDevice*)> const& Visitor)
        {
            IPixelStreamingModule& Module = IPixelStreamingModule::Get();
            Module.ForEachStreamer([Visitor](TSharedPtr<IPixelStreamingStreamer> Streamer) {
                if(Streamer)
                {
                    TWeakPtr<IPixelStreamingInputHandler> WeakInputHandler = Streamer->GetInputHandler();
                    TSharedPtr<IPixelStreamingInputHandler> InputHandler = WeakInputHandler.Pin();
                    if (InputHandler)
                    {
                        Visitor(InputHandler.Get());
                    }
                }
            });
        }
} // namespace UE::PixelStreaming
