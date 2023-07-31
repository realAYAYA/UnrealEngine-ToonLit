//  Copyright Epic Games, Inc. All Rights Reserved.

#import <CoreVideo/CoreVideo.h>
#import <CoreServices/CoreServices.h>

#import "VCAM-Swift.h"
#import "ObjCUtility.h"

@implementation ObjCUtility

+ (NSDate *) buildDate {
    
    NSString *compileDateTime = [NSString stringWithFormat:@"%s %s", __DATE__, __TIME__];
    
    NSDateFormatter *df = [NSDateFormatter new];
    [df setDateFormat:@"MMM d yyyy H:mm:ss"];
    [df setLocale:[NSLocale localeWithLocaleIdentifier:@"en_US"]];

    return [df dateFromString:compileDateTime];
}

@end
