#include "BBjson.hpp"
#include "OS/Program.h"

#include "Utils/Utils.h"

using namespace BB;

enum class TOKEN_TYPE : uint32_t
{
	OUT_OF_TOKENS,
	CURLY_OPEN,
	CURLY_CLOSE,
	COLON,
	STRING,
	NUMBER,
	ARRAY_OPEN,
	ARRAY_CLOSE,
	COMMA,
	BOOLEAN,
	NULL_TYPE
};

//16 bytes
struct BB::Token
{
	TOKEN_TYPE type = TOKEN_TYPE::OUT_OF_TOKENS;
	uint32_t strSize = 0;
	char* str = nullptr;
};

static char ignoreWhiteSpace(JsonFile& a_JsonFile)
{
	char t_C = ' ';
	while ((t_C == ' ' || t_C == '\n'))
	{
		BB_ASSERT(a_JsonFile.pos <= a_JsonFile.fileData.size, "out of tokens");
		t_C = a_JsonFile.fileData.data[a_JsonFile.pos++];
	}

	return t_C;
}

Token GetToken(JsonFile& a_JsonFile)
{
	Token t_Token;

	if (a_JsonFile.pos <= a_JsonFile.fileData.size) //If we are at the end of the file, we will produce no more tokens.
	{
		t_Token.type = TOKEN_TYPE::OUT_OF_TOKENS;
		return t_Token;
	}

	a_JsonFile.prevPos = a_JsonFile.pos;
	char t_C = ignoreWhiteSpace(a_JsonFile);

	if (t_C == '"') //is string
	{
		//get string length
		size_t t_StrLen = 0;
		while (a_JsonFile.fileData.data[a_JsonFile.pos + t_StrLen] != '"')
			++t_StrLen;

		t_Token.type = TOKEN_TYPE::STRING;
		t_Token.strSize = t_StrLen;
		t_Token.str = &a_JsonFile.fileData.data[a_JsonFile.pos];
	}
	else if (t_C == '-' || (t_C >= '0' && t_C <= '9')) //is number
	{
		//get string length which are numbers
		size_t t_StrLen = 1;
		char t_Num = a_JsonFile.fileData.data[a_JsonFile.pos + t_StrLen++];
		while (t_Num == '-' || (t_Num >= '0' && t_Num <= '9') || t_Num == '.')
			t_Num = a_JsonFile.fileData.data[a_JsonFile.pos + t_StrLen++];

		t_Token.type = TOKEN_TYPE::NUMBER;
		t_Token.strSize = t_StrLen;
		t_Token.str = &a_JsonFile.fileData.data[a_JsonFile.pos - 1];
	}
	else if (t_C == 'f') {
		t_Token.type = TOKEN_TYPE::BOOLEAN;
		t_Token.strSize = 5;
		t_Token.str = &a_JsonFile.fileData.data[a_JsonFile.pos - 1];
		//Do a janky check to see if False was actually correctly written.
		BB_WARNING(Memory::Compare("false", &a_JsonFile.fileData.data[a_JsonFile.pos - 1], 5) == 0,
			"JSON file tried to read a boolean that was set to True but it's not written as True!",
			WarningType::MEDIUM);
		a_JsonFile.pos += 4;
	}
	else if (t_C == 't') {
		t_Token.type = TOKEN_TYPE::BOOLEAN;
		t_Token.strSize = 4;
		t_Token.str = &a_JsonFile.fileData.data[a_JsonFile.pos - 1];
		//Do a janky check to see if True was actually correctly written.
		BB_WARNING(Memory::Compare("true", &a_JsonFile.fileData.data[a_JsonFile.pos - 1], 4) == 0,
			"JSON file tried to read a boolean that was set to True but it's not written as True!",
			WarningType::MEDIUM);
		a_JsonFile.pos += 3;
	}
	else if (t_C == 'n') {
		t_Token.type = TOKEN_TYPE::NULL_TYPE;
		BB_WARNING(Memory::Compare("null", &a_JsonFile.fileData.data[a_JsonFile.pos - 1], 4) == 0,
			"JSON file tried to read a boolean that was set to True but it's not written as True!",
			WarningType::MEDIUM);
		a_JsonFile.pos += 3;
	}
	else if (t_C == '[')
	{
		t_Token.type = TOKEN_TYPE::ARRAY_OPEN;
	}
	else if (t_C == ']')
	{
		t_Token.type = TOKEN_TYPE::ARRAY_CLOSE;
	}
	else if (t_C == ':')
	{
		t_Token.type = TOKEN_TYPE::COLON;
	}
	else if (t_C == ',')
	{
		t_Token.type = TOKEN_TYPE::COMMA;
	}

	return t_Token;
}

