//
//  FSTUFF_Apple.mm
//  FallingStuff
//
//  Created by David Ludwig on 6/4/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#include "FSTUFF.h"
#include "FSTUFF_Apple.h"

#define FSTUFF_USE_IMGUI 1

#include "imgui.h"
#ifdef __APPLE__
#include "imgui_impl_mtl.h"

#include "AAPLShaderTypes.h"

#import <Foundation/Foundation.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <GBDeviceInfo/GBDeviceInfo.h>
#elif TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif

FSTUFF_ViewSize FSTUFF_Apple_GetViewSize(void * _nativeView)
{
    FSTUFF_ViewSize out;

#if TARGET_OS_IOS
    UIScreen * screen = [UIScreen mainScreen]; //self.view.window.screen;
    const float scale = [screen scale];
    float ppi = [GBDeviceInfo deviceInfo].displayInfo.pixelsPerInch;
    if (ppi == 0.f) {
        ppi = 264.f;    // HACK: on iOS simulator (which GBDeviceInfo reports a PPI of zero), use an iPad Air 2's PPI (of 264)
    }
    const float width = ([screen bounds].size.width * scale);
    const float height = ([screen bounds].size.height * scale);
    out.widthMM = (width / ppi) * 25.4f;
    out.heightMM = (height / ppi) * 25.4f;
    out.widthPixels = std::lround(width);
    out.heightPixels = std::lround(height);
    out.widthOS = std::lround([screen bounds].size.width);
    out.heightOS = std::lround([screen bounds].size.height);
#else
    FSTUFF_Log("%s, _nativeView: %p\n", __FUNCTION__, _nativeView);
    NSView * nativeView = (__bridge NSView *) _nativeView;
    FSTUFF_Log(@"%s, nativeView: %@\n", __FUNCTION__, nativeView);
    FSTUFF_Log(@"%s, nativeView.window: %@\n", __FUNCTION__, nativeView.window);
    FSTUFF_Log("%s, nativeView.frame: {%f,%f,%f,%f}\n",
        __FUNCTION__,
        nativeView.frame.origin.x,
        nativeView.frame.origin.y,
        nativeView.frame.size.width,
        nativeView.frame.size.height
    );
    NSScreen * screen = nativeView.window.screen;
    FSTUFF_Log(@"%s, screen: %@\n", __FUNCTION__, screen);
    if ( ! screen) {
        screen = [NSScreen mainScreen];
        FSTUFF_Log(@"%s, 'screen' hacked to mainScreen: %@\n", __FUNCTION__, screen);
    }
    NSNumber * displayID = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
    FSTUFF_Log(@"%s, displayID: %@\n", __FUNCTION__, displayID);
    CGDirectDisplayID cgDisplayID = (CGDirectDisplayID) [displayID intValue];
    FSTUFF_Log("%s, cgDisplayID: %d\n", __FUNCTION__, cgDisplayID);
    const CGSize scrSizeMM = CGDisplayScreenSize(cgDisplayID);
    FSTUFF_Log("%s, scrSizeMM: {%f,%f}\n", __FUNCTION__, scrSizeMM.width, scrSizeMM.height);

    const CGSize ptsToMM = CGSizeMake(scrSizeMM.width / screen.frame.size.width,
                                      scrSizeMM.height / screen.frame.size.height);
    out.widthMM = nativeView.bounds.size.width * ptsToMM.width;
    out.heightMM = nativeView.bounds.size.height * ptsToMM.height;

    if ([nativeView isKindOfClass:[MTKView class]]) {
        MTKView * mtkView = (MTKView *) nativeView;
        FSTUFF_Log("%s, drawableSize: {%f,%f}\n", __FUNCTION__, mtkView.drawableSize.width, mtkView.drawableSize.height);
        out.widthPixels = (int) std::lround(mtkView.drawableSize.width);
        out.heightPixels = (int) std::lround(mtkView.drawableSize.height);
    } else {
        out.widthPixels = (int) std::lround(nativeView.bounds.size.width);
        out.heightPixels = (int) std::lround(nativeView.bounds.size.height);
    }
    out.widthOS = (int) std::lround(nativeView.bounds.size.width);
    out.heightOS = (int) std::lround(nativeView.bounds.size.height);

#endif

//    FSTUFF_Log(@"view size: {%f,%f} (pts?) --> {%f,%f} (mm?)\n",
//          self.view.bounds.size.width,
//          self.view.bounds.size.height,
//          viewSizeMM.width,
//          viewSizeMM.height);

    FSTUFF_Log(@"%s, view size: {%f,%f} (mm?)\n",
        __FUNCTION__,
        out.widthMM,
        out.heightMM);
    return out;
}

