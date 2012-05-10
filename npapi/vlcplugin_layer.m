#import <OpenGL/OpenGL.h>
#import "vlcplugin_layer.h"
#include <stdio.h>

@implementation VlcPluginLayer

- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
            forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
    
    CGLSetCurrentContext(ctx);

	// TODO: call vout OpenGL callback
    
    [super drawInCGLContext:ctx pixelFormat:pf forLayerTime:t displayTime:ts];
}

- (void)sayHi {

    printf("Hello, I am a VlcPluginLayer Objective-C instance\n");
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

float getDoubleClickThreshold() {

       NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
       return [defaults floatForKey:@"com.apple.mouse.doubleClickThreshold"];
}