/************************************************************************************************/
/*			Copyright (C) 2020 By Yang Chen (yngccc@gmail.com). All Rights Reserved.			*/
/************************************************************************************************/

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellscalingapi.h>
#include <xinput.h>
#undef far
#undef near

#include <cassert>
#include <cstdlib>
#include <cstdio>

struct Window {
	HWND handle;
	int width;
	int height;
	int mouseX;
	int mouseY;
	int rawMouseDx;
	int rawMouseDy;
};

Window createWindow(LRESULT(*wndMsgCallback)(HWND, UINT, WPARAM, LPARAM)) {
	HMODULE instanceHandle = GetModuleHandle(nullptr);
	WNDCLASSA windowClass = {};
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = wndMsgCallback;
	windowClass.hInstance = instanceHandle;
	windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	windowClass.lpszClassName = "YarrWindowClassName";
	assert(RegisterClassA(&windowClass));

	assert(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) == S_OK);
	int screenW = GetSystemMetrics(SM_CXSCREEN);
	int screenH = GetSystemMetrics(SM_CYSCREEN);
	int windowW = static_cast<int>(screenW * 0.7f);
	int windowH = static_cast<int>(screenH * 0.8f);
	int windowX = static_cast<int>((screenW - windowW) * 0.5f);
	int windowY = static_cast<int>((screenH - windowH) * 0.5f);
	DWORD window_style = WS_OVERLAPPEDWINDOW;
	char window_title[128] = {};
	snprintf(window_title, sizeof(window_title), "YARR %d x %d", windowW, windowH);
	HWND windowHandle = CreateWindowExA(0, windowClass.lpszClassName, window_title, window_style, windowX, windowY, windowW, windowH, nullptr, nullptr, instanceHandle, nullptr);
	assert(windowHandle);

	RAWINPUTDEVICE rawInputDevice;
	rawInputDevice.usUsagePage = 0x01;
	rawInputDevice.usUsage = 0x02;
	rawInputDevice.dwFlags = RIDEV_INPUTSINK;
	rawInputDevice.hwndTarget = windowHandle;
	RegisterRawInputDevices(&rawInputDevice, 1, sizeof(rawInputDevice));

	RECT windowRect;
	GetClientRect(windowHandle, &windowRect);

	Window wnd = {};
	wnd.handle = windowHandle;
	wnd.width = windowRect.right - windowRect.left;
	wnd.height = windowRect.bottom - windowRect.top;
	return wnd;
};

void windowHandleMsg(Window* wnd) {
	MSG msg;
	while (PeekMessageA(&msg, wnd->handle, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}

LRESULT windowMsgCallback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	LRESULT result = DefWindowProcA(hwnd, msg, wparam, lparam);
	return result;
}

int main(int argc, char** argv) {
	Window window = createWindow(windowMsgCallback);
	ShowWindow(window.handle, SW_SHOW);

	for (;;) {
		MSG msg;
		while (PeekMessageA(&msg, window.handle, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}
}