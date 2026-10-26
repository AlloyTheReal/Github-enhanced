#include "visuals.h"
#include <QWidget>
#include <QPointer>
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>

bool systemReducesMotion() {
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
}
bool systemReducesTransparency() {
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceTransparency;
}

bool installNativeGlass(QWidget *widget) {
    if (!widget || systemReducesTransparency())
        return false;
    NSView *host = (__bridge NSView *)(void *)widget->winId();
    if (!host || !host.superview)
        return false;
    static char association;
    if (objc_getAssociatedObject(host, &association))
        return true;
    NSView *material = nil;
    // Resolve the public class at runtime, so the app also builds with older SDKs.
    Class glassClass = nil;
    if (@available(macOS 26.0, *))
        glassClass = NSClassFromString(@"NSGlassEffectView");
    if (glassClass) {
        material = [[glassClass alloc] initWithFrame:host.frame];
        [material setValue:@24.0 forKey:@"cornerRadius"];
    } else {
        NSVisualEffectView *fallback = [[NSVisualEffectView alloc] initWithFrame:host.frame];
        fallback.material = NSVisualEffectMaterialSidebar;
        fallback.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        fallback.state = NSVisualEffectStateFollowsWindowActiveState;
        fallback.wantsLayer = YES;
        fallback.layer.cornerRadius = 24;
        fallback.layer.masksToBounds = YES;
        material = fallback;
    }
    material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    const bool dark = widget->palette().color(QPalette::Base).lightness() < 128;
    material.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
    host.window.opaque = NO;
    host.window.backgroundColor = NSColor.clearColor;
    [host.superview addSubview:material positioned:NSWindowBelow relativeTo:host];
    objc_setAssociatedObject(host, &association, material, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    QObject::connect(widget, &QObject::destroyed, [material] { [material removeFromSuperview]; });
    return true;
}