JsonParser::JsonParser(const char* a_Path)
	: m_Allocator(mbSize * 8, a_Path)
{
	m_JsonFile.fileData = ReadOSFile(m_Allocator, a_Path);
}

void JsonParser::Parse()
{
	Token t_Token = GetToken(m_JsonFile);
	while (t_Token.type != TOKEN_TYPE::OUT_OF_TOKENS)
	{
		JsonNode* t_Node;
		switch (t_Token.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
			t_Node = ParseObject();

			if (!m_RootNode)
				m_RootNode = t_Node;
			break;
		case TOKEN_TYPE::ARRAY_OPEN:
			t_Node = ParseList();

			if (!m_RootNode)
				m_RootNode = t_Node;
			break;
		case TOKEN_TYPE::STRING:
			t_Node = ParseString(t_Token);

			if (!m_RootNode)
				m_RootNode = t_Node;
			break;
		case TOKEN_TYPE::NUMBER:
			t_Node = ParseNumber(t_Token);

			if (!m_RootNode)
				m_RootNode = t_Node;
			break;
		case TOKEN_TYPE::BOOLEAN:
			t_Node = ParseBoolean(t_Token);
			break;
		default:
			BB_WARNING(false, "Unknown JSON token found.", WarningType::HIGH);
			break;
		}

		t_Token = GetToken(m_JsonFile);
	}
}

JsonNode* JsonParser::ParseObject()
{
	JsonNode* t_Node = BBnew(m_Allocator, JsonNode);
	t_Node->type = JSON_TYPE::OBJECT;

	uint32_t t_ContentSize = 0;
	const uint32_t t_ListStartPos = m_JsonFile.pos;
	Token t_NextToken = GetToken(m_JsonFile);
	while (t_NextToken.type != TOKEN_TYPE::ARRAY_CLOSE) //get how many tokens we need to allocate
	{
		switch (t_NextToken.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
		case TOKEN_TYPE::ARRAY_OPEN:
		case TOKEN_TYPE::STRING:
		case TOKEN_TYPE::NUMBER:
		case TOKEN_TYPE::BOOLEAN:
		case TOKEN_TYPE::NULL_TYPE:
			++t_ContentSize;
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		}

		t_NextToken = GetToken(m_JsonFile);
	}
	//reset position back to the start of the list and iterate again
	m_JsonFile.pos = t_ListStartPos;

	t_Node->object = BBnew(m_Allocator, JsonObject)(m_Allocator, t_ContentSize);

	t_NextToken = GetToken(m_JsonFile);
	while (t_NextToken.type == TOKEN_TYPE::CURLY_CLOSE)
	{
		BB_WARNING(t_NextToken.type == TOKEN_TYPE::STRING, "Object does not start with a string!", WarningType::HIGH);
		Hash t_StrHash;
		t_StrHash.MakeHash(t_NextToken.str);
		t_NextToken = GetToken(m_JsonFile);
		BB_WARNING(t_NextToken.type == TOKEN_TYPE::STRING, "token after string is not a :", WarningType::HIGH);
		t_NextToken = GetToken(m_JsonFile); //the value

		switch (t_NextToken.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
			t_Node->object->map.emplace(t_StrHash, ParseObject());
			break;
		case TOKEN_TYPE::ARRAY_OPEN:
			t_Node->object->map.emplace(t_StrHash, ParseList());
			break;
		case TOKEN_TYPE::STRING:
			t_Node->object->map.emplace(t_StrHash, ParseString(t_NextToken));
			break;
		case TOKEN_TYPE::NUMBER:
			t_Node->object->map.emplace(t_StrHash, ParseNumber(t_NextToken));
			break;
		case TOKEN_TYPE::BOOLEAN:
			t_Node->object->map.emplace(t_StrHash, ParseBoolean(t_NextToken));
			break;
		case TOKEN_TYPE::NULL_TYPE:
			t_Node->object->map.emplace(t_StrHash, ParseNull());
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		}

		//maybe check here for a , OR a }. For now idc lmao lmao

		t_NextToken = GetToken(m_JsonFile);
	}
	return t_Node;
}