void FSTUFF_Log(NSString * fmt, va_list args) {
    static FILE * outFile = NULL;
    static std::once_flag once;
    bool first = false;

    std::call_once(once, [&] {
        NSString * logFilePath = [NSString pathWithComponents:@[
            NSHomeDirectory(),
            @"Library",
            @"Logs",
            @"FallingStuff.log"
        ]];
        outFile = fopen([logFilePath UTF8String], "w");
        first = true;
    });
    if (first) {
        FSTUFF_Log("----------------------------------------------------------------------\nFalling Stuff, startup, Build-Date:\"%s\", Build-Number:%d\n", __DATE__, FSTUFF_BuildNumber);
    }
    if (fmt) {
        NSString * buffer = [[NSString alloc] initWithFormat:fmt arguments:args];
        if (outFile) {
            fprintf(outFile, "%s", [buffer UTF8String]);
            fflush(outFile);
        }
        if (outFile != stdout && outFile != stderr) {
            fprintf(stdout, "%s", [buffer UTF8String]);
        }
    }
}

void FSTUFF_Log(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    NSString * nsfmt = [[NSString alloc] initWithUTF8String:fmt];
    FSTUFF_Log(nsfmt, args);
    va_end(args);
}


#pragma mark - Renderer

void FSTUFF_Apple_CopyMatrix(matrix_float4x4 & dest, const gbMat4 & src)
{
    memcpy((void *)&(dest), (const void *)&(src.e), sizeof(float) * 16);
}

void FSTUFF_Apple_CopyMatrix(gbMat4 & dest, const matrix_float4x4 & src)
{
    memcpy((void *)&(dest.e), (const void *)&(src), sizeof(float) * 16);
}

void FSTUFF_Apple_CopyVector(vector_float4 & dest, const gbVec4 & src)
{
    memcpy((void *)&(dest), (const void *)&(src.e), sizeof(float) * 4);
}

void FSTUFF_Apple_CopyVector(gbMat4 & dest, const vector_float4 & src)
{
    memcpy((void *)&(dest.e), (const void *)&(src), sizeof(float) * 4);
}

#if TARGET_OS_OSX
NSWindow * FSTUFF_CreateConfigureSheet()
{
    @autoreleasepool {
        NSUInteger styleMask = NSWindowStyleMaskBorderless;
        NSPanel * configSheet = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 600, 400) styleMask:styleMask backing:NSBackingStoreBuffered defer:YES];
        [configSheet setBackgroundColor:[NSColor blackColor]];

        FSTUFF_MetalViewController * viewController = [[FSTUFF_MetalViewController alloc] init];
        viewController.label = @"Configuration Sheet";
        viewController.initialViewFrame = NSMakeRect(0, 0, 600, 400);
        viewController.sim->showSettings = true;
        viewController.sim->configurationMode = true;
        viewController.sim->SetGlobalScale(cpv(0.5,0.5));
        configSheet.contentViewController = viewController;
        MTKView * view = (MTKView *) viewController.view;
        if ([view isKindOfClass:[MTKView class]]) {
            view.autoResizeDrawable = YES;
        }
        return configSheet;
    }
}
#endif

void FSTUFF_Log(NSString * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FSTUFF_Log(fmt, args);
    va_end(args);
}

#endif  // __APPLE__

