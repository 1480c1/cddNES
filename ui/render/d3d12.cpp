#include "d3d12.h"

#include <stdio.h>

#include <windows.h>
#include <dxgi1_4.h>
#include <d3d12.h>

#include "shaders/d3d11/vs.h"
#include "shaders/d3d11/ps.h"

#define DEBUG false

static float VERTEX_DATA[] = {
	-1.0f, -1.0f,  // position0 (bottom-left)
	 0.0f,  1.0f,  // texcoord0
	-1.0f,  1.0f,  // position1 (top-left)
	 0.0f,  0.0f,  // texcoord1
	 1.0f,  1.0f,  // position2 (top-right)
	 1.0f,  0.0f,  // texcoord2
	 1.0f, -1.0f,  // position3 (bottom-right)
	 1.0f,  1.0f   // texcoord3
};

static uint32_t INDEX_DATA[] = {
	0, 1, 2,
	2, 3, 0
};

struct d3d12 {
	ID3D12Device *device;
	ID3D12CommandQueue *cq;
	IDXGISwapChain3 *swap_chain3;
	ID3D12DescriptorHeap *rtv_heap;
	ID3D12DescriptorHeap *srv_heap;
	ID3D12DescriptorHeap *sampler_heap;
	ID3D12RootSignature *rs;
	ID3D12PipelineState *pipeline;
	ID3D12Resource *vb;
	ID3D12Resource *ib;
	ID3D12Resource *tex;
	ID3D12Resource *tex_upload;
	ID3D12Resource *staging_tex;
	ID3D12CommandAllocator *ca;
	ID3D12GraphicsCommandList *cl;
	ID3D12Fence *fence;
	HANDLE event;
	IDXGIOutput *output;

	D3D12_GPU_DESCRIPTOR_HANDLE linear_dh;
	D3D12_GPU_DESCRIPTOR_HANDLE point_dh;

	uint64_t fence_val;

	bool vsync;
	enum sampler sampler;
	uint32_t w;
	uint32_t h;
};

static int32_t d3d12_get_output(IDXGIFactory2 *factory2, IDXGIOutput **output)
{
	int32_t r = 0;

	IDXGIAdapter1 *adapter = NULL;

	int32_t e = factory2->EnumAdapters1(0, &adapter);
	if (e == DXGI_ERROR_NOT_FOUND) {r = -1; rlog("EnumAdapters", e); goto except;}

	e = adapter->EnumOutputs(0, output);
	if (e != S_OK) {r = -1; rlog("EnumOutputs", e); goto except;}

	except:

	if (adapter)
		adapter->Release();

	return r;
}

void d3d12_set_sampler(struct render_mod *mod, enum sampler sampler)
{
	struct d3d12 *d3d12 = (struct d3d12 *) mod;

	d3d12->sampler = sampler;
}

static int32_t d3d12_buffer(ID3D12Device *device, void *bdata, uint64_t size, ID3D12Resource **b)
{
	int32_t r = 0;

	D3D12_RANGE range = {0};

	D3D12_HEAP_PROPERTIES hp = {0};
	hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	hp.CreationNodeMask = 1;
	hp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC rd = {0};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Alignment = 0;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT e = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL, __uuidof(ID3D12Resource), (void **) b);
	if (e != S_OK) {r = -1; rlog("CreateCommittedResource", e); goto except;}

	if (bdata) {
		void *data = NULL;
		e = (*b)->Map(0, &range, &data);
		if (e != S_OK) {r = -1; rlog("Map", e); goto except;}

		memcpy(data, bdata, (size_t) size);
		(*b)->Unmap(0, NULL);
	}

	except:

	if (r != 0 && *b) {
		(*b)->Release();
		*b = NULL;
	}

	return r;
}

static HRESULT d3d12_texture(ID3D12Device *device, uint32_t width, uint32_t height,
	D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, ID3D12Resource **tex)
{
	D3D12_HEAP_PROPERTIES hp = {0};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	hp.CreationNodeMask = 1;
	hp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC tdesc = {0};
	tdesc.MipLevels = 1;
	tdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.Flags = flags;
	tdesc.DepthOrArraySize = 1;
	tdesc.SampleDesc.Count = 1;
	tdesc.SampleDesc.Quality = 0;
	tdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE cv = {0};
	cv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

	return device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &tdesc, state,
		(flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? &cv : NULL,
		__uuidof(ID3D12Resource), (void **) tex);
}

