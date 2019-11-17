/*
 *  Copyright (C) 2002-2018  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <list>
#include <deque>
#include "mem.h"

void DEBUG_SetupConsole(void);
void DEBUG_DrawScreen(void);
bool DEBUG_Breakpoint(void);
bool DEBUG_IntBreakpoint(Bit8u intNum);
void DEBUG_Enable(bool pressed);
bool DEBUG_IsDebugging();
void DEBUG_SetIsDebugging(bool newVal);
void DEBUG_CheckExecuteBreakpoint(Bit16u seg, Bit32u off);
bool DEBUG_ExitLoop(void);
void DEBUG_RefreshPage(char scroll);
Bitu DEBUG_EnableDebugger(void);

// Inputs From DebugBox
void DEBUG_Run();
void DEBUG_Step();
void DEBUG_StepOver();
void DEBUG_StepOut();

// Calls to DebugBox
void DEBUG_OnDebugEnable();

// Misc
void DEBUG_PreUpdate();
void DEBUG_PostUpdate();

extern bool DosboxWindowIsFocused;

extern Bitu cycle_count;
extern Bitu debugCallback;

#ifdef C_HEAVY_DEBUG
bool DEBUG_HeavyIsBreakpoint(void);
void DEBUG_HeavyWriteLogInstruction(void);
#endif

#define MAXCMDLEN 254 
struct SCodeViewData {
	int     cursorPos;
	Bit16u  firstInstSize;
	Bit16u  useCS;
	Bit32u  useEIPlast, useEIPmid;
	Bit32u  useEIP;
	Bit16u  cursorSeg;
	Bit32u  cursorOfs;
	bool    ovrMode;
	char    inputStr[MAXCMDLEN + 1];
	char    suspInputStr[MAXCMDLEN + 1];
	int     inputPos;
};

enum EBreakpoint { BKPNT_UNKNOWN, BKPNT_PHYSICAL, BKPNT_INTERRUPT, BKPNT_MEMORY, BKPNT_MEMORY_PROT, BKPNT_MEMORY_LINEAR };

#define BPINT_ALL 0x100

class CBreakpoint
{
public:

	CBreakpoint(void);
	void					SetAddress(Bit16u seg, Bit32u off);
	void					SetAddress(PhysPt adr) { location = adr; type = BKPNT_PHYSICAL; };
	void					SetInt(Bit8u _intNr, Bit16u ah, Bit16u al) { intNr = _intNr, ahValue = ah; alValue = al; type = BKPNT_INTERRUPT; };
	void					SetOnce(bool _once) { once = _once; };
	void					SetType(EBreakpoint _type) { type = _type; };
	void					SetValue(Bit8u value) { ahValue = value; };
	void					SetOther(Bit8u other) { alValue = other; };

	bool					IsActive(void) { return active; };
	void					Activate(bool _active);

	bool					CanActivate() const { return canActivate; }
	void					SetCanActivate(bool _canActivate) { canActivate = _canActivate; }

	EBreakpoint				GetType(void) { return type; };
	bool					GetOnce(void) { return once; };
	PhysPt					GetLocation(void) { return location; };
	Bit16u					GetSegment(void) { return segment; };
	Bit32u					GetOffset(void) { return offset; };
	Bit8u					GetIntNr(void) { return intNr; };
	Bit16u					GetValue(void) { return ahValue; };
	Bit16u					GetOther(void) { return alValue; };

	// statics
	static CBreakpoint*		AddBreakpoint(Bit16u seg, Bit32u off, bool once);
	static CBreakpoint*		AddIntBreakpoint(Bit8u intNum, Bit16u ah, Bit16u al, bool once);
	static CBreakpoint*		AddMemBreakpoint(Bit16u seg, Bit32u off);
	static void				DeactivateBreakpoints();
	static void				ActivateBreakpoints();
	static void				ActivateBreakpointsExceptAt(PhysPt adr);
	static bool				CheckBreakpoint(PhysPt adr);
	static bool				CheckBreakpoint(Bitu seg, Bitu off);
	static bool				CheckIntBreakpoint(PhysPt adr, Bit8u intNr, Bit16u ahValue, Bit16u alValue);
	static CBreakpoint*		FindPhysBreakpoint(Bit16u seg, Bit32u off, bool once);
	static CBreakpoint*		FindOtherActiveBreakpoint(PhysPt adr, CBreakpoint* skip);
	static bool				IsBreakpoint(Bit16u seg, Bit32u off);
	static bool				DeleteBreakpoint(Bit16u seg, Bit32u off);
	static bool				DeleteByIndex(Bit16u index);
	static void				DeleteAll(void);
	static void				ShowList(void);
	const static std::list<CBreakpoint*>& GetBreakPointList();
	static void				SerializeAll(const char* filename, bool isReading = true);


private:
	EBreakpoint	type;
	// Physical
	PhysPt		location;
	Bit8u		oldData;
	Bit16u		segment;
	Bit32u		offset;
	// Int
	Bit8u		intNr;
	Bit16u		ahValue;
	Bit16u		alValue;
	// Shared
	bool		active;
	bool		once;
	bool		canActivate;

	static std::list<CBreakpoint*>	BPoints;
};

class CallFunction
{
public:
	Bitu GetFromSegment() const { return FromSegment; }
	Bitu GetToSegment() const { return ToSegment; }
	Bitu GetFromOffset() const { return FromOffset; }
	Bitu GetToOffset() const { return ToOffset; }
	Bitu GetReturnOffset() const { return ReturnOffset; }

	static void AddCall(Bitu fromSegment, Bitu fromOff, Bitu toSegment, Bitu toOff, Bitu retOff);
	static void NotifyReturn(Bitu seg, Bitu off);
	static void ClearCallStack();
	static const std::deque<CallFunction*>& GetCallStack();

private:

	Bitu FromSegment;
	Bitu ToSegment;
	Bitu FromOffset;
	Bitu ToOffset;
	Bitu ReturnOffset;

	static std::deque<CallFunction*> CallStack;
};