// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARSystem.h"
#include "ARTrackable.h"
#include "ILiveLinkSource.h"

class UTimecodeProvider;

/** Interface that publishes face ar blend shape information */
class APPLEARKITFACESUPPORT_API IARKitBlendShapePublisher
{
public:
	virtual void SetTimecodeProvider(UTimecodeProvider* InTimecodeProvider = nullptr) {}
	virtual void PublishBlendShapes(FName SubjectName, const FQualifiedFrameTime& FrameTime, const FARBlendShapeMap& BlendShapes, FName DeviceID = NAME_None) = 0;
};

/** Interface that publishes face ar blend shape information via LiveLink */
class APPLEARKITFACESUPPORT_API ILiveLinkSourceARKit :
	public IARKitBlendShapePublisher,
	public ILiveLinkSource
{
};

/** Factory that creates and registers the sources with the LiveLink client */
class APPLEARKITFACESUPPORT_API FAppleARKitLiveLinkSourceFactory
{
public:
	/** Creates a face mesh source that will autobind to the tracked face mesh */
	static TSharedPtr<ILiveLinkSourceARKit> CreateLiveLinkSource();

	/** Creates the singleton listener that will receive remote events and publish them locally */
	static void CreateLiveLinkRemoteListener();

	/** Creates the publisher that will send remote events to a specified IP */
	static TSharedPtr<IARKitBlendShapePublisher, ESPMode::ThreadSafe> CreateLiveLinkRemotePublisher(const FString& RemoteAddr = FString());

	/** Creates the publisher that will write the curve data to disk */
	static TSharedPtr<IARKitBlendShapePublisher, ESPMode::ThreadSafe> CreateLiveLinkLocalFileWriter();
};
