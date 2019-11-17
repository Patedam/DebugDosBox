#include "dosbox.h"
#include "debug.h"
#include "paging.h"
#include "regs.h"
#include "render.h"
#include "mem.h"
#include "cpu.h"
#include "mapper.h"
#include "debug/debug_inc.h"

#include "gui/debug_impl/imgui/imgui.h"
#include "gui/debug_impl/imgui/imgui_memory_editor.h"
#include "gui/debug_impl/imgui/imgui_debugbox_widgets.h"

#include <vector>
#include <string>
#include <sstream>
#include <SDL.h>

#pragma region Memory
Bit32u GetAddress(Bit16u seg, Bit32u offset);

extern Bit16u  dataSeg;
extern Bit32u  dataOfs;
static MemoryEditor mem_edit;

extern SCodeViewData codeViewData;

bool DosboxWindowIsFocused = false;

extern const char* RunningProgram;

// This draw the memory content in DrawMemory().
unsigned char ReadData(const unsigned char * data /* is null */, size_t offset)
{
	Bit8u ch = 0;
	Bit32u address = GetAddress(dataSeg, dataOfs + offset);
	if (mem_readb_checked(address, &ch))
	{
		ch = 0;
	}
	return ch;
}

void CopyMemorySegmentIntoClipboard()
{
	const int arraySize = (0xFFFF * 3) + 1;
	Bit8u ch = 0;
	Bit32u address = 0;
	char* memoryContent = new char[arraySize];
	for (int i = 0; i < 0xFFFF; ++i)
	{
		address = GetAddress(dataSeg, dataOfs + i);
		if (mem_readb_checked(address, &ch))
		{
			ch = 0;
		}
		if (i % 16 == 15) // Add a \n every 16 numbers
		{
			sprintf(memoryContent + (i * 3), "%02X\n", ch);
		}
		else
		{
			sprintf(memoryContent + (i * 3), "%02X ", ch);
		}
	}
	memoryContent[0xFFFF * 3] = 0;
	SDL_SetClipboardText(memoryContent);
	delete[] memoryContent;
}

