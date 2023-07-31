// Copyright Epic Games, Inc. All Rights Reserved.

#import <Foundation/Foundation.h>
#import <simd/simd.h>

@protocol LiveLinkProvider <NSObject>

- (void)addCameraSubject:(NSString *)subjectName;
- (void)removeCameraSubject:(NSString *)subjectName;

- (void)updateSubject:(NSString *)subjectName withTransform:(simd_float4x4)xform;
- (void)updateSubject:(NSString *)subjectName withTransform:(simd_float4x4)xform atTime:(double)time;

@end