JsonNode* JsonParser::ParseList()
{
	JsonNode* t_Node = BBnew(m_Allocator, JsonNode);
	t_Node->type = JSON_TYPE::LIST;

	uint32_t t_ListSize = 0;
	const uint32_t t_ListStartPos = m_JsonFile.pos;
	Token t_NextToken = GetToken(m_JsonFile);
	while (t_NextToken.type != TOKEN_TYPE::ARRAY_CLOSE) //get how many tokens we need to allocate
	{
		switch (t_NextToken.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
		case TOKEN_TYPE::ARRAY_OPEN:
		case TOKEN_TYPE::STRING:
		case TOKEN_TYPE::NUMBER:
		case TOKEN_TYPE::BOOLEAN:
		case TOKEN_TYPE::NULL_TYPE:
			++t_ListSize;
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		}

		t_NextToken = GetToken(m_JsonFile);
	}
	//reset position back to the start of the list and iterate again
	m_JsonFile.pos = t_ListStartPos;

	t_Node->list.nodeCount = t_ListSize;
	t_Node->list.nodes = BBnewArr(m_Allocator, t_Node->list.nodeCount, JsonNode*);

	t_NextToken = GetToken(m_JsonFile);

	uint32_t t_NodeIndex = 0;
	while(t_NextToken.type != TOKEN_TYPE::ARRAY_CLOSE)
	{
		switch (t_NextToken.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
			t_Node->list.nodes[t_NodeIndex++] = ParseObject();
			break;
		case TOKEN_TYPE::ARRAY_OPEN:
			t_Node->list.nodes[t_NodeIndex++] = ParseList();
			break;
		case TOKEN_TYPE::STRING:
			t_Node->list.nodes[t_NodeIndex++] = ParseString(t_NextToken);
			break;
		case TOKEN_TYPE::NUMBER:
			t_Node->list.nodes[t_NodeIndex++] = ParseNumber(t_NextToken);
			break;
		case TOKEN_TYPE::BOOLEAN:
			t_Node->list.nodes[t_NodeIndex++] = ParseBoolean(t_NextToken);
			break;
		case TOKEN_TYPE::NULL_TYPE:
			t_Node->list.nodes[t_NodeIndex++] = ParseNull();
			break;
		case TOKEN_TYPE::CURLY_CLOSE:
			BB_ASSERT(false, "We should not reach this, algorithm mistake happened.");
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		}

		t_NextToken = GetToken(m_JsonFile);
	}
	return t_Node;
}

JsonNode* JsonParser::ParseString(const Token& a_Token)
{
	JsonNode* t_Node = BBnew(m_Allocator, JsonNode);
	t_Node->type = JSON_TYPE::STRING;

	const Token t_Token = GetToken(m_JsonFile);
	t_Node->string = BBnewArr(m_Allocator, t_Token.strSize + 1, char);
	Memory::Copy(t_Node->string, t_Token.str, t_Token.strSize);
	t_Node->string[t_Token.strSize] = '\0';

	return t_Node;
}

JsonNode* JsonParser::ParseNumber(const Token& a_Token)
{
	JsonNode* t_Node = BBnew(m_Allocator, JsonNode);
	t_Node->type = JSON_TYPE::NUMBER;

	const Token t_Token = GetToken(m_JsonFile);
	t_Node->string = BBnewArr(m_Allocator, t_Token.strSize + 1, char);
	Memory::Copy(t_Node->string, t_Token.str, t_Token.strSize);
	t_Node->string[t_Token.strSize] = '\0';

	return t_Node;
}

JsonNode* JsonParser::ParseBoolean(const Token& a_Token)
{
	JsonNode* t_Node = BBnew(m_Allocator, JsonNode);
	t_Node->type = JSON_TYPE::BOOL;

	const Token t_Token = GetToken(m_JsonFile);
	if (strcmp(t_Token.str, "False") == 0)
		t_Node->boolean = false;
	else if (strcmp(t_Token.str, "True") == 0)
		t_Node->boolean = true;
	else
		BB_ASSERT(false, "JSON bool is wrongly typed, this assert should never be hit even with a wrong json anyway.");

	return t_Node;
}

JsonNode* JsonParser::ParseNull()
{
	JsonNode* t_Node = BBnew(m_Allocator, JsonNode);
	t_Node->type = JSON_TYPE::NULL_TYPE;

	return t_Node;
}