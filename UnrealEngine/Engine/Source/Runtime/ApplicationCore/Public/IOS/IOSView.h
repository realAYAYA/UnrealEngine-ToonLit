// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "IOS/IOSInputInterface.h"

#import <UIKit/UIKit.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#if WITH_ACCESSIBILITY
#include "GenericPlatform/Accessibility/GenericAccessibleInterfaces.h"
#endif

struct FKeyboardConfig
{
	UIKeyboardType KeyboardType;
	UITextAutocorrectionType AutocorrectionType;
	UITextAutocapitalizationType AutocapitalizationType;
	BOOL bSecureTextEntry;
	
	FORCEINLINE FKeyboardConfig() :
		KeyboardType(UIKeyboardTypeDefault),
		AutocorrectionType(UITextAutocorrectionTypeNo),
		AutocapitalizationType(UITextAutocapitalizationTypeNone),
		bSecureTextEntry(NO) {}
};

namespace MTL
{
    class Device;
}

APPLICATIONCORE_API
@interface FIOSView : UIView  <UIKeyInput, UITextInput>
{
@public
	// are we initialized?
	bool bIsInitialized;

//@private
	// keeps track of the number of active touches
	// used to bring up the three finger touch debug console after 3 active touches are registered
	int NumActiveTouches;

	// track the touches by pointer (which will stay the same for a given finger down) - note we don't deref the pointers in this array
	UITouch* AllTouches[10];
	float PreviousForces[10];
	bool HasMoved[10];

	// global metal device
	MTL::Device* MetalDevice;
	id<CAMetalDrawable> PanicDrawable;

	//// KEYBOARD MEMBERS
	
	// whether or not to use the new style virtual keyboard that sends events to the engine instead of using an alert
	bool bIsUsingIntegratedKeyboard;
	bool bSendEscapeOnClose;

	// caches for the TextInput
	NSString* CachedMarkedText;
	
	UIKeyboardType KeyboardType;
	UITextAutocorrectionType AutocorrectionType;
	UITextAutocapitalizationType AutocapitalizationType;
	BOOL bSecureTextEntry;
	
	volatile int32 KeyboardShowCount;
	NSUInteger SupportedInterfaceOrientations;
}

#if WITH_ACCESSIBILITY
/** Repopulate _accessibilityElements when the accessible window's ID has changed. */
-(void)SetAccessibilityWindow:(AccessibleWidgetId)WindowId;
#endif

//// SHARED FUNCTIONALITY
@property (nonatomic) uint SwapCount;
@property (assign, nonatomic) CGSize ViewSize;

-(bool)CreateFramebuffer;
-(void)DestroyFramebuffer;
-(void)UpdateRenderWidth:(uint32)Width andHeight:(uint32)Height;
-(void)CalculateContentScaleFactor:(int32)ScreenWidth ScreenHeight:(int32)ScreenHeight;

- (void)SwapBuffers;

//// METAL FUNCTIONALITY
// Return a drawable object (ie a back buffer texture) for the RHI to render to
- (id<CAMetalDrawable>)MakeDrawable;

//// KEYBOARD FUNCTIONALITY
-(void)InitKeyboard;
-(void)ActivateKeyboard:(bool)bInSendEscapeOnClose;
-(void)ActivateKeyboard:(bool)bInSendEscapeOnClose keyboardConfig:(FKeyboardConfig)KeyboardConfig;
-(void)DeactivateKeyboard;

// callable from outside to fake locations
-(void)HandleTouchAtLoc:(CGPoint)Loc PrevLoc:(CGPoint)PrevLoc TouchIndex:(int)TouchIndex Force:(float)Force Type:(TouchType)Type TouchesArray:(TArray<TouchInput>&)TouchesArray;

#if BUILD_EMBEDDED_APP
// startup UE before we have a view - so that we don't need block on Metal device creation, which can take .5-1.5 seconds!
+(void)StartupEmbeddedUnreal;
#endif

@end


/**
 * A view controller subclass that handles loading our IOS view as well as autorotation
 */
#if PLATFORM_TVOS
#import <GameController/GameController.h>
// if TVOS doesn't use the GCEventViewController, it will background the app when the user presses Menu/Pause
@interface IOSViewController : GCEventViewController
#else
@interface IOSViewController : UIViewController
#endif
{

}
@end
