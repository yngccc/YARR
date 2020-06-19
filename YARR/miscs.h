/************************************************************************************************/
/*			Copyright (C) 2020 By Yang Chen (yngccc@gmail.com). All Rights Reserved.			*/
/************************************************************************************************/

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include <comdef.h>
#include <commdlg.h>
#include <cderr.h>
#include <wtsapi32.h>
#include <initguid.h>
#undef near
#undef far

#define _XM_SSE4_INTRINSICS_
#include <directxmath.h>
#include <directxcolors.h>

#include <xinput.h>

#define _USE_MATH_DEFINES
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <stack>
#include <thread>
#include <mutex>
#include <charconv>
#include <fstream>
#include <filesystem>

using namespace std::string_literals;

#define megabytes(n) (n * 1024 * 1024)

template<typename I = size_t, typename T, size_t N>
constexpr I countof(T(&)[N]) {
	return static_cast<I>(N);
}

template<typename T = size_t>
T align(size_t x, size_t n) {
	size_t remainder = x % n;
	if (remainder == 0) {
		return static_cast<T>(x);
	}
	else {
		return static_cast<T>(x + (n - remainder));
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

struct Exception : public std::exception {
	std::string err;
	Exception(const std::string& str) : err(str) {
		debugPrintf("%s\n", err.c_str());
	}
	Exception(std::string&& str) : err(std::move(str)) {
		debugPrintf("%s\n", err.c_str());
	}
	const char* what() const {
		return err.c_str();
	}
};

std::string getErrorStr(DWORD error = GetLastError()) {
	char buf[256] = {};
	DWORD n = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, static_cast<DWORD>(sizeof(buf)), nullptr);
	if (n == 0) {
		snprintf(buf, sizeof(buf), "<no error string, FormatMessageA failed>");
	}
	return buf;
}

template <typename T>
struct RingBuffer {
	std::vector<T> buffer;
	size_t readPos = 0;
	size_t writePos = 0;

	RingBuffer(size_t size) : buffer(size + 1) {
	}
	void clear() {
		readPos = 0;
		writePos = 0;
	}
	size_t size() {
		if (writePos >= readPos) {
			return writePos - readPos;
		}
		else {
			return buffer.size() - readPos + writePos;
		}
	}
	T& operator[](size_t index) {
		assert(index < size());
		size_t readIndex = readPos + index;
		if (readIndex < buffer.size()) {
			return buffer[readIndex];
		}
		else {
			return buffer[readIndex - buffer.size()];
		}
	}
	void push(const T& elem) {
		buffer[writePos] = elem;
		writePos = (writePos + 1) % buffer.size();
		if (writePos == readPos) {
			readPos = (readPos + 1) % buffer.size();
		}
	}
};

struct Window {
	HWND handle = nullptr;
	int width = 0;
	int height = 0;
	int mouseX = 0;
	int mouseY = 0;
	int rawMouseDx = 0;
	int rawMouseDy = 0;
	int mouseWheel = 0;

	Window() = default;
	Window(LRESULT(*wndMsgCallback)(HWND, UINT, WPARAM, LPARAM)) {
		HMODULE instanceHandle = GetModuleHandle(nullptr);
		WNDCLASSA windowClass = {};
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = wndMsgCallback;
		windowClass.hInstance = instanceHandle;
		windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
		windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
		windowClass.lpszClassName = "YarrWindowClassName";
		ATOM registerResult = RegisterClassA(&windowClass);
		if (!registerResult) {
			throw Exception("RegisterClassA error:" + getErrorStr());
		}

		HRESULT setDpiAwarenessResult = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
		if (setDpiAwarenessResult != S_OK) {
			throw Exception("SetProcessDpiAwareness error:" + getErrorStr());
		}
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
		if (!windowHandle) {
			throw Exception("CreateWindowExA error:" + getErrorStr());
		}

		RAWINPUTDEVICE rawInputDevice;
		rawInputDevice.usUsagePage = 0x01;
		rawInputDevice.usUsage = 0x02;
		rawInputDevice.dwFlags = RIDEV_INPUTSINK;
		rawInputDevice.hwndTarget = windowHandle;
		BOOL registerSuccess = RegisterRawInputDevices(&rawInputDevice, 1, sizeof(rawInputDevice));
		if (!registerSuccess) {
			throw Exception("RegisterRawInputDevices error:" + getErrorStr());
		}

		RECT windowRect;
		BOOL getRectSuccess = GetClientRect(windowHandle, &windowRect);
		if (!getRectSuccess) {
			throw Exception("GetClientRect error:" + getErrorStr());
		}

		handle = windowHandle;
		width = windowRect.right - windowRect.left;
		height = windowRect.bottom - windowRect.top;
	};
	void show() {
		ShowWindow(handle, SW_SHOW);
	}
	void processMessages() {
		rawMouseDx = 0;
		rawMouseDy = 0;
		mouseWheel = 0;
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
	wchar_t buf[512];
	DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(countof(buf)));
	if (n >= countof(buf)) {
		throw Exception("GetModuleFileNameA error:" + getErrorStr());
	}
	std::filesystem::path path(buf);
	std::filesystem::path parentPath = path.parent_path();
	BOOL success = SetCurrentDirectoryW(parentPath.c_str());
	if (!success) {
		throw Exception("SetCurrentDirectoryW error:" + getErrorStr());
	}
}

bool fileExists(const std::filesystem::path& filePath) {
	DWORD dwAttrib = GetFileAttributesW(filePath.c_str());
	return dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<char> readFile(const std::filesystem::path& filePath) {
	std::fstream file(filePath, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		throw Exception("std::fstream error: cannot open \"" + filePath.string() + "\"");
	}
	std::vector<char> data(std::istreambuf_iterator<char>{file}, {});
	return data;
}


void writeFile(const std::filesystem::path& filePath, const std::string& str) {
	std::fstream file(filePath, std::ios::out | std::ios_base::trunc | std::ios::binary);
	file << str;
}

std::filesystem::path openFileDialog() {
	std::filesystem::path filePath;
	wchar_t buf[256] = {};
	OPENFILENAMEW openFileName = {};
	openFileName.lStructSize = sizeof(openFileName);
	openFileName.hwndOwner = GetActiveWindow();
	openFileName.lpstrFile = buf;
	openFileName.nMaxFile = static_cast<DWORD>(countof(buf));
	BOOL success = GetOpenFileNameW(&openFileName);
	if (!success) {
		DWORD error = CommDlgExtendedError();
		if (error == FNERR_BUFFERTOOSMALL) {
			throw Exception("GetOpenFileNameW error: FNERR_BUFFERTOOSMALL");
		}
		else if (error == FNERR_INVALIDFILENAME) {
			throw Exception("GetOpenFileNameW error: FNERR_INVALIDFILENAME");
		}
		else if (error == FNERR_SUBCLASSFAILURE) {
			throw Exception("GetOpenFileNameW error: FNERR_SUBCLASSFAILURE");
		}
		else {
			throw Exception("GetOpenFileNameW error: unknown error code");
		}
	}
	filePath = buf;
	return filePath;
}

struct Token {
	enum Type {
		Identifier,
		String,
		Number,
		EndOfFile
	};

	Type type;
	std::string_view str;

	void toFloat(float& fp) {
		if (type != Number) {
			throw Exception("Token::toFloat: token is not a Token::Number");
		}
		std::from_chars_result result = std::from_chars(str.data(), str.data() + str.length(), fp);
		if (result.ptr != str.data() + str.length()) {
			throw Exception("Token::toFloat: cannot parse string \"" + std::string(str) + "\"");
		}
	}
};

struct Parser {
	std::vector<char> fileData;
	size_t filePos = 0;

	Parser(const std::filesystem::path& filePath) : fileData(readFile(filePath)) {
	}
	void getToken(Token& token) {
		if (filePos >= fileData.size()) {
			token.type = Token::EndOfFile;
			return;
		}
		while (isspace(static_cast<unsigned char>(fileData[filePos])) || fileData[filePos] == ':' || fileData[filePos] == '[' || fileData[filePos] == ']' || fileData[filePos] == ',') {
			filePos += 1;
			if (filePos >= fileData.size()) {
				token.type = Token::EndOfFile;
				return;
			}
		}
		if (fileData[filePos] == '"') {
			filePos += 1;
			if (filePos > fileData.size()) {
				throw Exception("Parser::getToken error: cannot parse string, reached end of file before encountering second \"");
			}
			const char* ptr = fileData.data() + filePos;
			size_t len = 0;
			while (fileData[filePos] != '"') {
				if (fileData[filePos] == '\n') {
					throw Exception("Parser::getToken error: cannot parse string, reached newline before encountering second \"");
				}
				filePos += 1;
				len += 1;
				if (filePos >= fileData.size()) {
					throw Exception("Parser::getToken error: cannot parse string, reached end of file before encountering second \"");
				}
			}
			filePos += 1;
			token.type = Token::String;
			token.str = { ptr, len };
		}
		else if (isdigit(static_cast<unsigned char>(fileData[filePos])) || fileData[filePos] == '+' || fileData[filePos] == '-' || fileData[filePos] == '.') {
			const char* ptr = fileData.data() + filePos;
			size_t len = 1;
			filePos += 1;
			while (filePos < fileData.size() && (isalnum(static_cast<unsigned char>(fileData[filePos])) || fileData[filePos] == '.')) {
				filePos += 1;
				len += 1;
			}
			token.type = Token::Number;
			token.str = { ptr, len };
		}
		else if (isalpha(static_cast<unsigned char>(fileData[filePos]))) {
			const char* ptr = fileData.data() + filePos;
			size_t len = 1;
			filePos += 1;
			while (filePos < fileData.size() && (isalnum(static_cast<unsigned char>(fileData[filePos])) || fileData[filePos] == '-' || fileData[filePos] == '_')) {
				filePos += 1;
				len += 1;
			}
			token.type = Token::Identifier;
			token.str = { ptr, len };
		}
		else {
			throw Exception("Parser::getToken error: unknown character "s + fileData[filePos]);
		}
	}
};