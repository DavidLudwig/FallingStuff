
#ifndef FSTUFF_Apple_h
#define FSTUFF_Apple_h

#include "FSTUFF.h"

#ifdef __APPLE__

FSTUFF_ViewSize FSTUFF_Apple_GetViewSize(void * nativeView);

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#include <simd/simd.h>
#include "FSTUFF_AppleMetalStructs.h"

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
typedef UIViewController AppleViewController;
#else
#import <Cocoa/Cocoa.h>
typedef NSViewController AppleViewController;
#endif

void    FSTUFF_Apple_CopyMatrix(matrix_float4x4 & dest, const gbMat4 & src);
void    FSTUFF_Apple_CopyMatrix(gbMat4 & dest, const matrix_float4x4 & src);
void    FSTUFF_Apple_CopyVector(vector_float4 & dest, const gbVec4 & src);
void    FSTUFF_Apple_CopyVector(gbMat4 & dest, const vector_float4 & src);
void    FSTUFF_Log(NSString * fmt, ...) __attribute__((format(NSString, 1, 2)));
void    FSTUFF_FatalError_Inner(FSTUFF_CodeLocation location, NSString * fmt, ...) __attribute__((format(NSString, 2, 3)));


#if TARGET_OS_OSX
NSWindow * FSTUFF_CreateConfigureSheet();
#endif

#endif  // __OBJC__
#endif  // __APPLE__
#endif  // FSTUFF_Apple_h
