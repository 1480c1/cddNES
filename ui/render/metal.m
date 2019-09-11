#include "metal.h"

#include <Metal/Metal.h>
#include <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>

#include "shaders/metal/library.h"

struct mtl {
	id<MTLDevice> device;
	id<MTLCommandQueue> cq;
	id<MTLLibrary> library;
	id<MTLFunction> fs;
	id<MTLFunction> vs;
	id<MTLBuffer> vb;
	id<MTLBuffer> ib;
	id<MTLRenderPipelineState> pipeline_state;
	id<MTLTexture> tex;
	id<MTLTexture> staging;
	id<MTLSamplerState> ss_linear;
	id<MTLSamplerState> ss_nearest;

	NSWindow *window;
	CAMetalLayer *layer;
	CVDisplayLinkRef display_link;
	dispatch_semaphore_t semaphore;
	bool vsync;
	enum sampler sampler;

	uint32_t w;
	uint32_t h;
};

static id<MTLSamplerState> mtl_create_sampler(id<MTLDevice> device, enum sampler sampler)
{
	MTLSamplerMinMagFilter filter = (sampler == SAMPLE_LINEAR) ?
		MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;

	MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
	sd.minFilter = sd.magFilter = filter;
	sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
	sd.tAddressMode = MTLSamplerAddressModeClampToEdge;

	id<MTLSamplerState> ss = [device newSamplerStateWithDescriptor:sd];
	[sd release];

	return ss;
}

static id<MTLTexture> mtl_create_texture(id<MTLDevice> device, uint32_t width, uint32_t height)
{
	MTLTextureDescriptor *tdesc = [MTLTextureDescriptor new];
	tdesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
	tdesc.width = width;
	tdesc.height = height;

	id<MTLTexture> tex = [device newTextureWithDescriptor:tdesc];
	[tdesc release];

	return tex;
}

static CVReturn mtl_display_link(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow,
	const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
	displayLink, inNow, inOutputTime, flagsIn, flagsOut;
	struct mtl *ctx = (struct mtl *) displayLinkContext;

	dispatch_semaphore_signal(ctx->semaphore);

	return 0;
}

