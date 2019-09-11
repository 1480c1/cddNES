#include "d3d11.h"

#include <stdio.h>

#include <windows.h>
#include <d3d11.h>

#include "shaders/d3d11/vs.h"
#include "shaders/d3d11/ps.h"

struct d3d11 {
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	IDXGISwapChain *swap_chain;
	ID3D11VertexShader *vs;
	ID3D11PixelShader *ps;
	ID3D11SamplerState *ss_point;
	ID3D11SamplerState *ss_linear;
	ID3D11SamplerState *ss;
	ID3D11Buffer *vb;
	ID3D11Buffer *ib;
	ID3D11InputLayout *il;
	ID3D11Texture2D *tex;
	ID3D11Texture2D *staging;
	ID3D11ShaderResourceView *srv;
	IDXGIOutput *output;

	bool vsync;
	uint32_t w;
	uint32_t h;
};

static int32_t d3d11_get_output(IDXGIOutput **output)
{
	IDXGIFactory1 *factory = NULL;
	int32_t e = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **) &factory);

	if (e == S_OK) {
		IDXGIAdapter1 *adapter = NULL;
		e = factory->EnumAdapters1(0, &adapter);

		if (e != DXGI_ERROR_NOT_FOUND) {
			e = adapter->EnumOutputs(0, output);

			adapter->Release();
		}

		factory->Release();
	}

	return e;
}

void d3d11_set_sampler(struct render_mod *mod, enum sampler sampler)
{
	struct d3d11 *d3d11 = (struct d3d11 *) mod;

	d3d11->ss = (sampler == SAMPLE_LINEAR) ? d3d11->ss_linear : d3d11->ss_point;
}

static int32_t d3d11_sampler_state(struct d3d11 *d3d11, enum sampler sampler)
{
	D3D11_SAMPLER_DESC sdesc = {0};
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	int32_t e = d3d11->device->CreateSamplerState(&sdesc, &d3d11->ss_point);
	if (e != S_OK) {printf("CreateSamplerState=%d", e); return -1;}

	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	e = d3d11->device->CreateSamplerState(&sdesc, &d3d11->ss_linear);
	if (e != S_OK) {printf("CreateSamplerState=%d", e); return -1;}

	d3d11_set_sampler((struct render_mod *) d3d11, sampler);

	return 0;
}

static int32_t d3d11_vertex_buffer(struct d3d11 *d3d11)
{
	float vertex_data[] = {
		-1.0f, -1.0f,  // position0 (bottom-left)
		 0.0f,  1.0f,  // texcoord0
		-1.0f,  1.0f,  // position1 (top-left)
		 0.0f,  0.0f,  // texcoord1
		 1.0f,  1.0f,  // position2 (top-right)
		 1.0f,  0.0f,  // texcoord2
		 1.0f, -1.0f,  // position3 (bottom-right)
		 1.0f,  1.0f   // texcoord3
	};

	D3D11_BUFFER_DESC bd = {0};
	bd.ByteWidth = sizeof(vertex_data);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = {vertex_data, 0, 0};
	int32_t e = d3d11->device->CreateBuffer(&bd, &srd, &d3d11->vb);
	if (e != S_OK) {printf("CreateVertexBuffer=%d", e); return -1;}

	return 0;
}

static int32_t d3d11_index_buffer(struct d3d11 *d3d11)
{
	DWORD index_data[] = {
		0, 1, 2,
		2, 3, 0
	};

	D3D11_BUFFER_DESC bd = {0};
	bd.ByteWidth = sizeof(index_data);
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = {index_data, 0, 0};
	int32_t e = d3d11->device->CreateBuffer(&bd, &srd, &d3d11->ib);
	if (e != S_OK) {printf("CreateIndexBuffer=%d", e); return -1;}

	return 0;
}

static int32_t d3d11_texture(struct d3d11 *d3d11, uint32_t width, uint32_t height)
{
	D3D11_TEXTURE2D_DESC desc = {0};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	int32_t e = d3d11->device->CreateTexture2D(&desc, NULL, &d3d11->tex);
	if (e != S_OK) {printf("CreateTexture2D=%d", e); return -1;}

	return 0;
}

