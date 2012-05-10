#import <Foundation/Foundation.h>
#import "misc.h"

float getDoubleClickThreshold() {

       NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
       return [defaults floatForKey:@"com.apple.mouse.doubleClickThreshold"];
}