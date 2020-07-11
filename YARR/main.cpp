/************************************************************************************************/
/*			Copyright (C) 2020 By Yang Chen (yngccc@gmail.com). All Rights Reserved.			*/
/************************************************************************************************/

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS
#include "thirdparty/include/imgui/imgui.h"
#include "thirdparty/include/imgui/imgui_internal.h"

#include "scene.h"
#include "test.h"

struct Gamepad {
	XINPUT_STATE prevState = {};
	XINPUT_STATE state = {};

	void updateState() {
		prevState = state;
		DWORD error = XInputGetState(0, &state);
		if (error != ERROR_SUCCESS) {
			state = {};
		}
	}
	bool buttonDown(WORD button) {
		bool down = state.Gamepad.wButtons & button;
		return down;
	}
	bool buttonPressed(WORD button) {
		bool prevDown = prevState.Gamepad.wButtons & button;
		bool down = state.Gamepad.wButtons & button;
		return !prevDown && down;
	}
	bool buttonReleased(WORD button) {
		bool prevDown = prevState.Gamepad.wButtons & button;
		bool down = state.Gamepad.wButtons & button;
		return prevDown && !down;
	}
	uint8 leftTrigger() {
		return state.Gamepad.bLeftTrigger;
	}
	uint8 rightTrigger() {
		return state.Gamepad.bRightTrigger;
	}
};

static std::vector<std::wstring> cmdLineArgs = getCmdLineArgs();
static Window window;
static Gamepad gamepad;
static DX12Context dx12;
static std::vector<Scene> scenes = {};
static uint64 currentSceneIndex = 0;
static const char* settingsFilePath = "settings.ini";
static bool quit = false;
static double frameTime = 0;

