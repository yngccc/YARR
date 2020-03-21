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

#define dx12Assert(dx12Call) \
do { \
	HRESULT hr = dx12Call; \
	if (hr != S_OK) { \
		if (IsDebuggerPresent()) { \
			__debugbreak(); \
		} \
		else { \
			char* msg = new char[1024]; \
			snprintf(msg, 1024, "D3D Error:\n%s", #dx12Call); \
			DWORD response = 0; \
			char title[] = ""; \
			WTSSendMessageA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, title, 0, msg, (DWORD)strlen(msg), MB_OK, 0, &response, FALSE); \
		} \
		ExitProcess(1); \
	} \
} while(0);

struct DX12CommandList {
	ID3D12CommandAllocator* allocator;
	ID3D12GraphicsCommandList4* list;
	ID3D12Fence* fence;
	uint64_t fenceValue;
	HANDLE fenceEvent;
};

struct DX12DescriptorHeap {
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	ID3D12DescriptorHeap* heap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	int size, capacity, descriptorSize;
};

struct DX12TextureCopy {
	ID3D12Resource* dstTexture;
	char* srcTexture;
	int srcTextureSize;
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
	IDXGIFactory5* dxgiFactory;
	bool dxrSupport;

	ID3D12CommandQueue* graphicsCommandQueue;
	ID3D12Fence* graphicsCommandQueueFence;
	HANDLE graphicsCommandQueueFenceEvent;

	ID3D12CommandQueue* copyCommandQueue;

	DX12CommandList graphicsCommandLists[1];
	int currentGraphicsCommandListIndex;

	DX12CommandList copyCommandList;

	DX12DescriptorHeap rtvDescriptorHeap;
	DX12DescriptorHeap dsvDescriptorHeap;
	DX12DescriptorHeap cbvSrvUavDescriptorHeap;

	IDXGISwapChain4* swapChain;
	ID3D12Resource* swapChainImages[2];
	int swapChainImageRTVDescriptorIndices[2];
	int swapChainWidth, swapChainHeight;

