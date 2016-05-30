//
//  GameViewController.h
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2016 David Ludwig. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <MetalKit/MTKView.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
@interface GameViewController : UIViewController<MTKViewDelegate>
@end
#endif

#if ! TARGET_OS_IOS     // Mac OS X?
#import <Cocoa/Cocoa.h>
@interface GameViewController : NSViewController<MTKViewDelegate>
@end
#endif


