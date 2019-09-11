#include "ui-metal-shim.h"

#include <Metal/Metal.h>

#include "imgui/imgui_impl_metal.h"

bool ui_metal_init(struct render_device *device)
{
	id<MTLCommandQueue> cq = (id<MTLCommandQueue>) device;

	return ImGui_ImplMetal_Init(cq.device);
}

void ui_metal_shutdown(void)
{
	ImGui_ImplMetal_Shutdown();
}

static MTLRenderPassDescriptor *ui_metal_rpd(id<MTLTexture> tex)
{
	MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
	rpd.colorAttachments[0].texture = tex;
	rpd.colorAttachments[0].loadAction = MTLLoadActionLoad;
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

	return rpd;
}

void ui_metal_new_frame(struct render_context *context)
{
	MTLRenderPassDescriptor *rpd = ui_metal_rpd((id<MTLTexture>) context);

	ImGui_ImplMetal_NewFrame(rpd);
	[rpd release];
}

void ui_metal_render_draw_data(ImDrawData* draw_data, struct render_device *device, struct render_context *context)
{
	id<MTLCommandQueue> cq = (id<MTLCommandQueue>) device;
	MTLRenderPassDescriptor *rpd = ui_metal_rpd((id<MTLTexture>) context);

	id<MTLCommandBuffer> cb = [cq commandBuffer];
	id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];

	ImGui_ImplMetal_RenderDrawData(draw_data, cb, re);

	[re endEncoding];
	[cb commit];

	[rpd release];
	[cb release];
	[re release];
}
