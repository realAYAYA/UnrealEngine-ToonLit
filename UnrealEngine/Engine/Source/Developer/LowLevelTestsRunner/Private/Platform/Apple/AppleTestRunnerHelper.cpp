// Copyright Epic Games, Inc. All Rights Reserved.
#include "AppleTestRunnerHelper.h"

#if PLATFORM_IOS || PLATFORM_MAC

#include "TestRunner.h"

@implementation AppleTestsRunnerHelper

int Argc;
const char** Argv;
NSThread* TestsThread;
int _Result;

@synthesize Result = _Result;

-(id)initWithArgc:(int)argc Argv:(const char*[])argv
{
	[super init];
	
	Argc = argc;
	Argv = argv;
	return self;
}

- (void)startTestsOnThread
{
	TestsThread = [[NSThread alloc] initWithTarget:self selector: @selector(runTestsThread:) object: nil];
	[TestsThread start];
}

-(void)dealloc
{
	[TestsThread release];

	[super dealloc];
}

- (void)runTestsThread:(id)Arg
{
	_Result = RunTests(Argc, Argv);
	
	CFRunLoopStop(CFRunLoopGetMain());
}

@end

#endif
