//
//  FSTUFF_AppleMetalViewController.h
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2018 David Ludwig. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#include "FSTUFF.h"

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
typedef UIViewController AppleViewController;
#else
#import <Cocoa/Cocoa.h>
typedef NSViewController AppleViewController;
#endif

@interface FSTUFF_AppleMetalViewController : AppleViewController<MTKViewDelegate>
@property (readonly) FSTUFF_Simulation * sim;
@end
