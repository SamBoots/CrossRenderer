#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	void LoadglTFModel(Allocator a_TempAllocator, Allocator a_SystemAllocator, Model& a_Model, UploadBuffer& a_UploadBuffer, const RecordingCommandListHandle a_TransferCmdList, const char* a_Path);
}