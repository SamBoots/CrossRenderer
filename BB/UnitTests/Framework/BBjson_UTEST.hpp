#pragma once
#include "../TestValues.h"
#include "BBJson.hpp"
#include "BBMain.h"
#include "BBString.h"

TEST(BBjson, Small_Local_Memory_JSON)
{
	char t_JsonFile[] = R"(
{
  "fruit": "Apple",
  "size": "Large",
  "color": "Red"
}
	)";

	BB::FreelistAllocator_t t_Allocator{ BB::mbSize * 2 };
	BB::Buffer t_JsonBuffer{ t_JsonFile,  _countof(t_JsonFile) };
	BB::String t_JsonString{ t_Allocator, BB::mbSize * 2 };

	{
		BB::JsonParser t_Parser(t_JsonBuffer);
		t_Parser.Parse();
		BB::JsonNode* t_Node = t_Parser.GetRootNode();
		JsonNodeToString(t_Node, t_JsonString);
		BB_LOG(t_JsonString.c_str());
	}


	//Try to read the string as a json again.
	t_JsonBuffer.data = t_JsonString.data();
	t_JsonBuffer.size = t_JsonString.size();

	{
		BB::JsonParser t_Parser(t_JsonBuffer);
		t_Parser.Parse();
		BB::JsonNode* t_Node = t_Parser.GetRootNode();
		JsonNodeToString(t_Node, t_JsonString);
		BB_LOG(t_JsonString.c_str());
	}
	//call the destructor as I want to clear the allocator.
	t_JsonString.~Basic_String();
	t_Allocator.Clear();
}

TEST(BBjson, Big_Disk_JSON)
{
	const char* a_JsonPath = "Resources/Json/unittest_2.json";

	BB::FreelistAllocator_t t_Allocator{ BB::mbSize * 2 };
	BB::String t_JsonString{ t_Allocator, BB::mbSize * 2 };

	{
		BB::JsonParser t_Parser(a_JsonPath);
		t_Parser.Parse();
		BB::JsonNode* t_Node = t_Parser.GetRootNode();
		JsonNodeToString(t_Node, t_JsonString);
		BB_LOG(t_JsonString.c_str());
	}


	//Try to read the string as a json again.
	BB::Buffer t_JsonBuffer{ t_JsonString.data(), t_JsonString.size() };

	{
		BB::JsonParser t_Parser(t_JsonBuffer);
		t_Parser.Parse();
		BB::JsonNode* t_Node = t_Parser.GetRootNode();
		JsonNodeToString(t_Node, t_JsonString);
		BB_LOG(t_JsonString.c_str());
	}

	//call the destructor as I want to clear the allocator.
	t_JsonString.~Basic_String();
	t_Allocator.Clear();
}