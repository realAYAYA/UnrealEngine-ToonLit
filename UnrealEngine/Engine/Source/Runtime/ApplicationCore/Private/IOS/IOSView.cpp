// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSView.h"
#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "IOS/IOSPlatformProcess.h"

#import "IOS/IOSAsyncTask.h"
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIGeometry.h>

#include "IOS/IOSCommandLineHelper.h"

#if WITH_ACCESSIBILITY
#include "IOS/Accessibility/IOSAccessibilityCache.h"
#include "IOS/Accessibility/IOSAccessibilityElement.h"
#endif

namespace MTL
{
    class Device;
}

MTL::Device* GMetalDevice = nullptr;

@interface IndexedPosition : UITextPosition {
	NSUInteger _index;
	id <UITextInputDelegate> _inputDelegate;
}
@property (nonatomic) NSUInteger index;
+ (IndexedPosition *)positionWithIndex:(NSUInteger)index;
@end

@interface IndexedRange : UITextRange {
	NSRange _range;
}
@property (nonatomic) NSRange range;
+ (IndexedRange *)rangeWithNSRange:(NSRange)range;

@end


@implementation IndexedPosition
@synthesize index = _index;

+ (IndexedPosition *)positionWithIndex:(NSUInteger)index {
	IndexedPosition *pos = [[IndexedPosition alloc] init];
	pos.index = index;
	return pos;
}

@end

@implementation IndexedRange
@synthesize range = _range;

+ (IndexedRange *)rangeWithNSRange:(NSRange)nsrange {
	if (nsrange.location == NSNotFound)
		return nil;
	IndexedRange *range = [[IndexedRange alloc] init];
	range.range = nsrange;
	return range;
}

- (UITextPosition *)start {
	return [IndexedPosition positionWithIndex:self.range.location];
}

- (UITextPosition *)end {
	return [IndexedPosition positionWithIndex:(self.range.location + self.range.length)];
}

-(BOOL)isEmpty {
	return (self.range.length == 0);
}
@end



@implementation FIOSView

@synthesize keyboardType = KeyboardType;
@synthesize autocorrectionType = AutocorrectionType;
@synthesize autocapitalizationType = AutocapitalizationType;
@synthesize secureTextEntry = bSecureTextEntry;
@synthesize SwapCount, markedTextStyle;


#if BUILD_EMBEDDED_APP

+(void)StartupEmbeddedUnreal
{
	// special initialization code for embedded view
	
	//// LaunchIOS replacement ///
	FIOSCommandLineHelper::InitCommandArgs(TEXT(""));
	
	//#if !PLATFORM_TVOS
	//		// reset badge count on launch
	//		Application.applicationIconBadgeNumber = 0;
	//#endif
	
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];

	[AppDelegate NoUrlCommandLine];
	[AppDelegate StartGameThread];
}

#endif

/**
 * @return The Layer Class for the window
 */
+ (Class)layerClass
{
#if BUILD_EMBEDDED_APP
	SCOPED_BOOT_TIMING("MetalLayer class");
	GMetalDevice = MTLCreateSystemDefaultDevice();
	return [CAMetalLayer class];
#endif
	
	// make sure the project setting has enabled Metal support (per-project user settings in the editor)
	bool bSupportsMetal = false;
	bool bSupportsMetalMRT = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);

	bool bTriedToInit = false;

	// the check for the function pointer itself is to determine if the Metal framework exists, before calling it
	if ((bSupportsMetal || bSupportsMetalMRT) && MTLCreateSystemDefaultDevice != NULL)
	{
		SCOPED_BOOT_TIMING("CreateMetalDevice");
		// if the device is unable to run with Metal (pre-A7), this will return nullptr
		GMetalDevice = (__bridge MTL::Device*)MTLCreateSystemDefaultDevice();

		// just tracking for printout below
		bTriedToInit = true;
	}

