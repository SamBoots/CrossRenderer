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

void BB::Editor::DisplayDrawObjects(const BB::Slice<DrawObject> a_DrawObjects, const TransformPool& a_Pool)
{
	ImGui::Begin("Draw Objects");

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
	ImGui::End();
}

void BB::Editor::DisplayRenderResources(const BB::RenderResourceTracker& a_ResTracker)
{
	ImGui::Begin("Render resources");

	if (ImGui::CollapsingHeader("Resources"))
	{
		for (size_t i = 0; i < a_ResTracker.m_RenderResource.size(); i++)
		{
			ImGui::PushID(i);
			const RenderResource& t_Res = a_ResTracker.m_RenderResource[i];

			if (ImGui::TreeNode("PLACE_HOLDER_NAME"))
			{
				switch (t_Res.type)
				{
				case RESOURCE_TYPE::DESCRIPTOR:
				{
					const RenderDescriptorCreateInfo& t_Desc = t_Res.descriptor;
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
						if (ImGui::TreeNode("PLACE_HOLDER_DESC_NAME"))
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
					const RenderCommandQueueCreateInfo& t_Queue = t_Res.queue;
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
					const RenderCommandAllocatorCreateInfo& t_Alloc = t_Res.commandAllocator;
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
					const RenderCommandListCreateInfo& t_List = t_Res.commandList;
				}
				break;
				case RESOURCE_TYPE::BUFFER:
				{
					const RenderBufferCreateInfo& t_Buffer = t_Res.buffer;
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
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	ImGui::End();
}