void imGuiInit() {
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';
	io.IniFilename = "imgui.ini";
	io.FontGlobalScale = 1.5f;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

LRESULT processWindowMsg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;
	switch (msg) {
	default: {
		result = DefWindowProcA(hwnd, msg, wparam, lparam);
	} break;
	case WM_CLOSE:
	case WM_QUIT: {
		quit = true;
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
		ImGui::GetIO().DisplaySize = { static_cast<float>(window.width), static_cast<float>(window.height) };
		// ImGuizmo::SetRect(0, 0, (float)window.width, (float)window.height);
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
		ImGui::GetIO().AddInputCharacter(static_cast<uint>(wparam));
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
		int scroll = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
		window.mouseWheel += scroll;
		ImGui::GetIO().MouseWheel += scroll;
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

void updateCamera() {
	if (currentSceneIndex >= scenes.size()) {
		return;
	}
	Camera& camera = scenes[currentSceneIndex].camera;
	if (!ImGui::GetIO().WantCaptureKeyboard) {
		float translation[2] = {};
		float delta = ImGui::GetIO().DeltaTime * 10;
		if (ImGui::IsKeyDown('W')) {
			translation[0] += delta;
		}
		if (ImGui::IsKeyDown('S')) {
			translation[0] += -delta;
		}
		if (ImGui::IsKeyDown('A')) {
			translation[1] += -delta;
		}
		if (ImGui::IsKeyDown('D')) {
			translation[1] += delta;
		}
		DirectX::XMVECTOR v = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(camera.lookAt, camera.position));
		DirectX::XMVECTOR w = DirectX::XMVector3Cross(v, DirectX::XMVectorSet(0, 1, 0, 0));
		v = DirectX::XMVectorScale(v, translation[0]);
		w = DirectX::XMVectorScale(w, translation[1]);
		DirectX::XMVECTOR f = DirectX::XMVectorAdd(v, w);
		camera.lookAt = DirectX::XMVectorAdd(camera.lookAt, f);
		camera.position = DirectX::XMVectorAdd(camera.position, f);
	}
	if (!ImGui::GetIO().WantCaptureMouse) {
		if (ImGui::IsMouseDragging(0)) {
			ImVec2 mouseDelta = ImGui::GetIO().MouseDelta / ImVec2(static_cast<float>(window.width), static_cast<float>(window.height));
			float yawDelta = -mouseDelta.x * 5;
			float pitchDelta = -mouseDelta.y * 5;
			float newPitch = camera.pitchAngle + pitchDelta;
			if (newPitch >= M_PI / 2.1 || newPitch <= -M_PI / 2.1) {
				pitchDelta = 0;
			}
			else {
				camera.pitchAngle = newPitch;
			}
			DirectX::XMVECTOR v = DirectX::XMVectorSubtract(camera.lookAt, camera.position);
			DirectX::XMVECTOR axis = DirectX::XMVectorSet(0, 1, 0, 0);
			DirectX::XMMATRIX m = DirectX::XMMatrixRotationAxis(axis, yawDelta);
			v = DirectX::XMVector3Transform(v, m);
			axis = DirectX::XMVector3Cross(v, DirectX::XMVectorSet(0, 1, 0, 0));
			m = DirectX::XMMatrixRotationAxis(axis, pitchDelta);
			v = DirectX::XMVector3Transform(v, m);
			camera.lookAt = DirectX::XMVectorAdd(camera.position, v);
		}
		else if (ImGui::IsMouseDragging(1)) {
			ImVec2 mouseDelta = ImGui::GetIO().MouseDelta / ImVec2(static_cast<float>(window.width), static_cast<float>(window.height));
			DirectX::XMVECTOR v = DirectX::XMVectorSubtract(camera.position, camera.lookAt);
			DirectX::XMVECTOR v1 = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSet(0, 1, 0, 0), v));
			DirectX::XMVECTOR v2 = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(v, v1));
			DirectX::XMVECTOR v3 = DirectX::XMVectorAdd(DirectX::XMVectorScale(v1, -mouseDelta.x * 5), DirectX::XMVectorScale(v2, mouseDelta.y * 5));
			camera.position = DirectX::XMVectorAdd(camera.position, v3);
			camera.lookAt = DirectX::XMVectorAdd(camera.lookAt, v3);
		}
		else {
			DirectX::XMVECTOR v = DirectX::XMVectorSubtract(camera.position, camera.lookAt);
			DirectX::XMVECTOR d = DirectX::XMVectorScale(DirectX::XMVector3Normalize(v), static_cast<float>(-window.mouseWheel));
			v = DirectX::XMVectorAdd(v, d);
			v = DirectX::XMVector3ClampLength(v, 2, 1000);
			camera.position = DirectX::XMVectorAdd(camera.lookAt, v);
		}
	}
}

void updateFrameTime() {
	static LARGE_INTEGER perfFrequency;
	static LARGE_INTEGER perfCounter;
	static int queryPerfCounter = [&] {
		QueryPerformanceFrequency(&perfFrequency);
		QueryPerformanceCounter(&perfCounter);
		return 0;
	}();
	static_cast<void>(queryPerfCounter);

	LARGE_INTEGER currentPerfCounter;
	QueryPerformanceCounter(&currentPerfCounter);
	LONGLONG ticks = currentPerfCounter.QuadPart - perfCounter.QuadPart;
	perfCounter = currentPerfCounter;
	frameTime = static_cast<double>(ticks) / static_cast<double>(perfFrequency.QuadPart);
}