int32_t mtl_init(struct render_mod **mod_out, void *window, bool vsync,
	uint32_t width, uint32_t height, enum sampler sampler)
{
	struct mtl **ctx_out = (struct mtl **) mod_out;
	struct mtl *ctx = *ctx_out = calloc(1, sizeof(struct mtl));
	ctx->sampler = sampler;
	ctx->vsync = vsync;

	int32_t r = 0;

	// device, context
	ctx->device = MTLCreateSystemDefaultDevice();
	ctx->cq = [ctx->device newCommandQueue];
	ctx->semaphore = dispatch_semaphore_create(0);

	CVReturn e = CVDisplayLinkCreateWithCGDisplay(CGMainDisplayID(), &ctx->display_link);
	if (e != 0) {r = -1; printf("CVDisplayLinkCreateWithCGDisplay=%d\n", e); goto except;}

	CVDisplayLinkSetOutputCallback(ctx->display_link, mtl_display_link, ctx);
	CVDisplayLinkStart(ctx->display_link);

	// attach to window
	if (window) {
		ctx->layer = [CAMetalLayer layer];
		ctx->layer.device = ctx->device;
		ctx->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

		ctx->window = (NSWindow *) window;
		ctx->window.contentView.wantsLayer = YES;
		ctx->window.contentView.layer = ctx->layer;
	}

	// load library (program) and shaders
	dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	dispatch_data_t data = dispatch_data_create(MTL_LIBRARY, sizeof(MTL_LIBRARY),
		queue, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

	NSError *nse = nil;
	ctx->library = [ctx->device newLibraryWithData:data error:&nse];
	if (nse != nil) {r = -1; NSLog(@"%@", nse); goto except;}

	ctx->vs = [ctx->library newFunctionWithName:@"vs"];
	ctx->fs = [ctx->library newFunctionWithName:@"fs"];

	float vdata[] = {
		-1.0f,  1.0f,	// Position 0
		 0.0f,  0.0f,	// TexCoord 0
		-1.0f, -1.0f,	// Position 1
		 0.0f,  1.0f,	// TexCoord 1
		 1.0f, -1.0f,	// Position 2
		 1.0f,  1.0f,	// TexCoord 2
		 1.0f,  1.0f,	// Position 3
		 1.0f,  0.0f	// TexCoord 3
	};

	ctx->vb = [ctx->device newBufferWithBytes:vdata length:sizeof(vdata) options:MTLResourceOptionCPUCacheModeDefault];
	ctx->vb.label = @"vb";

	uint16_t idata[] = {
		0, 1, 2,
		2, 3, 0
	};

	ctx->ib = [ctx->device newBufferWithBytes:idata length:sizeof(idata) options:MTLResourceOptionCPUCacheModeDefault];
	ctx->ib.label = @"ib";

	MTLRenderPipelineDescriptor *pdesc = [MTLRenderPipelineDescriptor new];
	pdesc.label = @"cddNES";
	pdesc.sampleCount = 1;
	pdesc.vertexFunction = ctx->vs;
	pdesc.fragmentFunction = ctx->fs;
	pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	pdesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;

	ctx->pipeline_state = [ctx->device newRenderPipelineStateWithDescriptor:pdesc error:&nse];
	[pdesc release];
	if (nse != nil) {r = -1; NSLog(@"%@", nse); goto except;}

	ctx->tex = mtl_create_texture(ctx->device, width, height);
	ctx->ss_nearest = mtl_create_sampler(ctx->device, SAMPLE_NEAREST);
	ctx->ss_linear = mtl_create_sampler(ctx->device, SAMPLE_LINEAR);

	except:

	if (r != 0)
		mtl_destroy(mod_out);

	return r;
}

void mtl_destroy(struct render_mod **mod_out)
{
	struct mtl **ctx_out = (struct mtl **) mod_out;

	if (ctx_out == NULL || *ctx_out == NULL)
		return;

	struct mtl *ctx = *ctx_out;

	if (ctx->display_link != NULL) {
		if (CVDisplayLinkIsRunning(ctx->display_link))
			CVDisplayLinkStop(ctx->display_link);

		CVDisplayLinkRelease(ctx->display_link);
	}

	if (ctx->ss_nearest != nil)
		[ctx->ss_nearest release];

	if (ctx->ss_linear != nil)
		[ctx->ss_linear release];

	if (ctx->tex != nil)
		[ctx->tex release];

	if (ctx->staging != nil)
		[ctx->staging release];

	if (ctx->pipeline_state != nil)
		[ctx->pipeline_state release];

	if (ctx->vb != nil)
		[ctx->vb release];

	if (ctx->ib != nil)
		[ctx->ib release];

	if (ctx->vs != nil)
		[ctx->vs release];

	if (ctx->fs != nil)
		[ctx->fs release];

	if (ctx->library != nil)
		[ctx->library release];

	if (ctx->layer != nil)
		[ctx->layer release];

	if (ctx->cq != nil)
		[ctx->cq release];

	if (ctx->device != nil)
		[ctx->device release];

	if (ctx->semaphore != nil)
		[ctx->semaphore release];

	if (ctx->window) {
		ctx->window.contentView.wantsLayer = NO;
		ctx->window.contentView.layer = nil;
	}

	free(ctx);
	*ctx_out = NULL;
}

void mtl_get_device(struct render_mod *mod, struct render_device **device, struct render_context **context)
{
	struct mtl *ctx = (struct mtl *) mod;

	*device = (struct render_device *) ctx->cq;
	*context = (struct render_context *) ctx->staging;
}

static MTLViewport mtl_viewport(uint32_t window_w, uint32_t window_h, double ratio)
{
	MTLViewport vp = {0};
	vp.width = (double) window_w;
	vp.height = vp.width / ratio;

	if ((double) window_h / 240.0 < ((double) window_w / ratio) / 256.0) {
		vp.height = (double) window_h;
		vp.width = vp.height * ratio;
	}

	vp.originX = ((double) window_w - vp.width) / 2.0;
	vp.originY = ((double) window_h - vp.height) / 2.0;

	return vp;
}

void mtl_draw(struct render_mod *mod, uint32_t window_w, uint32_t window_h, uint32_t *pixels, uint32_t aspect)
{
	struct mtl *ctx = (struct mtl *) mod;

	MTLRegion region = MTLRegionMake2D(0, 0, 256, 240);
	[ctx->tex replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:4 * 256];

	NSScreen *screen = [NSScreen mainScreen];
	window_w *= screen.backingScaleFactor;
	window_h *= screen.backingScaleFactor;

	if (window_w != ctx->w || window_h != ctx->h) {
		ctx->w = window_w;
		ctx->h = window_h;

		if (ctx->staging)
			[ctx->staging release];

		ctx->staging = mtl_create_texture(ctx->device, ctx->w, ctx->h);

		CGSize size = {0};
		size.width = ctx->w;
		size.height = ctx->h;

		if (ctx->layer)
			ctx->layer.drawableSize = size;
	}

	MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
	rpd.colorAttachments[0].texture = ctx->staging;
	rpd.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
	rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];
	id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];

	[re setRenderPipelineState:ctx->pipeline_state];
	[re setViewport:mtl_viewport(ctx->w, ctx->h, ASPECT_RATIO(aspect))];
	[re setVertexBuffer:ctx->vb offset:0 atIndex:0];
	[re setFragmentTexture:ctx->tex atIndex:0];
	[re setFragmentSamplerState:((ctx->sampler == SAMPLE_LINEAR) ? ctx->ss_linear : ctx->ss_nearest) atIndex:0];
	[re drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip indexCount:6
		indexType:MTLIndexTypeUInt16 indexBuffer:ctx->ib indexBufferOffset:0];

	[re endEncoding];
	[cb commit];

	[rpd release];
	[cb release];
	[re release];
}

enum ParsecStatus mtl_submit_parsec(struct render_mod *mod, ParsecDSO *parsec)
{
	mod, parsec;

	return PARSEC_OK;
}

void mtl_present(struct render_mod *mod)
{
	struct mtl *ctx = (struct mtl *) mod;

	if (ctx->vsync)
		dispatch_semaphore_wait(ctx->semaphore, DISPATCH_TIME_FOREVER);

	if (ctx->layer) {
		id<CAMetalDrawable> drawable = [ctx->layer nextDrawable];
		id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];
		id<MTLBlitCommandEncoder> re = [cb blitCommandEncoder];

		MTLOrigin origin = {0};
		MTLSize size = MTLSizeMake(ctx->w, ctx->h, 1);
		[re copyFromTexture:ctx->staging sourceSlice:0 sourceLevel:0 sourceOrigin:origin sourceSize:size
			toTexture:drawable.texture destinationSlice:0 destinationLevel:0 destinationOrigin:origin];

		[re endEncoding];
		[cb presentDrawable:drawable];
		[cb commit];

		[cb release];
		[re release];
	}
}

void mtl_set_sampler(struct render_mod *mod, enum sampler sampler)
{
	struct mtl *ctx = (struct mtl *) mod;
	ctx->sampler = sampler;
}
