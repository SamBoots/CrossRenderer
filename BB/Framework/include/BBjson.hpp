#pragma once
#include "Common.h"
#include "BBMemory.h"

//tutorial/guide used: https://kishoreganesh.com/post/writing-a-json-parser-in-cplusplus/
namespace BB
{
	enum class JSON_TYPE
	{
		OBJECT,
		LIST,
		STRING,
		NUMBER,
		BOOL,
		NULL_TYPE
	};

	struct JsonNode;

	struct JsonObject
	{
		
	};

	struct JsonList
	{
		uint32_t nodeCount;
		JsonNode** nodes;
	};

	//20 bytes. Jank
	struct JsonNode
	{
		JSON_TYPE type;
		union
		{
			JsonObject* object;
			JsonList list;
			const char* string;
			int number;
			bool boolean;
		};
	};

	struct JsonFile
	{
		Buffer fileData{};
		uint32_t prevPos = 0;
		uint32_t pos = 0;
	};

	class JsonParser
	{
	public:
		JsonParser(const char* a_Path);

		void Parse();

		JsonNode* ParseObject();
		JsonNode* ParseString();
		JsonNode* ParseNumber();
		JsonNode* ParseList();
		JsonNode* ParseBoolean();
		JsonNode* ParseNull();

	private:
		LinearAllocator_t m_Allocator;
		JsonFile m_JsonFile;

		JsonNode* m_RootNode;
		JsonNode* m_CurrentNode;
	};
}

