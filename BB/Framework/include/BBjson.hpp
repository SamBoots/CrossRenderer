#pragma once
#include "Common.h"
#include "BBMemory.h"
#include "Hashmap.h"
#include "BBString.h"

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
		JSON_TYPE type = JSON_TYPE::NULL_TYPE; //4 bytes
		union //biggest type JsonList 12 bytes
		{
			struct JsonObject* object;
			JsonList list;
			char* string;
			float number;
			bool boolean;
		};

		inline struct JsonObject* GetObject() const
		{
			BB_ASSERT(type == JSON_TYPE::OBJECT, "json type not an object");
			return object;
		}
		inline JsonList GetList() const
		{
			BB_ASSERT(type == JSON_TYPE::LIST, "json type not an object");
			return list;
		}
		inline char* GetString() const
		{
			BB_ASSERT(type == JSON_TYPE::STRING, "json type not an object");
			return string;
		}
		inline float GetNumber() const
		{
			BB_ASSERT(type == JSON_TYPE::NUMBER, "json type not an object");
			return number;
		}
		inline bool GetBoolean() const
		{
			BB_ASSERT(type == JSON_TYPE::BOOL, "json type not an object");
			return boolean;
		}
	};

	struct JsonObject
	{
		struct Pair
		{
			char* name;
			JsonNode* node;
			Pair* next = nullptr;
		};

		JsonObject(Allocator a_Allocator, const uint32_t a_MapSize, Pair* a_PairHead)
			: map(a_Allocator, a_MapSize), pairLL(a_PairHead)
		{};
		OL_HashMap<char*, JsonNode*> map;
		Pair* pairLL;
	};

	struct JsonFile
	{
		Buffer fileData{};
		size_t pos = 0;
	};

	void JsonNodeToString(const JsonNode* t_Node, String& a_String);
	struct Token;
	class JsonParser
	{
	public:
		//load from disk
		JsonParser(const char* a_Path);
		//load from memory
		JsonParser(const Buffer& a_Buffer);
		~JsonParser();

		void Parse();

		JsonNode* GetRootNode() { return m_RootNode; }

		JsonNode* ParseObject();
		JsonNode* ParseList();
		JsonNode* ParseString(const Token& a_Token);
		JsonNode* ParseNumber(const Token& a_Token);
		JsonNode* ParseBoolean(const Token& a_Token);
		JsonNode* ParseNull();
	private:
		
		//jank
		JsonNode* PraseSingleToken(const Token& a_Token);
		LinearAllocator_t m_Allocator;
		JsonFile m_JsonFile;

		JsonNode* m_RootNode = nullptr;
	};
}

