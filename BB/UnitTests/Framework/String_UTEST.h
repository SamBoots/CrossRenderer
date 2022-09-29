#pragma once
#include "../TestValues.h"
#include "Storage/BBString.h"

#pragma region WString
TEST(String_DataStructure, append_insert_push_pop_copy_assignment)
{
	constexpr size_t StringReserve = 128;
	constexpr const char* TestString = "First Unit test of the string is now being done.";

	const size_t allocatorSize = BB::kbSize * 16;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::String t_String(t_Allocator);

	//test single string and compare.
	{
		t_String.append(TestString);

		EXPECT_EQ(t_String.compare(TestString), true) << "Append(const char*) failed";
	}

	t_String.clear();

	//test single string and compare, this time from a BB::String
	{
		BB::String t_TestString(t_Allocator, TestString);

		t_String.append(t_TestString);

		EXPECT_EQ(t_String.compare(t_TestString), true) << "Append(const char*) failed";
	}

	t_String.clear();

#pragma region append Test
	//test break the string apart and use parts of it.
	{
		const char* t_AppendString = "First Unit test of the string is now being done.";
		t_String.append(t_AppendString, 10);

		EXPECT_EQ(t_String.compare("First Unit"), true) << "Append(const char*, size_t) failed";

		t_String.append(t_AppendString + t_String.size(), 12);

		EXPECT_EQ(t_String.compare(10, " test of the"), true) << "Append(const char*, size_t) failed second call";

		t_String.append(t_AppendString + t_String.size(), 26);

		EXPECT_EQ(t_String.compare(t_AppendString), true) << "Append(const char*, size_t) failed third call";
	}

	t_String.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::String t_AppendString(t_Allocator, "First Unit test of the string is now being done.");
		t_String.append(t_AppendString, 0, 10);

		EXPECT_EQ(t_String.compare("First Unit"), true) << "Append(BB::String&, size_t, size_t) failed";

		t_String.append(t_AppendString, t_String.size(), 12);

		EXPECT_EQ(t_String.compare(10, " test of the"), true) << "Append(BB::String&, size_t, size_t) failed second call";

		t_String.append(t_AppendString, t_String.size(), 26);

		EXPECT_EQ(t_String.compare(t_AppendString), true) << "Append(BB::String&, size_t, size_t) failed third call";
	}
#pragma endregion //append Test

	t_String.clear();

