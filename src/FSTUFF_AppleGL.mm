//
//  FSTUFF_AppleGLViewController.mm
//  FallingStuff_iOSGLES
//
//  Created by David Ludwig on 6/4/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#include "FSTUFF.h"
#include "FSTUFF_OpenGLES.h"
#import "FSTUFF_Apple.h"
//#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/ES3/glext.h>
#import <GLKit/GLKit.h>
#include <array>
#include <vector>
#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#include <dlfcn.h>


void * FSTUFF_AppleGL_GetProcAddress(const char * name)
{
    return dlsym(RTLD_DEFAULT, name);
}


@interface FSTUFF_AppleGLViewController : GLKViewController
@end

@interface FSTUFF_AppleGLViewController () {
    FSTUFF_Simulation * sim;
    FSTUFF_GLESRenderer * renderer;
}
@property (strong, nonatomic) EAGLContext *context;
@end

@implementation FSTUFF_AppleGLViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    const FSTUFF_GLVersion glVersion = FSTUFF_GLVersion::GLESv2;

    EAGLRenderingAPI macGLVersion = kEAGLRenderingAPIOpenGLES2;
    switch (glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            macGLVersion = kEAGLRenderingAPIOpenGLES2;
            break;
        case FSTUFF_GLVersion::GLESv3:
            macGLVersion = kEAGLRenderingAPIOpenGLES3;
            break;
    }
    self.context = [[EAGLContext alloc] initWithAPI:macGLVersion];
    if ( ! self.context) {
        FSTUFF_Log(@"Failed to create ES context\n");
        return;
    }
    
    GLKView *view = (GLKView *)self.view;
    view.context = self.context;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;

    [EAGLContext setCurrentContext:self.context];

    sim = new FSTUFF_Simulation();
    renderer = new FSTUFF_GLESRenderer();
    renderer->sim = sim;
    renderer->glVersion = glVersion;
    renderer->nativeView = (__bridge_retained void *) view;
    renderer->nativeViewType = FSTUFF_NativeViewType::Apple;
    renderer->getProcAddress = FSTUFF_AppleGL_GetProcAddress;
    renderer->Init();
    sim->renderer = renderer;

    // Init the game/simulation
    sim->Init();
}

- (void)dealloc
{
    if (sim) {
        delete sim;
        sim = nullptr;
    }
    
    if (renderer) {
        delete renderer;
        renderer = nullptr;
    }
    
    [EAGLContext setCurrentContext:self.context];
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];

    if ([self isViewLoaded] && ([[self view] window] == nil)) {
        self.view = nil;
        
        [EAGLContext setCurrentContext:self.context];
        if ([EAGLContext currentContext] == self.context) {
            [EAGLContext setCurrentContext:nil];
        }
        self.context = nil;
    }

    // Dispose of any resources that can be recreated.
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}


#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{
    FSTUFF_Assert(sim);
    sim->Update();
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    FSTUFF_Assert(sim);
    renderer->BeginFrame();
    sim->Render();
}

@end
