#pragma once
#include "../TestValues.h"
#include "OS/Program.h"

TEST(Program_IO, Read_Write_Files)
{
	BB::LinearAllocator_t t_Allocator(1024);

	constexpr wchar* DOC_NAME = L"READWRITETEST.txt";
	constexpr char* DOC_DATA = "HELLO WORLD! I'm a BB engine unit test for file read and writing.";

	BB::OSFileHandle t_TestFile = BB::Program::CreateOSFile(DOC_NAME);
	
	BB::Buffer t_WriteBuffer;
	t_WriteBuffer.size = strnlen_s(DOC_DATA, 1024);
	t_WriteBuffer.data = DOC_DATA;
	BB::Program::WriteToFile(t_TestFile, t_WriteBuffer);

	BB::Program::CloseOSFile(t_TestFile);

	BB::Buffer t_ReadBuffer = BB::Program::ReadOSFile(t_Allocator, DOC_NAME);

	char* t_TestText = BBnewArr(t_Allocator, t_ReadBuffer.size + 1, char);
	memcpy(t_TestText, t_ReadBuffer.data, t_ReadBuffer.size);
	t_TestText[t_ReadBuffer.size] = '\0';

	//Should be equal, or else read or write is broken.
	ASSERT_STREQ(t_TestText, DOC_DATA);

	t_Allocator.Clear();
}

TEST(Program_IO, Read_Write_Files_Chunks)
{
	BB::LinearAllocator_t t_Allocator(1024);

	constexpr wchar* DOC_NAME = L"READWRITETEST_CHUNK.txt";
	constexpr char* DOC_DATA = "HELLO WORLD! I'm a BB engine unit test for file read and writing.";
	constexpr char* DOC_DATA_CHUNK_ONE = "HELLO WORLD!";
	constexpr char* DOC_DATA_CHUNK_TWO = " I'm a BB engine unit";
	constexpr char* DOC_DATA_CHUNK_THREE = " test for file read and writing.";

	BB::OSFileHandle t_TestFile = BB::Program::CreateOSFile(DOC_NAME);

	BB::Buffer t_WriteBuffer;
	{ //Chunk one
		t_WriteBuffer.size = strnlen_s(DOC_DATA_CHUNK_ONE, 1024);
		t_WriteBuffer.data = DOC_DATA_CHUNK_ONE;
		BB::Program::WriteToFile(t_TestFile, t_WriteBuffer);
	}
	{ //Chunk two
		t_WriteBuffer.size = strnlen_s(DOC_DATA_CHUNK_TWO, 1024);
		t_WriteBuffer.data = DOC_DATA_CHUNK_TWO;
		BB::Program::WriteToFile(t_TestFile, t_WriteBuffer);
	}
	{ //Chunk three
		t_WriteBuffer.size = strnlen_s(DOC_DATA_CHUNK_THREE, 1024);
		t_WriteBuffer.data = DOC_DATA_CHUNK_THREE;
		BB::Program::WriteToFile(t_TestFile, t_WriteBuffer);
	}

	BB::Program::CloseOSFile(t_TestFile);

	BB::Buffer t_ReadBuffer = BB::Program::ReadOSFile(t_Allocator, DOC_NAME);

	char* t_TestText = BBnewArr(t_Allocator, t_ReadBuffer.size + 1, char);
	memcpy(t_TestText, t_ReadBuffer.data, t_ReadBuffer.size);
	t_TestText[t_ReadBuffer.size] = '\0';

	//Should be equal, or else read or write is broken.
	ASSERT_STREQ(t_TestText, DOC_DATA);

	t_Allocator.Clear();
}

TEST(Program_IO, Read_Write_Files_Same_File)
{
	BB::LinearAllocator_t t_Allocator(1024);

	constexpr wchar* DOC_NAME = L"READWRITETEST_SAMEFILE.txt";
	constexpr char* DOC_DATA = "HELLO WORLD! I'm a BB engine unit test for file read and writing.";

	BB::OSFileHandle t_TestFile = BB::Program::CreateOSFile(DOC_NAME);

	BB::Buffer t_WriteBuffer;
	t_WriteBuffer.size = strnlen_s(DOC_DATA, 1024);
	t_WriteBuffer.data = DOC_DATA;
	BB::Program::WriteToFile(t_TestFile, t_WriteBuffer);

	BB::Program::SetOSFilePosition(t_TestFile, 0, BB::Program::OS_FILE_READ_POINT::BEGIN);

	BB::Buffer t_ReadBuffer = BB::Program::ReadOSFile(t_Allocator, t_TestFile);

	char* t_TestText = BBnewArr(t_Allocator, t_ReadBuffer.size + 1, char);
	memcpy(t_TestText, t_ReadBuffer.data, t_ReadBuffer.size);
	t_TestText[t_ReadBuffer.size] = '\0';

	//Should be equal, or else read or write is broken.
	ASSERT_STREQ(t_TestText, DOC_DATA);

	t_Allocator.Clear();
}