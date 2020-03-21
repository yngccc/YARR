/************************************************************************************************/
/*			Copyright (C) 2020 By Yang Chen (yngccc@gmail.com). All Rights Reserved.			*/
/************************************************************************************************/

#pragma once

#define _USE_MATH_DEFINES
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <stack>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include <comdef.h>
#include <commdlg.h>
#include <wtsapi32.h>
#include <initguid.h>
#undef near
#undef far

#include <directxmath.h>
#include <directxcolors.h>

template<typename I = size_t, typename T, size_t N>
constexpr I countof(T(&)[N]) {
	return static_cast<I>(N);
}

#define megabytes(n) (n * 1024 * 1024)

template<typename T = size_t>
T roundUp(size_t n, size_t multi) {
	size_t remainder = n % multi;
	if (remainder == 0) {
		return static_cast<T>(n);
	}
	else {
		return static_cast<T>(n + (multi - remainder));
	}
}

void debugPrintf(const char* fmt...) {
	char msg[256] = {};
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(msg, sizeof(msg), fmt, vl);
	va_end(vl);
	OutputDebugStringA(msg);
}

struct Window {
	HWND handle;
	int width, height;
	int mouseX, mouseY;
	int rawMouseDx, rawMouseDy;

	static Window create(LRESULT(*wndMsgCallback)(HWND, UINT, WPARAM, LPARAM)) {
		HMODULE instanceHandle = GetModuleHandle(nullptr);
		WNDCLASSA windowClass = {};
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = wndMsgCallback;
		windowClass.hInstance = instanceHandle;
		windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
		windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		windowClass.lpszClassName = "YarrWindowClassName";
		ATOM registerResult = RegisterClassA(&windowClass);
		assert(registerResult);

		HRESULT setDpiAwarenessResult = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
		assert(setDpiAwarenessResult == S_OK);
		int screenW = GetSystemMetrics(SM_CXSCREEN);
		int screenH = GetSystemMetrics(SM_CYSCREEN);
		int windowW = static_cast<int>(screenW * 0.9f);
		int windowH = static_cast<int>(screenH * 0.9f);
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
	void show() {
		ShowWindow(handle, SW_SHOW);
	}
	void processMessages() {
		rawMouseDx = 0;
		rawMouseDy = 0;
		MSG msg;
		while (PeekMessageA(&msg, handle, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}
	void lockCursor() {
		POINT cursorPoint = {};
		GetCursorPos(&cursorPoint);
		RECT rect = {};
		rect.left = cursorPoint.x;
		rect.top = cursorPoint.y;
		rect.right = rect.left;
		rect.bottom = rect.top;
		ClipCursor(&rect);
	}
	void unlockCursor() {
		ClipCursor(nullptr);
	}
};

void setCurrentDirToExeDir() {
	char path[512] = {};
	DWORD n = GetModuleFileNameA(nullptr, path, sizeof(path));
	assert(n != sizeof(path) && GetLastError() != ERROR_INSUFFICIENT_BUFFER);
	char* path_ptr = strrchr(path, '\\');
	assert(path_ptr);
	path_ptr[0] = '\0';
	assert(SetCurrentDirectoryA(path));
}

std::vector<char> readFile(const char* fileName) {
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	assert(fileHandle != INVALID_HANDLE_VALUE);
	DWORD fileSize = GetFileSize(fileHandle, nullptr);
	assert(fileSize != INVALID_FILE_SIZE);
	std::vector<char> fileContent;
	fileContent.resize(fileSize);
	DWORD bytesRead = 0;
	BOOL readFileResult = ReadFile(fileHandle, fileContent.data(), fileSize, &bytesRead, nullptr);
	assert(readFileResult);
	assert(bytesRead == fileSize);
	CloseHandle(fileHandle);
	return fileContent;
}
