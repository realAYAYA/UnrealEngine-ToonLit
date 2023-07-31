// Copyright Epic Games, Inc. All Rights Reserved.
//
//  ViewController.m
//  FrameworkWrapper
//
//  Created by Ryan West on 7/29/19.
//

#import "ViewController.h"

@implementation ViewController

-(void) viewDidLoad
{
    [super viewDidLoad];

    [UnrealContainerView DelayedCreateView];
    [UnrealContainerView WakeUpUnreal];
}

@end
