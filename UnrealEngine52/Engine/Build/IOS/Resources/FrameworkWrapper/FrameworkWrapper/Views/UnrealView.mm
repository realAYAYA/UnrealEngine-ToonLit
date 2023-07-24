// Copyright Epic Games, Inc. All Rights Reserved.
//
//  UnrealWrapper.m
//  FrameworkWrapper
//
//  Created by Andrew Grant on 9/4/18.
//


#import "UnrealView.h"

#if CAN_USE_UE && __cplusplus
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Weverything"

    #include "PreIOSEmbeddedView.h"
    #include "IOS/IOSView.h"
    #include "Misc/EmbeddedCommunication.h"

    #pragma clang diagnostic pop
#endif // CAN_USE_UE


@implementation UnrealContainerView

-(void) awakeFromNib
{
	[super awakeFromNib];
	[self createUnrealView];
}

static UIView* sharedUnrealView = nil;
static UnrealContainerView* DelayedCreateContainer = nil;

-(BOOL) IsUnrealUsable
{
#if CAN_USE_UE
	// check memory amount, and running OS version, etc
	return YES;
#else
	return NO;
#endif

}

+(void)DelayedCreateView
{
	static bool bHasBeenCreated = false;
	if (bHasBeenCreated)
	{
		return;
	}
	bHasBeenCreated = true;

    NW_LOG(@"DelayedCreateView start");

	// @todo these views will never be released
#if CAN_USE_UE
	if ([DelayedCreateContainer IsUnrealUsable])
	{
		sharedUnrealView = [[FIOSView alloc] initWithFrame:DelayedCreateContainer.bounds];
		// start hidden until we are needed
		sharedUnrealView.hidden = YES;
	}
	else
#endif
	{
		// make a dummy UE4 view
		UIImageView* ImageView = [[UIImageView alloc] initWithFrame:DelayedCreateContainer.bounds];
		ImageView.image = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Res/Logo" ofType:@"jpg"]];
		ImageView.contentMode = UIViewContentModeScaleAspectFit;
		ImageView.backgroundColor = [UIColor blackColor];
		
		sharedUnrealView = ImageView;
	}
	
	// makes the unreal view resize to fit parent
	sharedUnrealView.autoresizingMask = (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
	
	// now continue where we left off before
	[DelayedCreateContainer createUnrealView];
	DelayedCreateContainer = nil;

    NW_LOG(@"DelayedCreateView end");
}

-(void) createUnrealView
{
    if (sharedUnrealView == nil)
	{
#if CAN_USE_UE
		
//       [self SetupEmbeddedToNativeCallback];

		DelayedCreateContainer = self;
		
		// kick off unreal boot, but don't create the IOSView yet
        [FIOSView StartupEmbeddedUnreal];
#endif
		
		return;
    }
	else
	{
        [sharedUnrealView removeFromSuperview];
    }
    
	_unrealView = sharedUnrealView;
	_unrealView.frame = self.bounds;
	[self addSubview:self.unrealView];
	[self layoutSubviews];
}


#if CAN_USE_UE
+(id)UE4ToGeneric:(const FEmbeddedCommunicationMap&)UE4Map
{
	// look for a special single entry { "json", "xxxx" } map
	if (UE4Map.Num() == 1 && UE4Map.Find(TEXT("json")) != nullptr)
	{
		NSString* JSONString = [NSString stringWithFString:UE4Map.FindRef(TEXT("json"))];
		NSError* JSONError;
		id JSONObject = [NSJSONSerialization JSONObjectWithData:[JSONString dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingAllowFragments error:&JSONError];
		if (JSONObject != nil)
		{
			return JSONObject;
		}
		else
		{
            NW_LOG(@"Got JSON decoding error: %@", [JSONError localizedDescription]);
		}
	}

	// if we got here, we couldn't convert to generic json object, so just convert TMap to NSDictionary
	NSMutableDictionary* ReturnValues = [NSMutableDictionary dictionaryWithCapacity:UE4Map.Num()];
	for (auto Pair : UE4Map)
	{
		[ReturnValues setValue:[NSString stringWithFString:Pair.Value] forKey:[NSString stringWithFString:Pair.Key]];
	}
	
	return ReturnValues;
}
#endif

+(void)EmbeddedCallForSubsystem:(NSString* _Nullable)InSubsystem WithCommand:(NSString* _Nullable)Command Params:(NSDictionary<NSString*, NSString*>* _Nullable)Params Priority:(int)Priority CompletionHandler:(EmbeddedParamsCompletionType)InHandler
{
	// we have to copy the handler because it will go out of scope after this call (if it's a stack block) and can't be retained or anything
	EmbeddedParamsCompletionType Handler = [InHandler copy];
	
#if CAN_USE_UE
	FEmbeddedCallParamsHelper CallHelper;
	__block FEmbeddedCallParamsHelper* BlockHelper = &CallHelper;
	CallHelper.Command = Command;

    NW_LOG_DEBUG(@"NativeToEmbeddedCall: Subsystem: %@, Command: %@, Params:%@", InSubsystem, Command, Params);

	FString Subsystem = InSubsystem;
	FName SubsystemName(*Subsystem);
	FName CommandName = *(FString::Printf(TEXT("%s_%s"), *Subsystem, *CallHelper.Command));
	
	// wake up UE
	FEmbeddedCommunication::KeepAwake(CommandName, false);

	// convert Obj-C types to UE4 types
	if (Params != nil)
	{
        if (![Params isKindOfClass:[NSDictionary class]])
        {
            NW_LOG_DEBUG(@"WARNING! Command %@:%@ had params that was not a dictionary!", InSubsystem, Command);
        }
        else
        {
            [Params enumerateKeysAndObjectsUsingBlock:^(NSString* Key, id Value, BOOL* Stop)
             {
                 // convert complex types into JSON representations, not plain ToString type
                 if ([Value isKindOfClass:[NSDictionary class]] || [Value isKindOfClass:[NSArray class]])
                 {
                     NSData* JSONData = [NSJSONSerialization dataWithJSONObject:Value options:NSJSONWritingPrettyPrinted error:nil];
                     NSString* JSONString = [[NSString alloc] initWithData:JSONData encoding:NSUTF8StringEncoding];
                     BlockHelper->Parameters.Add(Key, JSONString);
                 }
                 else
                 {
                     BlockHelper->Parameters.Add(Key, [NSString stringWithFormat:@"%@",Value]);
                 }
             }];
        }
	}

	// this struct will peform Obj-C reference counting on an object from a C++ object that the UE4 lambdas will copy around
	// this makes sure that the Handler is referenced until all copies of CallHelper go away
	struct FObjCWrapper
	{
		FObjCWrapper(id InObj)
		: Obj(InObj)
		{
			[Obj retain];
		}
		FObjCWrapper(const FObjCWrapper& Other)
		{
			Obj = Other.Obj;
			[Obj retain];
		}
		~FObjCWrapper()
		{
			[Obj release];
		}
		id Obj;
	};

	FObjCWrapper ObjCWrapper(Handler);
	
	// wrapper owns it now
	[Handler release];

	// set up the UE4 completion handler to wrap the obj-c handler
	CallHelper.OnCompleteDelegate = [self, Handler, ObjCWrapper, CommandName](const FEmbeddedCommunicationMap& InReturnValues, FString InError)
	{

		// we can let UE4 go to sleep now
		FEmbeddedCommunication::AllowSleep(CommandName);

		// call the obj-c block after converting back to Obj-C types
		if (Handler != nil)
		{
			// convert from ue4 land
			id GenericResults = [UnrealContainerView UE4ToGeneric:InReturnValues];
			NSString* ErrorString = InError.Len() ? [NSString stringWithFString:InError] : nil;

			// and finally call it on IOS thread
			dispatch_async(dispatch_get_main_queue(), ^
			{
				// pass back the results strings, and an error if there was one
				Handler(GenericResults, ErrorString);
	
			});
		}
	};

	// now that we have set up the marshalling, call the UE4 delegate on the GameThread, and the completion handler will be called later
	FEmbeddedCommunication::RunOnGameThread(Priority, [SubsystemName, CallHelper]()
	{
		if (FEmbeddedDelegates::GetNativeToEmbeddedParamsDelegateForSubsystem(SubsystemName).IsBound())
		{
			FEmbeddedDelegates::GetNativeToEmbeddedParamsDelegateForSubsystem(SubsystemName).Broadcast(CallHelper);
		}
		else
		{
			// if no one is listening, just call the handler with an error
			CallHelper.OnCompleteDelegate({}, TEXT("No one was listening for subsystem ") + SubsystemName.ToString());
		}
	 });

	
#else
	if (Handler != nil)
	{
		// if no UE4, delay call the handler (so caller doesn't need to handle callback happening before this returns)
		dispatch_async(dispatch_get_main_queue(), ^
		   {
			   // pass back the map of strings, and an error if there was one
			   Handler(nil, @"UE4 is not running");
			   
			   // release from the [Handler copy] above
//               [Handler release];
		   });
	}
#endif
}

+(void)JSBridgeCallByName:(NSString*)UObjectName MethodName:(NSString*)MethodName MethodParamsJSON:(NSString*)MethodParamsJSON Priority:(int)Priority CompletionHandler:(EmbeddedParamsCompletionType)CompletionHandler
{
	NSString* Subsystem = @"browserProxy";
	NSString* Command = @"handlejs";
	NSString* Script = [NSString stringWithFormat:@"ExecuteUObjectMethod/%@/00000000000000000000000000000000/%@/{\"params\":%@}", UObjectName, MethodName, MethodParamsJSON];
	NSDictionary* Params = @{@"script" : Script};
	[UnrealContainerView EmbeddedCallForSubsystem:Subsystem WithCommand:Command Params:Params Priority:Priority CompletionHandler:CompletionHandler];
}

+(void)SetEmbeddedObject:(void* _Nullable)Object ForName:(NSString* _Nonnull)Name
{
#if CAN_USE_UE
	FEmbeddedDelegates::SetNamedObject(Name, Object);
#endif
}

+(void)HandleTouchMessage:(NSDictionary*)Message
{
#if CAN_USE_UE

	dispatch_async(dispatch_get_main_queue(), ^
	   {
		   NSString* Command = [Message objectForKey:@"command"];
		   NSDictionary* Args = [Message objectForKey:@"args"];
		   CGPoint Loc;
		   int SourceWidth = [[Args objectForKey:@"width"] intValue];
		   int SourceHeight = [[Args objectForKey:@"height"] intValue];
		   // convert from web to unreal coords
		   Loc.x = [[Args objectForKey:@"x"] intValue] * (float)sharedUnrealView.frame.size.width / (float)SourceWidth;
		   Loc.y = [[Args objectForKey:@"y"] intValue] * (float)sharedUnrealView.frame.size.height / (float)SourceHeight;
		   CGPoint PrevLoc = Loc;
		   int TouchIndex = [[Args objectForKey:@"finger"] intValue];
		   float Force = 1.0;

		   TouchType Type = TouchMoved;
		   if ([Command caseInsensitiveCompare:@"touchstart"] == NSOrderedSame)
		   {
			   Type = TouchBegan;
		   }
		   else if ([Command caseInsensitiveCompare:@"touchend"] == NSOrderedSame)
		   {
			   Type = TouchEnded;
		   }

		   TArray<TouchInput> TouchesArray;

		   [(FIOSView*)sharedUnrealView HandleTouchAtLoc:Loc PrevLoc:PrevLoc TouchIndex:TouchIndex Force:Force Type:Type TouchesArray:TouchesArray];

		   FIOSInputInterface::QueueTouchInput(TouchesArray);
			
	   });
#endif
}

+(void)WakeUpUnreal
{
#if CAN_USE_UE
	FEmbeddedCommunication::KeepAwake(TEXT("Native"), true);
#endif

	// we can't be hidden if we need to render!
	sharedUnrealView.hidden = NO;
}

+(void)AllowUnrealToSleep
{
#if CAN_USE_UE
	FEmbeddedCommunication::AllowSleep(TEXT("Native"));
#endif

	sharedUnrealView.hidden = YES;
}

+(void)ResizeUnrealView:(CGSize)Size
{
	sharedUnrealView.autoresizingMask = UIViewAutoresizingNone;
	sharedUnrealView.frame = CGRectMake(sharedUnrealView.frame.origin.x, sharedUnrealView.frame.origin.y, Size.width, Size.height);
	[sharedUnrealView layoutSubviews];

	// assume we want to be shown
	sharedUnrealView.hidden = NO;
}

+(void)PositionUnrealView:(CGPoint)Location
{
	sharedUnrealView.frame = CGRectMake(Location.x, Location.y, sharedUnrealView.frame.size.width, sharedUnrealView.frame.size.height);

	// assume we want to be shown
	sharedUnrealView.hidden = NO;
}

+(void)ResetUnrealViewToFullScreen
{
	sharedUnrealView.autoresizingMask = (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
	sharedUnrealView.frame = sharedUnrealView.superview.bounds;
	[sharedUnrealView layoutSubviews];

	// assume we want to be shown
	sharedUnrealView.hidden = NO;
}

-(void)TakeOwnershipOfUnrealView
{
	if (_unrealView.superview != self)
	{
		[_unrealView removeFromSuperview];
		
		_unrealView.frame = self.bounds;
		[self addSubview:_unrealView];
	}
}

+(void)EnsureVisible
{
	sharedUnrealView.hidden = NO;
}

float GSavedScaleFactor = 0;
+(void)GoFullResolution
{
	GSavedScaleFactor = sharedUnrealView.contentScaleFactor;
	sharedUnrealView.contentScaleFactor = sharedUnrealView.window.screen.nativeScale;
	[sharedUnrealView layoutSubviews];

	// assume we want to be shown
	sharedUnrealView.hidden = NO;
}

+(void)RestoreResolution
{
	if (GSavedScaleFactor != 0.0)
	{
		sharedUnrealView.contentScaleFactor = GSavedScaleFactor;
		[sharedUnrealView layoutSubviews];
	}
}

@end