#if !UE_BUILD_SHIPPING
	if (GMetalDevice == nullptr)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Not using Metal because: [Project Settings Disabled Metal? %s :: Commandline Forced ES2? %s :: Older OS? %s :: Pre-A7 Device? %s]"),
			bSupportsMetal ? TEXT("No") : TEXT("Yes"),
			TEXT("No"),
			(MTLCreateSystemDefaultDevice == NULL) ? TEXT("Yes") : TEXT("No"),
			bTriedToInit ? TEXT("Yes") : TEXT("Unknown (didn't test)"));
	}
#endif

	if (GMetalDevice != nullptr)
	{
		return [CAMetalLayer class];
	}
	else
	{
		return nil;
	}
}

- (id)initInternal:(CGRect)Frame
{
	SCOPED_BOOT_TIMING("[FIOSView initInternal]");

	CachedMarkedText = nil;

	check(GMetalDevice);
	// if the device is valid, we know Metal is usable (see +layerClass)
	MetalDevice = GMetalDevice;
	if (MetalDevice != nullptr)
	{
		// grab the MetalLayer and typecast it to match what's in layerClass
		CAMetalLayer* MetalLayer = (CAMetalLayer*)self.layer;
		MetalLayer.presentsWithTransaction = NO;
		MetalLayer.drawsAsynchronously = YES;
		
		// set a background color to make sure the layer appears
		CGFloat components[] = { 0.0, 0.0, 0.0, 1 };
		MetalLayer.backgroundColor = CGColorCreate(CGColorSpaceCreateDeviceRGB(), components);
		
		// set the device on the rendering layer and provide a pixel format
		MetalLayer.device = (__bridge id<MTLDevice>)MetalDevice;
		MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		MetalLayer.framebufferOnly = NO;
		
		NSLog(@"::: Created a UIView that will support Metal :::");
	}
	
	
#if !PLATFORM_TVOS
	SupportedInterfaceOrientations = UIInterfaceOrientationMaskAll;
	self.multipleTouchEnabled = YES;
#endif

	SwapCount = 0;
	FMemory::Memzero(AllTouches, sizeof(AllTouches));
	[self setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
	bIsInitialized = false;
	
	[self InitKeyboard];

#if BUILD_EMBEDDED_APP
	//// FAppEntry::PreInit replacement ///
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	
	AppDelegate.RootView = self;
	while (AppDelegate.RootView.superview != nil)
	{
		AppDelegate.RootView = AppDelegate.RootView.superview;
	}
	AppDelegate.IOSView = self;
	
	// initialize the backbuffer of the view (so the RHI can use it)
	[self CreateFramebuffer];
	
#endif
	

	return self;
}

- (id)initWithCoder:(NSCoder*)Decoder
{
	if ((self = [super initWithCoder:Decoder]))
	{
		self = [self initInternal:self.frame];
	}
	return self;
}

- (id)initWithFrame:(CGRect)Frame
{
	if ((self = [super initWithFrame:Frame]))
	{
		self = [self initInternal:self.frame];
	}
	return self;
}

-(void)dealloc
{
	[CachedMarkedText release];
	[markedTextStyle release];
	[super dealloc];
}

- (void)CalculateContentScaleFactor:(int32)ScreenWidth ScreenHeight:(int32)ScreenHeight
{
	const IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	const float mNativeScale = AppDelegate.NativeScale;
	const float RequestedContentScaleFactor = AppDelegate.MobileContentScaleFactor;
	const int32 RequestedResX = AppDelegate.RequestedResX;
	const int32 RequestedResY = AppDelegate.RequestedResY;
		
//	UE_LOG(LogIOS, Log, TEXT("RequestedContentScaleFactor %f to nativeScale which is = (s:%f, ns:%f, csf:%f"), RequestedContentScaleFactor, AppDelegate.ScreenScale, mNativeScale, self.contentScaleFactor);
	
	int32 Width = ScreenWidth;
	int32 Height = ScreenHeight;
	
	self.contentScaleFactor = mNativeScale;
	
	// 0 means to use native size
	if (RequestedContentScaleFactor != 0.0f || RequestedResX > 0 || RequestedResY > 0)
	{
		float AspectRatio = (float)ScreenHeight / (float)ScreenWidth;
		if (RequestedResX > 0)
		{
			// set long side for orientation to requested X
			if (ScreenHeight > ScreenWidth)
			{
				Height = RequestedResX;
				Width = FMath::TruncToInt(Height * AspectRatio + 0.5f);
			}
			else
			{
				Width = RequestedResX;
				Height = FMath::TruncToInt(Width * AspectRatio + 0.5f);
			}
		}
		else if (RequestedResY > 0)
		{
			// set short side for orientation to requested Y
			if (ScreenHeight > ScreenWidth)
			{
				Width = RequestedResY;
				Height = FMath::TruncToInt(Width * AspectRatio + 0.5f);
			}
			else
			{
				Height = RequestedResY;
				Width = FMath::TruncToInt(Height * AspectRatio + 0.5f);
			}
		}
		else
		{
			self.contentScaleFactor = RequestedContentScaleFactor;
		}
	}

	_ViewSize.width = Width;
	_ViewSize.height = Height;
}

- (bool)CreateFramebuffer
{
	if (!bIsInitialized)
	{
		[self CalculateContentScaleFactor:FMath::TruncToInt(self.frame.size.width) ScreenHeight:FMath::TruncToInt(self.frame.size.height)];
		bIsInitialized = true;
	}
	return true;
}

/**
 * If view is resized, update the frame buffer so it is the same size as the display area.
 */
- (void)layoutSubviews
{
#if !PLATFORM_TVOS
    auto orientation = [self.window.windowScene interfaceOrientation];
	FIOSApplication::OrientationChanged(orientation);
#endif
}

-(void)UpdateRenderWidth:(uint32)Width andHeight:(uint32)Height
{
	if (MetalDevice != nullptr)
	{
		// grab the MetalLayer and typecast it to match what's in layerClass, then set the new size
		CAMetalLayer* MetalLayer = (CAMetalLayer*)self.layer;
		MetalLayer.drawableSize = CGSizeMake(Width, Height);
	}
}

- (id<CAMetalDrawable>)MakeDrawable
{
    return [(CAMetalLayer*)self.layer nextDrawable];
}

- (void)DestroyFramebuffer
{
	if (bIsInitialized)
	{
		// we are ready to be re-initialized
		bIsInitialized = false;
	}
}

- (void)SwapBuffers
{
	SwapCount++;
}

#if WITH_ACCESSIBILITY

-(void)SetAccessibilityWindow:(AccessibleWidgetId)WindowId
{
	[FIOSAccessibilityCache AccessibilityElementCache].RootWindowId = WindowId;
 	if (WindowId != IAccessibleWidget::InvalidAccessibleWidgetId)
	{
		FIOSAccessibilityLeaf* Window = [[FIOSAccessibilityCache AccessibilityElementCache] GetAccessibilityElement: WindowId];
		// We go ahead and assume the window will have children and add the FIOSAccessibilityContainer
		// for the Window to enforce accessibility hierarchy
self.accessibilityElements = @[Window.accessibilityContainer];
	}
	else
	{
		self.accessibilityElements = Nil;
	}
}

-(BOOL)isAccessibilityElement
{
	return NO;
}

#endif

/**
 * Returns the unique ID for the given touch
 */
-(int32) GetTouchIndex:(UITouch*)Touch
{
	// look for existing touch
	for (int Index = 0; Index < UE_ARRAY_COUNT(AllTouches); Index++)
	{
		if (AllTouches[Index] == Touch)
		{
			return Index;
		}
	}
	
	// if we get here, it's a new touch, find a slot
	for (int Index = 0; Index < UE_ARRAY_COUNT(AllTouches); Index++)
	{
		if (AllTouches[Index] == nil)
		{
			AllTouches[Index] = Touch;
			return Index;
		}
	}
	
	// if we get here, that means we are trying to use more than 5 touches at once, which is an error
	return -1;
}


-(void)HandleTouchAtLoc:(CGPoint)Loc PrevLoc:(CGPoint)PrevLoc TouchIndex:(int)TouchIndex Force:(float)Force Type:(TouchType)Type TouchesArray:(TArray<TouchInput>&)TouchesArray
{
	// init some things on begin
	if (Type == TouchBegan)
	{
		PreviousForces[TouchIndex] = -1.0;
		HasMoved[TouchIndex] = 0;
		
		NumActiveTouches++;
	}
	
	float PreviousForce = PreviousForces[TouchIndex];
	
	// make a new touch event struct
	TouchInput TouchMessage;
	TouchMessage.Handle = TouchIndex;
	TouchMessage.Type = Type;
	TouchMessage.Position = FVector2D(FMath::Min<double>(_ViewSize.width - 1, Loc.x), FMath::Min<double>(_ViewSize.height - 1, Loc.y));
	TouchMessage.LastPosition = FVector2D(FMath::Min<double>(_ViewSize.width - 1, PrevLoc.x), FMath::Min<double>(_ViewSize.height - 1, PrevLoc.y));
	TouchMessage.Force = Type != TouchEnded ? Force : 0.0f;
	
	// skip moves that didn't actually move - this will help input handling to skip over the first
	// move since it is likely a big pop from the TouchBegan location (iOS filters out small movements
	// on first press)
	if (Type != TouchMoved || (PrevLoc.x != Loc.x || PrevLoc.y != Loc.y))
	{
		// track first move event, for helping with "pop" on the filtered small movements
		if (HasMoved[TouchIndex] == 0 && Type == TouchMoved)
		{
			TouchInput FirstMoveMessage = TouchMessage;
			FirstMoveMessage.Type = FirstMove;
			HasMoved[TouchIndex] = 1;
			
			TouchesArray.Add(FirstMoveMessage);
		}
		
		TouchesArray.Add(TouchMessage);
	}
	
	// if the force changed, send an event!
	if (PreviousForce != Force)
	{
		TouchInput ForceMessage = TouchMessage;
		ForceMessage.Type = ForceChanged;
		PreviousForces[TouchIndex] = Force;
		
		TouchesArray.Add(ForceMessage);
	}
	
	// clear out the touch when it ends
	if (Type == TouchEnded)
	{
		AllTouches[TouchIndex] = nil;
		NumActiveTouches--;
	}

#if !UE_BUILD_SHIPPING
#if WITH_IOS_SIMULATOR
	// use 2 on the simulator so that Option-Click will bring up console (option-click is for doing pinch gestures, which we don't care about, atm)
	if( NumActiveTouches >= 2 )
#else
		// If there are 3 active touches, bring up the console
		if( NumActiveTouches >= 4 )
#endif
		{
			bool bShowConsole = true;
			GConfig->GetBool(TEXT("/Script/Engine.InputSettings"), TEXT("bShowConsoleOnFourFingerTap"), bShowConsole, GInputIni);
			
			if (bShowConsole)
			{
				// disable the integrated keyboard when launching the console
				/*			if (bIsUsingIntegratedKeyboard)
				 {
				 // press the console key twice to get the big one up
				 // @todo keyboard: Find a direct way to bering this up (it can get into a bad state where two presses won't do it correctly)
				 // and also the ` key could be changed via .ini
				 FIOSInputInterface::QueueKeyInput('`', '`');
				 FIOSInputInterface::QueueKeyInput('`', '`');
				 
				 [self ActivateKeyboard:true];
				 }
				 else*/
				{
					// Route the command to the main iOS thread (all UI must go to the main thread)
					[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowConsole) withObject:nil waitUntilDone:NO];
				}
			}
		}
#endif
}