static int32_t d3d11_srv(struct d3d11 *d3d11)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {0};
	srvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;

	int32_t e = d3d11->device->CreateShaderResourceView(d3d11->tex, &srvd, &d3d11->srv);
	if (e != S_OK) {printf("CreateShaderResourceView=%d", e); return -1;}

	return 0;
}

int32_t d3d11_init(struct render_mod **mod_out, void *window, bool vsync,
	uint32_t width, uint32_t height, enum sampler sampler)
{

	struct d3d11 **d3d11_out = (struct d3d11 **) mod_out;
	struct d3d11 *d3d11 = *d3d11_out = (struct d3d11 *) calloc(1, sizeof(struct d3d11));
	d3d11->vsync = vsync;

	int32_t e = 0, r = 0;

	if (window) {
		DXGI_SWAP_CHAIN_DESC sd = {0};
		sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
		sd.OutputWindow = (HWND) window;
		sd.Windowed = TRUE;
		sd.BufferCount = 1;

		e = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
			D3D11_SDK_VERSION, &sd, &d3d11->swap_chain, &d3d11->device, NULL, &d3d11->context);
		if (e != S_OK) {printf("D3D11CreateDeviceAndSwapChain=%d", e); r = -1; goto except;}

	} else {
		e = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
			&d3d11->device, NULL, &d3d11->context);
		if (e != S_OK) {printf("D3D11CreateDevice=%d", e); r = -1; goto except;}
	}

	e = d3d11_get_output(&d3d11->output);
	if (e != S_OK) {printf("d3d11_get_output=%d\n", e); r = -1; goto except;}

	e = d3d11->device->CreateVertexShader(VS, sizeof(VS), NULL, &d3d11->vs);
	if (e != S_OK) {printf("CreateVertexShader=%d\n", e); r = -1; goto except;}

	e = d3d11->device->CreatePixelShader(PS, sizeof(PS), NULL, &d3d11->ps);
	if (e != S_OK) {printf("CreatePixelShader=%d\n", e); r = -1; goto except;}

	r = d3d11_sampler_state(d3d11, sampler);
	if (r != 0) goto except;

	r = d3d11_vertex_buffer(d3d11);
	if (r != 0) goto except;

	r = d3d11_index_buffer(d3d11);
	if (r != 0) goto except;

	D3D11_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 2 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	e = d3d11->device->CreateInputLayout(ied, 2, VS, sizeof(VS), &d3d11->il);
	if (e != S_OK) {printf("CreateInputLayout=%d", e); r = -1; goto except;}

	r = d3d11_texture(d3d11, width, height);
	if (r != 0) goto except;

	r = d3d11_srv(d3d11);
	if (r != 0) goto except;

	except:

	if (r != 0)
		d3d11_destroy(mod_out);

	return r;
}

void d3d11_destroy(struct render_mod **mod_out)
{
	struct d3d11 **d3d11_out = (struct d3d11 **) mod_out;

	if (d3d11_out == NULL || *d3d11_out == NULL)
		return;

	struct d3d11 *d3d11 = *d3d11_out;

	if (d3d11->staging)
		d3d11->staging->Release();

	if (d3d11->srv)
		d3d11->srv->Release();

	if (d3d11->tex)
		d3d11->tex->Release();

	if (d3d11->il)
		d3d11->il->Release();

	if (d3d11->ib)
		d3d11->ib->Release();

	if (d3d11->vb)
		d3d11->vb->Release();

	if (d3d11->ss_point)
		d3d11->ss_point->Release();

	if (d3d11->ss_linear)
		d3d11->ss_linear->Release();

	if (d3d11->vs)
		d3d11->vs->Release();

	if (d3d11->ps)
		d3d11->ps->Release();

	if (d3d11->swap_chain)
		d3d11->swap_chain->Release();

	if (d3d11->context)
		d3d11->context->Release();

	if (d3d11->device)
		d3d11->device->Release();

	if (d3d11->output)
		d3d11->output->Release();

	free(d3d11);
	*d3d11_out = NULL;
}

void d3d11_get_device(struct render_mod *mod, struct render_device **device, struct render_context **context)
{
	struct d3d11 *d3d11 = (struct d3d11 *) mod;

	*device = (struct render_device *) d3d11->device;
	*context = (struct render_context *) d3d11->context;
}