static void d3d12_srv(ID3D12Device *device, ID3D12Resource *tex, D3D12_CPU_DESCRIPTOR_HANDLE dh)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC sdesc = {0};
	sdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sdesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(tex, &sdesc, dh);
}

static int32_t d3d12_root_signature(struct d3d12 *d3d12)
{
	int32_t r = 0;

	D3D12_DESCRIPTOR_RANGE range[2] = {{0}, {0}};
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].NumDescriptors = 1;
	range[0].BaseShaderRegister = 0;
	range[0].RegisterSpace = 0;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[1] = range[0];
	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

	D3D12_ROOT_PARAMETER rp[2] = {{0}, {0}};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].DescriptorTable.NumDescriptorRanges = 1;
	rp[0].DescriptorTable.pDescriptorRanges = &range[0];
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rp[1] = rp[0];
	rp[1].DescriptorTable.pDescriptorRanges = &range[1];

	D3D12_ROOT_SIGNATURE_DESC rsdesc = {0};
	rsdesc.NumParameters = 2;
	rsdesc.pParameters = rp;
	rsdesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob *signature = NULL;
	ID3DBlob *error = NULL;
	HRESULT e = D3D12SerializeRootSignature(&rsdesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error);
	if (e != S_OK) {r = -1; rlog("D3D12SerializeRootSignature", e); goto except;}

	e = d3d12->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		__uuidof(ID3D12RootSignature), (void **) &d3d12->rs);
	if (e != S_OK) {r = -1; rlog("CreateRootSignature", e); goto except;}

	except:

	if (error)
		error->Release();

	if (signature)
		signature->Release();

	return r;
}

static HRESULT d3d12_pipeline(ID3D12Device *device, ID3D12RootSignature *rs, ID3D12PipelineState **pipeline)
{
	D3D12_INPUT_ELEMENT_DESC iedescs[2] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psdesc = {0};
	psdesc.InputLayout.pInputElementDescs = iedescs;
	psdesc.InputLayout.NumElements = 2;
	psdesc.pRootSignature = rs;
	psdesc.VS.pShaderBytecode = VS;
	psdesc.VS.BytecodeLength = sizeof(VS);
	psdesc.PS.pShaderBytecode = PS;
	psdesc.PS.BytecodeLength = sizeof(PS);
	psdesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psdesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psdesc.RasterizerState.FrontCounterClockwise = FALSE;
	psdesc.RasterizerState.DepthBias = 0;
	psdesc.RasterizerState.DepthBiasClamp = 0.0f;
	psdesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
	psdesc.RasterizerState.DepthClipEnable = TRUE;
	psdesc.RasterizerState.MultisampleEnable = FALSE;
	psdesc.RasterizerState.AntialiasedLineEnable = FALSE;
	psdesc.RasterizerState.ForcedSampleCount = 0;
	psdesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	psdesc.BlendState.AlphaToCoverageEnable = FALSE;
	psdesc.BlendState.IndependentBlendEnable = FALSE;
	psdesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psdesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	psdesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	psdesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	psdesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psdesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psdesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psdesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psdesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	psdesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psdesc.DepthStencilState.DepthEnable = FALSE;
	psdesc.DepthStencilState.StencilEnable = FALSE;
	psdesc.SampleMask = UINT_MAX;
	psdesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psdesc.NumRenderTargets = 1;
	psdesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	psdesc.SampleDesc.Count = 1;

	return device->CreateGraphicsPipelineState(&psdesc, __uuidof(ID3D12PipelineState), (void **) pipeline);
}

