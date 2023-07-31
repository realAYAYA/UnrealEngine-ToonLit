#pragma once

#include "../src/mtlpp.hpp"

MTLPP_CLASS(MTKView);

class Window
{
public:
    Window(const mtlpp::Device& device, void (*render)(const Window&), int32_t width, int32_t height);

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    mtlpp::Drawable GetDrawable() const;
    mtlpp::RenderPassDescriptor GetRenderPassDescriptor() const;

    static void Run();

private:
    class MtlView : public ns::Object<MTKView*>
    {
    public:
        MtlView() { }
        MtlView(MTKView* handle) : ns::Object<MTKView*>(handle) { }
    };

    MtlView m_view;
};