#pragma region insert Test
	//test break the string apart and use parts of it
	{
		constexpr const char* a_One = "One";
		constexpr const char* a_Two = "Two";
		constexpr const char* a_Three = "Three";
		constexpr const char* a_Four = "Four";

		t_String.insert(0, a_Four);
		EXPECT_EQ(t_String.compare("Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(0, a_Two);
		EXPECT_EQ(t_String.compare("TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(3, a_One);
		EXPECT_EQ(t_String.compare("TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(6, a_Three);
		EXPECT_EQ(t_String.compare("TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}

	t_String.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::String t_AppendString(t_Allocator, "One, Two, Three, Four");
		t_String.insert(0, t_AppendString, 17, 4);
		EXPECT_EQ(t_String.compare("Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
		
		t_String.insert(0, t_AppendString, 5, 3);
		EXPECT_EQ(t_String.compare("TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(3, t_AppendString, 0, 3);
		EXPECT_EQ(t_String.compare("TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	
		t_String.insert(6, t_AppendString, 10, 5);
		EXPECT_EQ(t_String.compare("TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}
#pragma endregion //insert Test

	//Copy
	BB::String t_CopyString(t_String);

	EXPECT_EQ(t_String.compare(t_String), true) << "Copy constructor failed.";

	BB::String t_CopyOperatorString(t_Allocator);
	t_CopyOperatorString = t_CopyString;

	EXPECT_EQ(t_String.compare(t_CopyString), true) << "Copy operator failed.";

	//Assignment
	BB::String t_AssignmentString(std::move(t_CopyOperatorString));

	EXPECT_EQ(t_String.compare(t_String), true) << "Assignment constructor failed.";
	EXPECT_EQ(t_CopyOperatorString.data(), nullptr) << "Assignment constructor failed, the other string has still has data while it shouldn't have.";

	BB::String t_AssignmentOperatorString(t_Allocator);
	t_AssignmentOperatorString = std::move(t_AssignmentString);

	EXPECT_EQ(t_String.compare(t_String), true) << "Assignment operator failed.";
	EXPECT_EQ(t_AssignmentString.data(), nullptr) << "Assignment operator failed, the other string has still has data while it shouldn't have.";
}

TEST(String_DataStructure, reserve_shrink)
{
	constexpr size_t StringReserve = 128;

	const size_t allocatorSize = BB::kbSize * 16;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::String t_String(t_Allocator);

	const char* t_AppendString = "First Unit test of the string is now being done.";

	t_String.reserve(128);
	const size_t t_StringCapacity = t_String.capacity();
	const size_t t_ModifiedC_StringSize = BB::Math::RoundUp(strlen(t_AppendString) + 1, BB::String_Specs::multipleValue);

	ASSERT_LT(t_ModifiedC_StringSize, t_StringCapacity) << "Modified string size is bigger then the string capacity, test is unaccurate.";

	t_String.append(t_AppendString);

	EXPECT_EQ(t_String.compare(t_AppendString), true) << "Append(const char*) failed";

	t_String.shrink_to_fit();

	EXPECT_EQ(t_String.capacity(), t_ModifiedC_StringSize) << "String did not shrink down to the correct size";
}
#pragma endregion //String

#pragma region WString
TEST(WideString_DataStructure, append_insert_push_pop_copy_assignment)
{
	constexpr size_t StringReserve = 128;
	constexpr const wchar_t* TestString = L"First Unit test of the string is now being done.";

	const size_t allocatorSize = BB::kbSize * 16;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::WString t_String(t_Allocator);

	//test single string and compare.
	{
		t_String.append(TestString);

		EXPECT_EQ(t_String.compare(TestString), true) << "Append(const char*) failed";
	}

	t_String.clear();

	//test single string and compare, this time from a BB::String
	{
		BB::WString t_TestString(t_Allocator, TestString);

		t_String.append(t_TestString);

		EXPECT_EQ(t_String.compare(t_TestString), true) << "Append(const char*) failed";
	}

	t_String.clear();

#pragma region append Test
	//test break the string apart and use parts of it.
	{
		const wchar_t* t_AppendString = L"First Unit test of the string is now being done.";
		t_String.append(t_AppendString, 10);

		EXPECT_EQ(t_String.compare(L"First Unit"), true) << "Append(const char*, size_t) failed";

		t_String.append(t_AppendString + t_String.size(), 12);

		EXPECT_EQ(t_String.compare(10, L" test of the"), true) << "Append(const char*, size_t) failed second call";

		t_String.append(t_AppendString + t_String.size(), 26);

		EXPECT_EQ(t_String.compare(t_AppendString), true) << "Append(const char*, size_t) failed third call";
	}

	t_String.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::WString t_AppendString(t_Allocator, L"First Unit test of the string is now being done.");
		t_String.append(t_AppendString, 0, 10);

		EXPECT_EQ(t_String.compare(L"First Unit"), true) << "Append(BB::String&, size_t, size_t) failed";

		t_String.append(t_AppendString, t_String.size(), 12);

		EXPECT_EQ(t_String.compare(10, L" test of the"), true) << "Append(BB::String&, size_t, size_t) failed second call";

		t_String.append(t_AppendString, t_String.size(), 26);

		EXPECT_EQ(t_String.compare(t_AppendString), true) << "Append(BB::String&, size_t, size_t) failed third call";
	}
#pragma endregion //append Test

	t_String.clear();

#pragma region insert Test
	//test break the string apart and use parts of it
	{
		constexpr const wchar_t* a_One = L"One";
		constexpr const wchar_t* a_Two = L"Two";
		constexpr const wchar_t* a_Three = L"Three";
		constexpr const wchar_t* a_Four = L"Four";

		t_String.insert(0, a_Four);
		EXPECT_EQ(t_String.compare(L"Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(0, a_Two);
		EXPECT_EQ(t_String.compare(L"TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(3, a_One);
		EXPECT_EQ(t_String.compare(L"TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(6, a_Three);
		EXPECT_EQ(t_String.compare(L"TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}

	t_String.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::WString t_AppendString(t_Allocator, L"One, Two, Three, Four");
		t_String.insert(0, t_AppendString, 17, 4);
		EXPECT_EQ(t_String.compare(L"Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(0, t_AppendString, 5, 3);
		EXPECT_EQ(t_String.compare(L"TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(3, t_AppendString, 0, 3);
		EXPECT_EQ(t_String.compare(L"TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		t_String.insert(6, t_AppendString, 10, 5);
		EXPECT_EQ(t_String.compare(L"TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}
#pragma endregion //insert Test

	//Copy
	BB::WString t_CopyString(t_String);

	EXPECT_EQ(t_String.compare(t_String), true) << "Copy constructor failed.";

	BB::WString t_CopyOperatorString(t_Allocator);
	t_CopyOperatorString = t_CopyString;

	EXPECT_EQ(t_String.compare(t_CopyString), true) << "Copy operator failed.";

	//Assignment
	BB::WString t_AssignmentString(std::move(t_CopyOperatorString));

	EXPECT_EQ(t_String.compare(t_String), true) << "Assignment constructor failed.";
	EXPECT_EQ(t_CopyOperatorString.data(), nullptr) << "Assignment constructor failed, the other string has still has data while it shouldn't have.";

	BB::WString t_AssignmentOperatorString(t_Allocator);
	t_AssignmentOperatorString = std::move(t_AssignmentString);

	EXPECT_EQ(t_String.compare(t_String), true) << "Assignment operator failed.";
	EXPECT_EQ(t_AssignmentString.data(), nullptr) << "Assignment operator failed, the other string has still has data while it shouldn't have.";
}

TEST(WideString_DataStructure, reserve_shrink)
{
	constexpr size_t StringReserve = 128;

	const size_t allocatorSize = BB::kbSize * 16;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::String t_String(t_Allocator);

	const char* t_AppendString = "First Unit test of the string is now being done.";

	t_String.reserve(128);
	const size_t t_StringCapacity = t_String.capacity();
	const size_t t_ModifiedC_StringSize = BB::Math::RoundUp(strlen(t_AppendString) + 1, BB::String_Specs::multipleValue);

	ASSERT_LT(t_ModifiedC_StringSize, t_StringCapacity) << "Modified string size is bigger then the string capacity, test is unaccurate.";

	t_String.append(t_AppendString);

	EXPECT_EQ(t_String.compare(t_AppendString), true) << "Append(const char*) failed";

	t_String.shrink_to_fit();

	EXPECT_EQ(t_String.capacity(), t_ModifiedC_StringSize) << "String did not shrink down to the correct size";
}

#pragma endregion //WString