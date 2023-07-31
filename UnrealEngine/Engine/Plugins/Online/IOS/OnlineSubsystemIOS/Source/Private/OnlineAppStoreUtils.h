// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"

/**
 * Delegate fires when a query for the app bundle id metadata completes
 *
 * @param ResponseDict dictionary containing the metadata response
 */
DECLARE_DELEGATE_OneParam(FOnQueryAppBundleIdResponse, NSDictionary* /*ResponseDict*/);

/** 
 * Helper class for querying app store metadata
 */
@interface FAppStoreUtils : NSObject
{
};


/** Retrieve app store json data */
-(void)queryAppBundleId: (FOnQueryAppBundleIdResponse) completionDelegate;

@end
