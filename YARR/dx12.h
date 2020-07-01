/************************************************************************************************/
/*			Copyright (C) 2020 By Yang Chen (yngccc@gmail.com). All Rights Reserved.			*/
/************************************************************************************************/

#pragma once

#include "miscs.h"

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>

#define USE_PIX
#include "pix3.h"

#define dx12Assert(dx12Call) \
{ \
	HRESULT dx12CallResult = dx12Call; \
	if (dx12CallResult != S_OK) { \
		char error[256] = "D3D12 error: "; \
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dx12CallResult, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error + 13, 256 - 13, nullptr); \
		OutputDebugStringA(error); \
        assert(false && #dx12Call); \
	} \
}

struct DX12CommandList {
	ID3D12CommandAllocator* allocator;
	ID3D12GraphicsCommandList4* list;
	ID3D12Fence* fence;
	uint64 fenceValue;
	HANDLE fenceEvent;
};

struct DX12Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};

struct DX12DescriptorHeap {
	ID3D12DescriptorHeap* heap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	uint64 size;
	uint64 capacity;
	uint64 descriptorSize;
};

struct DX12Buffer {
	ID3D12Resource* buffer = nullptr;
	char* bufferPtr = nullptr;
	uint64 size = 0;
	uint64 capacity = 0;
};

struct DX12Texture {
	ID3D12Resource* texture = nullptr;
};

struct DX12TextureCopy {
	DX12Texture dstTexture;
	char* srcTexture;
	uint64 srcTextureSize;
	D3D12_RESOURCE_STATES beforeResourceState;
	D3D12_RESOURCE_STATES afterResourceState;
};

struct DX12Context {
	ID3D12Device5* device;
	IDXGIAdapter1* adapter;
	ID3D12Debug1* debugController;
	ID3D12Debug2* debugController2;
	IDXGIDebug* dxgiDebug;
	IDXGIInfoQueue* dxgiInfoQueue;
	IDXGIFactory6* dxgiFactory;

	static const uint64 maxFrameInFlight = 2;
	uint64 totalFrame = 0;
	uint64 currentFrame = 0;

	ID3D12CommandQueue* graphicsCommandQueue;
	ID3D12Fence* graphicsCommandQueueFence;
	HANDLE graphicsCommandQueueFenceEvent;
	DX12CommandList graphicsCommandLists[maxFrameInFlight];

	ID3D12CommandQueue* copyCommandQueue;
	DX12CommandList copyCommandList;

	DX12DescriptorHeap rtvDescriptorHeaps[maxFrameInFlight];
	DX12DescriptorHeap dsvDescriptorHeaps[maxFrameInFlight];
	DX12DescriptorHeap cbvSrvUavDescriptorHeaps[maxFrameInFlight];

	IDXGISwapChain4* swapChain;
	ID3D12Resource* swapChainImages[3];

	DX12Buffer constantsBuffers[maxFrameInFlight];
	DX12Buffer imguiVertexBuffers[maxFrameInFlight];
	DX12Buffer imguiIndexBuffers[maxFrameInFlight];

	DX12Texture colorTexture;
	DX12Texture depthTexture;
	DX12Texture imguiTexture;

	ID3D12RootSignature* swapChainRootSignature = nullptr;
	ID3D12PipelineState* swapChainPipelineState = nullptr;

	ID3D12RootSignature* imguiRootSignature = nullptr;
	ID3D12PipelineState* imguiPipelineState = nullptr;

	ID3D12StateObject* rayTracingStateObject = nullptr;
	ID3D12StateObjectProperties* rayTracingObjectProps = nullptr;
	DX12Buffer rayTracingShaderTableBuffers[maxFrameInFlight];
	uint64 rayTracingShaderRecordSize;

	DX12Context() = default;
	DX12Context(const Window& window) {
		{
			dx12Assert(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
			dx12Assert(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController2)));
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(true);
			debugController->SetEnableSynchronizedCommandQueueValidation(true);
			debugController2->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_NONE);

