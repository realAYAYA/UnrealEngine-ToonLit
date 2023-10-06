// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#if PLATFORM_MAC
	#import <Appkit/Appkit.h>
#endif

enum class EAvfMediaCaptureAuthStatus : uint32
{
	// Mapped Apple AVFoundation values
    NotDetermined	= AVAuthorizationStatusNotDetermined,
	Restricted		= AVAuthorizationStatusRestricted,
	Denied			= AVAuthorizationStatusDenied,
	Authorized		= AVAuthorizationStatusAuthorized,
    
    // Extra Error values
	MissingInfoPListEntry,
	InvalidRequest
};

@interface AvfMediaCaptureHelper : NSObject

// Returns current status or error
+ (EAvfMediaCaptureAuthStatus) authorizationStatusForMediaType:(AVMediaType)mediaType;

// Returns current status or error and requests access if not determined
+ (EAvfMediaCaptureAuthStatus) requestAcessForMediaType:(AVMediaType)mediaType completionCallback:(void(^)(EAvfMediaCaptureAuthStatus AuthStatus))cbHandler;

- (BOOL) setupCaptureSession:(NSString*)deviceID sampleBufferCallback:(void(^)(CMSampleBufferRef sampleBuffer))sampleCallbackBlock
												 notificationCallback:(void(^)(NSNotification* const notification))notificationCallbackBlock;
- (void) stopCaptureSession;
- (void) startCaptureSession;
- (BOOL) isCaptureRunning;

- (NSString*) 	getCaptureDeviceName;
- (AVMediaType) getCaptureDeviceMediaType;

- (NSArray<AVCaptureDeviceFormat*>*) getCaptureDeviceAvailableFormats;

- (NSInteger) 	getCaptureDeviceActiveFormatIndex;
- (BOOL) 		setCaptureDeviceActiveFormatIndex:(NSInteger)formatIdx;

@end
