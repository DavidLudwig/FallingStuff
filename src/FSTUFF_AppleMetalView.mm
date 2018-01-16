//
//  FSTUFF_AppleMetalView.m
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#import "FSTUFF_AppleMetalView.h"
#import "FSTUFF_AppleMetalViewController.h"
#import "FSTUFF.h"

@implementation FSTUFF_AppleMetalView

- (BOOL) acceptsFirstResponder
{
    return YES;
}

#if ! TARGET_OS_IOS

//- (void) keyDown:(NSEvent *)theEvent
//{
//    FSTUFF_Log("%s, keyDown:%@\n", __FUNCTION__, theEvent);
//    if ([self.delegate isKindOfClass:[FSTUFF_AppleMetalViewController class]]) {
//        FSTUFF_AppleMetalViewController * controller = (FSTUFF_AppleMetalViewController *) self.delegate;
//        [controller keyDown:theEvent];
//    }
//}

#endif

@end