	static DX12Context create(Window* window) {
		DX12Context dx12 = {};
		{ // device
			dx12Assert(D3D12GetDebugInterface(IID_PPV_ARGS(&dx12.debugController)));
			dx12Assert(D3D12GetDebugInterface(IID_PPV_ARGS(&dx12.debugController2)));
			dx12.debugController->EnableDebugLayer();
			//dx12.debugController->SetEnableGPUBasedValidation(true);
			//dx12.debugController->SetEnableSynchronizedCommandQueueValidation(true);
			//dx12.debugController2->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_NONE);

			dx12Assert(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dx12.dxgiDebug)));
			dx12Assert(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dx12.dxgiInfoQueue)));
			dx12Assert(dx12.dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
			dx12Assert(dx12.dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
			dx12Assert(dx12.dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true));

			dx12Assert(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dx12.dxgiFactory)));

			for (int i = 0; dx12.dxgiFactory->EnumAdapters1(i, &dx12.adapter) != DXGI_ERROR_NOT_FOUND; i += 1) {
				DXGI_ADAPTER_DESC1 adapter_desc;
				dx12.adapter->GetDesc1(&adapter_desc);
				if (!(adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
					if (SUCCEEDED(D3D12CreateDevice(dx12.adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr))) {
						break;
					}
				}
			}
			dx12Assert(D3D12CreateDevice(dx12.adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dx12.device)));
		}
		{ // features
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
			dx12Assert(dx12.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)));
			dx12.dxrSupport = features.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
		}
		{ // command queue, command lists
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			dx12Assert(dx12.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&dx12.graphicsCommandQueue)));
			dx12Assert(dx12.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dx12.graphicsCommandQueueFence)));
			dx12.graphicsCommandQueueFenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
			assert(dx12.graphicsCommandQueueFenceEvent);

			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			dx12Assert(dx12.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&dx12.copyCommandQueue)));

			for (auto& list : dx12.graphicsCommandLists) {
				dx12Assert(dx12.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&list.allocator)));
				dx12Assert(dx12.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, list.allocator, nullptr, IID_PPV_ARGS(&list.list)));
				list.list->Close();
				dx12Assert(dx12.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&list.fence)));
				list.fenceValue = 0;
				list.fenceEvent = CreateEventA(nullptr, FALSE, TRUE, nullptr);
				assert(list.fenceEvent);
			}
			dx12.currentGraphicsCommandListIndex = 0;

			dx12Assert(dx12.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&dx12.copyCommandList.allocator)));
			dx12Assert(dx12.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, dx12.copyCommandList.allocator, nullptr, IID_PPV_ARGS(&dx12.copyCommandList.list)));
			dx12.copyCommandList.list->Close();
			dx12Assert(dx12.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dx12.copyCommandList.fence)));
			dx12.copyCommandList.fenceValue = 0;
			dx12.copyCommandList.fenceEvent = CreateEventA(nullptr, FALSE, TRUE, nullptr);
			assert(dx12.copyCommandList.fenceEvent);
		}
		{ // descriptor heaps
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = 16;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			dx12Assert(dx12.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dx12.rtvDescriptorHeap.heap)));
			dx12.rtvDescriptorHeap.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			dx12.rtvDescriptorHeap.cpu = dx12.rtvDescriptorHeap.heap->GetCPUDescriptorHandleForHeapStart();
			dx12.rtvDescriptorHeap.gpu = dx12.rtvDescriptorHeap.heap->GetGPUDescriptorHandleForHeapStart();
			dx12.rtvDescriptorHeap.size = 0;
			dx12.rtvDescriptorHeap.capacity = heapDesc.NumDescriptors;
			dx12.rtvDescriptorHeap.descriptorSize = dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			heapDesc.NumDescriptors = 4;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			dx12Assert(dx12.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dx12.dsvDescriptorHeap.heap)));
			dx12.dsvDescriptorHeap.type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dx12.dsvDescriptorHeap.cpu = dx12.dsvDescriptorHeap.heap->GetCPUDescriptorHandleForHeapStart();
			dx12.dsvDescriptorHeap.gpu = dx12.dsvDescriptorHeap.heap->GetGPUDescriptorHandleForHeapStart();
			dx12.dsvDescriptorHeap.size = 0;
			dx12.dsvDescriptorHeap.capacity = heapDesc.NumDescriptors;
			dx12.dsvDescriptorHeap.descriptorSize = dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

			heapDesc.NumDescriptors = 10000;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			dx12Assert(dx12.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dx12.cbvSrvUavDescriptorHeap.heap)));
			dx12.cbvSrvUavDescriptorHeap.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			dx12.cbvSrvUavDescriptorHeap.cpu = dx12.cbvSrvUavDescriptorHeap.heap->GetCPUDescriptorHandleForHeapStart();
			dx12.cbvSrvUavDescriptorHeap.gpu = dx12.cbvSrvUavDescriptorHeap.heap->GetGPUDescriptorHandleForHeapStart();
			dx12.cbvSrvUavDescriptorHeap.size = 0;
			dx12.cbvSrvUavDescriptorHeap.capacity = heapDesc.NumDescriptors;
			dx12.cbvSrvUavDescriptorHeap.descriptorSize = dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		{ // swap chain
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.Width = window->width;
			swapChainDesc.Height = window->height;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.BufferCount = countof<UINT>(dx12.swapChainImages);
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			dx12Assert(dx12.dxgiFactory->CreateSwapChainForHwnd(dx12.graphicsCommandQueue, window->handle, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&dx12.swapChain)));
			for (int i = 0; i < countof(dx12.swapChainImages); i += 1) {
				dx12.swapChain->GetBuffer(i, IID_PPV_ARGS(&dx12.swapChainImages[i]));
				dx12.swapChainImages[i]->SetName(L"swapChain");
				dx12.swapChainImageRTVDescriptorIndices[i] = dx12.createRTV(dx12.swapChainImages[i]);
			}
			dx12.swapChainWidth = window->width;
			dx12.swapChainHeight = window->height;
		}
		return dx12;
	}
	ID3D12Resource* createBuffer(size_t capacity, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES resourceState) {
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

		ID3D12Resource* buffer = nullptr;
		dx12Assert(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &resource_desc, resourceState, nullptr, IID_PPV_ARGS(&buffer)));
		return buffer;
	}
	ID3D12Resource* createTexture(int width, int height, int arraySize, int mips, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES resourceState) {
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
			D3D12_CLEAR_VALUE clearValue = { format, {0, 0, 0, 1} };
			dx12Assert(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, resourceState, &clearValue, IID_PPV_ARGS(&texture)));
		}
		else {
			dx12Assert(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, resourceState, nullptr, IID_PPV_ARGS(&texture)));
		}
		return texture;
	}
	void copyTextures(DX12TextureCopy* textureCopies, int textureCopyCount) {
		DWORD waitResult = WaitForSingleObject(copyCommandList.fenceEvent, INFINITE);
		assert(waitResult == WAIT_OBJECT_0);
		dx12Assert(copyCommandList.allocator->Reset());
		dx12Assert(copyCommandList.list->Reset(copyCommandList.allocator, nullptr));

		static std::vector<ID3D12Resource*> stageBuffers;
		for (auto& buffer : stageBuffers) {
			buffer->Release();
		}
		stageBuffers.resize(textureCopyCount);
		for (int textureCopyIndex = 0; textureCopyIndex < textureCopyCount; textureCopyIndex += 1) {
			DX12TextureCopy& textureCopy = textureCopies[textureCopyIndex];
			D3D12_RESOURCE_DESC textureDesc = textureCopy.dstTexture->GetDesc();
			int subresourceCount = static_cast<int>(textureDesc.MipLevels * textureDesc.DepthOrArraySize);
			std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
			std::vector<UINT> rowCounts;
			std::vector<UINT64> rowSizes;
			footprints.resize(subresourceCount);
			rowCounts.resize(subresourceCount);
			rowSizes.resize(subresourceCount);
			UINT64 totalSize = 0;
			device->GetCopyableFootprints(&textureDesc, 0, subresourceCount, 0, footprints.data(), rowCounts.data(), rowSizes.data(), &totalSize);
			assert(totalSize >= textureCopy.srcTextureSize);

			char* srcTexturePtr = textureCopy.srcTexture;
			ID3D12Resource* stageBuffer = createBuffer(totalSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
			stageBuffer->SetName(L"stageBuffer");
			stageBuffers[textureCopyIndex] = stageBuffer;
			char* stageBufferPtr = nullptr;
			D3D12_RANGE mapBufferRange = { 0, 0 };
			stageBuffer->Map(0, &mapBufferRange, reinterpret_cast<void**>(&stageBufferPtr));
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
			stageBuffer->Unmap(0, &mapBufferRange);

			if (textureCopy.beforeResourceState != D3D12_RESOURCE_STATE_COPY_DEST) {
				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barrier.Transition.pResource = textureCopy.dstTexture;
				barrier.Transition.StateBefore = textureCopy.beforeResourceState;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
				copyCommandList.list->ResourceBarrier(1, &barrier);
			}
			for (int subresourceIndex = 0; subresourceIndex < subresourceCount; subresourceIndex += 1) {
				D3D12_TEXTURE_COPY_LOCATION dstCopyLocation = { textureCopy.dstTexture, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
				dstCopyLocation.SubresourceIndex = subresourceIndex;
				D3D12_TEXTURE_COPY_LOCATION srcCopyLocation = { stageBuffer, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
				srcCopyLocation.PlacedFootprint = footprints[subresourceIndex];
				copyCommandList.list->CopyTextureRegion(&dstCopyLocation, 0, 0, 0, &srcCopyLocation, nullptr);
			}
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.pResource = textureCopy.dstTexture;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter = textureCopy.afterResourceState;
			copyCommandList.list->ResourceBarrier(1, &barrier);
		}
		dx12Assert(copyCommandList.list->Close());
		copyCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&copyCommandList.list));
		copyCommandList.fenceValue += 1;
		dx12Assert(copyCommandQueue->Signal(copyCommandList.fence, copyCommandList.fenceValue));
		dx12Assert(copyCommandList.fence->SetEventOnCompletion(copyCommandList.fenceValue, copyCommandList.fenceEvent));
	}
	D3D12_CPU_DESCRIPTOR_HANDLE getRTVDescriptorCPU(int index) {
		assert(index >= 0 && index < rtvDescriptorHeap.size);
		return D3D12_CPU_DESCRIPTOR_HANDLE{ rtvDescriptorHeap.cpu.ptr + static_cast<uint64_t>(rtvDescriptorHeap.descriptorSize)* index };
	}
	D3D12_GPU_DESCRIPTOR_HANDLE getRTVDescriptorGPU(int index) {
		assert(index >= 0 && index < rtvDescriptorHeap.size);
		return D3D12_GPU_DESCRIPTOR_HANDLE{ rtvDescriptorHeap.gpu.ptr + static_cast<uint64_t>(rtvDescriptorHeap.descriptorSize)* index };
	}
	D3D12_CPU_DESCRIPTOR_HANDLE getDSVDescriptorCPU(int index) {
		assert(index >= 0 && index < dsvDescriptorHeap.size);
		return D3D12_CPU_DESCRIPTOR_HANDLE{ dsvDescriptorHeap.cpu.ptr + static_cast<uint64_t>(dsvDescriptorHeap.descriptorSize)* index };
	}
	D3D12_GPU_DESCRIPTOR_HANDLE getDSVDescriptorGPU(int index) {
		assert(index >= 0 && index < dsvDescriptorHeap.size);
		return D3D12_GPU_DESCRIPTOR_HANDLE{ dsvDescriptorHeap.gpu.ptr + static_cast<uint64_t>(dsvDescriptorHeap.descriptorSize)* index };
	}
	D3D12_CPU_DESCRIPTOR_HANDLE getCbvSrvUavDescriptorCPU(int index) {
		assert(index >= 0 && index < cbvSrvUavDescriptorHeap.size);
		return D3D12_CPU_DESCRIPTOR_HANDLE{ cbvSrvUavDescriptorHeap.cpu.ptr + static_cast<uint64_t>(cbvSrvUavDescriptorHeap.descriptorSize)* index };
	}
	D3D12_GPU_DESCRIPTOR_HANDLE getCbvSrvUavDescriptorGPU(int index) {
		assert(index >= 0 && index < cbvSrvUavDescriptorHeap.size);
		return D3D12_GPU_DESCRIPTOR_HANDLE{ cbvSrvUavDescriptorHeap.gpu.ptr + static_cast<uint64_t>(cbvSrvUavDescriptorHeap.descriptorSize)* index };
	}
	int createRTV(ID3D12Resource* resource, int indexToUpdate = -1) {
		if (indexToUpdate < 0) {
			indexToUpdate = rtvDescriptorHeap.size;
			rtvDescriptorHeap.size += 1;
		}
		device->CreateRenderTargetView(resource, nullptr, getRTVDescriptorCPU(indexToUpdate));
		return indexToUpdate;
	}
	int createDSV(ID3D12Resource* resource, int indexToUpdate = -1) {
		if (indexToUpdate < 0) {
			indexToUpdate = dsvDescriptorHeap.size;
			dsvDescriptorHeap.size += 1;
		}
		device->CreateDepthStencilView(resource, nullptr, getDSVDescriptorCPU(indexToUpdate));
		return indexToUpdate;
	}
	int createCBV(ID3D12Resource* resource, int offset, int size, int indexToUpdate = -1) {
		if (indexToUpdate < 0) {
			indexToUpdate = cbvSrvUavDescriptorHeap.size;
			cbvSrvUavDescriptorHeap.size += 1;
		}
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { resource->GetGPUVirtualAddress() + offset, static_cast<UINT>(size) };
		device->CreateConstantBufferView(&cbvDesc, getCbvSrvUavDescriptorCPU(indexToUpdate));
		return indexToUpdate;
	}
	int createTextureSRV(ID3D12Resource* resource, int indexToUpdate = -1) {
		if (indexToUpdate < 0) {
			indexToUpdate = cbvSrvUavDescriptorHeap.size;
			cbvSrvUavDescriptorHeap.size += 1;
		}
		// D3D12_SHADER_RESOURCE_VIEW_DESC desc;
		device->CreateShaderResourceView(resource, nullptr, getCbvSrvUavDescriptorCPU(indexToUpdate));
		return indexToUpdate;
	}
	int createStructuredBufferSRV(ID3D12Resource* resource, int offset, int size, int stride, int indexToUpdate = -1) {
		if (indexToUpdate < 0) {
			indexToUpdate = cbvSrvUavDescriptorHeap.size;
			cbvSrvUavDescriptorHeap.size += 1;
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Buffer.FirstElement = offset;
		desc.Buffer.NumElements = size;
		desc.Buffer.StructureByteStride = stride;
		device->CreateShaderResourceView(resource, &desc, getCbvSrvUavDescriptorCPU(indexToUpdate));
		return indexToUpdate;
	}
	int createUAV(ID3D12Resource* resource, int indexToUpdate = -1) {
		if (indexToUpdate < 0) {
			indexToUpdate = cbvSrvUavDescriptorHeap.size;
			cbvSrvUavDescriptorHeap.size += 1;
		}
		device->CreateUnorderedAccessView(resource, nullptr, nullptr, getCbvSrvUavDescriptorCPU(indexToUpdate));
		return indexToUpdate;
	}
	int createAccelStructSRV(ID3D12Resource* resource, int indexToUpdate = -1) {
		if (indexToUpdate < 0) {
			indexToUpdate = cbvSrvUavDescriptorHeap.size;
			cbvSrvUavDescriptorHeap.size += 1;
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.RaytracingAccelerationStructure.Location = resource->GetGPUVirtualAddress();
		device->CreateShaderResourceView(nullptr, &srvDesc, getCbvSrvUavDescriptorCPU(indexToUpdate));
		return indexToUpdate;
	}
	void setCurrentCommandList() {
		currentGraphicsCommandListIndex = (currentGraphicsCommandListIndex + 1) % countof<int>(graphicsCommandLists);
		DX12CommandList& cmdList = graphicsCommandLists[currentGraphicsCommandListIndex];
		DWORD waitResult = WaitForSingleObject(cmdList.fenceEvent, INFINITE);
		assert(waitResult == WAIT_OBJECT_0);
		dx12Assert(cmdList.allocator->Reset());
		dx12Assert(cmdList.list->Reset(cmdList.allocator, nullptr));
	}
	void executeCurrentCommandList() {
		DX12CommandList& cmdList = graphicsCommandLists[currentGraphicsCommandListIndex];
		dx12Assert(cmdList.list->Close());
		graphicsCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&cmdList.list));
		cmdList.fenceValue += 1;
		dx12Assert(graphicsCommandQueue->Signal(cmdList.fence, cmdList.fenceValue));
		dx12Assert(cmdList.fence->SetEventOnCompletion(cmdList.fenceValue, cmdList.fenceEvent));
	}
	void presentSwapChainImage() {
		dx12Assert(swapChain->Present(1, 0));
	}
	void drainGraphicsCommandQueue() {
		static uint64_t signalValue = 0;
		signalValue += 1;
		dx12Assert(graphicsCommandQueue->Signal(graphicsCommandQueueFence, signalValue));
		dx12Assert(graphicsCommandQueueFence->SetEventOnCompletion(signalValue, graphicsCommandQueueFenceEvent));
		DWORD waitResult = WaitForSingleObject(graphicsCommandQueueFenceEvent, INFINITE);
		assert(waitResult == WAIT_OBJECT_0);
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
			createRTV(swapChainImages[i], swapChainImageRTVDescriptorIndices[i]);
		}
	}
};
