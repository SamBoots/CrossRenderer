#include "Editor.h"
#include "Frontend/RenderFrontend.h"

#include "Transform.h"
#include "LightSystem.h"
#include "RenderResourceTracker.h"

#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
using namespace BB;

static inline const char* DescriptorTypeStr(const RENDER_DESCRIPTOR_TYPE a_Type)
{
	switch (a_Type)
	{
	case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:			return "Descriptor Type: READONLY_CONSTANT";
	case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:			return "Descriptor Type: READONLY_BUFFER";
	case RENDER_DESCRIPTOR_TYPE::READWRITE:					return "Descriptor Type: READWRITE";
	case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT_DYNAMIC:	return "Descriptor Type: READONLY_CONSTANT_DYNAMIC";
	case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC:	return "Descriptor Type: READONLY_BUFFER_DYNAMIC";
	case RENDER_DESCRIPTOR_TYPE::READWRITE_DYNAMIC:			return "Descriptor Type: READWRITE_DYNAMIC";
	case RENDER_DESCRIPTOR_TYPE::IMAGE:						return "Descriptor Type: IMAGE";
	case RENDER_DESCRIPTOR_TYPE::SAMPLER:					return "Descriptor Type: SAMPLER";
	default:
		break;
	}
}

static inline const char* ShaderStageStr(const RENDER_SHADER_STAGE a_Stage)
{
	switch (a_Stage)
	{
	case RENDER_SHADER_STAGE::ALL:				return "Shader Stage: ALL";
	case RENDER_SHADER_STAGE::VERTEX:			return "Shader Stage: VERTEX";
	case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:	return "Shader Stage: FRAGMENT_PIXEL";
	default:
		BB_ASSERT(false, "RENDER_SHADER_STAGE unknown in resource tracker!");
		break;
	}
}

static inline const char* DescriptorFlagStr(const RENDER_DESCRIPTOR_FLAG a_Flag)
{
	switch (a_Flag)
	{
	case RENDER_DESCRIPTOR_FLAG::NONE:		return "Flags: NONE";
	case RENDER_DESCRIPTOR_FLAG::BINDLESS:	return "Flags: BINDLESS";
	default:
		BB_ASSERT(false, "RENDER_DESCRIPTOR_FLAG unknown in resource tracker!");
		break;
	}
}

static inline const char* InputFormat(const RENDER_INPUT_FORMAT a_Format)
{
	switch (a_Format)
	{
	case RENDER_INPUT_FORMAT::RGBA32:	return "INPUT FORMAT: RGBA32";
	case RENDER_INPUT_FORMAT::RGB32:	return "INPUT FORMAT: RGB32";
	case RENDER_INPUT_FORMAT::RG32:		return "INPUT FORMAT: RG32";
	case RENDER_INPUT_FORMAT::R32:		return "INPUT FORMAT: R32";
	case RENDER_INPUT_FORMAT::RGBA8:	return "INPUT FORMAT: RGBA8";
	case RENDER_INPUT_FORMAT::RG8:		return "INPUT FORMAT: RG8";

	default:
		BB_ASSERT(false, "RENDER_INPUT_FORMAT unknown in resource tracker!");
		break;
	}
}

static inline const char* BlendFactor(const RENDER_BLEND_FACTOR a_BlendFac)
{
	switch (a_BlendFac)
	{
	case RENDER_BLEND_FACTOR::ZERO:					return "ZERO";
	case RENDER_BLEND_FACTOR::ONE:					return "ONE";
	case RENDER_BLEND_FACTOR::SRC_ALPHA:			return "SRC_ALPHA";
	case RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA:	return "ONE_MINUS_SRC_ALPHA";

	default:
		BB_ASSERT(false, "RENDER_BLEND_FACTOR unknown in resource tracker!");
		break;
	}
}

static inline const char* BlendOp(const RENDER_BLEND_OP a_BlendOp)
{
	switch (a_BlendOp)
	{
	case RENDER_BLEND_OP::ADD:		return "Blend Op: ADD";
	case RENDER_BLEND_OP::SUBTRACT:	return "Blend Op: SUBTRACT";

	default:
		BB_ASSERT(false, "RENDER_BLEND_OP unknown in resource tracker!");
		break;
	}
}

