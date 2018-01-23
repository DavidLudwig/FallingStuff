//
//  FallingStuff_OSXScreenSaverView.m
//  FallingStuff_OSXScreenSaver
//
//  Created by David Ludwig on 12/28/17.
//  Copyright © 2017 David Ludwig. All rights reserved.
//

#import "FallingStuff_OSXScreenSaverView.h"
#include "FSTUFF.h"

extern void FSTUFF_Log(const char * fmt, ...);
extern void FSTUFF_Log(NSString * fmt, ...);

@interface CustomView : NSView
@end
@implementation CustomView
- (void)drawRect:(NSRect)rect
{
    [super drawRect:rect];
    FSTUFF_Log("CustomView drawRect, self.window:%p\n", self.window);

    NSColor* color = [NSColor colorWithCalibratedRed:1
                                  green:0
                                   blue:0
                                  alpha:1];
    [color set];
    NSBezierPath * path = [NSBezierPath bezierPathWithRect:rect];
    [path fill];
}
@end

@implementation FallingStuff_OSXScreenSaverView

- (instancetype)initWithFrame:(NSRect)frame isPreview:(BOOL)isPreview
{
    FSTUFF_Log("%s, frame:{%f,%f,%f,%f}, isPreview:%d\n",
        __FUNCTION__,
        frame.origin.x, frame.origin.y, frame.size.width, frame.size.height,
        (int)isPreview);

    self = [super initWithFrame:frame isPreview:isPreview];
    if (self) {
        [self setAnimationTimeInterval:1/30.0];
        FSTUFF_MetalViewController * viewController = [[FSTUFF_MetalViewController alloc] init];
        viewController.label = @"Screen Saver Main Display";
        self.viewController = viewController;
        NSView * view = self.viewController.view;
        FSTUFF_Log("%s, sim:%p\n", __FUNCTION__, self.viewController.sim);
        view.frame = CGRectMake(0, 0, self.frame.size.width, self.frame.size.height);
        [self addSubview:view];
        if (isPreview) {
            self.viewController.sim->SetGlobalScale({0.2f, 0.2f});
        }
    }
    return self;
}

- (void)startAnimation
{
    FSTUFF_Log("FallingStuff_OSXScreenSaverView startAnimation, self.window:%p\n", self.window);
    [super startAnimation];
//    FSTUFF_Init(self.viewController.sim);
//    self.viewController.sim->Init();
}

- (void)animateOneFrame {
    [super animateOneFrame];
//    if (self.viewController.sim->doEndConfiguration) {
//        [self.window.sheetParent endSheet:self.window];
//    }
}

- (void)stopAnimation
{
    [super stopAnimation];
}

//- (void)drawRect:(NSRect)rect
//{
//    [super drawRect:rect];
//}

- (void)drawRect:(NSRect)rect
{
    [super drawRect:rect];
    
    FSTUFF_Log("%s, self.window:%p\n", __FUNCTION__, self.window);
    FSTUFF_Log(@"%s, self.subviews:%@\n", __FUNCTION__, self.subviews);

    NSColor* color = [NSColor colorWithCalibratedRed:1
                                  green:0
                                   blue:0
                                  alpha:1];
    [color set];
    NSBezierPath * path = [NSBezierPath bezierPathWithRect:rect];
    [path fill];
}

//- (void)animateOneFrame
//{
//    return;
//}

- (BOOL)hasConfigureSheet
{
    return YES;
}

- (NSWindow*)configureSheet
{
    extern NSWindow * FSTUFF_CreateConfigureSheet();
    NSWindow * window = FSTUFF_CreateConfigureSheet();
    //[window retain];
    return window;
}

@end
