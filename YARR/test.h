#include "miscs.h"

static uint64 _testErrorCount_ = 0;
static uint64 _testCount_ = 0;
static uint64 _caseErrorCount_ = 0;
static uint64 _caseCount_ = 0;
static uint64 _assertErrorCount_ = 0;
static uint64 _assertCount_ = 0;

void TEST(const char* name) {
	_testCount_ += 1;
	_caseErrorCount_ = 0;
	_caseCount_ = 0;
	OutputDebugStringA((name + "\n"s).c_str());
}

void TESTEND() {
	if (_caseErrorCount_ > 0) {
		_testErrorCount_ += 1;
	}
	OutputDebugStringA("\n");
}

void CASE(const char* name) {
	_caseCount_ += 1;
	_assertErrorCount_ = 0;
	_assertCount_ = 0;
	OutputDebugStringA(("  "s + name + ": ").c_str());
}

void CASEEND() {
	if (_assertErrorCount_ > 0) {
		_caseErrorCount_ += 1;
		OutputDebugStringA("failed");
	}
	else {
		OutputDebugStringA("passed");
	}
	OutputDebugStringA((" [" + std::to_string(_assertCount_ - _assertErrorCount_) + "/" + std::to_string(_assertCount_) + "]\n").c_str());
}

void ASSERT(bool cond) {
	_assertCount_ += 1;
	if (!cond) {
		_assertErrorCount_ += 1;
	}
}

void REPORT() {
	OutputDebugStringA(("Total of " + std::to_string(_testCount_ - _testErrorCount_) + "/" + std::to_string(_testCount_) + " tests passed\n\n\n").c_str());
}

void runTests() {
	TEST("RingBuffer");
	{
		CASE("Add");
		{
			RingBuffer<int> ringBuffer(128);
			ASSERT(ringBuffer.size() == 0);
			for (int i = 0; i < 128; i += 1) {
				ringBuffer.push(i);
			}
			ASSERT(ringBuffer.size() == 128);
			for (int i = 0; i < 128; i += 1) {
				ASSERT(ringBuffer[i] == i);
			}
			ringBuffer.push(128);
			ASSERT(ringBuffer.size() == 128);
			for (int i = 0; i < 128; i += 1) {
				ASSERT(ringBuffer[i] == i + 1);
			}
			ringBuffer.push(129);
			ASSERT(ringBuffer.size() == 128);
			for (int i = 0; i < 128; i += 1) {
				ASSERT(ringBuffer[i] == i + 2);
			}
			ringBuffer.clear();
			for (int i = 0; i < 128 + 100; i += 1) {
				ringBuffer.push(i);
			}
			ASSERT(ringBuffer.size() == 128);
			for (int i = 0; i < 128; i += 1) {
				ASSERT(ringBuffer[i] == i + 100);
			}
		}
		CASEEND();
	}
	TESTEND();
	REPORT();
}
