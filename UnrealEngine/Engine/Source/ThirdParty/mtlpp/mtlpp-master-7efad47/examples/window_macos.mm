#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>
#import "window.hpp"

@interface WindowViewController : NSViewController<MTKViewDelegate> {
    @public void (*m_render)(const Window&);
    @public const Window* m_window;
}

@end

@implementation WindowViewController
-(void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{

}

-(void)drawInMTKView:(nonnull MTKView *)view
{
    (*m_render)(*m_window);
}
@end

Window::Window(const mtlpp::Device& device, void (*render)(const Window&), int32_t width, int32_t height)
{
    NSRect frame = NSMakeRect(0, 0, width, height);
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
#if MTLPP_IS_AVAILABLE_MAC(10_12)
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
#else
        styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask)
#endif
        backing:NSBackingStoreBuffered
        defer:NO];
	
	window.title = [NSString stringWithFormat:@"%@ : %@", [[NSProcessInfo processInfo] processName], device.GetName().GetPtr()];
	
    WindowViewController* viewController = [WindowViewController new];
    viewController->m_render = render;
    viewController->m_window = this;

    MTKView* view = [[MTKView alloc] initWithFrame:frame];
    view.device = (id<MTLDevice>)device.GetPtr();
    view.delegate = viewController;
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    [window.contentView addSubview:view];
    [window center];
    [window orderFrontRegardless];

    m_view = view;

}

uint32_t Window::GetWidth() const
{
    return uint32_t(((MTKView*)m_view.GetPtr()).frame.size.width);
}

uint32_t Window::GetHeight() const
{
    return uint32_t(((MTKView*)m_view.GetPtr()).frame.size.height);
}

mtlpp::Drawable Window::GetDrawable() const
{
    return ((MTKView*)m_view.GetPtr()).currentDrawable;
}

mtlpp::RenderPassDescriptor Window::GetRenderPassDescriptor() const
{
    return ((MTKView*)m_view.GetPtr()).currentRenderPassDescriptor;
}

void Window::Run()
{
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSMenu* menubar = [NSMenu new];
    NSMenuItem* appMenuItem = [NSMenuItem new];
    NSMenu* appMenu = [NSMenu new];
    NSMenuItem* quitMenuItem = [[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(stop:) keyEquivalent:@"q"];
    [menubar addItem:appMenuItem];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    [NSApp setMainMenu:menubar];

    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}