void DrawMemoryEditor()
{
	ImGui::Begin("RAM");

	ImGui::InputInt("Segment", (int*)&dataSeg, 1, 100, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::InputInt("Offset", (int*)&dataOfs, 1, 100, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
	if (ImGui::Button("Copy Content"))
	{
		CopyMemorySegmentIntoClipboard();
	}

	ImGui::Separator();

	mem_edit.DrawContents(0, 0xFFFF /*max segment offset*/, dataOfs);
	
	ImGui::End();
}
#pragma endregion

#pragma region Registers
struct 
{
	Bit32u eax;
} LatestRegistersValue;

static void GetRegisterRectangleWithPadding(ImVec2& outItemMinRect, ImVec2& outItemMaxRect, bool takeWholeWidth = false)
{
	// Padding
	static float rectPaddingOffsetMin = 2.0f;
	static float rectPaddingOffsetMax = 4.0f;

	outItemMinRect = ImGui::GetItemRectMin();
	outItemMaxRect = ImGui::GetItemRectMax();

	if (takeWholeWidth)
	{
		outItemMaxRect.x = outItemMinRect.x + ImGui::GetContentRegionAvailWidth();
	}

	outItemMinRect.x -= rectPaddingOffsetMin;
	outItemMinRect.y -= rectPaddingOffsetMin;
	outItemMaxRect.x += rectPaddingOffsetMax;
	outItemMaxRect.y += rectPaddingOffsetMax;
}

static struct OldValues
{
	struct Value
	{
		bool hasChanged = false;
		Bitu previousVal = 0x0;
	};

	Value eax;
	Value ebx;
	Value ecx;
	Value edx;

	Value esi;
	Value edi;
	Value ebp;
	Value esp;

	Value eip;

	Value ds;
	Value es;
	Value fs;
	Value gs;
	Value ss;
	Value cs;

	Value cf;
	Value zf;
	Value sf;
	Value of;
	Value af;
	Value pf;
	Value df;
	Value fif;
	Value tf;

} PreviousRegsValues;

void RefreshRegisters()
{
	static Bitu previousCycleCount = 0;
	if (DEBUG_IsDebugging() && cycle_count != previousCycleCount)
	{
		PreviousRegsValues.eax.hasChanged = reg_eax != PreviousRegsValues.eax.previousVal;
		PreviousRegsValues.eax.previousVal = reg_eax;
		PreviousRegsValues.ebx.hasChanged = reg_ebx != PreviousRegsValues.ebx.previousVal;
		PreviousRegsValues.ebx.previousVal = reg_ebx;
		PreviousRegsValues.ecx.hasChanged = reg_ecx != PreviousRegsValues.ecx.previousVal;
		PreviousRegsValues.ecx.previousVal = reg_ecx;
		PreviousRegsValues.edx.hasChanged = reg_edx != PreviousRegsValues.edx.previousVal;
		PreviousRegsValues.edx.previousVal = reg_edx;

		PreviousRegsValues.esi.hasChanged = reg_esi != PreviousRegsValues.esi.previousVal;
		PreviousRegsValues.esi.previousVal = reg_esi;
		PreviousRegsValues.edi.hasChanged = reg_edi != PreviousRegsValues.edi.previousVal;
		PreviousRegsValues.edi.previousVal = reg_edi;
		PreviousRegsValues.ebp.hasChanged = reg_ebp != PreviousRegsValues.ebp.previousVal;
		PreviousRegsValues.ebp.previousVal = reg_ebp;
		PreviousRegsValues.esp.hasChanged = reg_esp != PreviousRegsValues.esp.previousVal;
		PreviousRegsValues.esp.previousVal = reg_esp;
		PreviousRegsValues.eip.hasChanged = reg_eip != PreviousRegsValues.eip.previousVal;
		PreviousRegsValues.eip.previousVal = reg_eip;

		PreviousRegsValues.ds.hasChanged = SegValue(ds) != PreviousRegsValues.ds.previousVal;
		PreviousRegsValues.ds.previousVal = SegValue(ds);
		PreviousRegsValues.es.hasChanged = SegValue(es) != PreviousRegsValues.es.previousVal;
		PreviousRegsValues.es.previousVal = SegValue(es);
		PreviousRegsValues.fs.hasChanged = SegValue(fs) != PreviousRegsValues.fs.previousVal;
		PreviousRegsValues.fs.previousVal = SegValue(fs);
		PreviousRegsValues.gs.hasChanged = SegValue(gs) != PreviousRegsValues.gs.previousVal;
		PreviousRegsValues.gs.previousVal = SegValue(gs);
		PreviousRegsValues.ss.hasChanged = SegValue(ss) != PreviousRegsValues.ss.previousVal;
		PreviousRegsValues.ss.previousVal = SegValue(ss);
		PreviousRegsValues.cs.hasChanged = SegValue(cs) != PreviousRegsValues.cs.previousVal;
		PreviousRegsValues.cs.previousVal = SegValue(cs);

		PreviousRegsValues.ds.hasChanged = SegValue(ds) != PreviousRegsValues.ds.previousVal;
		PreviousRegsValues.ds.previousVal = SegValue(ds);

		PreviousRegsValues.cf.hasChanged  = GETFLAG(CF) != PreviousRegsValues.cf.previousVal;
		PreviousRegsValues.cf.previousVal = GETFLAG(CF);
		PreviousRegsValues.zf.hasChanged  = GETFLAG(ZF) != PreviousRegsValues.zf.previousVal;
		PreviousRegsValues.zf.previousVal = GETFLAG(ZF);
		PreviousRegsValues.sf.hasChanged  = GETFLAG(SF) != PreviousRegsValues.sf.previousVal;
		PreviousRegsValues.sf.previousVal = GETFLAG(SF);
		PreviousRegsValues.of.hasChanged  = GETFLAG(OF) != PreviousRegsValues.of.previousVal;
		PreviousRegsValues.of.previousVal = GETFLAG(OF);
		PreviousRegsValues.af.hasChanged  = GETFLAG(AF) != PreviousRegsValues.af.previousVal;
		PreviousRegsValues.af.previousVal = GETFLAG(AF);
		PreviousRegsValues.pf.hasChanged  = GETFLAG(PF) != PreviousRegsValues.pf.previousVal;
		PreviousRegsValues.pf.previousVal = GETFLAG(PF);
		PreviousRegsValues.df.hasChanged  = GETFLAG(DF) != PreviousRegsValues.df.previousVal;
		PreviousRegsValues.df.previousVal = GETFLAG(DF);
		PreviousRegsValues.fif.hasChanged = GETFLAG(IF) != PreviousRegsValues.fif.previousVal;
		PreviousRegsValues.fif.previousVal= GETFLAG(IF);
		PreviousRegsValues.tf.hasChanged  = GETFLAG(TF) != PreviousRegsValues.tf.previousVal;
		PreviousRegsValues.tf.previousVal = GETFLAG(TF);

		previousCycleCount = cycle_count;
	}
}

inline const ImVec4& GetColor(OldValues::Value& val)
{
	static ImVec4 normalColor(1.0f, 1.0f, 1.0f, 1.0f);
	static ImVec4 modifiedColor(0.f, 1.0f, 1.0f, 1.0f);

	return DEBUG_IsDebugging() ? ((val.hasChanged) ? modifiedColor : normalColor) : normalColor;
}

void DrawRegisters()
{
	if (ImGui::Begin("Registers", nullptr, ImGuiWindowFlags_AlwaysAutoResize) == false)
	{
		ImGui::End();
		return;
	}

	RefreshRegisters();

	ImVec2 rectMinPos;
	ImVec2 rectMaxPos;
	
	static ImVec4 exx_registers_border_color(0.0f, 1.0f, 0.0f, 1.0f);
	static ImVec4 exi_exp_registers_border_color(0.0f, 1.0f, 1.0f, 1.0f);
	static ImVec4 segments_border_color(1.0f, 1.0f, 0.0f, 1.0f);
	static ImVec4 flags_border_color(1.0f, 0.0f, 0.0f, 1.0f);

//	IMGUI_API ImVec2        CalcTextSize(const char* text, const char* text_end = NULL, bool hide_text_after_double_hash = false, float wrap_width = -1.0f);

	const ImGuiStyle& style = ImGui::GetStyle();

	static int regLine = 4, regCol = 1;
	static ImVec2 exx_registers_text_size = ImGui::CalcTextSize("EXX=FFFFFFFF", nullptr, false, 0.0f);
	static ImVec2 exx_registers_child_size(
		(exx_registers_text_size.x * regCol) + (style.FramePadding.x * 2.0f) + (style.ItemSpacing.x * 2.0f),
		(exx_registers_text_size.y * regLine) + (style.FramePadding.y * 2.0f) + (style.ItemSpacing.y * 2.0f) + (style.ItemInnerSpacing.y * regLine));
		//(exx_registers_text_size.x) + (style.FramePadding.x * 2.0f) + (style.ItemSpacing.x) + 1.0f,
		//(exx_registers_text_size.y * regLine) + (style.FramePadding.y * 2.0f) + (style.ItemSpacing.y) + (style.ItemInnerSpacing.y * regLine) + 1.0f);

	ImGui::PushStyleColor(ImGuiCol_Border, exx_registers_border_color);
	ImGui::BeginChild("exx_regs", exx_registers_child_size, true, ImGuiWindowFlags_NoScrollbar);
		ImGui::TextColored(GetColor(PreviousRegsValues.eax), "EAX=%08X", reg_eax);
		ImGui::TextColored(GetColor(PreviousRegsValues.ebx), "EBX=%08X", reg_ebx);
		ImGui::TextColored(GetColor(PreviousRegsValues.ecx), "ECX=%08X", reg_ecx);
		ImGui::TextColored(GetColor(PreviousRegsValues.edx), "EDX=%08X", reg_edx);
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Border, exi_exp_registers_border_color);
	ImGui::BeginChild("exi_exp_regs", exx_registers_child_size, true, ImGuiWindowFlags_NoScrollbar);
		ImGui::TextColored(GetColor(PreviousRegsValues.esi), "ESI=%08X", reg_esi);
		ImGui::TextColored(GetColor(PreviousRegsValues.edi), "EDI=%08X", reg_edi);
		ImGui::TextColored(GetColor(PreviousRegsValues.ebp), "EBP=%08X", reg_ebp);
		ImGui::TextColored(GetColor(PreviousRegsValues.esp), "ESP=%08X", reg_esp);
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::Spacing();


	static int segLine = 3, segCol = 2;
	static ImVec2 segments_text_size = ImGui::CalcTextSize("DS=FFFF", nullptr, false, 0.0f);
	static ImVec2 segments_child_size(
		(segments_text_size.x * segCol) + (style.FramePadding.x * 2.0f) + (style.ItemSpacing.x * 2.0f) + (style.ItemInnerSpacing.x * segCol),
		(segments_text_size.y * segLine) + (style.FramePadding.y * 2.0f) + (style.ItemSpacing.y * 2.0f) + (style.ItemInnerSpacing.y * segLine));

	ImGui::PushStyleColor(ImGuiCol_Border, segments_border_color);
	ImGui::BeginChild("segments", segments_child_size, true, ImGuiWindowFlags_NoScrollbar);
		ImGui::TextColored(GetColor(PreviousRegsValues.ds), "DS=%04X", SegValue(ds)); ImGui::SameLine(); ImGui::TextColored(GetColor(PreviousRegsValues.fs), "FS=%04X", SegValue(fs));
		ImGui::TextColored(GetColor(PreviousRegsValues.es), "ES=%04X", SegValue(es)); ImGui::SameLine(); ImGui::TextColored(GetColor(PreviousRegsValues.gs), "GS=%04X", SegValue(gs));
		ImGui::TextColored(GetColor(PreviousRegsValues.cs), "CS=%04X", SegValue(cs)); ImGui::SameLine(); ImGui::TextColored(GetColor(PreviousRegsValues.ss), "SS=%04X", SegValue(ss));
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::SameLine();

	static int flagLine = 3, flagCol = 3;
	static ImVec2 flags_text_size = ImGui::CalcTextSize("CF=0", nullptr, false, 0.0f);
	static ImVec2 flags_child_size(
		(flags_text_size.x * flagCol) + (style.FramePadding.x * 2.0f) + (style.ItemSpacing.x * 2.0f) + (style.ItemInnerSpacing.x * flagCol),
		(flags_text_size.y * flagLine) + (style.FramePadding.y * 2.0f) + (style.ItemSpacing.y * 2.0f) + (style.ItemInnerSpacing.y * flagLine));
	
	ImGui::PushStyleColor(ImGuiCol_Border, flags_border_color);
	ImGui::BeginChild("flags", flags_child_size, true, ImGuiWindowFlags_NoScrollbar);
		ImGui::TextColored(GetColor(PreviousRegsValues.cf), "CF=%01X", GETFLAG(CF) ? 1 : 0);
		ImGui::SameLine();
		ImGui::TextColored(GetColor(PreviousRegsValues.zf), "ZF=%01X", GETFLAG(ZF) ? 1 : 0);
		ImGui::SameLine();
		ImGui::TextColored(GetColor(PreviousRegsValues.sf), "SF=%01X", GETFLAG(SF) ? 1 : 0);

		ImGui::TextColored(GetColor(PreviousRegsValues.of), "Oj=%01X", GETFLAG(OF) ? 1 : 0);
		ImGui::SameLine();
		ImGui::TextColored(GetColor(PreviousRegsValues.af), "AF=%01X", GETFLAG(AF) ? 1 : 0);
		ImGui::SameLine();
		ImGui::TextColored(GetColor(PreviousRegsValues.pf), "PF=%01X", GETFLAG(PF) ? 1 : 0);

		ImGui::TextColored(GetColor(PreviousRegsValues.df), "Dj=%01X", GETFLAG(DF) ? 1 : 0);
		ImGui::SameLine();
		ImGui::TextColored(GetColor(PreviousRegsValues.fif), "IF=%01X", GETFLAG(IF) ? 1 : 0);
		ImGui::SameLine();
		ImGui::TextColored(GetColor(PreviousRegsValues.tf), "TF=%01X", GETFLAG(TF) ? 1 : 0);
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::Spacing();

	ImGui::TextColored(GetColor(PreviousRegsValues.eip), "EIP=%08X", reg_eip);
	GetRegisterRectangleWithPadding(rectMinPos, rectMaxPos, true);
	ImGui::GetWindowDrawList()->AddRect(rectMinPos, rectMaxPos, IM_COL32_WHITE, 5.0f);
	
	ImGui::Spacing();

	ImGui::Text("CycleCount=%u", cycle_count);
	GetRegisterRectangleWithPadding(rectMinPos, rectMaxPos, true);
	ImGui::GetWindowDrawList()->AddRect(rectMinPos, rectMaxPos, IM_COL32_WHITE, 5.0f);

	ImGui::End();
}
#pragma endregion

#pragma region Palette
void CopyPaletteIntoClipboard()
{
	const int arraySize = (256 * 9) + 1;
	Bit8u ch = 0;
	Bit32u address = 0;
	char* paletteContent = new char[arraySize];
	for (int i = 0; i < 256; ++i)
	{
		auto& palCol = render.pal.rgb[i];
		sprintf(paletteContent + (i * 9), "%02X %02X %02X\n", palCol.red, palCol.green, palCol.blue);
	}
	paletteContent[256 * 9] = 0;
	SDL_SetClipboardText(paletteContent);
	delete[] paletteContent;
}

void DrawPalette()
{
	if (ImGui::Begin("Palette Display", nullptr, ImGuiWindowFlags_AlwaysAutoResize) == false)
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Copy Palette To Clipboard"))
	{
		CopyPaletteIntoClipboard();
	}

	static constexpr int width = 16;
	static constexpr int height = 16;
	int x = 0;
	for (int i = 0; i < 256; ++i)
	{
		auto& palCol = render.pal.rgb[i];
		const ImVec4 col_v4(palCol.red / 255.f, palCol.green / 255.f, palCol.blue / 255.f, 1.0f);
		if (ImGui::ColorButton("##ColorButton", col_v4, 0))
		{
			;
		}
		if (++x < width)
		{
			ImGui::SameLine();
		}
		else
		{
			x = 0;
		}
	}

	ImGui::End();
}
#pragma endregion

#pragma region CodeView
struct LineOfCode
{
	Bit32u EIPOffset = 0;
	std::string Code;
	bool IsBreaked = false;
};

struct CodeSyncPoint
{
	Bit32u Offset;

	CodeSyncPoint(Bit32u offset) : Offset(offset) {}
};

static std::deque<LineOfCode> LinesOfCode;
static std::vector<CodeSyncPoint> CodeSyncPoints;

static bool ForceGoTo = false;
static int GoToOffset = 0;

void DrawCode()
{
	if (ImGui::Begin("Code Display", nullptr, ImGuiWindowFlags_None) == false)
	{
		ImGui::End();
		return;
	}

	bool shouldGoTo = ForceGoTo;
	ForceGoTo = false;
	if (ImGui::Button("GoTo"))
	{
		shouldGoTo = true;
	}
	ImGui::SameLine();
	if (ImGui::InputInt("Offset", (int*)&GoToOffset, 1, 100, "%04X", ImGuiInputTextFlags_CharsHexadecimal))
	{
		shouldGoTo = true;
	}
	ImGui::Separator();

	{
		ImGui::BeginChild(ImGui::GetID("CodeContent"));

		static bool hasScrolled = false;
		static Bit16u lastCS = 0xFFFF;
		if (lastCS != codeViewData.useCS && DEBUG_IsDebugging())
		{
			LinesOfCode.clear();

			Bit32u disEIP = codeViewData.useEIP;
			PhysPt start = GetAddress(codeViewData.useCS, codeViewData.useEIP);
			Bitu size;
			static char line20[21] = "                    ";
			char buffer[512];
			Bitu totalSize = 0;
			OpInfo opInfo;
			while (totalSize < 0xFFFF)
			{
				Bitu drawsize = size = DasmI386(&opInfo, start, disEIP, cpu.code.big);
				sprintf(buffer, "%04X:%04X  %s", codeViewData.useCS, disEIP, opInfo.dline);

				if (opInfo.IsCallOrJmp)
				{
					// TODO : Add code sync point
					CodeSyncPoints.emplace_back(opInfo.callOrJmpOffset);
					opInfo.IsCallOrJmp = false;
				}

				LineOfCode line;
				line.Code = buffer;
				line.EIPOffset = disEIP;
				LinesOfCode.push_back(line);

				start += size;
				disEIP += size;
				totalSize += size;
			}

			// Patch using Code Sync Points
			for (CodeSyncPoint& codeSyncPoint : CodeSyncPoints)
			{
				Bit32u index = 0;
				for (LineOfCode& line : LinesOfCode)
				{
					if (line.EIPOffset > codeSyncPoint.Offset)
					{
						if (index > 0)
						{
							if (LinesOfCode[index - 1].EIPOffset != codeSyncPoint.Offset)
							{
								OpInfo opInfo;
								PhysPt start = GetAddress(codeViewData.useCS, codeSyncPoint.Offset);
								Bitu size = DasmI386(&opInfo, start, codeSyncPoint.Offset, cpu.code.big);
								sprintf(buffer, "%04X:%04X  %s", codeViewData.useCS, codeSyncPoint.Offset, opInfo.dline);

								if (codeSyncPoint.Offset + size == line.EIPOffset)
								{
									// Missing a line, we add it now
									LineOfCode line;
									line.Code = buffer;
									line.EIPOffset = codeSyncPoint.Offset;
									LinesOfCode.insert(LinesOfCode.begin() + index, line);
								}
								else
								{
									// Replace current line because its invalid anyway
									line.Code = buffer;
									line.EIPOffset = codeSyncPoint.Offset;

									// Check and patch next instructions
									for (Bit32u nextInstructionIndex = index+1; nextInstructionIndex < LinesOfCode.size(); ++nextInstructionIndex)
									{
										LineOfCode& nextInstruction = LinesOfCode[nextInstructionIndex];
										if (line.EIPOffset + size == nextInstruction.EIPOffset)
										{
											break;
										}
										else
										{
											// Need to implement patching on multiple lines. Never happened yet
											// TODO: clean that patching code when implementing that, and optimize
											break;
										}
									}
								}
							}
						}
						break;
					}
					++index;
				}
			}

			lastCS = codeViewData.useCS;

			hasScrolled = false;
		}

		static Bit32u lastEIP = 0xFFFF;
		static int CurrentEIPIndex = 0;
		if ((lastEIP != reg_eip && DEBUG_IsDebugging()) || shouldGoTo)
		{
			Bit32u goToEip = !shouldGoTo ? reg_eip : GoToOffset;

			// Find new EIP index
			bool foundIndex = false;
			for (size_t i = 0; i < LinesOfCode.size(); ++i)
			{
				if (LinesOfCode[i].EIPOffset >= goToEip)
				{
					CurrentEIPIndex = i;
					foundIndex = true;
					break;
				}
			}

			if (foundIndex)
			{
				lastEIP = reg_eip;
			}

			hasScrolled = false;
		}

		static ImVec4 defaultColor(1.0f, 1.0f, 1.0f, 1.0f);
		static ImVec4 greenColor(0.0f, 1.0f, 0.0f, 1.0f);
		static ImVec4 redColor(1.0f, 0.0f, 0.0f, 1.0f);
		static ImVec4 orangeColor(1.0f, 0.5f, 0.0f, 1.0f);

		ImGuiListClipper clipper(LinesOfCode.size());
		while (clipper.Step())
		{
			if (clipper.StepNo > 1 && (DEBUG_IsDebugging() || shouldGoTo))
			{
				// At stepNo > 1 we have the size of an item
				if (!hasScrolled)
				{
					int desiredIndex = CurrentEIPIndex - 10;
					if (desiredIndex < 0)
						desiredIndex = 0;
					float desiredPos = (clipper.ItemsHeight) * desiredIndex;
					ImGui::SetScrollY(desiredPos);

					hasScrolled = true;
				}
			}

			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			{
				LineOfCode& lineOfCode = LinesOfCode[i];
				ImGui::PushID(i);
				lineOfCode.IsBreaked = CBreakpoint::IsBreakpoint(codeViewData.useCS, lineOfCode.EIPOffset);
				if (ImGui::Checkbox("##BreakPointCheckBox", &lineOfCode.IsBreaked))
				{
					if (lineOfCode.IsBreaked)
					{
						CBreakpoint::AddBreakpoint(codeViewData.useCS, lineOfCode.EIPOffset, false);
						DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X\n", codeViewData.useCS, lineOfCode.EIPOffset);

						if (DEBUG_IsDebugging() == false)
						{
							CBreakpoint::ActivateBreakpoints();
						}
					}
					else
					{
						if (CBreakpoint::DeleteBreakpoint(codeViewData.useCS, lineOfCode.EIPOffset))
						{
							DEBUG_ShowMsg("DEBUG: Breakpoint deletion success.\n");
						}
						else
						{
							DEBUG_ShowMsg("DEBUG: Failed to delete breakpoint.\n");
						}
					}

					CBreakpoint::SerializeAll(RunningProgram, false);
				}
				ImGui::SameLine();
				if (lineOfCode.EIPOffset == reg_eip)
				{
					ImGui::TextColored(greenColor, "%d: %s", i, lineOfCode.Code.c_str());
					//ImGui::SetScrollHereY();
				}
				else 
				{
					CBreakpoint* breakPoint = CBreakpoint::FindPhysBreakpoint(codeViewData.useCS, lineOfCode.EIPOffset, false);
					if (breakPoint)
					{
						if (breakPoint->CanActivate())
						{
							ImGui::TextColored(redColor, "%d: %s", i, lineOfCode.Code.c_str());
						}
						else
						{
							ImGui::TextColored(orangeColor, "%d: %s", i, lineOfCode.Code.c_str());
						}
					}
					else
					{
						ImGui::TextColored(defaultColor, "%d: %s", i, lineOfCode.Code.c_str());
					}
				}
				ImGui::PopID();
			}
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

#pragma endregion

#pragma region(Help Window)
void DrawHelpWindow()
{
	if (!ImGui::Begin("Help"))
	{
		ImGui::End();
		return;
	}
	
	if (!DosboxWindowIsFocused)
	{
		ImGui::Text("Debug mode:");
		ImGui::Text("- No input goes to DosBox");
		ImGui::Text("- F9 to Pause");
		ImGui::Text("- F5 to Run");
	}
	else
	{
		ImGui::Text("Dosbox Window is focused");
		ImGui::Text("All inputs are captured");	
	}
	ImGui::End();
}
#pragma endregion

#pragma region(Logs)
struct ExampleAppLog
{
	ImGuiTextBuffer     Buf;
	ImGuiTextFilter     Filter;
	ImVector<int>       LineOffsets;        // Index to lines offset
	bool                ScrollToBottom;

	void    Clear() { Buf.clear(); LineOffsets.clear(); }

	void    AddLog(const char* fmt, va_list args) IM_FMTARGS(2)
	{
		int old_size = Buf.size();

		Buf.appendfv(fmt, args);
		/* Add newline if not present */
		Bitu len=strlen(fmt);
		if (fmt[len - 1] != '\n')
		{
			Buf.appendf("\n");
		}

		for (int new_size = Buf.size(); old_size < new_size; old_size++)
			if (Buf[old_size] == '\n')
				LineOffsets.push_back(old_size);
		ScrollToBottom = true;
	}

	void    Draw(const char* title, bool* p_open = NULL)
	{
		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title, p_open))
		{
			ImGui::End();
			return;
		}
		if (ImGui::Button("Clear")) Clear();
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");
		ImGui::SameLine();
		Filter.Draw("Filter", -100.0f);
		ImGui::Separator();
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (copy) ImGui::LogToClipboard();

		if (Filter.IsActive())
		{
			const char* buf_begin = Buf.begin();
			const char* line = buf_begin;
			for (int line_no = 0; line != NULL; line_no++)
			{
				const char* line_end = (line_no < LineOffsets.Size) ? buf_begin + LineOffsets[line_no] : NULL;
				if (Filter.PassFilter(line, line_end))
					ImGui::TextUnformatted(line, line_end);
				line = line_end && line_end[1] ? line_end + 1 : NULL;
			}
		}
		else
		{
			ImGui::TextUnformatted(Buf.begin());
		}

		if (ScrollToBottom)
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;
		ImGui::EndChild();
		ImGui::End();
	}
} s_Log;

void DEBUG_ShowMsg(char const* format, ...) 
{
	va_list args;
	va_start(args, format);
	s_Log.AddLog(format, args);
	va_end(args);
}

void DrawLogWindow()
{
	bool isOpen = true;
	s_Log.Draw("Example: Log", &isOpen);
}
#pragma endregion

#pragma region(CallStack)
void DrawCallStackWindow()
{
	if (!ImGui::Begin("CallStack"))
	{
		ImGui::End();
		return;
	}

	if (!DosboxWindowIsFocused)
	{
		const std::deque<CallFunction*>& callstack = CallFunction::GetCallStack();
		for (CallFunction* cf : callstack)
		{
			ImGui::Text("Call from %04X:%04X to %04X:%04X - RetOff: %04X", cf->GetFromSegment(), cf->GetFromOffset(), cf->GetToSegment(), cf->GetToOffset(), cf->GetReturnOffset());
		}
	}
	else
	{
		ImGui::Text("CallStack only displayed when breaked");
	}
	ImGui::End();
}
#pragma endregion

#pragma region(Breakpoints)
void DrawBreakPoints()
{
	if (!ImGui::Begin("Breakpoints"))
	{
		ImGui::End();
		return;
	}

	Bit32s indexToRemove = -1;
	auto& list = CBreakpoint::GetBreakPointList();
	Bit32s nr = 0;
	for (CBreakpoint* bp : list)
	{
		ImGui::PushID(nr);

		bool isActive = bp->CanActivate();
		if (ImGui::Checkbox("##BreakPointCheckBox", &isActive))
		{
			if (isActive)
			{
				// We should activate
				bp->SetCanActivate(true);
				bp->Activate(true);
			}
			else
			{
				// We should deactivate
				bp->Activate(false);
				bp->SetCanActivate(false);
			}
			CBreakpoint::SerializeAll(RunningProgram, false);
		}
		ImGui::SameLine();
		ImGui::Text("%04X:%04X\n", bp->GetSegment(), bp->GetOffset());
		ImGui::SameLine();
		if (ImGui::Button("GoTo##GoToBreakPointButton"))
		{
			GoToOffset = bp->GetOffset();
			ForceGoTo = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("X##DeleteBreakpointButton"))
		{
			indexToRemove = nr;
		}

		ImGui::PopID();

		++nr;
	}

	if (indexToRemove != -1)
	{
		CBreakpoint::DeleteByIndex(indexToRemove);
		CBreakpoint::SerializeAll(RunningProgram, false);
		indexToRemove = -1;
	}

	ImGui::End();
}
#pragma endregion

void DrawDebugWindows()
{
	DrawMemoryEditor();
	DrawRegisters();
	DrawPalette();
	DrawCode();
	DrawHelpWindow();
	DrawLogWindow();
	DrawCallStackWindow();
	DrawBreakPoints();
}

void InitDebug()
{
	mem_edit.ReadFn = &ReadData;
	mem_edit.ReadOnly = true;
}

void DEBUG_OnDebugEnable()
{
	DosboxWindowIsFocused = false;
}

void UpdateDebugWindows()
{
	static bool wasFocused = false;
	if (!wasFocused && DosboxWindowIsFocused)
	{
		wasFocused = true;
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
		Input_CaptureMouse();
	}
	else if (wasFocused && !DosboxWindowIsFocused)
	{
		wasFocused = false;
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		Input_CaptureMouse();
	}
}