int32_t d3d12_init(struct render_mod **mod_out, void *window, bool vsync,
	uint32_t width, uint32_t height, enum sampler sampler)
{
	struct d3d12 **d3d12_out = (struct d3d12 **) mod_out;
	struct d3d12 *d3d12 = *d3d12_out = (struct d3d12 *) calloc(1, sizeof(struct d3d12));
	int32_t r = 0;

	d3d12->vsync = vsync;
	d3d12->sampler = sampler;
	d3d12->fence_val = 1;

	IDXGIFactory2 *factory2 = NULL;
	IDXGISwapChain1 *swap_chain1 = NULL;
	D3D12_COMMAND_QUEUE_DESC qdesc = {0};
	DXGI_SWAP_CHAIN_DESC1 scdesc = {0};
	D3D12_DESCRIPTOR_HEAP_DESC dhdesc = {0};
	D3D12_SAMPLER_DESC sd = {0};


	// factory, device, command queue
	UINT factory_flags = 0;

	if (DEBUG) {
		factory_flags = DXGI_CREATE_FACTORY_DEBUG;

		ID3D12Debug *debug = NULL;
		HRESULT e = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void **) &debug);
		if (e != S_OK) {r = -1; rlog("D3D12GetDebugInterface", e); goto except;}

		debug->EnableDebugLayer();
	}

	HRESULT e = CreateDXGIFactory2(factory_flags, __uuidof(IDXGIFactory2), (void **) &factory2);
	if (e != S_OK) {r = -1; rlog("CreateDXGIFactory2", e); goto except;}

	e = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void **) &d3d12->device);
	if (e != S_OK) {r = -1; rlog("D3D12CreateDevice", e); goto except;}

	qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	e = d3d12->device->CreateCommandQueue(&qdesc, __uuidof(ID3D12CommandQueue), (void **) &d3d12->cq);
	if (e != S_OK) {r = -1; rlog("D3D12CreateCommandQueue", e); goto except;}


	// swap chain
	scdesc.BufferCount = 2;
	scdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scdesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scdesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scdesc.SampleDesc.Count = 1;
	e = factory2->CreateSwapChainForHwnd(d3d12->cq, (HWND) window, &scdesc, NULL, NULL, &swap_chain1);
	if (e != S_OK) {r = -1; rlog("CreateSwapChainForHwnd", e); goto except;}

	e = swap_chain1->QueryInterface(__uuidof(IDXGISwapChain3), (void **) &d3d12->swap_chain3);
	if (e != S_OK) {r = -1; rlog("QueryInterface", e); goto except;}


	// root signature
	r = d3d12_root_signature(d3d12);
	if (r != 0) goto except;


	// pipeline (input layout, vertex shader, pixel shader, blend mode)
	e = d3d12_pipeline(d3d12->device, d3d12->rs, &d3d12->pipeline);
	if (e != S_OK) {r = -1; rlog("CreateGraphicsPipelineState", e); goto except;}


	// vertex buffer
	r = d3d12_buffer(d3d12->device, VERTEX_DATA, sizeof(VERTEX_DATA), &d3d12->vb);
	if (r != 0) goto except;


	// index buffer
	r = d3d12_buffer(d3d12->device, INDEX_DATA, sizeof(INDEX_DATA), &d3d12->ib);
	if (r != 0) goto except;


	// input texture, GPU
	e = d3d12_texture(d3d12->device, width, height, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &d3d12->tex);
	if (e != S_OK) {r = -1; rlog("CreateCommittedResource", e); goto except;}


	// input texture, upload
	uint64_t tex_upload_size = 0;
	D3D12_RESOURCE_DESC tdesc = d3d12->tex->GetDesc();
	d3d12->device->GetCopyableFootprints(&tdesc, 0, 1, 0, NULL, NULL, NULL, &tex_upload_size);
	r = d3d12_buffer(d3d12->device, NULL, tex_upload_size, &d3d12->tex_upload);
	if (r != 0) goto except;


	// shader resource view for input texture
	memset(&dhdesc, 0, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	dhdesc.NumDescriptors = 1;
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dhdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	e = d3d12->device->CreateDescriptorHeap(&dhdesc, __uuidof(ID3D12DescriptorHeap), (void **) &d3d12->srv_heap);
	if (e != S_OK) {r = -1; rlog("CreateDescriptorHeap", e); goto except;}

	d3d12_srv(d3d12->device, d3d12->tex, d3d12->srv_heap->GetCPUDescriptorHandleForHeapStart());


	// RTV heap for staging texture
	memset(&dhdesc, 0, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	dhdesc.NumDescriptors = 1;
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	dhdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	e = d3d12->device->CreateDescriptorHeap(&dhdesc, __uuidof(ID3D12DescriptorHeap), (void **) &d3d12->rtv_heap);
	if (e != S_OK) {r = -1; rlog("CreateDescriptorHeap", e); goto except;}


	// samplers
	memset(&dhdesc, 0, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	dhdesc.NumDescriptors = 2;
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	dhdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	e = d3d12->device->CreateDescriptorHeap(&dhdesc, __uuidof(ID3D12DescriptorHeap), (void **) &d3d12->sampler_heap);
	if (e != S_OK) {r = -1; rlog("CreateDescriptorHeap", e); goto except;}

	D3D12_CPU_DESCRIPTOR_HANDLE sampler_dh = d3d12->sampler_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE sampler_dh_gpu = d3d12->sampler_heap->GetGPUDescriptorHandleForHeapStart();
	uint32_t sampler_dsize = d3d12->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	// linear
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	d3d12->device->CreateSampler(&sd, sampler_dh);
	d3d12->linear_dh = sampler_dh_gpu;
	sampler_dh.ptr += sampler_dsize;
	sampler_dh_gpu.ptr += sampler_dsize;

	// point
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	d3d12->device->CreateSampler(&sd, sampler_dh);
	d3d12->point_dh = sampler_dh_gpu;


	// command allocator, command list, fence, event
	e = d3d12->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		__uuidof(ID3D12CommandAllocator), (void **) &d3d12->ca);
	if (e != S_OK) {r = -1; rlog("CreateCommandAllocator", e); goto except;}

	e = d3d12->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->ca, NULL,
		__uuidof(ID3D12GraphicsCommandList), (void **) &d3d12->cl);
	if (e != S_OK) {r = -1; rlog("CreateCommandList", e); goto except;}

	e = d3d12->cl->Close();
	if (e != S_OK) {rlog("Close", e); goto except;}

	e = d3d12->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **) &d3d12->fence);
	if (e != S_OK) {r = -1; rlog("CreateFence", e); goto except;}

	d3d12->event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!d3d12->event) {r = -1; rlog("CreateEvent", GetLastError()); goto except;}


	// output for vsync event
	r = d3d12_get_output(factory2, &d3d12->output);
	if (r != 0) goto except;


	except:

	if (factory2)
		factory2->Release();

	if (swap_chain1)
		swap_chain1->Release();

	if (r != 0)
		d3d12_destroy(mod_out);

	return r;
}

