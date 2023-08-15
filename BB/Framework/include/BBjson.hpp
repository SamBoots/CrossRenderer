#pragma once
#include "Common.h"
#include "BBMemory.h"
#include "Hashmap.h"

//tutorial/guide used: https://kishoreganesh.com/post/writing-a-json-parser-in-cplusplus/
namespace BB
{
	enum class JSON_TYPE : uint32_t
	{
		OBJECT,
		LIST,
		STRING,
		NUMBER,
		BOOL,
		NULL_TYPE
	};

	struct JsonList //12 bytes
	{
		uint32_t nodeCount;
		struct JsonNode** nodes;
	};

	struct JsonNode //16 bytes
	{
		JSON_TYPE type; //4 bytes
		union //biggest type JsonList 12 bytes
		{
			struct JsonObject* object;
			JsonList list;
			char* string;
			int number;
			bool boolean;
		};
	};

	struct JsonObject
	{
		JsonObject(Allocator a_Allocator, const uint32_t a_MapSize)
			: map(a_Allocator, a_MapSize) {};
		OL_HashMap<Hash, JsonNode*> map;
	};

	struct JsonFile
	{
		Buffer fileData{};
		uint32_t prevPos = 0;
		uint32_t pos = 0;
	};

	struct Token;

	class JsonParser
	{
	public:
		JsonParser(const char* a_Path);

		void Parse();

		JsonNode* ParseObject();
		JsonNode* ParseList();
		JsonNode* ParseString(const Token& a_Token);
		JsonNode* ParseNumber(const Token& a_Token);
		JsonNode* ParseBoolean(const Token& a_Token);
		JsonNode* ParseNull();

	private:
		LinearAllocator_t m_Allocator;
		JsonFile m_JsonFile;

		JsonNode* m_RootNode;
		JsonNode* m_CurrentNode;
	};
}

