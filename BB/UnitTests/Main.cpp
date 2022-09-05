// main.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <gtest/gtest.h>
#include "Framework/Allocators_UTEST.h"
#include "Framework/Array_UTEST.h"
#include "Framework/Pool_UTEST.h"
#include "Framework/Hashmap_UTEST.h"
#include "Framework/MemoryArena_UTEST.h"
#include "Framework/Slice_UTEST.h"
#include "Framework/Slotmap_UTEST.h"
#include "Framework/String_UTEST.h"

#include "OS/OSDevice.h"

int main()
{
	testing::InitGoogleTest();
	RUN_ALL_TESTS();
	BB::WindowHandle mainWindow = BB::AppOSDevice().CreateOSWindow(BB::OS_WINDOW_STYLE::MAIN, 250, 200, 250, 200, "Unit Test Main Window");

	BB::WindowHandle childWindow = BB::AppOSDevice().CreateOSWindow(BB::OS_WINDOW_STYLE::CHILD, 100, 100, 250, 50, "Unit Test Child Window 1");

	BB::WindowHandle deletedWindow = BB::AppOSDevice().CreateOSWindow(BB::OS_WINDOW_STYLE::CHILD, 150, 100, 250, 100, "Unit Test deleted window");

	BB::AppOSDevice().DestroyOSWindow(deletedWindow);

	while (BB::AppOSDevice().ProcessMessages())
	{

	}

	return 0;
}