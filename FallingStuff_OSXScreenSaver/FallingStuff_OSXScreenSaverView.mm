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
    self = [super initWithFrame:frame isPreview:isPreview];
    if (self) {

        FSTUFF_Log(@"init...\n");
        [self setAnimationTimeInterval:1/30.0];
        FSTUFF_Log(@"creating view controller...\n");
        self.viewController = [[FSTUFF_AppleMetalViewController alloc] init];
        FSTUFF_Log(@"getting view...\n");
        NSView * view = self.viewController.view;
//        NSView * view = [[CustomView alloc] init];

        FSTUFF_Log(@"view:%@\n", view);

//        [self addSubview:self.viewController.view];
        view.frame = self.frame;
//        NSLog(@"hello");
//        fprintf(stdout, "hello\n");
//        fprintf(stderr, "hello\n");
//
        
        [self addSubview:view];
    }
    FSTUFF_Log("FallingStuff_OSXScreenSaverView initWithFrame, self.window:%p\n", self.window);
    return self;
}

- (void)startAnimation
{
    FSTUFF_Log("FallingStuff_OSXScreenSaverView startAnimation, self.window:%p\n", self.window);
    [super startAnimation];
    FSTUFF_Init(self.viewController.sim);
//    self.viewController.sim->Init();
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
    
    FSTUFF_Log("FallingStuff_OSXScreenSaverView drawRect, self.window:%p\n", self.window);

    NSColor* color = [NSColor colorWithCalibratedRed:0
                                  green:0
                                   blue:1
                                  alpha:1];
    [color set];
    NSBezierPath * path = [NSBezierPath bezierPathWithRect:rect];
    [path fill];
}

- (void)animateOneFrame
{
    return;
}

- (BOOL)hasConfigureSheet
{
    return NO;
}

- (NSWindow*)configureSheet
{
    return nil;
}

@end