static void d3d11_refresh_staging(struct d3d11 *d3d11, uint32_t w, uint32_t h)
{
	D3D11_TEXTURE2D_DESC desc = {0};
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	int32_t e = d3d11->device->CreateTexture2D(&desc, NULL, &d3d11->staging);
	if (e != S_OK) {printf("CreateTexture2D=%d\n", e);}
}

static void d3d11_set_viewport(struct d3d11 *d3d11, uint32_t window_w, uint32_t window_h, float ratio)
{
	D3D11_VIEWPORT viewport = {0};
	viewport.Width = (float) window_w;
	viewport.Height = viewport.Width / ratio;

	if ((float) window_h / 240.0f < ((float) window_w / ratio) / 256.0f) {
		viewport.Height = (float) window_h;
		viewport.Width = viewport.Height * ratio;
	}

	viewport.TopLeftX = ((float) window_w - viewport.Width) / 2.0f;
	viewport.TopLeftY = ((float) window_h - viewport.Height) / 2.0f;

	d3d11->context->RSSetViewports(1, &viewport);
}

void d3d11_draw(struct render_mod *mod, uint32_t window_w, uint32_t window_h, uint32_t *pixels, uint32_t aspect)
{
	struct d3d11 *d3d11 = (struct d3d11 *) mod;

	D3D11_MAPPED_SUBRESOURCE res = {0};
	if (d3d11->context->Map(d3d11->tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) == S_OK) {
		memcpy((char *) res.pData, pixels, sizeof(uint32_t) * 256 * 240);
		d3d11->context->Unmap(d3d11->tex, 0);
	}

	if (d3d11->staging == NULL || window_w != d3d11->w || window_h != d3d11->h) {
		if (d3d11->swap_chain)
			d3d11->swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

		if (d3d11->staging) {
			d3d11->staging->Release();
			d3d11->staging = NULL;
		}

		d3d11->w = window_w;
		d3d11->h = window_h;

		d3d11_refresh_staging(d3d11, window_w, window_h);
	}

	d3d11_set_viewport(d3d11, window_w, window_h, ASPECT_RATIO(aspect));

	ID3D11RenderTargetView *rtv = NULL;
	int32_t e = d3d11->device->CreateRenderTargetView(d3d11->staging, NULL, &rtv);

	if (e == S_OK) {
		UINT stride = 4 * sizeof(float);
		UINT offset = 0;

		d3d11->context->VSSetShader(d3d11->vs, NULL, 0);
		d3d11->context->PSSetShader(d3d11->ps, NULL, 0);
		d3d11->context->PSSetShaderResources(0, 1, &d3d11->srv);
		d3d11->context->PSSetSamplers(0, 1, &d3d11->ss);
		d3d11->context->IASetVertexBuffers(0, 1, &d3d11->vb, &stride, &offset);
		d3d11->context->IASetIndexBuffer(d3d11->ib, DXGI_FORMAT_R32_UINT, 0);
		d3d11->context->IASetInputLayout(d3d11->il);
		d3d11->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		d3d11->context->OMSetRenderTargets(1, &rtv, NULL);

		FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		d3d11->context->ClearRenderTargetView(rtv, clear_color);

		d3d11->context->DrawIndexed(6, 0, 0);

		rtv->Release();

	} else {
		printf("CreateRenderTargetView=%d\n", e);
	}
}

enum ParsecStatus d3d11_submit_parsec(struct render_mod *mod, ParsecDSO *parsec)
{
	struct d3d11 *d3d11 = (struct d3d11 *) mod;

	return ParsecHostD3D11SubmitFrame(parsec, d3d11->device, d3d11->context, d3d11->staging);
}

void d3d11_present(struct render_mod *mod)
{
	struct d3d11 *d3d11 = (struct d3d11 *) mod;

	if (d3d11->swap_chain) {
		ID3D11Texture2D *back_buffer = NULL;
		int32_t e = d3d11->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **) &back_buffer);

		if (e == S_OK) {
			d3d11->context->CopyResource(back_buffer, d3d11->staging);
			d3d11->swap_chain->Present(d3d11->vsync ? 1 : 0, 0);

			back_buffer->Release();

		} else {
			printf("GetBuffer=%d\n", e);
		}
	} else {
		d3d11->output->WaitForVBlank();
	}
}
