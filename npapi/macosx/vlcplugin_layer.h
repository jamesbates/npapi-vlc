#ifndef __VLCPLUGIN_LAYER_H
#define __VLCPLUGIN_LAYER_H

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>


typedef void (*callback_func_t)(void *);


@interface VlcPluginLayer : CAOpenGLLayer
{
@private
    CGLContextObj _lastKnownGLContext;
    callback_func_t _callback_func;
    void *_callback_args;
    callback_func_t _callback_exit_func;
}

- (CGLContextObj)lastKnownGLContext;

- (void)setupCallbackWithFunc:(callback_func_t)callback_func args:(void *)callback_args exitFunc:(callback_func_t)callback_exit_func;
@end

#endif



/* Some pure C functions to manipulate the Obj-C instance, which can be called from outside Objective-C (e.g. from C++) */
#ifdef __cplusplus
extern "C" {
#endif

void *createVlcPluginLayer();
void destroyVlcPluginLayer(void *vlcPluginLayer);
void setVlcPluginLayerBounds(void *vlcPluginLayer, CGRect bounds);

#ifdef __cplusplus
}
#endif

#endif
