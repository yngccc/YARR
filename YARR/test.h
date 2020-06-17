#include "miscs.h"

static size_t _testErrorCount_ = 0;
static size_t _testCount_ = 0;
static size_t _caseErrorCount_ = 0;
static size_t _caseCount_ = 0;
static size_t _assertErrorCount_ = 0;
static size_t _assertCount_ = 0;

void TEST(const char* name) {
	_testCount_ += 1;
	_caseErrorCount_ = 0;
	_caseCount_ = 0;
	debugPrintf("%s:\n", name);
}

void TESTEND() {
	if (_caseErrorCount_ > 0) {
		_testErrorCount_ += 1;
	}
	debugPrintf("\n");
}

void CASE(const char* name) {
	_caseCount_ += 1;
	_assertErrorCount_ = 0;
	_assertCount_ = 0;
	debugPrintf("  %s: ", name);
}

void CASEEND() {
	if (_assertErrorCount_ > 0) {
		_caseErrorCount_ += 1;
		debugPrintf("failed");
	}
	else {
		debugPrintf("passed");
	}
	debugPrintf(" [%d/%d]\n", _assertCount_ - _assertErrorCount_, _assertCount_);
}

void ASSERT(bool cond) {
	_assertCount_ += 1;
	if (!cond) {
		_assertErrorCount_ += 1;
	}
}

void REPORT() {
	debugPrintf("Total of %d/%d tests passed\n\n\n", _testCount_ - _testErrorCount_, _testCount_);
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
