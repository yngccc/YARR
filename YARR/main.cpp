/************************************************************************************************/
/*			Copyright (C) 2020 By Yang Chen (yngccc@gmail.com). All Rights Reserved.			*/
/************************************************************************************************/

#include "scene.h"

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "libs/imgui.cpp"
#include "libs/imgui_draw.cpp"
#include "libs/imgui_widgets.cpp"
#include "libs/imgui_demo.cpp"

static bool quitProgram = false;
static Window window = {};
static DX12Context dx12 = {};

struct DX12Resources {
	ID3D12Resource* constantsBuffer;
	int constantsBufferCapacity;
	int constantsBufferCurrentOffset;

	ID3D12Resource* colorTexture;
	int colorTextureUAVDescriptorIndex;
	int colorTextureSRVDescriptorIndex;
	int colorTextureRTVDescriptorIndex;

	ID3D12Resource* depthTexture;
	int depthTextureDSVDescriptorIndex;

	ID3D12Resource* imguiVertexBuffer;
	ID3D12Resource* imguiIndexBuffer;
	int imguiVertexBufferCapacity;
	int imguiIndexBufferCapacity;

	ID3D12Resource* imguiTexture;
	int imguiTextureSRVDescriptorIndex;
};
DX12Resources dx12Resources = {};

struct DX12Pipelines {
	ID3D12RootSignature* swapChainRootSignature;
	ID3D12PipelineState* swapChainPipelineState;

	ID3D12RootSignature* imguiRootSignature;
	ID3D12PipelineState* imguiPipelineState;
};
static DX12Pipelines dx12Pipelines = {};

struct DX12RayTracing {
	ID3D12StateObject* sceneStateObject;
	ID3D12StateObjectProperties* sceneStateObjectProps;
	ID3D12Resource* sceneShaderTable;
	int sceneShaderRecordSize;
	int sceneDescriptorTableDescriptorIndex;
};
static DX12RayTracing dx12RayTracing = {};

void initImGui() {
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui::GetIO().KeyMap[ImGuiKey_Tab] = VK_TAB;
	ImGui::GetIO().KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	ImGui::GetIO().KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	ImGui::GetIO().KeyMap[ImGuiKey_UpArrow] = VK_UP;
	ImGui::GetIO().KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	ImGui::GetIO().KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	ImGui::GetIO().KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	ImGui::GetIO().KeyMap[ImGuiKey_Home] = VK_HOME;
	ImGui::GetIO().KeyMap[ImGuiKey_End] = VK_END;
	ImGui::GetIO().KeyMap[ImGuiKey_Backspace] = VK_BACK;
	ImGui::GetIO().KeyMap[ImGuiKey_Enter] = VK_RETURN;
	ImGui::GetIO().KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	ImGui::GetIO().KeyMap[ImGuiKey_A] = 'A';
	ImGui::GetIO().KeyMap[ImGuiKey_C] = 'C';
	ImGui::GetIO().KeyMap[ImGuiKey_V] = 'V';
	ImGui::GetIO().KeyMap[ImGuiKey_X] = 'X';
	ImGui::GetIO().KeyMap[ImGuiKey_Y] = 'Y';
	ImGui::GetIO().KeyMap[ImGuiKey_Z] = 'Z';
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().FontGlobalScale = 1.5f;
	ImGui::GetIO().DisplaySize = { static_cast<float>(window.width), static_cast<float>(window.height) };
	// ImGuizmo::SetRect(0, 0, static_cast<float>(window.width), static_cast<float>(window.height));
}

