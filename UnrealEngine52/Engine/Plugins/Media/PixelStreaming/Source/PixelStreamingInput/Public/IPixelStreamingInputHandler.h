// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"
#include "IInputDevice.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Templates/SharedPointer.h"
#include "Serialization/MemoryReader.h"
#include "PixelStreamingInputEnums.h"

/**
 * The IPixelStreamingInputHandler, used to handle input from a remote peer and pass it to UE accordingly. Setting the target viewport allows for
 * scaling of input from browser to application, and setting the target window ensure that if windows are tiled (eg editor)
 * that the streamed input only affect the target window.
 */
class PIXELSTREAMINGINPUT_API IPixelStreamingInputHandler : public IInputDevice
{
public:
	/**
	 * @brief Handle the message from the WebRTC data channel.
	 * @param Buffer The data channel message
	 */
	virtual void OnMessage(TArray<uint8> Buffer) = 0;

	/**
	 * @brief Set the viewport this input device is associated with.
	 * @param InTargetViewport The viewport to set
	 */
	virtual void SetTargetViewport(TWeakPtr<SViewport> InTargetViewport) = 0;

	/**
	 * @brief Get the viewport this input device is associated with.
	 * @return The viewport this input device is associated with
	 */
	virtual TWeakPtr<SViewport> GetTargetViewport() = 0;

	/**
	 * @brief Set the viewport this input device is associated with.
	 * @param InTargetWindow The viewport to set
	 */
	virtual void SetTargetWindow(TWeakPtr<SWindow> InTargetWindow) = 0;

	/**
	 * @brief Get the viewport this input device is associated with.
	 * @return The viewport this input device is associated with
	 */
	virtual TWeakPtr<SWindow> GetTargetWindow() = 0;

	/**
	 * @brief Set the target screen size for this streamer. This is used to when the streamer doesn't have a singular target window / viewport
	 * and as such we just use the manual scale
	 * @param InTargetWindow The target screen size
	 */
	UE_DEPRECATED(5.2, "SetTargetScreenSize() is deprecated. Please use SetTargetScreenRect() instead.")
	virtual void SetTargetScreenSize(TWeakPtr<FIntPoint> InTargetScreenSize) = 0;

	/**
	 * @brief Get the target screen size for this streamer
	 * @return The target screen size
	 */
	UE_DEPRECATED(5.2, "GetTargetScreenSize() is deprecated. Please use GetTargetScreenRect() instead.")
	virtual TWeakPtr<FIntPoint> GetTargetScreenSize() = 0;

	/**
	 * @brief Set the target screen rectangle for this streamer. This is used to when the streamer doesn't have a singular target window / viewport
	 * and as such we just use the manual scale
	 * @param InTargetScreenRect The target screen rectangle
	 */
	virtual void SetTargetScreenRect(TWeakPtr<FIntRect> InTargetScreenRect) = 0;

	/**
	 * @brief Get the target screen rectangle for this streamer
	 * @return The target screen rectangle
	 */
	virtual TWeakPtr<FIntRect> GetTargetScreenRect() = 0;

	/**
	 * @brief Set whether the input devices is faking touch events using keyboard and mouse this can be useful for debugging.
	 * @return true
	 * @return false
	 */
	virtual bool IsFakingTouchEvents() const = 0;

	/**
	 * @brief Register a function to be called whenever the specified message type is received.
	 *
	 * @param MessageType The human readable identifier for the message
	 * @param Handler The function called when this message type is received. This handler must take a single parameter (an FMemoryReader) and have a return type of void
	 */
	virtual void RegisterMessageHandler(const FString& MessageType, const TFunction<void(FMemoryReader)>& Handler) = 0;

	/**
	 * @brief Register a custom function to execute when command JSON is received over the data channel: "{ type: "Command", YourCommand: YourCommandValue }".
	 * Note: You can also override the default Pixel Streaming command handlers by setting handlers with the same name as those already used, e.g. "Stat.FPS".
	 */
	virtual void SetCommandHandler(const FString& CommandName, const TFunction<void(FString, FString)>& Handler) = 0;

	/**
	 * @brief Find the function to be called whenever the specified message type is received.
	 *
	 * @param MessageType The human readable identifier for the message
	 * @return TFunction<void(FMemoryReader)> The function called when this message type is received.
	 */
	virtual TFunction<void(FMemoryReader)> FindMessageHandler(const FString& MessageType) = 0;

	/**
	 * @brief Set the input handlers type. This controls whether input is routed to widgets or windows
	 */
	virtual void SetInputType(EPixelStreamingInputType InputType) = 0;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSendMessage, FString, FMemoryReader);
	FOnSendMessage OnSendMessage;
};