void d3d12_destroy(struct render_mod **mod_out)
{
	struct d3d12 **d3d12_out = (struct d3d12 **) mod_out;

	if (d3d12_out == NULL || *d3d12_out == NULL)
		return;

	struct d3d12 *d3d12 = *d3d12_out;

	if (d3d12->staging_tex)
		d3d12->staging_tex->Release();

	if (d3d12->output)
		d3d12->output->Release();

	if (d3d12->event)
		CloseHandle(d3d12->event);

	if (d3d12->fence)
		d3d12->fence->Release();

	if (d3d12->cl)
		d3d12->cl->Release();

	if (d3d12->ca)
		d3d12->ca->Release();

	if (d3d12->sampler_heap)
		d3d12->sampler_heap->Release();

	if (d3d12->rtv_heap)
		d3d12->rtv_heap->Release();

	if (d3d12->srv_heap)
		d3d12->srv_heap->Release();

	if (d3d12->tex_upload)
		d3d12->tex_upload->Release();

	if (d3d12->tex)
		d3d12->tex->Release();

	if (d3d12->ib)
		d3d12->ib->Release();

	if (d3d12->vb)
		d3d12->vb->Release();

	if (d3d12->pipeline)
		d3d12->pipeline->Release();

	if (d3d12->rs)
		d3d12->rs->Release();

	if (d3d12->swap_chain3)
		d3d12->swap_chain3->Release();

	if (d3d12->cq)
		d3d12->cq->Release();

	if (d3d12->device)
		d3d12->device->Release();

	free(d3d12);
	*d3d12_out = NULL;
}

void d3d12_get_device(struct render_mod *mod, struct render_device **device, struct render_context **context)
{
	struct d3d12 *d3d12 = (struct d3d12 *) mod;

	*device = (struct render_device *) d3d12->cq;
	*context = (struct render_context *) d3d12->rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
}

static int32_t d3d12_staging(struct d3d12 *d3d12, uint32_t w, uint32_t h)
{
	if (d3d12->staging_tex) {
		d3d12->staging_tex->Release();
		d3d12->staging_tex = NULL;
	}

	HRESULT e = d3d12_texture(d3d12->device, w, h, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &d3d12->staging_tex);
	if (e != S_OK) {rlog("CreateCommittedResource", e); return -1;}

	d3d12->device->CreateRenderTargetView(d3d12->staging_tex, NULL,
		d3d12->rtv_heap->GetCPUDescriptorHandleForHeapStart());

	return 0;
}

