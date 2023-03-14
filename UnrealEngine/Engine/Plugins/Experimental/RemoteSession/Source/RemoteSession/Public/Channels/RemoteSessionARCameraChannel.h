// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionChannel.h"
#include "UObject/GCObject.h"

class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;
class FAppleImageUtilsConversionTaskBase;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ISceneViewExtension;
class IBackChannelPacket;

class REMOTESESSION_API FRemoteSessionARCameraChannel :
	public IRemoteSessionChannel,
	public FGCObject
{
public:

	FRemoteSessionARCameraChannel(ERemoteSessionChannelMode InRole, TSharedPtr<IBackChannelConnection, ESPMode::ThreadSafe> InConnection);

	~FRemoteSessionARCameraChannel();

	virtual void Tick(const float InDeltaTime) override;

	/* Begin IRemoteSessionChannel implementation */
	static const TCHAR* StaticType() { return TEXT("FRemoteSessionARCameraChannel"); }
	virtual const TCHAR* GetType() const override { return StaticType(); }
	/* End IRemoteSessionChannel implementation */

	/** Returns the post process material to render with. The textures are set on the fly */
	UMaterialInterface* GetPostProcessMaterial() const;

protected:
	/** Sends the most recent compressed AR camera image to the remote */
	void SendARCameraImage();
	/** Compresses in the background and queues for sending */
	void QueueARCameraImage();

	/** Handles data coming from the client */
	void ReceiveARCameraImage(IBackChannelPacket& Message);

	/** Updates the next texture to be rendered */
	void UpdateRenderingTexture();

	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FRemoteSessionARCameraChannel");
	}
	//~ FGCObject

private:
	/** A wrapper around the async compression task */
	struct FCompressionTask
	{
		FCompressionTask() : Width(0), Height(0) {}

		int32 Width;
		int32 Height;
		TSharedPtr<FAppleImageUtilsConversionTaskBase, ESPMode::ThreadSafe> AsyncTask;
	};

	struct FDecompressedImage
	{
		FDecompressedImage() : Width(0), Height(0) {}

		int32 Width;
		int32 Height;
		TArray<uint8> ImageData;
	};

	/** Only compress one frame at a time to prevent flooding the GPU */
	TSharedPtr<FCompressionTask, ESPMode::ThreadSafe> CompressionTask;

	/** Images that have been received and are available for rendering */
	TArray<TSharedPtr<FDecompressedImage, ESPMode::ThreadSafe>> DecompressionQueue;
	FThreadSafeCounter DecompressionTaskCount;
	FCriticalSection DecompressionQueueLock;

	/** The post process material that the overlay will render with */
	UMaterialInterface* PPMaterial;
	/** To set the current texture for rendering */
	UMaterialInstanceDynamic* MaterialInstanceDynamic;
	/** Textures to use when rendering */
	UTexture2D* RenderingTextures[2];
	UTexture2D* LastSetTexture;
	FThreadSafeCounter RenderingTextureIndex;
	FThreadSafeCounter RenderingTexturesUpdateCount[2];

	TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;

	TSharedPtr<IBackChannelConnection, ESPMode::ThreadSafe> Connection;
	ERemoteSessionChannelMode Role;

	/** So we can manage callback lifetimes properly */
	FDelegateHandle MessageCallbackHandle;
};

class REMOTESESSION_API FRemoteSessionARCameraChannelFactoryWorker : public IRemoteSessionChannelFactoryWorker
{
public:
	virtual TSharedPtr<IRemoteSessionChannel> Construct(ERemoteSessionChannelMode InMode, TSharedPtr<IBackChannelConnection, ESPMode::ThreadSafe> InConnection) const override;
};