void initDX12Resources() {
	dx12Resources.constantsBufferCapacity = megabytes(32);
	dx12Resources.constantsBufferCurrentOffset = 0;
	dx12Resources.constantsBuffer = dx12.createBuffer(dx12Resources.constantsBufferCapacity, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
	dx12Resources.constantsBuffer->SetName(L"constantsBuffer");

	dx12Resources.colorTexture = dx12.createTexture(window.width, window.height, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dx12Resources.colorTexture->SetName(L"colorTexture");
	dx12Resources.colorTextureUAVDescriptorIndex = dx12.createUAV(dx12Resources.colorTexture);
	dx12Resources.colorTextureSRVDescriptorIndex = dx12.createTextureSRV(dx12Resources.colorTexture);
	dx12Resources.colorTextureRTVDescriptorIndex = dx12.createRTV(dx12Resources.colorTexture);

	dx12Resources.depthTexture = dx12.createTexture(window.width, window.height, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	dx12Resources.depthTexture->SetName(L"depthTexture");
	dx12Resources.depthTextureDSVDescriptorIndex = dx12.createDSV(dx12Resources.depthTexture);

	dx12Resources.imguiVertexBufferCapacity = megabytes(1);
	dx12Resources.imguiIndexBufferCapacity = megabytes(1);
	dx12Resources.imguiVertexBuffer = dx12.createBuffer(dx12Resources.imguiVertexBufferCapacity, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
	dx12Resources.imguiIndexBuffer = dx12.createBuffer(dx12Resources.imguiIndexBufferCapacity, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
	dx12Resources.imguiVertexBuffer->SetName(L"imguiVertexBuffer");
	dx12Resources.imguiIndexBuffer->SetName(L"imguiIndexBuffer");

	char* imguiTextureData = nullptr;
	int imguiTextureWidth, imguiTextureHeight;
	ImFont* imFont = ImGui::GetIO().Fonts->AddFontDefault();
	assert(imFont);
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(reinterpret_cast<unsigned char**>(&imguiTextureData), &imguiTextureWidth, &imguiTextureHeight);
	dx12Resources.imguiTexture = dx12.createTexture(imguiTextureWidth, imguiTextureHeight, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
	dx12Resources.imguiTexture->SetName(L"imguiTexture");
	dx12Resources.imguiTextureSRVDescriptorIndex = dx12.createTextureSRV(dx12Resources.imguiTexture);
	DX12TextureCopy textureCopy = { dx12Resources.imguiTexture, imguiTextureData, imguiTextureWidth * imguiTextureHeight * 4, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
	dx12.copyTextures(&textureCopy, 1);
}

void initDX12Pipelines() {
	{
		std::vector<char> vsBytecode = readFile("swapChainVS.cso");
		std::vector<char> psBytecode = readFile("swapChainPS.cso");
		dx12Assert(dx12.device->CreateRootSignature(0, psBytecode.data(), psBytecode.size(), IID_PPV_ARGS(&dx12Pipelines.swapChainRootSignature)));
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
		dx12Assert(dx12.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dx12Pipelines.swapChainPipelineState)));
	}
	{
		std::vector<char> vsBytecode = readFile("imguiVS.cso");
		std::vector<char> psBytecode = readFile("imguiPS.cso");
		dx12Assert(dx12.device->CreateRootSignature(0, psBytecode.data(), psBytecode.size(), IID_PPV_ARGS(&dx12Pipelines.imguiRootSignature)));
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
		dx12Assert(dx12.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dx12Pipelines.imguiPipelineState)));
	}
}

void initDX12RayTracing() {
	{
		std::vector<char> bytecode = readFile("rtScene.cso");
		D3D12_STATE_SUBOBJECT stateSubobjects[1];

		D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
		dxilLibDesc.DXILLibrary.pShaderBytecode = bytecode.data();
		dxilLibDesc.DXILLibrary.BytecodeLength = bytecode.size();
		D3D12_EXPORT_DESC exportDescs[] = {
			{L"rayGen"}, {L"closestHit"}, {L"miss"},
			{L"rootSig"}, {L"rootSigRayGenAssociation"}, {L"rootSigHitGroupAssociation"},
			{L"shaderConfig"}, {L"pipelineConfig"},
			{L"hitGroup"}
		};
		dxilLibDesc.NumExports = countof<UINT>(exportDescs);
		dxilLibDesc.pExports = exportDescs;
		stateSubobjects[0] = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxilLibDesc };

		D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
		stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		stateObjectDesc.NumSubobjects = countof<UINT>(stateSubobjects);
		stateObjectDesc.pSubobjects = stateSubobjects;
		dx12Assert(dx12.device->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&dx12RayTracing.sceneStateObject)));

		dx12Assert(dx12RayTracing.sceneStateObject->QueryInterface(IID_PPV_ARGS(&dx12RayTracing.sceneStateObjectProps)));

		dx12RayTracing.sceneShaderRecordSize = roundUp<int>(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 32, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		dx12RayTracing.sceneShaderTable = dx12.createBuffer(dx12RayTracing.sceneShaderRecordSize * 3, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		dx12RayTracing.sceneShaderTable->SetName(L"shaderTable");
	}
}

LRESULT windowMsgHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;
	switch (msg) {
	default: {
		result = DefWindowProcA(hwnd, msg, wparam, lparam);
	} break;
	case WM_CLOSE:
	case WM_QUIT: {
		quitProgram = true;
	} break;
	case WM_PAINT: {
		ValidateRect(hwnd, nullptr);
	} break;
	case WM_SIZE: {
		window.width = LOWORD(lparam);
		window.height = HIWORD(lparam);
		char title[128] = {};
		snprintf(title, sizeof(title), "YARR %d x %d", window.width, window.height);
		SetWindowTextA(window.handle, title);
		// ImGui::GetIO().DisplaySize = { (float)window->width, (float)window->height };
		// ImGuizmo::SetRect(0, 0, (float)window->width, (float)window->height);
		dx12.resizeSwapChain(window.width, window.height);
	} break;
	case WM_SHOWWINDOW: {
	} break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP: {
		bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
		ImGui::GetIO().KeysDown[wparam] = down;
		if (wparam == VK_SHIFT) {
			ImGui::GetIO().KeyShift = down;
		}
		else if (wparam == VK_CONTROL) {
			ImGui::GetIO().KeyCtrl = down;
		}
		else if (wparam == VK_MENU) {
			ImGui::GetIO().KeyAlt = down;
		}
	} break;
	case WM_CHAR:
	case WM_SYSCHAR: {
		ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(wparam));
	} break;
	case WM_MOUSEMOVE: {
		window.mouseX = GET_X_LPARAM(lparam);
		window.mouseY = GET_Y_LPARAM(lparam);
		ImGui::GetIO().MousePos = { static_cast<float>(window.mouseX), static_cast<float>(window.mouseY) };
	} break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP: {
		ImGui::GetIO().MouseDown[0] = (msg == WM_LBUTTONDOWN);
	} break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP: {
		ImGui::GetIO().MouseDown[1] = (msg == WM_RBUTTONDOWN);
	} break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP: {
		ImGui::GetIO().MouseDown[2] = (msg == WM_MBUTTONDOWN);
	} break;
	case WM_MOUSEWHEEL: {
		ImGui::GetIO().MouseWheel = static_cast<float>(WHEEL_DELTA) / GET_WHEEL_DELTA_WPARAM(wparam);
	} break;
	case WM_INPUT: {
		RAWINPUT rawInput;
		UINT rawInputSize = sizeof(rawInput);
		GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &rawInput, &rawInputSize, sizeof(RAWINPUTHEADER));
		if (rawInput.header.dwType == RIM_TYPEMOUSE) {
			RAWMOUSE& rawMouse = rawInput.data.mouse;
			if (rawMouse.usFlags == MOUSE_MOVE_RELATIVE) {
				window.rawMouseDx += rawMouse.lLastX;
				window.rawMouseDy += rawMouse.lLastY;
			}
		}
	} break;
	}
	return result;
}