static void d3d12_wait(struct d3d12 *d3d12)
{
	uint64_t fence_val = d3d12->fence_val;
	int32_t e = d3d12->cq->Signal(d3d12->fence, fence_val);
	if (e != S_OK) {rlog("Signal", e); return;}

	d3d12->fence_val++;

	if (d3d12->fence->GetCompletedValue() < fence_val) {
		e = d3d12->fence->SetEventOnCompletion(fence_val, d3d12->event);
		if (e != S_OK) {rlog("SetEventOnCompletion", e); return;}

		WaitForSingleObject(d3d12->event, INFINITE);
	}

	e = d3d12->ca->Reset();
	if (e != S_OK) {rlog("Reset", e); return;}
}

static void d3d12_set_viewport(struct d3d12 *d3d12, uint32_t window_w, uint32_t window_h, float ratio)
{
	D3D12_VIEWPORT viewport = {0};
	viewport.Width = (float) window_w;
	viewport.Height = viewport.Width / ratio;

	if ((float) window_h / 240.0f < ((float) window_w / ratio) / 256.0f) {
		viewport.Height = (float) window_h;
		viewport.Width = viewport.Height * ratio;
	}

	viewport.TopLeftX = ((float) window_w - viewport.Width) / 2.0f;
	viewport.TopLeftY = ((float) window_h - viewport.Height) / 2.0f;

	d3d12->cl->RSSetViewports(1, &viewport);
}

void d3d12_draw(struct render_mod *mod, uint32_t window_w, uint32_t window_h, uint32_t *pixels, uint32_t aspect)
{
	struct d3d12 *d3d12 = (struct d3d12 *) mod;


	// refresh back buffers and staging texture on window resize
	if (window_w != d3d12->w || window_h != d3d12->h) {
		int32_t e = d3d12->swap_chain3->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		if (e != S_OK) {rlog("ResizeBuffers", e); return;}

		e = d3d12_staging(d3d12, window_w, window_h);
		if (e != 0) return;

		d3d12->w = window_w;
		d3d12->h = window_h;
	}


	// begin command list
	int32_t e = d3d12->cl->Reset(d3d12->ca, d3d12->pipeline);
	if (e != S_OK) {rlog("Reset", e); return;}


	// copy pixel bytes to input texture
	D3D12_SUBRESOURCE_DATA tdata = {0};
	tdata.pData = pixels;
	tdata.RowPitch = 256 * 4;
	tdata.SlicePitch = tdata.RowPitch * 240;

	void *data = NULL;
	D3D12_RANGE range = {0};
	e = d3d12->tex_upload->Map(0, &range, &data);
	if (e != S_OK) {rlog("Map", e); return;}

	memcpy(data, pixels, 240 * 256 * 4);
	d3d12->tex_upload->Unmap(0, NULL);

	D3D12_RESOURCE_BARRIER rb = {0};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb.Transition.pResource = d3d12->tex;
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	d3d12->cl->ResourceBarrier(1, &rb);

	D3D12_TEXTURE_COPY_LOCATION tsrc = {0};
	tsrc.pResource = d3d12->tex_upload;
	tsrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	tsrc.PlacedFootprint.Offset = 0;
	tsrc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	tsrc.PlacedFootprint.Footprint.Width = 256;
	tsrc.PlacedFootprint.Footprint.Height = 240;
	tsrc.PlacedFootprint.Footprint.Depth = 1;
	tsrc.PlacedFootprint.Footprint.RowPitch = 256 * 4;

	D3D12_TEXTURE_COPY_LOCATION tdest = {0};
	tdest.pResource = d3d12->tex;
	tdest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	tdest.SubresourceIndex = 0;

	d3d12->cl->CopyTextureRegion(&tdest, 0, 0, 0, &tsrc, NULL);

	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.Subresource = 0;
	rb.Transition.pResource = d3d12->tex;
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	d3d12->cl->ResourceBarrier(1, &rb);


	// render to staging texture
	d3d12->cl->SetGraphicsRootSignature(d3d12->rs);

	ID3D12DescriptorHeap *heaps[2] = {d3d12->srv_heap, d3d12->sampler_heap};
	d3d12->cl->SetDescriptorHeaps(2, heaps);

	d3d12->cl->SetGraphicsRootDescriptorTable(0, d3d12->srv_heap->GetGPUDescriptorHandleForHeapStart());
	d3d12->cl->SetGraphicsRootDescriptorTable(1,
		(d3d12->sampler == SAMPLE_LINEAR) ? d3d12->linear_dh : d3d12->point_dh);

	d3d12_set_viewport(d3d12, window_w, window_h, ASPECT_RATIO(aspect));

	D3D12_RECT scissor = {0};
	scissor.right = window_w;
	scissor.bottom = window_h;
	d3d12->cl->RSSetScissorRects(1, &scissor);

	D3D12_CPU_DESCRIPTOR_HANDLE dh = d3d12->rtv_heap->GetCPUDescriptorHandleForHeapStart();

	float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	d3d12->cl->ClearRenderTargetView(dh, color, 0, NULL);
	d3d12->cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	d3d12->cl->OMSetRenderTargets(1, &dh, FALSE, NULL);

	D3D12_VERTEX_BUFFER_VIEW vbview = {0};
	vbview.BufferLocation = d3d12->vb->GetGPUVirtualAddress();
	vbview.StrideInBytes = 4 * sizeof(float);
	vbview.SizeInBytes = sizeof(VERTEX_DATA);
	d3d12->cl->IASetVertexBuffers(0, 1, &vbview);

	D3D12_INDEX_BUFFER_VIEW ibview = {0};
	ibview.BufferLocation = d3d12->ib->GetGPUVirtualAddress();
	ibview.SizeInBytes = sizeof(INDEX_DATA);
	ibview.Format = DXGI_FORMAT_R32_UINT;
	d3d12->cl->IASetIndexBuffer(&ibview);

	d3d12->cl->DrawIndexedInstanced(6, 1, 0, 0, 0);


	// finish command list
	e = d3d12->cl->Close();
	if (e != S_OK) {rlog("Close", e); return;}

	d3d12->cq->ExecuteCommandLists(1, (ID3D12CommandList **) &d3d12->cl);


	// wait for copy and draw to complete
	d3d12_wait(d3d12);
}

