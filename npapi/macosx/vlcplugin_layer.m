#import <OpenGL/OpenGL.h>
#import "vlcplugin_layer.h"
#include <stdio.h>

@implementation VlcPluginLayer

- (id)init {

    id result = [super init];
    if (result) {
    
        _callback_func = nil;
        _callback_args = nil;
        _lastKnownGLContext = nil;
    }
    
    return result;
}

- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
            forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
    
    //printf("drawInCGLContext received.\n");
    
    CGLSetCurrentContext(ctx);
    _lastKnownGLContext = ctx;

	if (_callback_func && _callback_args) {
	
	    _callback_func(_callback_args);
		_callback_func = nil;
	}
	
    [super drawInCGLContext:ctx pixelFormat:pf forLayerTime:t displayTime:ts];
    
    if (_callback_exit_func && _callback_args) {
    
        /* Calling the end function will release the mutex, so we reset the instance variable -before- calling
           the function */
        callback_func_t the_exitf = _callback_exit_func;
        _callback_exit_func = nil;

        the_exitf(_callback_args);
    }
}

- (void)setupCallbackWithFunc:(callback_func_t)callback_func args:(void *)callback_args exitFunc:(callback_func_t)callback_exit_func {

    _callback_func = callback_func;
    _callback_exit_func = callback_exit_func;
    _callback_args = callback_args;
}
/*
- (void)setNeedsDisplay {

    //printf("VlcPluginLayer: setNeedsDisplay received.\n");
    [super setNeedsDisplay];
}
*/
- (CGLContextObj)lastKnownGLContext {

    return _lastKnownGLContext;
}

@end


/* Some pure C functions to manipulate the Objc-C instance, which can be called from outside Objective-C (e.g. from C++) */

void *createVlcPluginLayer() {
    
    VlcPluginLayer *newLayer = [VlcPluginLayer layer];
    return [newLayer retain];
}

void destroyVlcPluginLayer(void *vlcPluginLayer) {

    [(VlcPluginLayer *)vlcPluginLayer release];
}

void setVlcPluginLayerBounds(void *vlcPluginLayer, CGRect rect) {

    ((VlcPluginLayer *)vlcPluginLayer).bounds = rect;
}
