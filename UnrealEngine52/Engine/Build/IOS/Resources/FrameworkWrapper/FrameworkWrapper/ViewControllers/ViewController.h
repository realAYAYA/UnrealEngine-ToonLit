// Copyright Epic Games, Inc. All Rights Reserved.
//
//  ViewController.h
//  FrameworkWrapper
//
//  Created by Ryan West on 7/29/19.
//

#ifndef ViewController_h
#define ViewController_h

#import <UIKit/UIKit.h>
#import "UnrealView.h"

@interface ViewController : UIViewController

// MARK: Outlets
    @property (weak, nonatomic) IBOutlet UnrealContainerView *unrealView;

@end

#endif /* ViewController_h */
