#ifndef __VLCPLUGIN_LAYER_H
#define __VLCPLUGIN_LAYER_H

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

@interface VlcPluginLayer : CAOpenGLLayer

- (void)sayHi;
@end

#else
/* types referred to by manipulations functions below */
//#include <CoreGraphics/CGGeometry.h>
#endif



/* Some pure C functions to manipulate the Obj-C instance, which can be called from outside Objective-C (e.g. from C++) */
#ifdef __cplusplus
extern "C" {
#endif

void *createVlcPluginLayer();
void destroyVlcPluginLayer(void *vlcPluginLayer);
void setVlcPluginLayerBounds(void *vlcPluginLayer, CGRect bounds);
float getDoubleClickThreshold();

#ifdef __cplusplus
}
#endif

#endif
