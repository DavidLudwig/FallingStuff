//
//  FSTUFF_MTKView.m
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2016 David Ludwig. All rights reserved.
//

#import "FSTUFF_MTKView.h"
#import "GameViewController.h"

@implementation FSTUFF_MTKView

- (BOOL) acceptsFirstResponder
{
    return YES;
}

#if ! TARGET_OS_IOS

- (void) keyDown:(NSEvent *)theEvent
{
    if ([self.delegate isKindOfClass:[GameViewController class]]) {
        GameViewController * controller = (GameViewController *) self.delegate;
        [controller keyDown:theEvent];
    }
}

#endif

@end