void imguiCommands() {
	// ImGui::GetIO().DeltaTime = 0;
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	ImGui::Render();
}

void updateCamera() {
	static DirectX::XMVECTOR eyePosition = DirectX::XMVectorSet(0, 0, 10, 0);
	static DirectX::XMVECTOR lookAtPosition = DirectX::XMVectorSet(0, 0, 0, 0);
	static DirectX::XMVECTOR upPosition = DirectX::XMVectorSet(0, 1, 0, 0);
	static float pitchAngle = 0;

	float mouseWheel = ImGui::GetIO().MouseWheel;
	if (ImGui::IsMouseDragging(0)) {
		ImVec2 mouseDelta = ImGui::GetIO().MouseDelta / ImVec2(static_cast<float>(window.width), static_cast<float>(window.height));
		float yawDelta = -mouseDelta.x * 10;
		float pitchDelta = mouseDelta.y * 10;
		float newPitch = pitchAngle + pitchDelta;
		if (newPitch >= M_PI / 2.1 || newPitch <= -M_PI / 2.1) {
			pitchDelta = 0;
		} 
		else {
			pitchAngle = newPitch;
		}
		DirectX::XMVECTOR v = DirectX::XMVectorSubtract(eyePosition, lookAtPosition);
		DirectX::XMVECTOR axis = DirectX::XMVectorSet(0, 1, 0, 0);
		DirectX::XMMATRIX m = DirectX::XMMatrixRotationAxis(axis, yawDelta);
		v = DirectX::XMVector3Transform(v, m);
		axis = DirectX::XMVector3Cross(v, DirectX::XMVectorSet(0, 1, 0, 0));
		m = DirectX::XMMatrixRotationAxis(axis, pitchDelta);
		v = DirectX::XMVector3Transform(v, m);
		eyePosition = DirectX::XMVectorAdd(lookAtPosition, v);
	}

	DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtRH(eyePosition, lookAtPosition, upPosition);
	DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45), static_cast<float>(window.width) / window.height, 1, 1000);
	DirectX::XMMATRIX viewProjMat = viewMat * projMat;
	DirectX::XMMATRIX screenToWorldMat = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, viewProjMat));
	D3D12_RANGE constantsBufferRange = { 0, 256 };
	char* constantsBufferPtr = nullptr;
	dx12Resources.constantsBuffer->Map(0, &constantsBufferRange, reinterpret_cast<void**>(&constantsBufferPtr));
	memcpy(constantsBufferPtr, &screenToWorldMat, sizeof(screenToWorldMat));
	memcpy(constantsBufferPtr + sizeof(screenToWorldMat), &eyePosition, sizeof(eyePosition));
	dx12Resources.constantsBuffer->Unmap(0, nullptr);
}

