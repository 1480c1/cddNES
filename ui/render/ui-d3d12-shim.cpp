#include "ui-d3d12-shim.h"

#include <d3d12.h>
#include <stdio.h>

#include "imgui/imgui_impl_dx12.h"

#if defined(__x86_64__)

struct ui_d3d12_shim {
	ID3D12Device *device;
	ID3D12CommandQueue *cq;
	ID3D12CommandAllocator *ca;
	ID3D12GraphicsCommandList *cl;
	ID3D12DescriptorHeap *srv;
	ID3D12Fence *fence;
	HANDLE event;

	uint64_t fence_val;
};

void ui_d3d12_init(struct render_device *device, struct ui_d3d12_shim **shim_out)
{
	struct ui_d3d12_shim *shim = *shim_out = (struct ui_d3d12_shim *) calloc(1, sizeof(struct ui_d3d12_shim));
	shim->fence_val = 1;


	// command queue - shared from app
	shim->cq = (ID3D12CommandQueue *) device;
	shim->cq->AddRef();


	// device obtained via command queue
	HRESULT e = shim->cq->GetDevice(__uuidof(ID3D12Device), (void **) &shim->device);
	if (e != S_OK) {rlog("GetDevice", e); return;}


	// command allocator, command list
	e = shim->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		__uuidof(ID3D12CommandAllocator), (void **) &shim->ca);
	if (e != S_OK) {rlog("CreateCommandAllocator", e); return;}

	e = shim->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, shim->ca, NULL,
		__uuidof(ID3D12GraphicsCommandList), (void **) &shim->cl);
	if (e != S_OK) {rlog("CreateCommandList", e); return;}

	e = shim->cl->Close();
	if (e != S_OK) {rlog("Close", e); return;}


	// SRV heap for imgui fonts
	D3D12_DESCRIPTOR_HEAP_DESC dhdesc = {0};
	dhdesc.NumDescriptors = 1;
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dhdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	e = shim->device->CreateDescriptorHeap(&dhdesc, __uuidof(ID3D12DescriptorHeap), (void **) &shim->srv);
	if (e != S_OK) {rlog("CreateDescriptorHeap", e); return;}


	// fence and event
	e = shim->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **) &shim->fence);
	if (e != S_OK) {rlog("CreateFence", e); return;}

	shim->event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!shim->event) {rlog("CreateEvent", GetLastError()); return;}


	// init imgui
	ImGui_ImplDX12_Init(shim->device, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
		shim->srv->GetCPUDescriptorHandleForHeapStart(), shim->srv->GetGPUDescriptorHandleForHeapStart());
}

void ui_d3d12_shutdown(struct ui_d3d12_shim **shim_out)
{
	if (!shim_out || !*shim_out)
		return;

	struct ui_d3d12_shim *shim = *shim_out;

	ImGui_ImplDX12_Shutdown();

	if (shim->event)
		CloseHandle(shim->event);

	if (shim->fence)
		shim->fence->Release();

	if (shim->srv)
		shim->srv->Release();

	if (shim->cl)
		shim->cl->Release();

	if (shim->ca)
		shim->ca->Release();

	if (shim->cq)
		shim->cq->Release();

	if (shim->device)
		shim->device->Release();

	free(shim);
	*shim_out = NULL;
}

void ui_d3d12_new_frame(void)
{
	ImGui_ImplDX12_NewFrame();
}

void ui_d3d12_render_draw_data(struct ui_d3d12_shim *shim, struct render_context *context, ImDrawData* draw_data)
{
	D3D12_CPU_DESCRIPTOR_HANDLE dhandle = {0};
	dhandle.ptr = (UINT64) context;


	// begin command list
	HRESULT e = shim->cl->Reset(shim->ca, NULL);
	if (e != S_OK) {rlog("Reset", e); return;}


	// imgui expects a SRV heap set for the fonts and the RTV set before the RenderDrawData function
	shim->cl->SetDescriptorHeaps(1, &shim->srv);
	shim->cl->OMSetRenderTargets(1, &dhandle, FALSE, NULL);


	// render imgui
	ImGui_ImplDX12_RenderDrawData(draw_data, shim->cl);


	// execute imgui drawing commands
	e = shim->cl->Close();
	if (e != S_OK) {rlog("Close", e); return;}

	shim->cq->ExecuteCommandLists(1, (ID3D12CommandList **) &shim->cl);


	// wait for all drawing commands to complete
	uint64_t fence_val = shim->fence_val;
	e = shim->cq->Signal(shim->fence, fence_val);
	if (e != S_OK) {rlog("Signal", e); return;}

	shim->fence_val++;

	if (shim->fence->GetCompletedValue() < fence_val) {
		e = shim->fence->SetEventOnCompletion(fence_val, shim->event);
		if (e != S_OK) {rlog("SetEventOnCompletion", e); return;}

		WaitForSingleObject(shim->event, INFINITE);
	}

	e = shim->ca->Reset();
	if (e != S_OK) {rlog("Reset", e); return;}
}

#endif