/**
 * Pass touch events to the input queue for slate to pull off of, and trigger the debug console.
 *
 * @param View The view the event happened in
 * @param Touches Set of touch events from the OS
 */
-(void) HandleTouches:(NSSet*)Touches ofType:(TouchType)Type
{
	TArray<TouchInput> TouchesArray;
	for (UITouch* Touch in Touches)
	{
		// ignore mouse-produced touches, these will be handled by FIOSInputInterface
        if ( Touch.type == UITouchTypeIndirectPointer ) // Requires UIApplicationSupportsIndirectInputEvents:true in plist
        {
            continue;
        }
		// get info from the touch
		CGPoint Loc = [Touch locationInView:self];
		CGPoint PrevLoc = [Touch previousLocationInView:self];
		
		// View may have been modified via Cvars ("r.mobile.DesiredResX/Y" or CommandLine "mcfs, mobileresx/y"
		CGPoint ViewSizeModifier = CGPointMake(_ViewSize.width/self.frame.size.width, _ViewSize.height/self.frame.size.height);
		Loc.x *= ViewSizeModifier.x;
		Loc.y *= ViewSizeModifier.y;
		PrevLoc.x *= ViewSizeModifier.x;
		PrevLoc.y *= ViewSizeModifier.y;
			
		// convert Touch pointer to a unique 0 based index
		int32 TouchIndex = [self GetTouchIndex:Touch];
		if (TouchIndex < 0)
		{
			continue;
		}

		double Force = Touch.force;
		
		// map larger values to 1..10, so 10 is a max across platforms
		if (Force > 1.0)
		{
			Force = 10.0 * Force / Touch.maximumPossibleForce;
		}
		
		// Handle devices without force touch
		if ((Type == TouchBegan || Type == TouchMoved) && Force == 0.0)
		{
			Force = 1.0;
		}

		[self  HandleTouchAtLoc:Loc PrevLoc:PrevLoc TouchIndex:TouchIndex Force:(float)Force Type:Type TouchesArray:TouchesArray];
	}

	FIOSInputInterface::QueueTouchInput(TouchesArray);
}

