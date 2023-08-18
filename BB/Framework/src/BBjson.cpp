#include "BBjson.hpp"
#include "OS/Program.h"

#include "Utils/Utils.h"
#include <string>

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
	char t_C = a_JsonFile.fileData.data[a_JsonFile.pos++];
	while ((t_C == ' ' || t_C == '\n' || t_C == '\r'))
	{
		BB_ASSERT(a_JsonFile.pos <= a_JsonFile.fileData.size, "out of tokens");
		t_C = a_JsonFile.fileData.data[a_JsonFile.pos++];
	}

	return t_C;
}

Token GetToken(JsonFile& a_JsonFile)
{
	Token t_Token;

	if (a_JsonFile.pos > a_JsonFile.fileData.size) //If we are at the end of the file, we will produce no more tokens.
	{
		t_Token.type = TOKEN_TYPE::OUT_OF_TOKENS;
		return t_Token;
	}

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

		a_JsonFile.pos += t_StrLen + 1; //includes the last "
	}
	else if (t_C == '-' || (t_C >= '0' && t_C <= '9')) //is number
	{
		//get string length which are numbers
		size_t t_StrLen = 1;
		char t_Num = a_JsonFile.fileData.data[a_JsonFile.pos + t_StrLen];
		while (t_Num == '-' || (t_Num >= '0' && t_Num <= '9') || t_Num == '.')
			t_Num = a_JsonFile.fileData.data[a_JsonFile.pos + t_StrLen++];

		t_Token.type = TOKEN_TYPE::NUMBER;
		t_Token.strSize = t_StrLen;
		t_Token.str = &a_JsonFile.fileData.data[a_JsonFile.pos - 1];

		a_JsonFile.pos += t_StrLen - 1; //includes the last
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
	else if (t_C == '{')
	{
		t_Token.type = TOKEN_TYPE::CURLY_OPEN;
	}
	else if (t_C == '}')
	{
		t_Token.type = TOKEN_TYPE::CURLY_CLOSE;
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

void BB::JsonNodeToString(const JsonNode* a_Node, String& a_String)
{
	switch (a_Node->type)
	{
	case BB::JSON_TYPE::OBJECT:
	{
		a_String.append("{");
		a_String.append("\n");

		const JsonObject::Pair* t_Pair = a_Node->object->pairLL;
		while (t_Pair != nullptr)
		{
			a_String.append("\"");
			a_String.append(t_Pair->name);
			a_String.append("\"");
			a_String.append(" : ");
			JsonNodeToString(t_Pair->node, a_String);

			if (t_Pair->next != nullptr)
				a_String.append(",\n");
			else
				a_String.append("\n");
			t_Pair = t_Pair->next;
		}

		a_String.append("}\n");
	}
		break;
	case BB::JSON_TYPE::LIST:
		a_String.append("[\n");

		for (size_t i = 0; i < a_Node->list.nodeCount; i++)
		{
			JsonNodeToString(a_Node->list.nodes[i], a_String);
			a_String.append(",\n");
		}

		a_String.append("]\n");
		break;
	case BB::JSON_TYPE::STRING:
		a_String.append("\"");
		a_String.append(a_Node->string);
		a_String.append("\"");
		break;
	case BB::JSON_TYPE::NUMBER:
		BB_WARNING(false, "Not supporting number to string yet.", WarningType::LOW);
		a_String.append("\"");
		//t_JsonString.append(t_Node->number);
		a_String.append("\"");
		break;
	case BB::JSON_TYPE::BOOL:
		if (a_Node->boolean)
			a_String.append("true");
		else
			a_String.append("false");
		break;
	case BB::JSON_TYPE::NULL_TYPE:
		a_String.append("null");
		break;
	default:
		break;
	}
}

JsonParser::JsonParser(const char* a_Path)
	: m_Allocator(mbSize * 8, a_Path)
{
	m_JsonFile.fileData = ReadOSFile(m_Allocator, a_Path);
}

JsonParser::JsonParser(const Buffer& a_Buffer)
	: m_Allocator(mbSize * 8, "Json from memory read")
{
	m_JsonFile.fileData = a_Buffer;
}

JsonParser::~JsonParser()
{
	m_Allocator.Clear();
}

JsonNode* JsonParser::PraseSingleToken(const Token& a_Token)
{
	JsonNode* t_Node;
	switch (a_Token.type)
	{
	case TOKEN_TYPE::CURLY_OPEN:
		t_Node = ParseObject();
		break;
	case TOKEN_TYPE::ARRAY_OPEN:
		t_Node = ParseList();
		break;
	case TOKEN_TYPE::STRING:
		t_Node = ParseString(a_Token);
		break;
	case TOKEN_TYPE::NUMBER:
		t_Node = ParseNumber(a_Token);
		break;
	case TOKEN_TYPE::BOOLEAN:
		t_Node = ParseBoolean(a_Token);
		break;
	default:
		BB_WARNING(false, "unknown json token found.", WarningType::HIGH);
		break;
	}
	return t_Node;
}

void JsonParser::Parse()
{
	Token t_Token = GetToken(m_JsonFile);
	m_RootNode = PraseSingleToken(t_Token);

	//ignore?
	/*t_Token = GetToken(m_JsonFile);
	JsonNode* t_NextRoot = m_RootNode;
	while (t_Token.type != TOKEN_TYPE::OUT_OF_TOKENS)
	{
		t_NextRoot->next = PraseSingleToken(t_Token);
		t_Token = GetToken(m_JsonFile);
		t_NextRoot = m_RootNode->next;
	}
	t_NextRoot->next = nullptr;*/
}

JsonNode* JsonParser::ParseObject()
{
	JsonNode* t_ObjectNode = BBnew(m_Allocator, JsonNode);
	t_ObjectNode->type = JSON_TYPE::OBJECT;

	JsonObject::Pair* t_PairHead = BBnew(m_Allocator, JsonObject::Pair);
	uint32_t t_PairCount = 0;

	Token t_NextToken = GetToken(m_JsonFile);
	uint32_t t_ArrayIndex = 0;

	JsonObject::Pair* t_Pair = t_PairHead;
	bool t_ContinueLoop = true;
	while (t_ContinueLoop)
	{
		BB_WARNING(t_NextToken.type == TOKEN_TYPE::STRING, "Object does not start with a string!", WarningType::HIGH);
		char* t_ElementName = BBnewArr(m_Allocator, t_NextToken.strSize + 1, char);
		Memory::Copy(t_ElementName, t_NextToken.str, t_NextToken.strSize);
		t_ElementName[t_NextToken.strSize] = '\0';

		t_NextToken = GetToken(m_JsonFile);
		BB_WARNING(t_NextToken.type == TOKEN_TYPE::COLON, "token after string is not a :", WarningType::HIGH);
		t_NextToken = GetToken(m_JsonFile); //the value

		t_Pair->name = t_ElementName;
		switch (t_NextToken.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
			t_Pair->node = ParseObject();
			break;
		case TOKEN_TYPE::ARRAY_OPEN:
			t_Pair->node = ParseList();
			break;
		case TOKEN_TYPE::STRING:
			t_Pair->node = ParseString(t_NextToken);
			break;
		case TOKEN_TYPE::NUMBER:
			t_Pair->node = ParseNumber(t_NextToken);
			break;
		case TOKEN_TYPE::BOOLEAN:
			t_Pair->node = ParseBoolean(t_NextToken);
			break;
		case TOKEN_TYPE::NULL_TYPE:
			t_Pair->node = ParseNull();
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		}

		t_NextToken = GetToken(m_JsonFile);

		++t_PairCount;
		if (t_NextToken.type == TOKEN_TYPE::CURLY_CLOSE)
			t_ContinueLoop = false;
		else
		{
			BB_ASSERT(t_NextToken.type == TOKEN_TYPE::COMMA, "the token after a object pair should be a } or ,");
			t_NextToken = GetToken(m_JsonFile);
			t_Pair->next = BBnew(m_Allocator, JsonObject::Pair);
			t_Pair = t_Pair->next;
		}
	}
	t_ObjectNode->object = BBnew(m_Allocator, JsonObject)(m_Allocator, t_PairCount, t_PairHead);
	t_Pair = t_PairHead;
	for (size_t i = 0; i < t_PairCount; i++)
	{
		t_ObjectNode->object->map.insert(t_Pair->name, t_Pair->node);
		t_Pair = t_Pair->next;
	}

	return t_ObjectNode;
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
		{
			uint32_t t_ObjectStack = 1;
			bool t_LocalLoop = true;
			t_NextToken = GetToken(m_JsonFile);
			while (t_LocalLoop)
			{
				if (t_NextToken.type == TOKEN_TYPE::CURLY_OPEN)
				{
					++t_ObjectStack;
				}
				else if (t_NextToken.type == TOKEN_TYPE::CURLY_CLOSE)
				{
					if (--t_ObjectStack == 0)
						t_LocalLoop = false;
				}
				else
					t_NextToken = GetToken(m_JsonFile);
			}
			++t_ListSize;
		}
		break;
		case TOKEN_TYPE::ARRAY_OPEN:
		{
			uint32_t t_ArrayStack = 1;
			t_NextToken = GetToken(m_JsonFile);
			bool t_LocalLoop = true;
			while (t_LocalLoop)
			{
				if (t_NextToken.type == TOKEN_TYPE::ARRAY_OPEN)
				{
					++t_ArrayStack;
				}
				else if (t_NextToken.type == TOKEN_TYPE::ARRAY_CLOSE)
				{
					if (--t_ArrayStack == 0)
						t_LocalLoop = false;
				}
				else
					t_NextToken = GetToken(m_JsonFile);
			}
			++t_ListSize;
		}
		break;
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

	t_Node->string = BBnewArr(m_Allocator, a_Token.strSize + 1, char);
	Memory::Copy(t_Node->string, a_Token.str, a_Token.strSize);
	t_Node->string[a_Token.strSize] = '\0';

	return t_Node;
}

JsonNode* JsonParser::ParseNumber(const Token& a_Token)
{
	JsonNode* t_Node = BBnew(m_Allocator, JsonNode);
	t_Node->type = JSON_TYPE::NUMBER;

	t_Node->number = std::stof(a_Token.str);

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