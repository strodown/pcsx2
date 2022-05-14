/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "InputManager.h"
#include "Device.h"
#include "keyboard.h"
#include "state_management.h"

#ifdef SDL_BUILD
#include "SDL/joystick.h"
#endif

InputDeviceManager device_manager;

#include <wx/app.h>

// Externs
extern unsigned int GSmakeSnapshot(char* path);
namespace Implementations
{
	extern void GSwindow_CycleAspectRatio();
	extern void Sys_TakeSnapshot();
	extern void Framelimiter_TurboToggle();
	extern void Framelimiter_SlomoToggle();
}
extern void States_CycleSlotForward();
extern void States_CycleSlotBackward();
extern void States_FreezeCurrentSlot();
extern void States_DefrostCurrentSlot();

static int g_buttonStates = 0;
static int g_buttonChanged = 0;

// Needs to be moved to individual device code, as does the keyboard input.
void PollForJoystickInput(int cpad)
{
	int index = Device::uid_to_index(cpad);
	if (index < 0)
		return;

	auto& gamePad = device_manager.devices[index];

	gamePad->UpdateDeviceState();

	g_buttonChanged = 0; // Reset schanges

	for (u32 i = 0; i < MAX_KEYS; i++)
	{
		s32 value = gamePad->GetInput((gamePadValues)i);
		if (value != 0)
		{
			g_key_status.press(cpad, i, value);

			if ((g_buttonStates & (1 << i)) == 0) g_buttonChanged |= 1 << i;
			g_buttonStates |= 1 << i;
		}
		else
		{

			g_key_status.release(cpad, i);

			if ((g_buttonStates & (1 << i)) != 0) g_buttonChanged |= 1 << i;
			g_buttonStates &= ~(1 << i);
		}
	}
	int hotkeyMask = (1 << (int)gamePadValues::PAD_HOTKEY);
	bool hotKey = (g_buttonStates & hotkeyMask) != 0;
	if (hotKey & (g_buttonChanged & ~hotkeyMask) != 0)
	{
		if      (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_START   ))) != 0) wxApp::GetInstance()->Exit();                 // Exit
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_L1      ))) != 0) Implementations::Sys_TakeSnapshot();          // Screenshot
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_UP      ))) != 0) States_CycleSlotForward();                    // Slot++
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_DOWN    ))) != 0) States_CycleSlotBackward();                   // Slot--
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_SQUARE  ))) != 0) States_FreezeCurrentSlot();                   // Save
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_TRIANGLE))) != 0) States_DefrostCurrentSlot();                  // Load
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_R1      ))) != 0) Implementations::GSwindow_CycleAspectRatio(); // Ratio cycle
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_LEFT    ))) != 0) Implementations::Framelimiter_SlomoToggle();  // Slow motion
		else if (((g_buttonStates & g_buttonChanged) & (1 << (int)(gamePadValues::PAD_RIGHT   ))) != 0) Implementations::Framelimiter_TurboToggle();  // Turbo mode
	}
}
void InputDeviceManager::Update()
{
	// Poll keyboard/mouse event. There is currently no way to separate pad0 from pad1 event.
	// So we will populate both pad in the same time
	for (u32 cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
	{
		g_key_status.keyboard_state_acces(cpad);
	}
	UpdateKeyboardInput();

	// Get joystick state + Commit
	for (u32 cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
	{
		g_key_status.joystick_state_acces(cpad);

		PollForJoystickInput(cpad);

		g_key_status.commit_status(cpad);
	}

	Pad::rumble_all();
}

/*
 * Find and set up joysticks, potentially other devices.
 */
void EnumerateDevices()
{
#ifdef SDL_BUILD
	JoystickInfo::EnumerateJoysticks(device_manager.devices);
#endif
}