/**
 * Handle the various touch types from the OS
 *
 * @param touches Array of touch events
 * @param event Event information
 */
- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event 
{
	[self HandleTouches:touches ofType:TouchBegan];
	}

- (void) touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event
{
	[self HandleTouches:touches ofType:TouchMoved];
}

- (void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event
{
	[self HandleTouches:touches ofType:TouchEnded];
}

- (void) touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event
{
	[self HandleTouches:touches ofType:TouchEnded];
}












#pragma mark Keyboard


-(BOOL)canBecomeFirstResponder
{
	return YES;
}

- (BOOL)hasText
{
	return YES;
}

- (void)insertText:(NSString *)theText
{
	if (nil != CachedMarkedText) {
		[CachedMarkedText release];
		CachedMarkedText = nil;
	}

	// insert text one key at a time, as chars, not keydowns
	for (int32 CharIndex = 0; CharIndex < [theText length]; CharIndex++)
	{
		int32 Char = [theText characterAtIndex:CharIndex];

		// FPlatformMisc::LowLevelOutputDebugStringf(TEXT("sending key '%c' to game\n"), Char);

		if (Char == '\n')
		{
			// send the enter keypress
			FIOSInputInterface::QueueKeyInput(KEYCODE_ENTER, Char);
			
			// hide the keyboard
			[self resignFirstResponder];
		}
		else
		{
			FIOSInputInterface::QueueKeyInput(Char, Char);
		}
	}
}

- (void)deleteBackward
{
	if (nil != CachedMarkedText) {
		[CachedMarkedText release];
		CachedMarkedText = nil;
	}
	FIOSInputInterface::QueueKeyInput(KEYCODE_BACKSPACE, '\b');
}

-(void)ActivateKeyboard:(bool)bInSendEscapeOnClose
{
	FKeyboardConfig DefaultConfig;
	[self ActivateKeyboard:bInSendEscapeOnClose keyboardConfig:DefaultConfig];
}

-(void)ActivateKeyboard:(bool)bInSendEscapeOnClose keyboardConfig:(FKeyboardConfig)KeyboardConfig
{
	FPlatformAtomics::InterlockedIncrement(&KeyboardShowCount);
	
	dispatch_async(dispatch_get_main_queue(),^ {
		volatile int32 ShowCount = KeyboardShowCount;
		if (ShowCount == 1)
		{
			self.keyboardType = KeyboardConfig.KeyboardType;
			self.autocorrectionType = KeyboardConfig.AutocorrectionType;
			self.autocapitalizationType = KeyboardConfig.AutocapitalizationType;
			self.secureTextEntry = KeyboardConfig.bSecureTextEntry;
		
			// Remember the setting
			bSendEscapeOnClose = bInSendEscapeOnClose;
		
			// Dismiss the existing keyboard, if one exists, so the style can be overridden.
			[self endEditing:YES];
			[self becomeFirstResponder];
            
            FIOSInputInterface::SetKeyboardInhibited(true);
		}
		
		FPlatformAtomics::InterlockedDecrement(&KeyboardShowCount);
	});
}

-(void)DeactivateKeyboard
{
	dispatch_async(dispatch_get_main_queue(),^ {
		volatile int32 ShowCount = KeyboardShowCount;
		if (ShowCount == 0)
		{
			// Wait briefly, in case a keyboard activation is triggered.
			FPlatformProcess::Sleep(0.1F);
			
			ShowCount = KeyboardShowCount;
			if (ShowCount == 0)
			{
				// Dismiss the existing keyboard, if one exists.
				[self endEditing:YES];
                FIOSInputInterface::SetKeyboardInhibited(false);
			}
		}
	});
}

-(BOOL)becomeFirstResponder
{
	volatile int32 ShowCount = KeyboardShowCount;
	if (ShowCount >= 1)
	{
		return [super becomeFirstResponder];
	}
	else
	{
		return NO;
	}
}

-(BOOL)resignFirstResponder
{
	if (bSendEscapeOnClose)
	{
		// tell the console to close itself
		FIOSInputInterface::QueueKeyInput(KEYCODE_ESCAPE, 0);
	}
	
	return [super resignFirstResponder];
}


// @todo keyboard: This is a helper define to show functions that _may_ need to be implemented as we go forward with keyboard support
// for now, the very basics work, but most likely at some point for optimal functionality, we'll want to know the actual string
// in the box, but that needs to come from Slate, and we currently don't have a way to get it
#define REPORT_EVENT UE_LOG(LogIOS, Display, TEXT("Got a keyboard call, line %d"), __LINE__);


- (NSString *)textInRange:(UITextRange *)range
{
	if (nil != CachedMarkedText) {
		return CachedMarkedText;
	}
	return nil;
	//IndexedRange *r = (IndexedRange *)range;
	//return ([textStore substringWithRange:r.range]);
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text
{
	REPORT_EVENT;
	return;
//	IndexedRange *r = (IndexedRange *)range;
//	[textStore replaceCharactersInRange:r.range withString:text];
}

- (UITextRange *)selectedTextRange
{
	// @todo keyboard: This is called
	return [IndexedRange rangeWithNSRange:NSMakeRange(0,0)];//self.textView.selectedTextRange];
}


- (void)setSelectedTextRange:(UITextRange *)range
{
	REPORT_EVENT;
	//IndexedRange *indexedRange = (IndexedRange *)range;
	//self.textView.selectedTextRange = indexedRange.range;
}


- (UITextRange *)markedTextRange
{
	if (nil != CachedMarkedText) {
		return[[[UITextRange alloc] init] autorelease];
	}
	return nil; // Nil if no marked text.
}


- (void)setMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange
{
	if (markedText == CachedMarkedText) {
		return;
	}
	if (nil != CachedMarkedText) {
		[CachedMarkedText release];
	}
	CachedMarkedText = markedText;
	[CachedMarkedText retain];
	//NSLog(@"setting marked text to %@", markedText);
}


- (void)unmarkText
{
	if (CachedMarkedText != nil)
	{
		[self insertText:CachedMarkedText];
		[CachedMarkedText release];
		CachedMarkedText = nil;
	}
}


- (UITextPosition *)beginningOfDocument
{
	// @todo keyboard: This is called
	return [IndexedPosition positionWithIndex:0];
}


- (UITextPosition *)endOfDocument
{
	REPORT_EVENT;
	return [IndexedPosition positionWithIndex:0];
}


- (UITextRange *)textRangeFromPosition:(UITextPosition *)fromPosition toPosition:(UITextPosition *)toPosition
{
	// @todo keyboard: This is called
	// Generate IndexedPosition instances that wrap the to and from ranges.
	IndexedPosition *fromIndexedPosition = (IndexedPosition *)fromPosition;
	IndexedPosition *toIndexedPosition = (IndexedPosition *)toPosition;
	NSRange range = NSMakeRange(MIN(fromIndexedPosition.index, toIndexedPosition.index), ABS(toIndexedPosition.index - fromIndexedPosition.index));
 
	return [IndexedRange rangeWithNSRange:range];
}


- (UITextPosition *)positionFromPosition:(UITextPosition *)position offset:(NSInteger)offset
{
	// @todo keyboard: This is called
	return nil;
}


- (UITextPosition *)positionFromPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction offset:(NSInteger)offset
{
	REPORT_EVENT;
	return nil;
}


- (NSComparisonResult)comparePosition:(UITextPosition *)position toPosition:(UITextPosition *)other
{
	// @todo keyboard: This is called
	return NSOrderedSame;
}


- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition
{
	REPORT_EVENT;
	IndexedPosition *fromIndexedPosition = (IndexedPosition *)from;
	IndexedPosition *toIndexedPosition = (IndexedPosition *)toPosition;
	return (toIndexedPosition.index - fromIndexedPosition.index);
}



- (UITextPosition *)positionWithinRange:(UITextRange *)range farthestInDirection:(UITextLayoutDirection)direction
{
	REPORT_EVENT;
	return nil;
}


- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction
{
	REPORT_EVENT;
	return nil;
}

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
	REPORT_EVENT;
	// assume left to right for now
	return NSWritingDirectionLeftToRight;
}


- (void)setBaseWritingDirection:(NSWritingDirection)writingDirection forRange:(UITextRange *)range
{
	// @todo keyboard: This is called
}



- (CGRect)firstRectForRange:(UITextRange *)range
{
	REPORT_EVENT;
	return CGRectMake(0,0,0,0);
}


- (CGRect)caretRectForPosition:(UITextPosition *)position
{
	// @todo keyboard: This is called
	return CGRectMake(0,0,0,0);
}


- (UITextPosition *)closestPositionToPoint:(CGPoint)point
{
	REPORT_EVENT;
	return nil;
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange *)range
{
	REPORT_EVENT;
	return nil;
}

- (UITextRange *)characterRangeAtPoint:(CGPoint)point
{
	REPORT_EVENT;
	return nil;
}

- (NSArray *)selectionRectsForRange:(UITextRange *)range
{
	REPORT_EVENT;
	return nil;
}


- (NSDictionary *)textStylingAtPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
	// @todo keyboard: This is called
	return @{ };
}


