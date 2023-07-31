#include "../src/mtlpp.hpp"
#include "window.hpp"

ns::Ref<mtlpp::Device>     g_device;
mtlpp::CommandQueue        g_commandQueue;
mtlpp::Buffer              g_vertexBuffer;
mtlpp::RenderPipelineState g_renderPipelineState;

void Render(const Window& win)
{
    mtlpp::CommandBuffer commandBuffer = g_commandQueue.CommandBufferWithUnretainedReferences();
    
    mtlpp::RenderPassDescriptor renderPassDesc = win.GetRenderPassDescriptor();
    if (renderPassDesc)
    {
        mtlpp::RenderCommandEncoder renderCommandEncoder = commandBuffer.RenderCommandEncoder(renderPassDesc);
        renderCommandEncoder.SetRenderPipelineState(g_renderPipelineState);
        renderCommandEncoder.SetVertexBuffer(g_vertexBuffer, 0, 0);
		renderCommandEncoder.UseResource(g_vertexBuffer, mtlpp::ResourceUsage::Read);
        renderCommandEncoder.Draw(mtlpp::PrimitiveType::Triangle, 0, 3);
        renderCommandEncoder.EndEncoding();
        commandBuffer.Present(win.GetDrawable());
    }

	const float vertexData[] =
	{
		0.0f,  1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
	};
	
    commandBuffer.Commit();

	// g_vertexBuffer = nil;
	if (g_vertexBuffer.ValidateAccess(mtlpp::ResourceUsage::Read, false))
		g_vertexBuffer.GetContents();

	if (g_vertexBuffer.ValidateAccess(mtlpp::ResourceUsage::Write, false))
		memcpy(g_vertexBuffer.GetContents(), vertexData, sizeof(vertexData));
	
	commandBuffer.WaitUntilCompleted();
	
}

int main()
{
    const char shadersSrc[] = R"""(
        #include <metal_stdlib>
        using namespace metal;

        vertex float4 vertFunc(
            const device packed_float3* vertexArray [[buffer(0)]],
            unsigned int vID[[vertex_id]])
        {
            return float4(vertexArray[vID], 1.0);
        }

        fragment half4 fragFunc()
        {
            return half4(1.0, 0.0, 0.0, 1.0);
        }
    )""";

    const float vertexData[] =
    {
         0.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
    };
	
	ns::Array<ns::Ref<mtlpp::Device>> devices = mtlpp::Device::CopyAllDevices();
	g_device = devices[0];
	
//    g_device = mtlpp::Device::CreateSystemDefaultDevice();
    g_commandQueue = g_device->NewCommandQueue();

	ns::AutoReleasedError Err;
    mtlpp::Library library = g_device->NewLibrary(shadersSrc, mtlpp::CompileOptions(), &Err);
    mtlpp::Function vertFunc = library.NewFunction("vertFunc");
    mtlpp::Function fragFunc = library.NewFunction("fragFunc");

    g_vertexBuffer = g_device->NewBuffer(vertexData, sizeof(vertexData), mtlpp::ResourceOptions::CpuCacheModeDefaultCache);

    mtlpp::RenderPipelineDescriptor renderPipelineDesc;
    renderPipelineDesc.SetVertexFunction(vertFunc);
    renderPipelineDesc.SetFragmentFunction(fragFunc);
    renderPipelineDesc.GetColorAttachments()[0].SetPixelFormat(mtlpp::PixelFormat::BGRA8Unorm);
    g_renderPipelineState = g_device->NewRenderPipelineState(renderPipelineDesc, &Err);

    Window win(g_device, &Render, 320, 240);
    Window::Run();

    return 0;
}

