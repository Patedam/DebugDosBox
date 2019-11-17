#include "imgui_debugbox_widgets.h"

bool ImGui::InputInt(
	const char* label,
	int* v,
	int step,
	int step_fast,
	const char* format,
	ImGuiInputTextFlags extra_flags)
{
		return InputScalar(
			label,
			ImGuiDataType_S32,
			(void*)v,
			(void*)(step>0 ? &step : NULL),
			(void*)(step_fast>0 ? &step_fast : NULL),
			(format?format:((extra_flags & ImGuiInputTextFlags_CharsHexadecimal) ? "%08X" : "%d")),
			extra_flags);
}