void graphicsCommands(Scene& scene) {
	DX12CommandList& cmdList = dx12.graphicsCommandLists[dx12.currentGraphicsCommandListIndex];
	{ // color texture
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.pResource = dx12Resources.colorTexture;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		cmdList.list->ResourceBarrier(1, &barrier);

		cmdList.list->SetPipelineState1(dx12RayTracing.sceneStateObject);
		cmdList.list->SetDescriptorHeaps(1, &dx12.cbvSrvUavDescriptorHeap.heap);

		D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
		dispatchRaysDesc.Width = dx12.swapChainWidth;
		dispatchRaysDesc.Height = dx12.swapChainHeight;
		dispatchRaysDesc.Depth = 1;
		D3D12_GPU_VIRTUAL_ADDRESS shaderTablePtr = dx12RayTracing.sceneShaderTable->GetGPUVirtualAddress();
		uint64_t shaderRecordSize = dx12RayTracing.sceneShaderRecordSize;
		dispatchRaysDesc.RayGenerationShaderRecord = { shaderTablePtr, shaderRecordSize };
		dispatchRaysDesc.MissShaderTable = { shaderTablePtr + shaderRecordSize, shaderRecordSize, shaderRecordSize };
		dispatchRaysDesc.HitGroupTable = { shaderTablePtr + shaderRecordSize * 2, shaderRecordSize, shaderRecordSize };
		cmdList.list->DispatchRays(&dispatchRaysDesc);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		cmdList.list->ResourceBarrier(1, &barrier);
	}
	{ // swap chain
		UINT currentSwapChainImageIndex = dx12.swapChain->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER swapChainImageBarrier = {};
		swapChainImageBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		swapChainImageBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		swapChainImageBarrier.Transition.pResource = dx12.swapChainImages[currentSwapChainImageIndex];
		swapChainImageBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		swapChainImageBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		cmdList.list->ResourceBarrier(1, &swapChainImageBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE swapChainImageRTVDescriptor = dx12.getRTVDescriptorCPU(dx12.swapChainImageRTVDescriptorIndices[currentSwapChainImageIndex]);
		cmdList.list->OMSetRenderTargets(1, &swapChainImageRTVDescriptor, false, nullptr);
		cmdList.list->ClearRenderTargetView(swapChainImageRTVDescriptor, DirectX::Colors::Black, 0, nullptr);

		D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(dx12.swapChainWidth), static_cast<float>(dx12.swapChainHeight), 0, 1 };
		RECT scissor = { 0, 0, dx12.swapChainWidth, dx12.swapChainHeight };
		cmdList.list->RSSetViewports(1, &viewport);
		cmdList.list->RSSetScissorRects(1, &scissor);

		cmdList.list->SetPipelineState(dx12Pipelines.swapChainPipelineState);
		cmdList.list->SetDescriptorHeaps(1, &dx12.cbvSrvUavDescriptorHeap.heap);
		cmdList.list->SetGraphicsRootSignature(dx12Pipelines.swapChainRootSignature);
		cmdList.list->SetGraphicsRootDescriptorTable(0, dx12.getCbvSrvUavDescriptorGPU(dx12Resources.colorTextureSRVDescriptorIndex));
		cmdList.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList.list->DrawInstanced(3, 1, 0, 0);

		ImDrawData* imguiDrawData = ImGui::GetDrawData();
		char* imguiVertexBufferPtr = nullptr;
		char* imguiIndexBufferPtr = nullptr;
		D3D12_RANGE mapBufferRange = { 0, 0 };
		dx12Assert(dx12Resources.imguiVertexBuffer->Map(0, &mapBufferRange, reinterpret_cast<void**>(&imguiVertexBufferPtr)));
		dx12Assert(dx12Resources.imguiIndexBuffer->Map(0, &mapBufferRange, reinterpret_cast<void**>(&imguiIndexBufferPtr)));

		int imguiVertexBufferOffset = 0;
		int imguiIndexBufferOffset = 0;
		for (int i = 0; i < imguiDrawData->CmdListsCount; i += 1) {
			ImDrawList* dlist = imguiDrawData->CmdLists[i];
			int verticesSize = dlist->VtxBuffer.Size * sizeof(ImDrawVert);
			int indicesSize = dlist->IdxBuffer.Size * sizeof(ImDrawIdx);
			int newImGuiVertexBufferOffset = imguiVertexBufferOffset + roundUp<int>(verticesSize, sizeof(ImDrawVert));
			int newImGuiIndexBufferOffset = imguiIndexBufferOffset + roundUp<int>(indicesSize, sizeof(ImDrawIdx));
			assert(newImGuiVertexBufferOffset < dx12Resources.imguiVertexBufferCapacity);
			assert(newImGuiIndexBufferOffset < dx12Resources.imguiIndexBufferCapacity);
			memcpy(imguiVertexBufferPtr + imguiVertexBufferOffset, dlist->VtxBuffer.Data, verticesSize);
			memcpy(imguiIndexBufferPtr + imguiIndexBufferOffset, dlist->IdxBuffer.Data, indicesSize);
			imguiVertexBufferOffset = newImGuiVertexBufferOffset;
			imguiIndexBufferOffset = newImGuiIndexBufferOffset;
		}
		dx12Resources.imguiVertexBuffer->Unmap(0, nullptr);
		dx12Resources.imguiIndexBuffer->Unmap(0, nullptr);

		cmdList.list->SetPipelineState(dx12Pipelines.imguiPipelineState);
		cmdList.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D12_VERTEX_BUFFER_VIEW imguiVertexBufferView = { dx12Resources.imguiVertexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(imguiVertexBufferOffset), sizeof(ImDrawVert) };
		D3D12_INDEX_BUFFER_VIEW imguiIndexBufferView = { dx12Resources.imguiIndexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(imguiIndexBufferOffset), DXGI_FORMAT_R16_UINT };
		cmdList.list->IASetVertexBuffers(0, 1, &imguiVertexBufferView);
		cmdList.list->IASetIndexBuffer(&imguiIndexBufferView);
		float blendFactor[] = { 0.f, 0.f, 0.f, 0.f };
		cmdList.list->OMSetBlendFactor(blendFactor);
		cmdList.list->SetGraphicsRootSignature(dx12Pipelines.imguiRootSignature);
		int imguiConstants[2] = { dx12.swapChainWidth, dx12.swapChainHeight };
		cmdList.list->SetGraphicsRoot32BitConstants(0, 2, imguiConstants, 0);
		cmdList.list->SetGraphicsRootDescriptorTable(1, dx12.getCbvSrvUavDescriptorGPU(dx12Resources.imguiTextureSRVDescriptorIndex));

		imguiVertexBufferOffset = 0;
		imguiIndexBufferOffset = 0;
		for (int i = 0; i < imguiDrawData->CmdListsCount; i += 1) {
			ImDrawList* dlist = imguiDrawData->CmdLists[i];
			int verticesSize = dlist->VtxBuffer.Size * sizeof(ImDrawVert);
			int indicesSize = dlist->IdxBuffer.Size * sizeof(ImDrawIdx);
			int vertexIndex = imguiVertexBufferOffset / sizeof(ImDrawVert);
			int indiceIndex = imguiIndexBufferOffset / sizeof(ImDrawIdx);
			imguiVertexBufferOffset += roundUp<int>(verticesSize, sizeof(ImDrawVert));
			imguiIndexBufferOffset += roundUp<int>(indicesSize, sizeof(ImDrawIdx));
			for (int i = 0; i < dlist->CmdBuffer.Size; i += 1) {
				ImDrawCmd* dcmd = &dlist->CmdBuffer.Data[i];
				D3D12_RECT scissor = { static_cast<LONG>(dcmd->ClipRect.x), static_cast<LONG>(dcmd->ClipRect.y), static_cast<LONG>(dcmd->ClipRect.z), static_cast<LONG>(dcmd->ClipRect.w) };
				cmdList.list->RSSetScissorRects(1, &scissor);
				cmdList.list->DrawIndexedInstanced(dcmd->ElemCount, 1, indiceIndex, vertexIndex, 0);
				indiceIndex += dcmd->ElemCount;
			}
		}

		swapChainImageBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		swapChainImageBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		cmdList.list->ResourceBarrier(1, &swapChainImageBarrier);
	}
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	setCurrentDirToExeDir();
	window = Window::create(windowMsgHandler);
	dx12 = DX12Context::create(&window);
	initImGui();
	initDX12Resources();
	initDX12Pipelines();
	initDX12RayTracing();

	Scene scene = Scene::create();
	scene.createModelFromGLTF(&dx12, "glTF/DamagedHelmet.gltf");
	scene.rebuildTopAccelStruct(&dx12);

	dx12RayTracing.sceneDescriptorTableDescriptorIndex = dx12.createUAV(dx12Resources.colorTexture);
	dx12.createCBV(dx12Resources.constantsBuffer, 0, 256);
	dx12.createAccelStructSRV(scene.topAccelStructBuffer);
	dx12.createStructuredBufferSRV(scene.instanceInfosBuffer, 0, scene.instanceCount, sizeof(InstanceInfo));
	dx12.createStructuredBufferSRV(scene.triangleInfosBuffer, 0, scene.triangleCount, sizeof(TriangleInfo));
	dx12.createStructuredBufferSRV(scene.materialInfosBuffer, 0, scene.materialCount, sizeof(MaterialInfo));
	for (auto& model : scene.models) {
		for (auto& image : model.images) {
			dx12.createTextureSRV(image.image);
		}
	}

	char* shaderRecordPtr = nullptr;
	dx12Assert(dx12RayTracing.sceneShaderTable->Map(0, nullptr, reinterpret_cast<void**>(&shaderRecordPtr)));
	D3D12_GPU_DESCRIPTOR_HANDLE sceneDescriptorTableDescriptor = dx12.getCbvSrvUavDescriptorGPU(dx12RayTracing.sceneDescriptorTableDescriptorIndex);
	memcpy(shaderRecordPtr, dx12RayTracing.sceneStateObjectProps->GetShaderIdentifier(L"rayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	memcpy(shaderRecordPtr + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &sceneDescriptorTableDescriptor, 8);
	memcpy(shaderRecordPtr + dx12RayTracing.sceneShaderRecordSize, dx12RayTracing.sceneStateObjectProps->GetShaderIdentifier(L"miss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	memcpy(shaderRecordPtr + dx12RayTracing.sceneShaderRecordSize * 2, dx12RayTracing.sceneStateObjectProps->GetShaderIdentifier(L"hitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	memcpy(shaderRecordPtr + dx12RayTracing.sceneShaderRecordSize * 2 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &sceneDescriptorTableDescriptor, 8);
	dx12RayTracing.sceneShaderTable->Unmap(0, nullptr);

	window.show();

	while (!quitProgram) {
		window.processMessages();

		imguiCommands();

		dx12.setCurrentCommandList();
		updateCamera();
		graphicsCommands(scene);
		dx12.executeCurrentCommandList();
		dx12.presentSwapChainImage();
	}
	return 0;
}