void BB::Editor::DisplayDrawObjects(const BB::Slice<DrawObject> a_DrawObjects, const TransformPool& a_Pool)
{
	if (ImGui::Begin("Draw Objects"))
	{
		for (size_t i = 0; i < a_DrawObjects.size(); i++)
		{
			Transform& t_Trans = a_Pool.GetTransform(a_DrawObjects[i].transformHandle);

			ImGui::PushID(i);
			if (ImGui::CollapsingHeader("DrawObject"))
			{
				ImGui::SliderFloat3("Position", glm::value_ptr(t_Trans.m_Pos), -100, 100);
				ImGui::SliderFloat4("Rotation quat", glm::value_ptr(t_Trans.m_Rot), -100, 100);
				ImGui::SliderFloat3("Scale", glm::value_ptr(t_Trans.m_Scale), -100, 100);
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
}

void BB::Editor::DisplayRenderResources(BB::RenderResourceTracker& a_ResTracker)
{
	if (ImGui::Begin("Render resources"))
	{
		if (ImGui::BeginMenu("Options"))
		{
			if (ImGui::Button("Sort by time"))
			{
				a_ResTracker.SortByTime();
			}
			if (ImGui::Button("Sort by type"))
			{
				a_ResTracker.SortByType();
			}
			ImGui::EndMenu();
		}
		RenderResourceTracker::Entry* t_Entry = a_ResTracker.m_HeadEntry;
		uint32_t t_EntryCount = 0;
		while (t_Entry != nullptr)
		{
			ImGui::PushID(t_EntryCount++);
			BB_ASSERT(t_EntryCount <= a_ResTracker.m_Entries, "Render Resource tracker has too many entries while they are not marked!");
			const char* t_ResName = "UNNAMED";
			if (t_Entry->name != nullptr)
				t_ResName = t_Entry->name;

			if (ImGui::CollapsingHeader(t_ResName))
			{
				switch (t_Entry->type)
				{
				case RESOURCE_TYPE::DESCRIPTOR:
				{
					const RenderDescriptorCreateInfo& t_Desc =
						*reinterpret_cast<RenderDescriptorCreateInfo*>(t_Entry->typeInfo);
					switch (t_Desc.bindingSet)
					{
					case RENDER_BINDING_SET::PER_FRAME:
						ImGui::Text("Binding set: PER_FRAME");
						break;
					case RENDER_BINDING_SET::PER_PASS:
						ImGui::Text("Binding set: PER_PASS");
						break;
					case RENDER_BINDING_SET::PER_MATERIAL:
						ImGui::Text("Binding set: PER_MATERIAL");
						break;
					case RENDER_BINDING_SET::PER_OBJECT:
						ImGui::Text("Binding set: PER_OBJECT");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_BINDING_SET for resource trackering editor");
						break;
					}

					for (size_t i = 0; i < t_Desc.bindings.size(); i++)
					{
						if (ImGui::TreeNode("PLACE_HOLDER_DESC_NAME_WILL_CRASH"))
						{
							const DescriptorBinding& t_Bind = t_Desc.bindings[i];
							ImGui::Text("Binding: %u", t_Bind.binding);
							ImGui::Text("DescriptorCount: %u", t_Bind.descriptorCount);
							ImGui::Text(DescriptorTypeStr(t_Bind.type));
							ImGui::Text(ShaderStageStr(t_Bind.stage));
							ImGui::Text(DescriptorFlagStr(t_Bind.flags));
							ImGui::TreePop();
						}
					}
				}
				break;
				case RESOURCE_TYPE::COMMAND_QUEUE:
				{
					const RenderCommandQueueCreateInfo& t_Queue =
						*reinterpret_cast<RenderCommandQueueCreateInfo*>(t_Entry->typeInfo);
					switch (t_Queue.queue)
					{
					case RENDER_QUEUE_TYPE::GRAPHICS:
						ImGui::Text("Queue type: GRAPHICS");
						break;
					case RENDER_QUEUE_TYPE::TRANSFER_COPY:
						ImGui::Text("Queue type: TRANSFER_COPY");
						break;
					case RENDER_QUEUE_TYPE::COMPUTE:
						ImGui::Text("Queue type: COMPUTE");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_QUEUE_TYPE for resource trackering editor");
						break;
					}
					break;
				}
				case RESOURCE_TYPE::COMMAND_ALLOCATOR:
				{
					const RenderCommandAllocatorCreateInfo& t_Alloc =
						*reinterpret_cast<RenderCommandAllocatorCreateInfo*>(t_Entry->typeInfo);
					ImGui::Text("Commandlist count: %u", t_Alloc.commandListCount);
					switch (t_Alloc.queueType)
					{
					case RENDER_QUEUE_TYPE::GRAPHICS:
						ImGui::Text("Queue type: GRAPHICS");
						break;
					case RENDER_QUEUE_TYPE::TRANSFER_COPY:
						ImGui::Text("Queue type: TRANSFER_COPY");
						break;
					case RENDER_QUEUE_TYPE::COMPUTE:
						ImGui::Text("Queue type: COMPUTE");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_QUEUE_TYPE for resource trackering editor");
						break;
					}
				}
				break;
				case RESOURCE_TYPE::COMMAND_LIST:
				{
					const RenderCommandListCreateInfo& t_List =
						*reinterpret_cast<RenderCommandListCreateInfo*>(t_Entry->typeInfo);
				}
				break;
				case RESOURCE_TYPE::PIPELINE:
				{
					const PipelineDebugInfo& t_Pipeline =
						*reinterpret_cast<PipelineDebugInfo*>(t_Entry->typeInfo);

					ImGui::Text("Depthtest enabled: %d", t_Pipeline.enableDepthTest);

					if (t_Pipeline.constantData.dwordSize)
						if (ImGui::TreeNode("Constant data"))
						{
							ImGui::Text("dwordsize: %u", t_Pipeline.constantData.dwordSize);
							ImGui::Text(ShaderStageStr(t_Pipeline.constantData.shaderStage));
							ImGui::TreePop();
						}

					if (ImGui::TreeNode("Rasterizer State"))
					{
						ImGui::Text("Counterclockwise: %d", t_Pipeline.rasterState.frontCounterClockwise);
						switch (t_Pipeline.rasterState.cullMode)
						{
						case RENDER_CULL_MODE::NONE:
							ImGui::Text("Cullmode: NONE");
							break;
						case RENDER_CULL_MODE::FRONT:
							ImGui::Text("Cullmode: FRONT");
							break;
						case RENDER_CULL_MODE::BACK:
							ImGui::Text("Cullmode: BACK");
							break;
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Blend states"))
					{
						for (size_t i = 0; i < t_Pipeline.renderTargetBlendCount; i++)
						{
							const PipelineRenderTargetBlend& t_BlendInfo = t_Pipeline.renderTargetBlends[i];
							if (ImGui::TreeNode((void*)(intptr_t)i, "Blend State %u:", i))
							{
								ImGui::Text("Blend Enable: %d", t_BlendInfo.blendEnable);
								ImGui::Text("Source Blend", BlendFactor(t_BlendInfo.srcBlend));
								ImGui::Text("Destination Blend", BlendFactor(t_BlendInfo.dstBlend));
								ImGui::Text("Blend Op", BlendOp(t_BlendInfo.blendOp));
								ImGui::Text("Source Blend Alpha", BlendFactor(t_BlendInfo.srcBlendAlpha));
								ImGui::Text("Destination Blend Alpha", BlendFactor(t_BlendInfo.dstBlendAlpha));
								ImGui::Text("Blend Op Alpha", BlendOp(t_BlendInfo.blendOpAlpha));
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Shaders"))
					{
						for (size_t i = 0; i < t_Pipeline.shaderCount; i++)
						{
							const PipelineDebugInfo::ShaderInfo& t_ShaderInfo = t_Pipeline.shaderInfo[i];
							if (ImGui::TreeNode((void*)(intptr_t)i, "Shader %u:", i))
							{
								if (t_ShaderInfo.optionalShaderpath)
									ImGui::Text(t_ShaderInfo.optionalShaderpath);
								ImGui::Text(ShaderStageStr(t_ShaderInfo.shaderStage));
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Vertex attributes"))
					{
						for (size_t i = 0; i < t_Pipeline.attributeCount; i++)
						{
							const VertexAttributeDesc& t_Attri = t_Pipeline.attributes[i];
							if (ImGui::TreeNode((void*)(intptr_t)i, "Vertex Attribute %u:", i))
							{
								ImGui::Text("binding: %u", t_Attri.location);
								ImGui::Text(InputFormat(t_Attri.format));
								ImGui::Text("offset: %u", t_Attri.offset);
								ImGui::Text(t_Attri.semanticName);
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				case RESOURCE_TYPE::BUFFER:
				{
					const RenderBufferCreateInfo& t_Buffer =
						*reinterpret_cast<RenderBufferCreateInfo*>(t_Entry->typeInfo);
					ImGui::Text("Size: %u", t_Buffer.size);
					switch (t_Buffer.usage)
					{
					case RENDER_BUFFER_USAGE::VERTEX:
						ImGui::Text("Usage: VERTEX");
						break;
					case RENDER_BUFFER_USAGE::INDEX:
						ImGui::Text("Usage: INDEX");
						break;
					case RENDER_BUFFER_USAGE::UNIFORM:
						ImGui::Text("Usage: UNIFORM");
						break;
					case RENDER_BUFFER_USAGE::STORAGE:
						ImGui::Text("Usage: STORAGE");
						break;
					case RENDER_BUFFER_USAGE::STAGING:
						ImGui::Text("Usage: STAGING");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_BUFFER_USAGE for resource trackering editor");
						break;
					}
					switch (t_Buffer.memProperties)
					{
					case RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
						ImGui::Text("Memory properties: DEVICE_LOCAL");
						break;
					case RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
						ImGui::Text("Memory properties: HOST_VISIBLE");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_MEMORY_PROPERTIES for resource trackering editor");
						break;
					}
				}
				break;
				case RESOURCE_TYPE::IMAGE:
				{

				}
				break;
				case RESOURCE_TYPE::SAMPLER:
				{

				}
				break;
				case RESOURCE_TYPE::FENCE:
				{

				}
				break;
				default:
					BB_ASSERT(false, "Unknown RESOURCE_TYPE for resource trackering editor");
					break;
				}
			}
			ImGui::PopID();
			t_Entry = t_Entry->next;
		}
	}
	ImGui::End();
}