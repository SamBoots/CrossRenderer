#include "RenderResourceTracker.h"
#include "Editor.h"

using namespace BB;

void BB::RenderResourceTracker::AddResource(const RenderResource& a_Resource)
{
	m_RenderResource.emplace_back(a_Resource);
}

void BB::RenderResourceTracker::Editor() const
{
	Editor::DisplayRenderResources(*this);
}