#pragma once

#include "gui/debug_impl/imgui/imgui.h"


namespace ImGui
{
	IMGUI_API bool InputInt(
		const char* label,
		int* v,
		int step = 1,
		int step_fast = 100,
		const char* format = NULL,
		ImGuiInputTextFlags extra_flags = 0);
}
