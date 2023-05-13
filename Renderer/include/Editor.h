#pragma once
#include "Slice.h"

namespace BB
{
	struct DrawObject;
	struct LightSystem;
	class TransformPool;

	namespace Editor
	{
		void DisplayDrawObjects(const BB::Slice<DrawObject> a_DrawObjects, const TransformPool& a_Pool);
		void DisplayLightSystem(const BB::LightSystem& a_System);
	}
}