			dx12Assert(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)));
			dx12Assert(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)));
			dx12Assert(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
			dx12Assert(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
			dx12Assert(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true));

			dx12Assert(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory)));
			dx12Assert(dxgiFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
			dx12Assert(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

			D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
			dx12Assert(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)));
			assert(features.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0 && "D3D12 error: RaytracingTier < D3D12_RAYTRACING_TIER_1_0");

			//D3D12_FEATURE_DATA_D3D12_OPTIONS7 featureData = {};
			//device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &featureData, sizeof(featureData));
			//if (featureData.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1) {
			//	//Supported Mesh Shader Use
			//	int stop = 0;
			//}
		}
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			dx12Assert(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&graphicsCommandQueue)));
			dx12Assert(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&graphicsCommandQueueFence)));
			graphicsCommandQueueFenceEvent = CreateEventA(nullptr, false, false, nullptr);
			assert(graphicsCommandQueueFenceEvent && "D3D12 error: CreateEvent failed to create graphicsCommandQueueFenceEvent");

			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			dx12Assert(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&copyCommandQueue)));

			for (auto& list : graphicsCommandLists) {
				dx12Assert(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&list.allocator)));
				dx12Assert(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, list.allocator, nullptr, IID_PPV_ARGS(&list.list)));

				dx12Assert(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&list.fence)));
				list.fenceValue = 0;
				list.fenceEvent = CreateEventA(nullptr, false, false, nullptr);
				assert(list.fenceEvent && "D3D12 error: CreateEvent failed to create graphicsCommandList");
			}

			dx12Assert(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&copyCommandList.allocator)));
			dx12Assert(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, copyCommandList.allocator, nullptr, IID_PPV_ARGS(&copyCommandList.list)));

			dx12Assert(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&copyCommandList.fence)));
			copyCommandList.fenceValue = 0;
			copyCommandList.fenceEvent = CreateEventA(nullptr, false, false, nullptr);
			assert(copyCommandList.fenceEvent && "D3D12 error: CreateEvent failed to create graphicsCommandList");
		}
		{
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.Width = window.width;
			swapChainDesc.Height = window.height;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.BufferCount = countof<UINT>(swapChainImages);
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			dx12Assert(dxgiFactory->CreateSwapChainForHwnd(graphicsCommandQueue, window.handle, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain)));
			for (int i = 0; i < countof(swapChainImages); i += 1) {
				swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainImages[i]));
				swapChainImages[i]->SetName(L"swapChain");
			}
		}
		for (int i = 0; i < maxFrameInFlight; i += 1) {
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = 16;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			dx12Assert(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvDescriptorHeaps[i].heap)));
			rtvDescriptorHeaps[i].cpuHandle = rtvDescriptorHeaps[i].heap->GetCPUDescriptorHandleForHeapStart();
			rtvDescriptorHeaps[i].gpuHandle = rtvDescriptorHeaps[i].heap->GetGPUDescriptorHandleForHeapStart();
			rtvDescriptorHeaps[i].size = 0;
			rtvDescriptorHeaps[i].capacity = heapDesc.NumDescriptors;
			rtvDescriptorHeaps[i].descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			heapDesc.NumDescriptors = 4;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			dx12Assert(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsvDescriptorHeaps[i].heap)));
			dsvDescriptorHeaps[i].cpuHandle = dsvDescriptorHeaps[i].heap->GetCPUDescriptorHandleForHeapStart();
			dsvDescriptorHeaps[i].gpuHandle = dsvDescriptorHeaps[i].heap->GetGPUDescriptorHandleForHeapStart();
			dsvDescriptorHeaps[i].size = 0;
			dsvDescriptorHeaps[i].capacity = heapDesc.NumDescriptors;
			dsvDescriptorHeaps[i].descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

			heapDesc.NumDescriptors = 10000;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			dx12Assert(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvSrvUavDescriptorHeaps[i].heap)));
			cbvSrvUavDescriptorHeaps[i].cpuHandle = cbvSrvUavDescriptorHeaps[i].heap->GetCPUDescriptorHandleForHeapStart();
			cbvSrvUavDescriptorHeaps[i].gpuHandle = cbvSrvUavDescriptorHeaps[i].heap->GetGPUDescriptorHandleForHeapStart();
			cbvSrvUavDescriptorHeaps[i].size = 0;
			cbvSrvUavDescriptorHeaps[i].capacity = heapDesc.NumDescriptors;
			cbvSrvUavDescriptorHeaps[i].descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		for (int i = 0; i < maxFrameInFlight; i += 1) {
			constantsBuffers[i] = createBuffer(megabytes(32), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
			constantsBuffers[i].buffer->SetName(L"constantsBuffer");
			imguiVertexBuffers[i] = createBuffer(megabytes(1), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
			imguiVertexBuffers[i].buffer->SetName(L"imguiVertexBuffer");
			imguiIndexBuffers[i] = createBuffer(megabytes(1), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
			imguiIndexBuffers[i].buffer->SetName(L"imguiIndexBuffer");
		}
		{
			colorTexture = createTexture(window.width, window.height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			colorTexture.texture->SetName(L"colorTexture");
			depthTexture = createTexture(window.width, window.height, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthTexture.texture->SetName(L"depthTexture");

			ImGuiIO& io = ImGui::GetIO();
			ImFont* imFont = io.Fonts->AddFontDefault();
			assert(imFont);
			char* imguiTextureData;
			int imguiTextureWidth;
			int imguiTextureHeight;
			io.Fonts->GetTexDataAsRGBA32(reinterpret_cast<uint8**>(&imguiTextureData), &imguiTextureWidth, &imguiTextureHeight);
			imguiTexture = createTexture(imguiTextureWidth, imguiTextureHeight, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
			imguiTexture.texture->SetName(L"imguiTexture");
			DX12TextureCopy textureCopy = { imguiTexture, imguiTextureData,  4ull * imguiTextureWidth * imguiTextureHeight, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
			copyTextures(&textureCopy, 1);
		}
		compileShaders(true);
	}
	void compileShaders(bool forceCompile = false) {
		static auto swapChainVSWriteTime = std::filesystem::last_write_time("swapChainVS.cso");
		static auto swapChainPSWriteTime = std::filesystem::last_write_time("swapChainPS.cso");
		static auto imguiVSWriteTime = std::filesystem::last_write_time("imguiVS.cso");
		static auto imguiPSWriteTime = std::filesystem::last_write_time("imguiPS.cso");
		static auto sceneRTWriteTime = std::filesystem::last_write_time("sceneRT.cso");

		auto curSwapChainVSWriteTime = std::filesystem::last_write_time("swapChainVS.cso");
		auto curSwapChainPSWriteTime = std::filesystem::last_write_time("swapChainPS.cso");
		auto curImguiVSWriteTime = std::filesystem::last_write_time("imguiVS.cso");
		auto curImguiPSWriteTime = std::filesystem::last_write_time("imguiPS.cso");
		auto curSceneRTWriteTime = std::filesystem::last_write_time("sceneRT.cso");

		bool compileSwapChainShaders = curSwapChainVSWriteTime > swapChainVSWriteTime || curSwapChainPSWriteTime > swapChainPSWriteTime;
		bool compileImguiShaders = curImguiVSWriteTime > imguiVSWriteTime || curImguiPSWriteTime > imguiPSWriteTime;
		bool compileSceneRTShader = curSceneRTWriteTime > sceneRTWriteTime;
		
		if (forceCompile || compileSwapChainShaders || compileImguiShaders || compileSceneRTShader) {
			drainGraphicsCommandQueue();
		}
		if (forceCompile || compileSwapChainShaders) {
			if (swapChainRootSignature) {
				swapChainRootSignature->Release();
			}
			if (swapChainPipelineState) {
				swapChainPipelineState->Release();
			}
			swapChainVSWriteTime = curSwapChainVSWriteTime;
			swapChainPSWriteTime = curSwapChainPSWriteTime;

			std::vector<char> vsBytecode = readFile("swapChainVS.cso");
			std::vector<char> psBytecode = readFile("swapChainPS.cso");
			dx12Assert(device->CreateRootSignature(0, psBytecode.data(), psBytecode.size(), IID_PPV_ARGS(&swapChainRootSignature)));
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.VS = { vsBytecode.data(), vsBytecode.size() };
			psoDesc.PS = { psBytecode.data(), psBytecode.size() };
			psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count = 1;
			dx12Assert(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&swapChainPipelineState)));
		}
		if (forceCompile || compileImguiShaders) {
			if (imguiRootSignature) {
				imguiRootSignature->Release();
			}
			if (imguiPipelineState) {
				imguiPipelineState->Release();
			}
			imguiVSWriteTime = curImguiVSWriteTime;
			imguiPSWriteTime = curImguiPSWriteTime;

			std::vector<char> vsBytecode = readFile("imguiVS.cso");
			std::vector<char> psBytecode = readFile("imguiPS.cso");
			dx12Assert(device->CreateRootSignature(0, psBytecode.data(), psBytecode.size(), IID_PPV_ARGS(&imguiRootSignature)));
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.VS = { vsBytecode.data(), vsBytecode.size() };
			psoDesc.PS = { psBytecode.data(), psBytecode.size() };
			psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
			psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.RasterizerState.DepthClipEnable = true;
			D3D12_INPUT_ELEMENT_DESC inputElemDesc[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
			};
			psoDesc.InputLayout = { inputElemDesc, countof<UINT>(inputElemDesc) };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count = 1;
			dx12Assert(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&imguiPipelineState)));
		}
		if (forceCompile || compileSceneRTShader) {
			if (rayTracingStateObject) {
				rayTracingStateObject->Release();
			}
			if (rayTracingObjectProps) {
				rayTracingObjectProps->Release();
			}
			sceneRTWriteTime = curSceneRTWriteTime;

			D3D12_STATE_SUBOBJECT stateSubobjects[6] = {};

			std::vector<char> bytecode = readFile("sceneRT.cso");
			D3D12_EXPORT_DESC exportDescs[] = { {L"rayGen"}, {L"closestHit"}, {L"anyHit"}, {L"miss"} };
			D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
			dxilLibDesc.DXILLibrary.pShaderBytecode = bytecode.data();
			dxilLibDesc.DXILLibrary.BytecodeLength = bytecode.size();
			dxilLibDesc.NumExports = countof<UINT>(exportDescs);
			dxilLibDesc.pExports = exportDescs;
			stateSubobjects[0] = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxilLibDesc };

			D3D12_DESCRIPTOR_RANGE descriptorRange[4] = {};
			descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			descriptorRange[0].NumDescriptors = 1;
			descriptorRange[0].OffsetInDescriptorsFromTableStart = 0;
			descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			descriptorRange[1].NumDescriptors = 1;
			descriptorRange[1].OffsetInDescriptorsFromTableStart = 1;
			descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descriptorRange[2].NumDescriptors = 4;
			descriptorRange[2].OffsetInDescriptorsFromTableStart = 2;
			descriptorRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descriptorRange[3].NumDescriptors = UINT_MAX;
			descriptorRange[3].BaseShaderRegister = 4;
			descriptorRange[3].OffsetInDescriptorsFromTableStart = 6;

			D3D12_ROOT_PARAMETER rootParams[1] = {};
			rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParams[0].DescriptorTable.NumDescriptorRanges = countof<UINT>(descriptorRange);
			rootParams[0].DescriptorTable.pDescriptorRanges = descriptorRange;

			D3D12_STATIC_SAMPLER_DESC staticSamplerDesc = {};
			staticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
			rootSigDesc.NumParameters = countof<UINT>(rootParams);
			rootSigDesc.pParameters = rootParams;
			rootSigDesc.NumStaticSamplers = 1;
			rootSigDesc.pStaticSamplers = &staticSamplerDesc;
			rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			ID3D12RootSignature* rootSig = nullptr;
			ID3DBlob* rootSigBlob = nullptr;
			ID3DBlob* rootSigError = nullptr;
			dx12Assert(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &rootSigError));
			dx12Assert(device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig)));
			rootSigBlob->Release();
			stateSubobjects[1] = { D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &rootSig };

			const wchar_t* exportNames[] = { L"rayGen", L"closestHit", L"anyHit", L"miss", };
			D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association0 = {};
			association0.NumExports = countof<UINT>(exportNames);
			association0.pExports = exportNames;
			association0.pSubobjectToAssociate = &stateSubobjects[1];
			stateSubobjects[2] = { D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association0 };

			D3D12_HIT_GROUP_DESC hitGroupDesc = {};
			hitGroupDesc.HitGroupExport = L"hitGroup";
			hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			hitGroupDesc.AnyHitShaderImport = L"anyHit";
			hitGroupDesc.ClosestHitShaderImport = L"closestHit";
			stateSubobjects[3] = { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDesc };

			D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
			pipelineConfig.MaxTraceRecursionDepth = 2;
			stateSubobjects[4] = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig };

			D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
			shaderConfig.MaxPayloadSizeInBytes = 32;
			shaderConfig.MaxAttributeSizeInBytes = 8;
			stateSubobjects[5] = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig };

			D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
			stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			stateObjectDesc.NumSubobjects = countof<UINT>(stateSubobjects);
			stateObjectDesc.pSubobjects = stateSubobjects;
			dx12Assert(device->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&rayTracingStateObject)));
			dx12Assert(rayTracingStateObject->QueryInterface(IID_PPV_ARGS(&rayTracingObjectProps)));

			rayTracingShaderRecordSize = align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 32, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
			for (int i = 0; i < maxFrameInFlight; i += 1) {
				rayTracingShaderTableBuffers[i] = createBuffer(3 * rayTracingShaderRecordSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				rayTracingShaderTableBuffers[i].buffer->SetName(L"rtSceneShaderTable");
			}
		}
	}
	DX12Buffer createBuffer(uint64 capacity, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES resourceState) const {
		D3D12_HEAP_PROPERTIES heap_prop = {};
		heap_prop.Type = heapType;
		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Width = capacity;
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.Flags = resourceFlags;
		DX12Buffer buffer;
		buffer.capacity = capacity;
		dx12Assert(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &resource_desc, resourceState, nullptr, IID_PPV_ARGS(&buffer.buffer)));
		return buffer;
	}
	DX12Texture createTexture(uint width, uint height, uint arraySize, uint mips, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES resourceState) const {
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = arraySize;
		resourceDesc.MipLevels = mips;
		resourceDesc.Format = format;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = resourceFlags;

		ID3D12Resource* texture = nullptr;
		if (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET || resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
			D3D12_CLEAR_VALUE clearValue = { format };
			dx12Assert(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, resourceState, &clearValue, IID_PPV_ARGS(&texture)));
		}
		else {
			dx12Assert(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, resourceState, nullptr, IID_PPV_ARGS(&texture)));
		}
		return DX12Texture{ texture };
	}
	void copyTextures(DX12TextureCopy* textureCopies, uint64 textureCopiesCount) {
		static std::vector<DX12Buffer> stageBuffers;
		for (uint64 i = 0; i < textureCopiesCount; i += 1) {
			DX12TextureCopy& textureCopy = textureCopies[i];
			D3D12_RESOURCE_DESC textureDesc = textureCopy.dstTexture.texture->GetDesc();
			int subresourceCount = static_cast<int>(textureDesc.MipLevels * textureDesc.DepthOrArraySize);
			std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
			std::vector<UINT> rowCounts;
			std::vector<UINT64> rowSizes;
			footprints.resize(subresourceCount);
			rowCounts.resize(subresourceCount);
			rowSizes.resize(subresourceCount);
			uint64 totalSize = 0;
			device->GetCopyableFootprints(&textureDesc, 0, subresourceCount, 0, footprints.data(), rowCounts.data(), rowSizes.data(), &totalSize);
			assert(totalSize >= textureCopy.srcTextureSize);

			DX12Buffer stageBuffer = createBuffer(totalSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
			stageBuffers.push_back(stageBuffer);

			char* stageBufferPtr = nullptr;
			D3D12_RANGE mapBufferRange = { 0, 0 };
			stageBuffer.buffer->Map(0, &mapBufferRange, reinterpret_cast<void**>(&stageBufferPtr));
			char* srcTexturePtr = textureCopy.srcTexture;
			for (int subresourceIndex = 0; subresourceIndex < subresourceCount; subresourceIndex += 1) {
				UINT64 offset = footprints[subresourceIndex].Offset;
				UINT rowPitch = footprints[subresourceIndex].Footprint.RowPitch;
				UINT rowCount = rowCounts[subresourceIndex];
				UINT64 rowSize = rowSizes[subresourceIndex];
				char* ptr = stageBufferPtr + offset;
				for (int rowIndex = 0; rowIndex < static_cast<int>(rowCount); rowIndex += 1) {
					memcpy(ptr, srcTexturePtr, rowSize);
					ptr += rowPitch;
					srcTexturePtr += rowSize;
				}
			}
			stageBuffer.buffer->Unmap(0, &mapBufferRange);

			if (textureCopy.beforeResourceState != D3D12_RESOURCE_STATE_COPY_DEST) {
				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barrier.Transition.pResource = textureCopy.dstTexture.texture;
				barrier.Transition.StateBefore = textureCopy.beforeResourceState;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
				copyCommandList.list->ResourceBarrier(1, &barrier);
			}
			for (int subresourceIndex = 0; subresourceIndex < subresourceCount; subresourceIndex += 1) {
				D3D12_TEXTURE_COPY_LOCATION dstCopyLocation = { textureCopy.dstTexture.texture, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
				dstCopyLocation.SubresourceIndex = subresourceIndex;
				D3D12_TEXTURE_COPY_LOCATION srcCopyLocation = { stageBuffer.buffer, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
				srcCopyLocation.PlacedFootprint = footprints[subresourceIndex];
				copyCommandList.list->CopyTextureRegion(&dstCopyLocation, 0, 0, 0, &srcCopyLocation, nullptr);
			}
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.pResource = textureCopy.dstTexture.texture;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter = textureCopy.afterResourceState;
			copyCommandList.list->ResourceBarrier(1, &barrier);
		}
		closeAndExecuteCommandList(copyCommandList);
		waitAndResetCommandList(copyCommandList);
		for (auto& buffer : stageBuffers) {
			buffer.buffer->Release();
		}
		stageBuffers.resize(0);
	}
	uint64 appendConstantBuffer(void* data, uint64 dataSize) {
		DX12Buffer& buffer = constantsBuffers[currentFrame];
		uint64 alignedSize = align(buffer.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		assert(alignedSize + dataSize < buffer.capacity);
		memcpy(buffer.bufferPtr + alignedSize, data, dataSize);
		buffer.size = alignedSize + dataSize;
		return alignedSize;
	}
	void resetAndMapConstantBuffer() {
		DX12Buffer& buffer = constantsBuffers[currentFrame];
		buffer.size = 0;
		dx12Assert(buffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&constantsBuffers[currentFrame].bufferPtr)));
	}
	void unmapConstantBuffer() {
		constantsBuffers[currentFrame].buffer->Unmap(0, nullptr);
	}
	void resetDescriptorHeaps() {
		rtvDescriptorHeaps[currentFrame].size = 0;
		dsvDescriptorHeaps[currentFrame].size = 0;
		cbvSrvUavDescriptorHeaps[currentFrame].size = 0;
	}
	DX12Descriptor appendDescriptorRTV(ID3D12Resource* resource) {
		DX12DescriptorHeap& heap = rtvDescriptorHeaps[currentFrame];
		assert(heap.size < heap.capacity && "D3D12 error: appendRTV exceeded heap capacity");
		DX12Descriptor descriptor = { {heap.cpuHandle.ptr + heap.descriptorSize * heap.size}, {heap.gpuHandle.ptr + heap.descriptorSize * heap.size} };
		device->CreateRenderTargetView(resource, nullptr, descriptor.cpuHandle);
		heap.size += 1;
		return descriptor;
	}
	DX12Descriptor appendDescriptorCBV(ID3D12Resource* resource, uint64 offset, uint64 size) {
		DX12DescriptorHeap& heap = cbvSrvUavDescriptorHeaps[currentFrame];
		assert(heap.size < heap.capacity && "D3D12 error: appendCBV exceeded heap capacity");
		DX12Descriptor descriptor = { {heap.cpuHandle.ptr + heap.descriptorSize * heap.size}, {heap.gpuHandle.ptr + heap.descriptorSize * heap.size} };
		uint64 alignedSize = align(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc = { resource->GetGPUVirtualAddress() + offset, static_cast<uint>(alignedSize) };
		device->CreateConstantBufferView(&desc, descriptor.cpuHandle);
		heap.size += 1;
		return descriptor;
	}
	DX12Descriptor appendDescriptorUAV(ID3D12Resource* resource) {
		DX12DescriptorHeap& heap = cbvSrvUavDescriptorHeaps[currentFrame];
		assert(heap.size < heap.capacity && "D3D12 error: appendUAV exceeded heap capacity");
		DX12Descriptor descriptor = { {heap.cpuHandle.ptr + heap.descriptorSize * heap.size}, {heap.gpuHandle.ptr + heap.descriptorSize * heap.size} };
		device->CreateUnorderedAccessView(resource, nullptr, nullptr, descriptor.cpuHandle);
		heap.size += 1;
		return descriptor;
	}
	DX12Descriptor appendDescriptorSRVTexture(ID3D12Resource* resource) {
		DX12DescriptorHeap& heap = cbvSrvUavDescriptorHeaps[currentFrame];
		assert(heap.size < heap.capacity && "D3D12 error: appendSRVTexture exceeded heap capacity");
		DX12Descriptor descriptor = { {heap.cpuHandle.ptr + heap.descriptorSize * heap.size}, {heap.gpuHandle.ptr + heap.descriptorSize * heap.size} };
		device->CreateShaderResourceView(resource, nullptr, descriptor.cpuHandle);
		heap.size += 1;
		return descriptor;
	}
	DX12Descriptor appendDescriptorSRVStructuredBuffer(ID3D12Resource* resource, uint64 firstElem, uint64 numElem, uint64 stride) {
		DX12DescriptorHeap& heap = cbvSrvUavDescriptorHeaps[currentFrame];
		assert(heap.size < heap.capacity && "D3D12 error: appendSRVStructuredBuffer exceeded heap capacity");
		DX12Descriptor descriptor = { {heap.cpuHandle.ptr + heap.descriptorSize * heap.size}, {heap.gpuHandle.ptr + heap.descriptorSize * heap.size} };
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Buffer.FirstElement = firstElem;
		desc.Buffer.NumElements = static_cast<uint>(numElem);
		desc.Buffer.StructureByteStride = static_cast<uint>(stride);
		device->CreateShaderResourceView(resource, &desc, descriptor.cpuHandle);
		heap.size += 1;
		return descriptor;
	}
	DX12Descriptor appendDescriptorSRVTLAS(ID3D12Resource* resource) {
		DX12DescriptorHeap& heap = cbvSrvUavDescriptorHeaps[currentFrame];
		assert(heap.size < heap.capacity && "D3D12 error: appendSRVTLAS exceeded heap capacity");
		DX12Descriptor descriptor = { {heap.cpuHandle.ptr + heap.descriptorSize * heap.size}, {heap.gpuHandle.ptr + heap.descriptorSize * heap.size} };
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.RaytracingAccelerationStructure.Location = resource->GetGPUVirtualAddress();
		device->CreateShaderResourceView(nullptr, &srvDesc, descriptor.cpuHandle);
		heap.size += 1;
		return descriptor;
	}
	void closeAndExecuteCommandList(DX12CommandList& cmdList) {
		dx12Assert(cmdList.list->Close());
		graphicsCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&cmdList.list));
		cmdList.fenceValue += 1;
		dx12Assert(cmdList.fence->SetEventOnCompletion(cmdList.fenceValue, cmdList.fenceEvent));
		dx12Assert(graphicsCommandQueue->Signal(cmdList.fence, cmdList.fenceValue));
	}
	void waitAndResetCommandList(DX12CommandList& cmdList) {
		DWORD waitResult = WaitForSingleObject(cmdList.fenceEvent, INFINITE);
		assert(waitResult == WAIT_OBJECT_0 && "WaitForSingleObject error");
		dx12Assert(cmdList.allocator->Reset());
		dx12Assert(cmdList.list->Reset(cmdList.allocator, nullptr));
	}
	void drainGraphicsCommandQueue() const {
		static uint64 signalValue = 0;
		signalValue += 1;
		dx12Assert(graphicsCommandQueueFence->SetEventOnCompletion(signalValue, graphicsCommandQueueFenceEvent));
		dx12Assert(graphicsCommandQueue->Signal(graphicsCommandQueueFence, signalValue));
		DWORD waitResult = WaitForSingleObject(graphicsCommandQueueFenceEvent, INFINITE);
		assert(waitResult == WAIT_OBJECT_0 && "WaitForSingleObject error");
	}
	void resizeSwapChain(int width, int height) {
		drainGraphicsCommandQueue();
		for (int i = 0; i < countof(swapChainImages); i += 1) {
			swapChainImages[i]->Release();
		}
		dx12Assert(swapChain->ResizeBuffers(countof<UINT>(swapChainImages), width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
		for (int i = 0; i < countof(swapChainImages); i += 1) {
			swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainImages[i]));
			swapChainImages[i]->SetName(L"swapChain");
		}
	}
};