- (void) setInputDelegate: (id <UITextInputDelegate>) delegate
{
	// @todo keyboard: This is called
}

- (id <UITextInputTokenizer>) tokenizer
{
	// @todo keyboard: This is called
	return nil;
}

- (id <UITextInputDelegate>) inputDelegate
{
	// @todo keyboard: This is called
	return nil;
}





#if !PLATFORM_TVOS
- (void)keyboardWasShown:(NSNotification*)aNotification
{
	// send a callback to let the game know where to sldie the textbox up above
	NSDictionary* info = [aNotification userInfo];
	CGRect Frame = [[info objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
	
	FPlatformRect ScreenRect;
	ScreenRect.Top = FMath::TruncToInt(Frame.origin.y);
	ScreenRect.Bottom = FMath::TruncToInt(Frame.origin.y + Frame.size.height);
	ScreenRect.Left = FMath::TruncToInt(Frame.origin.x);
	ScreenRect.Right = FMath::TruncToInt(Frame.origin.x + Frame.size.width);
	
	[FIOSAsyncTask CreateTaskWithBlock:^bool(void){
		[IOSAppDelegate GetDelegate].IOSApplication->OnVirtualKeyboardShown().Broadcast(ScreenRect);
		return true;
	 }];
}

// Called when the UIKeyboardWillHideNotification is sent
- (void)keyboardWillBeHidden:(NSNotification*)aNotification
{
	[FIOSAsyncTask CreateTaskWithBlock:^bool(void){
		[IOSAppDelegate GetDelegate].IOSApplication->OnVirtualKeyboardHidden().Broadcast();
		return true;
	 }];
}
#endif

- (void)InitKeyboard
{
#if !PLATFORM_TVOS
	KeyboardShowCount = 0;
	
	bool bUseIntegratedKeyboard = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bUseIntegratedKeyboard"), bUseIntegratedKeyboard, GEngineIni);
	
	// get notifications when the keyboard is in view
	bIsUsingIntegratedKeyboard = FParse::Param(FCommandLine::Get(), TEXT("NewKeyboard")) || bUseIntegratedKeyboard;
	if (bIsUsingIntegratedKeyboard)
	{
		[[NSNotificationCenter defaultCenter] addObserver:self
												 selector:@selector(keyboardWasShown:)
													 name:UIKeyboardDidShowNotification object:nil];
		
		[[NSNotificationCenter defaultCenter] addObserver:self
												 selector:@selector(keyboardWillBeHidden:)
													 name:UIKeyboardWillHideNotification object:nil];
		
	}
#endif
}


@end


#pragma mark IOSViewController

@implementation IOSViewController

/**
 * The ViewController was created, so now we need to create our view to be controlled
 */
- (void) loadView
{
#if PLATFORM_VISIONOS
	CGRect Frame = CGRectMake(0, 0, 1000, 1000);
#else
	// get the landcape size of the screen
	CGRect Frame = [[UIScreen mainScreen] bounds];
	if (![IOSAppDelegate GetDelegate].bDeviceInPortraitMode)
	{
		Swap(Frame.size.width, Frame.size.height);
	}
#endif

	self.view = [[UIView alloc] initWithFrame:Frame];

	// settings copied from InterfaceBuilder
	self.edgesForExtendedLayout = UIRectEdgeNone;

	self.view.clearsContextBeforeDrawing = NO;
#if !PLATFORM_TVOS
	self.view.multipleTouchEnabled = NO;
#endif
}

/**
 * View was unloaded from us
 */ 
- (void) viewDidUnload
{
	UE_LOG(LogIOS, Log, TEXT("IOSViewController unloaded the view. This is unexpected, tell Josh Adams"));
	[super viewDidUnload];
}

#if !PLATFORM_TVOS
/**
 * Tell the OS about the default supported orientations
 */
- (NSUInteger)supportedInterfaceOrientations
{
	const IOSAppDelegate *AppDelegate = [IOSAppDelegate GetDelegate];
	const FIOSView *View = [AppDelegate IOSView];
	if (View != nil)
	{
		// if a Blueprint has changed the default rotation constraints, honour that change
		if (View->SupportedInterfaceOrientations != UIInterfaceOrientationMaskAll)
		{
			return View->SupportedInterfaceOrientations;
		}
	}

	// View either not yet created or Blueprint is not overriding the default, so use what the Window has set
	UIApplication *app = [UIApplication sharedApplication];
	return [AppDelegate application:app supportedInterfaceOrientationsForWindow:[AppDelegate window]];
}
#endif

/**
 * Tell the OS that our view controller can auto-rotate between supported orientations
 */
- (BOOL)shouldAutorotate
{
	return YES;
}

/**
 * Tell the OS to hide the status bar (iOS 7 method for hiding)
 */
- (BOOL)prefersStatusBarHidden
{
	return YES;
}

- (BOOL)prefersPointerLocked
{
    UE_LOG(LogIOS, Log, TEXT("IOSViewController prefersPointerLocked"));
    return YES;
}

/**
 * Tell the OS to hide the home bar
 */
- (BOOL)prefersHomeIndicatorAutoHidden
{
	return YES;
}

/*
* Set the preferred landscape orientation 
*/
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation
{
	FString PreferredLandscapeOrientation = "";
	bool bSupportsLandscapeLeft = false;
	bool bSupportsLandscapeRight = false;

	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsLandscapeLeftOrientation"), bSupportsLandscapeLeft, GEngineIni);
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsLandscapeRightOrientation"), bSupportsLandscapeRight, GEngineIni);
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("PreferredLandscapeOrientation"), PreferredLandscapeOrientation, GEngineIni);

	if(bSupportsLandscapeLeft && bSupportsLandscapeRight)
	{
		if (PreferredLandscapeOrientation.Equals("LandscapeRight"))
		{
			return UIInterfaceOrientationLandscapeRight;
		}
		return UIInterfaceOrientationLandscapeLeft;
	}

	return UIInterfaceOrientationPortrait;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
	return UIRectEdgeBottom;
}





@end
