//
//  FSTUFF_UtilApple.mm
//  FallingStuff
//
//  Created by David Ludwig on 6/4/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#import <Foundation/Foundation.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <GBDeviceInfo/GBDeviceInfo.h>
#elif TARGET_OS_MAC
#import <AppKit/AppKit.h>
#endif

#include "FSTUFF.h"

void FSTUFF_GetViewSizeMM(void * _nativeView, float * outWidthMM, float * outHeightMM)
{
#if TARGET_OS_IOS
    UIScreen * screen = [UIScreen mainScreen]; //self.view.window.screen;
    const float scale = [screen scale];
    float ppi = [GBDeviceInfo deviceInfo].displayInfo.pixelsPerInch;
    if (ppi == 0.f) {
        ppi = 264.f;    // HACK: on iOS simulator (which GBDeviceInfo reports a PPI of zero), use an iPad Air 2's PPI (of 264)
    }
    const float width = ([screen bounds].size.width * scale);
    const float height = ([screen bounds].size.height * scale);
    *outWidthMM = (width / ppi) * 25.4f;
    *outHeightMM = (height / ppi) * 25.4f;
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
    NSNumber * displayID = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
    FSTUFF_Log(@"%s, displayID: %@\n", __FUNCTION__, displayID);
    CGDirectDisplayID cgDisplayID = (CGDirectDisplayID) [displayID intValue];
    FSTUFF_Log("%s, cgDisplayID: %d\n", __FUNCTION__, cgDisplayID);
    const CGSize scrSizeMM = CGDisplayScreenSize(cgDisplayID);
    FSTUFF_Log("%s, scrSizeMM: {%f,%f}\n", __FUNCTION__, scrSizeMM.width, scrSizeMM.height);
    const CGSize ptsToMM = CGSizeMake(scrSizeMM.width / screen.frame.size.width,
                                      scrSizeMM.height / screen.frame.size.height);
    *outWidthMM = nativeView.bounds.size.width * ptsToMM.width;
    *outHeightMM = nativeView.bounds.size.height * ptsToMM.height;
#endif

//    NSLog(@"view size: {%f,%f} (pts?) --> {%f,%f} (mm?)\n",
//          self.view.bounds.size.width,
//          self.view.bounds.size.height,
//          viewSizeMM.width,
//          viewSizeMM.height);

    FSTUFF_Log(@"%s, view size: {%f,%f} (mm?)\n",
        __FUNCTION__,
        *outWidthMM,
        *outHeightMM);
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
        FSTUFF_Log("----------------------------------------------------------------------\nFalling Stuff, startup, Build-Date:\"%s\", Build:%d\n", __DATE__, FSTUFF_BUILD);
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

void FSTUFF_Log(NSString * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FSTUFF_Log(fmt, args);
    va_end(args);
}
