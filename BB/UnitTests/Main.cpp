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

#include "OS/OSDevice.h"

using namespace BB;
int main()
{
	testing::InitGoogleTest();
	RUN_ALL_TESTS();
	WindowHandle mainWindow = OS::CreateOSWindow(OS::OS_WINDOW_STYLE::MAIN, 250, 200, 250, 200, "Unit Test Main Window");

	WindowHandle childWindow = OS::CreateOSWindow(OS::OS_WINDOW_STYLE::CHILD, 100, 100, 250, 50, "Unit Test Child Window 1");

	WindowHandle destroyWindow = OS::CreateOSWindow(OS::OS_WINDOW_STYLE::CHILD, 150, 100, 250, 100, "Unit Test childWindow window");

	bool hasWindows = true;
	while (hasWindows)
	{
		hasWindows = OS::ProcessMessages();
	}

	return 0;
}