// main.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <gtest/gtest.h>
#include "Framework/Allocators_UTEST.h"
#include "Framework/Array_UTEST.h"
#include "Framework/Pool_UTEST.h"
#include "Framework/Hashmap_UTEST.h"
#include "Framework/MemoryArena_UTEST.h"
#include "Framework/MemoryOperations_UTEST.h"
#include "Framework/Slice_UTEST.h"
#include "Framework/Slotmap_UTEST.h"
#include "Framework/String_UTEST.h"
#include "Framework/FileReadWrite_UTEST.h"

#include "OS/Program.h"

using namespace BB;
int main()
{
	testing::InitGoogleTest();
	RUN_ALL_TESTS();
	WindowHandle mainWindow = Program::CreateOSWindow(Program::OS_WINDOW_STYLE::MAIN, 250, 200, 250, 200, L"Unit Test Main Window");

	WindowHandle childWindow = Program::CreateOSWindow(Program::OS_WINDOW_STYLE::CHILD, 100, 100, 250, 50, L"Unit Test Child Window 1");

	WindowHandle destroyWindow = Program::CreateOSWindow(Program::OS_WINDOW_STYLE::CHILD, 150, 100, 250, 100, L"Unit Test childWindow window");

	bool hasWindows = true;
	while (hasWindows)
	{
		hasWindows = Program::ProcessMessages();
	}

	return 0;
}