enum ParsecStatus d3d12_submit_parsec(struct render_mod *mod, ParsecDSO *parsec)
{
	struct d3d12 *d3d12 = (struct d3d12 *) mod;
	d3d12; parsec;

	return PARSEC_OK;
}

void d3d12_present(struct render_mod *mod)
{
	struct d3d12 *d3d12 = (struct d3d12 *) mod;

	ID3D12Resource *back_buffer = NULL;
	D3D12_TEXTURE_COPY_LOCATION tsrc = {0};
	D3D12_TEXTURE_COPY_LOCATION tdest = {0};


	// begin command list
	int32_t e = d3d12->cl->Reset(d3d12->ca, d3d12->pipeline);
	if (e != S_OK) {rlog("Reset", e); goto except;}


	// copy to back buffer
	uint32_t frame = d3d12->swap_chain3->GetCurrentBackBufferIndex();

	e = d3d12->swap_chain3->GetBuffer(frame, __uuidof(ID3D12Resource), (void **) &back_buffer);
	if (e != S_OK) {rlog("GetBuffer", e); goto except;}

	D3D12_RESOURCE_BARRIER rb[2] = {{0}, {0}};
	rb[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb[0].Transition.pResource = d3d12->staging_tex;
	rb[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	rb[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	rb[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb[1].Transition.pResource = back_buffer;
	rb[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	rb[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	d3d12->cl->ResourceBarrier(2, rb);

	tsrc.pResource = d3d12->staging_tex;
	tsrc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	tsrc.SubresourceIndex = 0;

	tdest.pResource = back_buffer;
	tdest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	tdest.SubresourceIndex = 0;

	d3d12->cl->CopyTextureRegion(&tdest, 0, 0, 0, &tsrc, NULL);

	rb[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb[0].Transition.pResource = d3d12->staging_tex;
	rb[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rb[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	rb[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb[1].Transition.pResource = back_buffer;
	rb[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	d3d12->cl->ResourceBarrier(2, rb);


	// finish command list
	e = d3d12->cl->Close();
	if (e != S_OK) {rlog("Close", e); goto except;}

	d3d12->cq->ExecuteCommandLists(1, (ID3D12CommandList **) &d3d12->cl);


	// present
	e = d3d12->swap_chain3->Present(d3d12->vsync ? 1 : 0, 0);
	if (e != S_OK) {rlog("Present", e); goto except;}


	// wait for present to complete
	d3d12_wait(d3d12);

	except:

	if (back_buffer)
		back_buffer->Release();
}
