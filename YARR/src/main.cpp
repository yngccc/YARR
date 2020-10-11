/************************************************************************************************/
/*			Copyright (C) 2020 By Yang Chen (yngccc@gmail.com). All Rights Reserved.			*/
/************************************************************************************************/

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../thirdparty/include/imgui/imgui.h"
#include "../thirdparty/include/imgui/imgui_internal.h"
#include "../thirdparty/include/imgui/ImGuizmo.h"

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
static DX12Buffer ddgiConstantBuffer;
static const char* settingsFilePath = "settings.ini";
static bool quit = false;
static double frameTime = 0;
static bool fullScreen = false;

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
		int width = LOWORD(lparam);
		int height = HIWORD(lparam);
		if (width > 0 && height > 0) {
			char title[128] = {};
			snprintf(title, sizeof(title), "YARR %d x %d", width, height);
			SetWindowTextA(window.handle, title);
			dx12.resizeSwapChainBuffers(width, height);
		}
		window.width = width;
		window.height = height;
	} break;
	case WM_SHOWWINDOW: {
	} break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP: {
		bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[wparam] = down;
		if (wparam == VK_SHIFT) {
			io.KeyShift = down;
		}
		else if (wparam == VK_CONTROL) {
			io.KeyCtrl = down;
		}
		else if (wparam == VK_MENU) {
			io.KeyAlt = down;
		}
		if (io.KeysDown[VK_F4] && io.KeyAlt) {
			quit = true;
		}
		if (io.KeysDown[VK_RETURN] && io.KeyAlt) {
			fullScreen = !fullScreen;
			dx12.swapChain->SetFullscreenState(fullScreen, dx12.output);
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
	camera.viewMat = DirectX::XMMatrixLookAtRH(camera.position, camera.lookAt, camera.up);
	camera.projMat = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45), static_cast<float>(window.width) / window.height, 1, 1000);
	camera.viewProjMat = camera.viewMat * camera.projMat;
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

	ImGui::GetIO().DeltaTime = static_cast<float>(frameTime);
	ImGui::GetIO().DisplaySize = { static_cast<float>(window.width), static_cast<float>(window.height) };
	ImGuizmo::SetRect(0, 0, static_cast<float>(window.width), static_cast<float>(window.height));
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	ImVec2 mainMenuBarPos;
	ImVec2 mainMenuBarSize;
	if (ImGui::BeginMainMenuBar()) {
		mainMenuBarPos = ImGui::GetWindowPos();
		mainMenuBarSize = ImGui::GetWindowSize();
		if (ImGui::BeginMenu("File")) {
			if (ImGui::BeginMenu("New")) {
				if (ImGui::MenuItem("Scene")) {
					addScene("New Scene", "");
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Open")) {
				if (ImGui::MenuItem("Scene")) {
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
			ImGui::Separator();
			if (ImGui::MenuItem("Quit")) {
				quit = true;
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

	if (ImGui::Begin("Properties")) {
		if (ImGui::BeginTabBar("PropertiesTabBar")) {
			if (currentSceneIndex < scenes.size()) {
				Scene& scene = scenes[currentSceneIndex];
				DirectX::XMFLOAT4X4 imguizmoView;
				DirectX::XMFLOAT4X4 imguizmoProj;
				DirectX::XMStoreFloat4x4(&imguizmoView, scene.camera.viewMat);
				DirectX::XMStoreFloat4x4(&imguizmoProj, scene.camera.projMat);
				if (ImGui::BeginTabItem("Lights")) {
					static int lightIndex = 0;
					ImGui::Text("Add light: ");
					ImGui::SameLine();
					if (ImGui::Button("direct")) {
						scene.lights.push_back(SceneLight{ DIRECTIONAL_LIGHT });
						lightIndex = scene.lights.size() - 1;
					}
					ImGui::SameLine();
					if (ImGui::Button("point")) {
						scene.lights.push_back(SceneLight{ POINT_LIGHT });
						lightIndex = scene.lights.size() - 1;
					}
					char str[16] = {};
					if (!scene.lights.empty()) {
						std::to_chars(str, str + sizeof(str), lightIndex);
					}
					if (ImGui::BeginCombo("lights", str)) {
						for (int i = 0; i < scene.lights.size(); i += 1) {
							std::to_chars(str, str + sizeof(str), i);
							if (ImGui::Selectable(str, i == lightIndex)) {
								lightIndex = i;
							}
						}
						ImGui::EndCombo();
					}
					if (lightIndex >= 0 && lightIndex < scene.lights.size()) {
						SceneLight& light = scene.lights[lightIndex];
						bool noLight = false;
						if (ImGui::Button("delete")) {
							scene.lights.erase(scene.lights.begin() + lightIndex);
							if (scene.lights.empty()) {
								noLight = true;
							}
							else {
								lightIndex = std::max(lightIndex - 1, 0);
								light = scene.lights[lightIndex];
							}
						}
						if (!noLight) {
							if (light.type == DIRECTIONAL_LIGHT) {
								ImGui::InputFloat3("direction", light.direction);
								vec3Normalize(light.direction);
								ImGui::ColorEdit3("color", light.color);

								float up[3] = { 0, 1, 0 };
								float rotation[4];
								rotationBetweenVecs(up, light.direction, rotation);
								DirectX::XMFLOAT4X4 imguizmoMat;
								matFromRotation(rotation, imguizmoMat.m[0]);
								ImGuizmo::Manipulate(imguizmoView.m[0], imguizmoProj.m[0], ImGuizmo::ROTATE, ImGuizmo::LOCAL, imguizmoMat.m[0]);
								DirectX::XMVECTOR s, r, t;
								DirectX::XMMatrixDecompose(&s, &r, &t, DirectX::XMLoadFloat4x4(&imguizmoMat));
								DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 1, 0, 0), DirectX::XMMatrixRotationQuaternion(r)));
								light.direction[0] = DirectX::XMVectorGetX(dir);
								light.direction[1] = DirectX::XMVectorGetY(dir);
								light.direction[2] = DirectX::XMVectorGetZ(dir);
							}
							else if (light.type == POINT_LIGHT) {
								ImGui::InputFloat3("position", light.position);
								ImGui::ColorEdit3("color", light.color);

								DirectX::XMFLOAT4X4 imguizmoMat = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, light.position[0], light.position[1], light.position[2], 1 };
								ImGuizmo::Manipulate(imguizmoView.m[0], imguizmoProj.m[0], ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, imguizmoMat.m[0]);
								DirectX::XMVECTOR s, r, t;
								DirectX::XMMatrixDecompose(&s, &r, &t, DirectX::XMLoadFloat4x4(&imguizmoMat));
								light.position[0] = DirectX::XMVectorGetX(t);
								light.position[1] = DirectX::XMVectorGetY(t);
								light.position[2] = DirectX::XMVectorGetZ(t);
							}
							else {
								ImGui::Text("UI not implemented for this light type");
							}
						}
					}
					else {
						ImGui::Text("Scene has no light");
					}
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();

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

	ImGui::Render();
}

void graphicsCommands() {
	dx12.compileShaders();
	dx12.resetDescriptorHeaps();
	dx12.resetAndMapFrameDataBuffer();

	DX12CommandList& cmdList = dx12.graphicsCommandLists[dx12.currentFrame];

	if (currentSceneIndex < scenes.size()) {
		const Scene& scene = scenes[currentSceneIndex];
		if (scene.tlasBuffer.buffer) {
			struct {
				DirectX::XMMATRIX screenToWorldMat;
				DirectX::XMVECTOR cameraPosition;
				int bounceCount;
				int sampleCount;
				int frameCount;
				int lightCount;
			} constants = {
					DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, scene.camera.viewProjMat)),
					scene.camera.position,
					2, 16,
					static_cast<int>(dx12.totalFrame % INT_MAX),
					static_cast<int>(scene.lights.size())
			};
			uint64 constantsOffset = dx12.appendFrameDataBuffer(&constants, sizeof(constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			uint64 lightsOffset = dx12.appendFrameDataBuffer(scene.lights.data(), scene.lights.size() * sizeof(scene.lights[0]), sizeof(scene.lights[0])) / sizeof(scene.lights[0]);
			{
				DX12Descriptor firstDescriptor = dx12.appendDescriptorCBV(dx12.frameDataBuffers[dx12.currentFrame].buffer, constantsOffset, sizeof(constants));
				dx12.appendDescriptorUAV(dx12.positionTexture.texture);
				dx12.appendDescriptorUAV(dx12.normalTexture.texture);
				dx12.appendDescriptorUAV(dx12.baseColorTexture.texture);
				dx12.appendDescriptorUAV(dx12.emissiveTexture.texture);
				dx12.appendDescriptorSRVTLAS(scene.tlasBuffer.buffer);
				dx12.appendDescriptorSRVStructuredBuffer(scene.instanceInfosBuffer.buffer, 0, scene.instanceInfoCount, sizeof(InstanceInfo));
				dx12.appendDescriptorSRVStructuredBuffer(scene.geometryInfosBuffer.buffer, 0, scene.geometryInfoCount, sizeof(GeometryInfo));
				dx12.appendDescriptorSRVStructuredBuffer(scene.triangleInfosBuffer.buffer, 0, scene.triangleInfoCount, sizeof(TriangleInfo));
				dx12.appendDescriptorSRVStructuredBuffer(scene.materialInfosBuffer.buffer, 0, scene.materialInfoCount, sizeof(MaterialInfo));
				for (auto& [name, model] : scene.models) {
					for (auto& texture : model.textures) {
						dx12.appendDescriptorSRVTexture(texture.texture);
					}
				}
				uint8 shaderTableBuffer[DX12Context::shaderTableRecordSize * 3];
				memcpy(shaderTableBuffer, dx12.primaryRayObjectProps->GetShaderIdentifier(L"rayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(shaderTableBuffer + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize, dx12.primaryRayObjectProps->GetShaderIdentifier(L"miss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize * 2, dx12.primaryRayObjectProps->GetShaderIdentifier(L"hitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize * 2 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
				uint64 shaderTableOffset = dx12.appendFrameDataBuffer(shaderTableBuffer, sizeof(shaderTableBuffer), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
				{
					PIXScopedEvent(cmdList.list, PIX_COLOR_DEFAULT, "primaryRays");

					cmdList.list->SetDescriptorHeaps(1, &dx12.cbvSrvUavDescriptorHeaps[dx12.currentFrame].heap);
					cmdList.list->SetPipelineState1(dx12.primaryRayStateObject);
					D3D12_GPU_VIRTUAL_ADDRESS shaderTablePtr = dx12.frameDataBuffers[dx12.currentFrame].buffer->GetGPUVirtualAddress() + shaderTableOffset;
					D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
					dispatchRaysDesc.Width = dx12.renderResolutionX;
					dispatchRaysDesc.Height = dx12.renderResolutionY;
					dispatchRaysDesc.Depth = 1;
					dispatchRaysDesc.RayGenerationShaderRecord = { shaderTablePtr, DX12Context::shaderTableRecordSize };
					dispatchRaysDesc.MissShaderTable = { shaderTablePtr + DX12Context::shaderTableRecordSize, DX12Context::shaderTableRecordSize, DX12Context::shaderTableRecordSize };
					dispatchRaysDesc.HitGroupTable = { shaderTablePtr + DX12Context::shaderTableRecordSize * 2, DX12Context::shaderTableRecordSize, DX12Context::shaderTableRecordSize };
					cmdList.list->DispatchRays(&dispatchRaysDesc);
				}
			}
			{
				DX12Descriptor firstDescriptor = dx12.appendDescriptorCBV(dx12.frameDataBuffers[dx12.currentFrame].buffer, constantsOffset, sizeof(constants));
				dx12.appendDescriptorSRVTexture(dx12.positionTexture.texture);
				dx12.appendDescriptorSRVTexture(dx12.normalTexture.texture);
				dx12.appendDescriptorSRVTexture(dx12.baseColorTexture.texture);
				dx12.appendDescriptorSRVTLAS(scene.tlasBuffer.buffer);
				if (scene.lights.size() > 0) {
					dx12.appendDescriptorSRVStructuredBuffer(dx12.frameDataBuffers[dx12.currentFrame].buffer, lightsOffset, scene.lights.size(), sizeof(scene.lights[0]));
				}
				dx12.appendDescriptorUAV(dx12.outputTexture.texture);
				uint8 shaderTableBuffer[DX12Context::shaderTableRecordSize * 3];
				memcpy(shaderTableBuffer, dx12.directLightRayObjectProps->GetShaderIdentifier(L"rayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(shaderTableBuffer + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize, dx12.directLightRayObjectProps->GetShaderIdentifier(L"miss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize * 2, dx12.directLightRayObjectProps->GetShaderIdentifier(L"hitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(shaderTableBuffer + DX12Context::shaderTableRecordSize * 2 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &firstDescriptor.gpuHandle, 8);
				uint64 shaderTableOffset = dx12.appendFrameDataBuffer(shaderTableBuffer, sizeof(shaderTableBuffer), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
				{
					PIXScopedEvent(cmdList.list, PIX_COLOR_DEFAULT, "directLightRays");
					cmdList.list->SetDescriptorHeaps(1, &dx12.cbvSrvUavDescriptorHeaps[dx12.currentFrame].heap);
					cmdList.list->SetPipelineState1(dx12.directLightRayStateObject);
					D3D12_GPU_VIRTUAL_ADDRESS shaderTablePtr = dx12.frameDataBuffers[dx12.currentFrame].buffer->GetGPUVirtualAddress() + shaderTableOffset;
					D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
					dispatchRaysDesc.Width = dx12.renderResolutionX;
					dispatchRaysDesc.Height = dx12.renderResolutionY;
					dispatchRaysDesc.Depth = 1;
					dispatchRaysDesc.RayGenerationShaderRecord = { shaderTablePtr, DX12Context::shaderTableRecordSize };
					dispatchRaysDesc.MissShaderTable = { shaderTablePtr + DX12Context::shaderTableRecordSize, DX12Context::shaderTableRecordSize, DX12Context::shaderTableRecordSize };
					dispatchRaysDesc.HitGroupTable = { shaderTablePtr + DX12Context::shaderTableRecordSize * 2, DX12Context::shaderTableRecordSize, DX12Context::shaderTableRecordSize };
					cmdList.list->DispatchRays(&dispatchRaysDesc);
				}
			}
		}
	}
	{
		PIXScopedEvent(cmdList.list, PIX_COLOR_DEFAULT, "swapChain + imGui");

		uint currentSwapChainImageIndex = dx12.swapChain->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER textureBarriers[2] = {};
		ID3D12Resource* textures[2] = { dx12.outputTexture.texture, dx12.swapChainImages[currentSwapChainImageIndex] };
		D3D12_RESOURCE_STATES textureStates[2][2] = {
			{D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE},
			{D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET }
		};
		for (int i = 0; i < countof(textureBarriers); i += 1) {
			textureBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			textureBarriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			textureBarriers[i].Transition.pResource = textures[i];
			textureBarriers[i].Transition.StateBefore = textureStates[i][0];
			textureBarriers[i].Transition.StateAfter = textureStates[i][1];
		}
		cmdList.list->ResourceBarrier(countof(textureBarriers), textureBarriers);

		DX12Descriptor swapChainDescriptor = dx12.appendDescriptorRTV(dx12.swapChainImages[currentSwapChainImageIndex]);
		cmdList.list->OMSetRenderTargets(1, &swapChainDescriptor.cpuHandle, false, nullptr);
		cmdList.list->ClearRenderTargetView(swapChainDescriptor.cpuHandle, DirectX::Colors::Black, 0, nullptr);

		D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(window.width), static_cast<float>(window.height), 0, 1 };
		RECT scissor = { 0, 0, window.width, window.height };
		cmdList.list->RSSetViewports(1, &viewport);
		cmdList.list->RSSetScissorRects(1, &scissor);

		cmdList.list->SetDescriptorHeaps(1, &dx12.cbvSrvUavDescriptorHeaps[dx12.currentFrame].heap);

		cmdList.list->SetPipelineState(dx12.swapChainPipelineState);
		cmdList.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList.list->SetGraphicsRootSignature(dx12.swapChainRootSignature);
		DX12Descriptor outputDescriptor = dx12.appendDescriptorSRVTexture(dx12.outputTexture.texture);
		cmdList.list->SetGraphicsRootDescriptorTable(0, outputDescriptor.gpuHandle);
		cmdList.list->DrawInstanced(3, 1, 0, 0);

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

		d3dAssert(imguiVertexBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&imguiVertexBuffer.mappedPtr)));
		d3dAssert(imguiIndexBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&imguiIndexBuffer.mappedPtr)));
		uint64 imguiVertexBufferOffset = 0;
		uint64 imguiIndexBufferOffset = 0;
		const ImDrawData* imguiDrawData = ImGui::GetDrawData();
		for (int i = 0; i < imguiDrawData->CmdListsCount; i += 1) {
			const ImDrawList& dlist = *imguiDrawData->CmdLists[i];
			uint64 verticesSize = dlist.VtxBuffer.Size * sizeof(ImDrawVert);
			uint64 indicesSize = dlist.IdxBuffer.Size * sizeof(ImDrawIdx);
			memcpy(imguiVertexBuffer.mappedPtr + imguiVertexBufferOffset, dlist.VtxBuffer.Data, verticesSize);
			memcpy(imguiIndexBuffer.mappedPtr + imguiIndexBufferOffset, dlist.IdxBuffer.Data, indicesSize);
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

		for (int i = 0; i < countof(textureBarriers); i += 1) {
			textureBarriers[i].Transition.StateBefore = textureStates[i][1];
			textureBarriers[i].Transition.StateAfter = textureStates[i][0];
		}
		cmdList.list->ResourceBarrier(countof(textureBarriers), textureBarriers);
	}

	dx12.unmapFrameDataBuffer();
	dx12.closeAndExecuteCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);
	HRESULT presentResult = dx12.swapChain->Present(0, 0);
	if (presentResult != S_OK) {
		if (presentResult == DXGI_STATUS_OCCLUDED) {
			OutputDebugStringA("SwapChain Present: DXGI_STATUS_OCCLUDED");
		}
		else if (presentResult == DXGI_STATUS_MODE_CHANGED) {
			OutputDebugStringA("SwapChain Present: DXGI_STATUS_MODE_CHANGED");
		}
		else if (presentResult == DXGI_STATUS_MODE_CHANGE_IN_PROGRESS) {
			OutputDebugStringA("SwapChain Present: DXGI_STATUS_MODE_CHANGE_IN_PROGRESS");
		}
		else {
			assert(false && "swapChain Present Error");
		}
	}
	dx12.totalFrame += 1;
	dx12.currentFrame = (dx12.currentFrame + 1) % dx12.maxFrameInFlight;
	if (dx12.totalFrame >= dx12.maxFrameInFlight) {
		dx12.waitAndResetCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);
	}
}

struct SettingInfo {
	enum Type {
		FullScreen,
		Scene,
		EndOfFile
	};

	Type type;
	std::string_view name;
	std::string_view path;
	int fullScreen;
};

struct SettingParser : public Parser {
	using Parser::Parser;

	void getInfo(SettingInfo& info) {
		Token token;
		getToken(token);
		if (token.type == Token::Identifier) {
			if (token.str == "FullScreen") {
				info.type = SettingInfo::FullScreen;
				getToken(token);
				token.toInt(info.fullScreen);
			}
			else if (token.str == "Scene") {
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
	settingsFileStr += "FullScreen: "s + (fullScreen ? "1\r\n" : "0\r\n");
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
	window.show();

	if (fileExists(settingsFilePath)) {
		SettingParser parser(settingsFilePath);
		SettingInfo info;
		while (true) {
			parser.getInfo(info);
			if (info.type == SettingInfo::FullScreen) {
				dx12.swapChain->SetFullscreenState(info.fullScreen, nullptr);
			}
			else if (info.type == SettingInfo::Scene) {
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
