// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>

@interface IOSLaunchDaemonView : UIView
{
@public
    UIActivityIndicatorView*    pSpinner;
	//NSTextStorage*				pTextStorage;
}
@end


@interface IOSLaunchDaemonViewController : UIViewController
{
}
@end