void addScene(const std::string& sceneName, const std::filesystem::path& sceneFilePath) {
	std::string name = sceneName;
	int n = 0;
	while (true) {
		bool duplicate = false;
		for (auto& scene : scenes) {
			if (scene.name == name) {
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			name = sceneName + "(" + std::to_string(n) + ")";
			n += 1;
		}
		else {
			break;
		}
	}
	if (sceneFilePath.empty()) {
		Scene scene(name);
		scenes.push_back(std::move(scene));
	}
	else {
		Scene scene(name, sceneFilePath, dx12);
		scene.rebuildTLAS(dx12);
		scenes.push_back(std::move(scene));
	}
	currentSceneIndex = scenes.size() - 1;
}

struct ImGuiLogWindow {
	bool open = true;
	bool autoScroll = true;
	ImGuiTextBuffer textBuffer;
	ImGuiTextFilter filter;
	ImVector<int> lineOffsets;

	ImGuiLogWindow() {
		lineOffsets.push_back(0);
	}
	void addMessage(const std::string& str) {
		int oldSize = textBuffer.size();
		textBuffer.append(str.data(), str.data() + str.length());
		for (int newSize = textBuffer.size(); oldSize < newSize; oldSize += 1) {
			if (textBuffer[oldSize] == '\n') {
				lineOffsets.push_back(oldSize + 1);
			}
		}
	}
	void addError(const std::string& str) {
		addMessage("[error] " + str + "\n");
	}
	void clear() {
		textBuffer.clear();
		lineOffsets.clear();
		lineOffsets.push_back(0);
	}
};

struct ImGuiMetricsWindow {
	bool open = true;
	double timeSinceLastFrameTime = 0;
	double frameTimePollingInterval = 0.5;
	RingBuffer<double> frameTimes = RingBuffer<double>(256);
	std::string frameTimeStr;

	ImGuiMetricsWindow() {
		frameTimes.push(0);
	}
};

void imguiCommands() {
	static ImGuiLogWindow logWindow;
	static ImGuiMetricsWindow metricsWindow;

	ImGui::GetIO().DisplaySize = { static_cast<float>(window.width), static_cast<float>(window.height) };
	// ImGuizmo::SetRect(0, 0, static_cast<float>(window.width), static_cast<float>(window.height));
	ImGui::GetIO().DeltaTime = static_cast<float>(frameTime);
	ImGui::NewFrame();

	ImVec2 mainMenuBarPos;
	ImVec2 mainMenuBarSize;
	if (ImGui::BeginMainMenuBar()) {
		mainMenuBarPos = ImGui::GetWindowPos();
		mainMenuBarSize = ImGui::GetWindowSize();
		if (ImGui::BeginMenu("Scene")) {
			if (ImGui::MenuItem("New")) {
				addScene("New Scene", "");
			}
			if (ImGui::MenuItem("Load")) {
				try {
					std::filesystem::path filePath = openFileDialog();
					addScene("New Scene", filePath);
				}
				catch (const std::exception& e) {
					logWindow.addError(e.what());
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Model")) {
			if (ImGui::MenuItem("Load")) {
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::SetNextWindowPos(ImVec2(0.0f, mainMenuBarPos.y + mainMenuBarSize.y));
	ImGui::SetNextWindowSize(ImVec2(mainMenuBarSize.x, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	if (ImGui::Begin("secondMainBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
		if (ImGui::BeginTabBar("scenesTabbar")) {
			int sceneTodelete = -1;
			int sceneIndex = 0;
			for (auto& scene : scenes) {
				bool open = true;
				if (ImGui::BeginTabItem(scene.name.c_str(), &open)) {
					currentSceneIndex = sceneIndex;
					ImGui::EndTabItem();
				}
				if (!open) {
					sceneTodelete = sceneIndex;
				}
				sceneIndex += 1;
			}
			if (sceneTodelete != -1) {
				scenes[sceneTodelete].deleteGPUResources();
				scenes.erase(scenes.begin() + sceneTodelete);
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(3);

	if (ImGui::Begin("Log", &logWindow.open)) {
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		const char* buf = logWindow.textBuffer.begin();
		const char* bufEnd = logWindow.textBuffer.end();
		ImGuiListClipper clipper;
		clipper.Begin(logWindow.lineOffsets.Size);
		while (clipper.Step()) {
			for (int lineNo = clipper.DisplayStart; lineNo < clipper.DisplayEnd; lineNo += 1) {
				const char* lineStart = buf + logWindow.lineOffsets[lineNo];
				const char* lineEnd = (lineNo + 1 < logWindow.lineOffsets.Size) ? (buf + logWindow.lineOffsets[lineNo + 1] - 1) : bufEnd;
				ImGui::TextUnformatted(lineStart, lineEnd);
			}
		}
		clipper.End();
		ImGui::PopStyleVar();
		if (logWindow.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}
		ImGui::EndChild();
	}
	ImGui::End();

	metricsWindow.timeSinceLastFrameTime += frameTime;
	if (metricsWindow.timeSinceLastFrameTime >= metricsWindow.frameTimePollingInterval) {
		metricsWindow.timeSinceLastFrameTime = 0.0;
		metricsWindow.frameTimes.push(frameTime);
		metricsWindow.frameTimeStr = std::to_string(frameTime * 1000) + " ms";
	}
	if (ImGui::Begin("Metrics", &metricsWindow.open)) {
		ImGui::PlotLines(
			metricsWindow.frameTimeStr.c_str(),
			[](void* data, int idx) -> float {
				auto* frameTimes = static_cast<RingBuffer<double>*>(data);
				return static_cast<float>((*frameTimes)[idx] * 1000);
			},
			&metricsWindow.frameTimes, static_cast<int>(metricsWindow.frameTimes.size()), 0, nullptr, 0, 100);
	}
	ImGui::End();
}

void graphicsCommands() {
	dx12.compileShaders();
	dx12.resetDescriptorHeaps();
	dx12.resetAndMapConstantBuffer();

	DX12CommandList& cmdList = dx12.graphicsCommandLists[dx12.currentFrame];
	if (currentSceneIndex < scenes.size()) {
		const Scene& scene = scenes[currentSceneIndex];
		if (scene.tlasBuffer.buffer) {
			DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtRH(scene.camera.position, scene.camera.lookAt, scene.camera.up);
			DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45), static_cast<float>(window.width) / window.height, 1, 1000);
			DirectX::XMMATRIX viewProjMat = viewMat * projMat;
			struct {
				DirectX::XMMATRIX screenToWorldMat;
				DirectX::XMVECTOR cameraPosition;
				int bounceCount;
				int sampleCount;
				int frameCount;
			} constants = { DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, viewProjMat)), scene.camera.position, 2, 16, static_cast<int>(dx12.totalFrame % INT_MAX) };
			uint64 constantsOffset = dx12.appendConstantBuffer(&constants, sizeof(constants));

			DX12Descriptor firstDescriptor = dx12.appendDescriptorUAV(dx12.positionTexture.texture);
			dx12.appendDescriptorUAV(dx12.normalTexture.texture);
			dx12.appendDescriptorUAV(dx12.colorTexture.texture);
			dx12.appendDescriptorUAV(dx12.emissiveTexture.texture);
			dx12.appendDescriptorCBV(dx12.constantsBuffers[dx12.currentFrame].buffer, constantsOffset, sizeof(constants));
			dx12.appendDescriptorSRVTLAS(scene.tlasBuffer.buffer);
			dx12.appendDescriptorSRVStructuredBuffer(scene.instanceInfosBuffer.buffer, 0, scene.instanceCount, sizeof(InstanceInfo));
			dx12.appendDescriptorSRVStructuredBuffer(scene.triangleInfosBuffer.buffer, 0, scene.triangleCount, sizeof(TriangleInfo));
			dx12.appendDescriptorSRVStructuredBuffer(scene.materialInfosBuffer.buffer, 0, scene.materialCount, sizeof(MaterialInfo));
			for (auto& [name, model] : scene.models) {
				for (auto& texture : model.textures) {
					dx12.appendDescriptorSRVTexture(texture.texture);
				}
			}
			DX12Buffer& shaderTableBuffer = dx12.primaryRayShaderTableBuffers[dx12.currentFrame];
			dx12Assert(shaderTableBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&shaderTableBuffer.bufferPtr)));
			memcpy(shaderTableBuffer.bufferPtr, dx12.primaryRayObjectProps->GetShaderIdentifier(L"primaryRayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			memcpy(shaderTableBuffer.bufferPtr + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
			memcpy(shaderTableBuffer.bufferPtr + dx12.rayTracingShaderRecordSize, dx12.primaryRayObjectProps->GetShaderIdentifier(L"primaryRayMiss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			memcpy(shaderTableBuffer.bufferPtr + dx12.rayTracingShaderRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
			memcpy(shaderTableBuffer.bufferPtr + dx12.rayTracingShaderRecordSize * 2, dx12.primaryRayObjectProps->GetShaderIdentifier(L"primaryRayHitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			memcpy(shaderTableBuffer.bufferPtr + dx12.rayTracingShaderRecordSize * 2 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
			shaderTableBuffer.buffer->Unmap(0, nullptr);
			{
				PIXScopedEvent(cmdList.list, PIX_COLOR_DEFAULT, "Primary Rays");

				D3D12_RESOURCE_BARRIER barriers[4] = {};
				ID3D12Resource* textures[4] = { dx12.positionTexture.texture, dx12.normalTexture.texture, dx12.colorTexture.texture, dx12.emissiveTexture.texture };
				for (int i = 0; i < countof(barriers); i += 1) {
					barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					barriers[i].Transition.pResource = textures[i];
					barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
					barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}
				cmdList.list->ResourceBarrier(countof(barriers), barriers);

				cmdList.list->SetPipelineState1(dx12.primaryRayStateObject);
				cmdList.list->SetDescriptorHeaps(1, &dx12.cbvSrvUavDescriptorHeaps[dx12.currentFrame].heap);

				D3D12_GPU_VIRTUAL_ADDRESS shaderTablePtr = shaderTableBuffer.buffer->GetGPUVirtualAddress();
				uint64 recordSize = dx12.rayTracingShaderRecordSize;
				D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
				dispatchRaysDesc.Width = window.width;
				dispatchRaysDesc.Height = window.height;
				dispatchRaysDesc.Depth = 1;
				dispatchRaysDesc.RayGenerationShaderRecord = { shaderTablePtr, recordSize };
				dispatchRaysDesc.MissShaderTable = { shaderTablePtr + recordSize, recordSize, recordSize };
				dispatchRaysDesc.HitGroupTable = { shaderTablePtr + recordSize * 2, recordSize, recordSize };
				cmdList.list->DispatchRays(&dispatchRaysDesc);

				for (int i = 0; i < countof(barriers); i += 1) {
					barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				}
				cmdList.list->ResourceBarrier(countof(barriers), barriers);
			}
		}
	}
	{
		PIXScopedEvent(cmdList.list, PIX_COLOR_DEFAULT, "Tone Map to Swap Chain and UI");

		UINT currentSwapChainImageIndex = dx12.swapChain->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER swapChainImageBarrier = {};
		swapChainImageBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		swapChainImageBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		swapChainImageBarrier.Transition.pResource = dx12.swapChainImages[currentSwapChainImageIndex];
		swapChainImageBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		swapChainImageBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		cmdList.list->ResourceBarrier(1, &swapChainImageBarrier);

		DX12Descriptor swapChainDescriptor = dx12.appendDescriptorRTV(dx12.swapChainImages[currentSwapChainImageIndex]);
		cmdList.list->OMSetRenderTargets(1, &swapChainDescriptor.cpuHandle, false, nullptr);
		cmdList.list->ClearRenderTargetView(swapChainDescriptor.cpuHandle, DirectX::Colors::Black, 0, nullptr);

		D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(window.width), static_cast<float>(window.height), 0, 1 };
		RECT scissor = { 0, 0, window.width, window.height };
		cmdList.list->RSSetViewports(1, &viewport);
		cmdList.list->RSSetScissorRects(1, &scissor);

		cmdList.list->SetPipelineState(dx12.swapChainPipelineState);
		cmdList.list->SetDescriptorHeaps(1, &dx12.cbvSrvUavDescriptorHeaps[dx12.currentFrame].heap);
		cmdList.list->SetGraphicsRootSignature(dx12.swapChainRootSignature);
		DX12Descriptor colorTextureDescriptor = dx12.appendDescriptorSRVTexture(dx12.colorTexture.texture);
		cmdList.list->SetGraphicsRootDescriptorTable(0, colorTextureDescriptor.gpuHandle);
		cmdList.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList.list->DrawInstanced(3, 1, 0, 0);

		ImGui::Render();
		cmdList.list->SetPipelineState(dx12.imguiPipelineState);
		float blendFactor[] = { 0.f, 0.f, 0.f, 0.f };
		cmdList.list->OMSetBlendFactor(blendFactor);
		cmdList.list->SetGraphicsRootSignature(dx12.imguiRootSignature);
		int imguiConstants[2] = { window.width, window.height };
		cmdList.list->SetGraphicsRoot32BitConstants(0, 2, imguiConstants, 0);
		DX12Descriptor imguiTextureDescriptor = dx12.appendDescriptorSRVTexture(dx12.imguiTexture.texture);
		cmdList.list->SetGraphicsRootDescriptorTable(1, imguiTextureDescriptor.gpuHandle);

		DX12Buffer& imguiVertexBuffer = dx12.imguiVertexBuffers[dx12.currentFrame];
		DX12Buffer& imguiIndexBuffer = dx12.imguiIndexBuffers[dx12.currentFrame];
		D3D12_VERTEX_BUFFER_VIEW imguiVertexBufferView = { imguiVertexBuffer.buffer->GetGPUVirtualAddress(), static_cast<uint>(imguiVertexBuffer.capacity), sizeof(ImDrawVert) };
		D3D12_INDEX_BUFFER_VIEW imguiIndexBufferView = { imguiIndexBuffer.buffer->GetGPUVirtualAddress(), static_cast<uint>(imguiIndexBuffer.capacity), DXGI_FORMAT_R16_UINT };
		cmdList.list->IASetVertexBuffers(0, 1, &imguiVertexBufferView);
		cmdList.list->IASetIndexBuffer(&imguiIndexBufferView);
		cmdList.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		dx12Assert(imguiVertexBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&imguiVertexBuffer.bufferPtr)));
		dx12Assert(imguiIndexBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&imguiIndexBuffer.bufferPtr)));
		uint64 imguiVertexBufferOffset = 0;
		uint64 imguiIndexBufferOffset = 0;
		const ImDrawData* imguiDrawData = ImGui::GetDrawData();
		for (int i = 0; i < imguiDrawData->CmdListsCount; i += 1) {
			const ImDrawList& dlist = *imguiDrawData->CmdLists[i];
			uint64 verticesSize = dlist.VtxBuffer.Size * sizeof(ImDrawVert);
			uint64 indicesSize = dlist.IdxBuffer.Size * sizeof(ImDrawIdx);
			memcpy(imguiVertexBuffer.bufferPtr + imguiVertexBufferOffset, dlist.VtxBuffer.Data, verticesSize);
			memcpy(imguiIndexBuffer.bufferPtr + imguiIndexBufferOffset, dlist.IdxBuffer.Data, indicesSize);
			uint64 vertexIndex = imguiVertexBufferOffset / sizeof(ImDrawVert);
			uint64 indiceIndex = imguiIndexBufferOffset / sizeof(ImDrawIdx);
			for (int i = 0; i < dlist.CmdBuffer.Size; i += 1) {
				const ImDrawCmd& dcmd = dlist.CmdBuffer[i];
				D3D12_RECT scissor = {
					static_cast<LONG>(dcmd.ClipRect.x), static_cast<LONG>(dcmd.ClipRect.y),
					static_cast<LONG>(dcmd.ClipRect.z), static_cast<LONG>(dcmd.ClipRect.w)
				};
				cmdList.list->RSSetScissorRects(1, &scissor);
				cmdList.list->DrawIndexedInstanced(dcmd.ElemCount, 1, static_cast<uint>(indiceIndex), static_cast<int>(vertexIndex), 0);
				indiceIndex += dcmd.ElemCount;
			}
			imguiVertexBufferOffset = imguiVertexBufferOffset + align(verticesSize, sizeof(ImDrawVert));
			imguiIndexBufferOffset = imguiIndexBufferOffset + align(indicesSize, sizeof(ImDrawIdx));
			assert(imguiVertexBufferOffset < imguiVertexBuffer.capacity);
			assert(imguiIndexBufferOffset < imguiIndexBuffer.capacity);
		}
		imguiVertexBuffer.buffer->Unmap(0, nullptr);
		imguiIndexBuffer.buffer->Unmap(0, nullptr);

		swapChainImageBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		swapChainImageBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		cmdList.list->ResourceBarrier(1, &swapChainImageBarrier);
	}

	dx12.unmapConstantBuffer();
	dx12.closeAndExecuteCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);
	dx12Assert(dx12.swapChain->Present(0, 0));
	dx12.currentFrame = (dx12.currentFrame + 1) % dx12.maxFrameInFlight;
	dx12.totalFrame += 1;
	if (dx12.totalFrame >= dx12.maxFrameInFlight) {
		dx12.waitAndResetCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);
	}
}

struct SettingInfo {
	enum Type {
		Scene,
		EndOfFile
	};

	Type type;
	std::string_view name;
	std::string_view path;
};

struct SettingParser : public Parser {
	using Parser::Parser;

	void getInfo(SettingInfo& info) {
		Token token;
		getToken(token);
		if (token.type == Token::Identifier) {
			if (token.str == "Scene") {
				info.type = SettingInfo::Scene;
				getToken(token);
				if (token.type != Token::String) {
					throw Exception("SettingParser getInfo error: scene name is not a string");
				}
				info.name = token.str;
				getToken(token);
				if (token.type != Token::String) {
					throw Exception("SettingParser getInfo error: scene filePath is not a string");
				}
				info.path = token.str;
			}
			else {
				throw Exception("SettingParser getInfo error: unknown identifier \"" + std::string(token.str) + "\"");
			}
		}
		else if (token.type == Token::EndOfFile) {
			info.type = SettingInfo::EndOfFile;
		}
		else {
			throw Exception("SettingParser getInfo error: first token is not an identifier");
		}
	}
};

void saveSettings() {
	std::string settingsFileStr;
	for (auto& scene : scenes) {
		scene.writeToFile();
		settingsFileStr += "Scene: \"" + scene.name + "\" \"" + scene.filePath.string() + "\"\r\n";
	}
	setCurrentDirToExeDir();
	writeFile(settingsFilePath, settingsFileStr);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	runTests();

	setCurrentDirToExeDir();
	CoInitialize(nullptr);
	imGuiInit();

	window = Window(processWindowMsg);
	dx12 = DX12Context(window, std::any_of(cmdLineArgs.begin(), cmdLineArgs.end(), [](auto& args) { return args == L"-d3dDebug"; }));

	if (fileExists(settingsFilePath)) {
		SettingParser parser(settingsFilePath);
		SettingInfo info;
		while (true) {
			parser.getInfo(info);
			if (info.type == SettingInfo::Scene) {
				addScene(std::string(info.name), info.path);
			}
			else if (info.type == SettingInfo::EndOfFile) {
				break;
			}
			else {
				assert(false);
			}
		}
	}

	window.show();

	while (!quit) {
		updateFrameTime();
		window.processMessages();
		gamepad.updateState();
		imguiCommands();
		updateCamera();
		graphicsCommands();
	}
	saveSettings();
	return 0;
}
