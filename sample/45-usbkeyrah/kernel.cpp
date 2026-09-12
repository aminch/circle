//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2026  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include <circle/string.h>
#include <circle/util.h>
#include <assert.h>

static const char FromKernel[] = "kernel";

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE)			// TRUE: enable plug-and-play
{
	s_pThis = this;

	for (unsigned i = 0; i < MAX_KEYBOARDS; i++)
	{
		m_Keyboard[i].nIndex = i + 1;
		m_Keyboard[i].pKeyboard = 0;
		m_Keyboard[i].bHaveLast = FALSE;
		m_Keyboard[i].ucLastModifiers = 0;
		memset (m_Keyboard[i].LastKeys, 0, sizeof m_Keyboard[i].LastKeys);
		m_Keyboard[i].nReports = 0;
		m_Keyboard[i].nReportsPrev = 0;
		m_Keyboard[i].nQuietLoops = 0;
		m_Keyboard[i].bWasActive = FALSE;
		m_Keyboard[i].bSilenceLogged = FALSE;
	}

	for (unsigned i = 0; i < MAX_GAMEPADS; i++)
	{
		m_GamePad[i].nIndex = i + 1;
		m_GamePad[i].pGamePad = 0;
		m_GamePad[i].bHaveLast = FALSE;
		memset (&m_GamePad[i].LastState, 0, sizeof m_GamePad[i].LastState);
		m_GamePad[i].nReports = 0;
		m_GamePad[i].nReportsPrev = 0;
		m_GamePad[i].nQuietLoops = 0;
		m_GamePad[i].bWasActive = FALSE;
		m_GamePad[i].bSilenceLogged = FALSE;
	}

	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
	s_pThis = 0;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
	m_Logger.Write (FromKernel, LogNotice,
			"Combined keyboard + gamepad test: up to %u keyboards, %u gamepads",
			(unsigned) MAX_KEYBOARDS, (unsigned) MAX_GAMEPADS);
	m_Logger.Write (FromKernel, LogNotice,
			"One line is printed per connect, disconnect, key change or control change");

	LogOptions ();

	// Loop period is LOOP_PERIOD_MS, so 10 iterations == 1 second.
	for (unsigned nCount = 0; 1; nCount++)
	{
		// This must be called from TASK_LEVEL to update the tree of connected USB devices.
		boolean bUpdated = m_USBHCI.UpdatePlugAndPlay ();

		if (bUpdated)
		{
			TryBindKeyboards ();
			TryBindGamePads ();
		}

		// Picks up both new bindings and losses from the removed handlers.
		ReportDeviceListChanges ();

		// Update per-device "quiet" counters and warn once when a device that
		// was delivering reports falls silent.
		PollActivity ();

		// Steady "still alive" line every few seconds. When the USB stack
		// wedges this is the last line, or it keeps ticking while every
		// device report counter stays frozen - both are visible here.
		if (nCount % HEARTBEAT_LOOPS == 0)
		{
			Heartbeat (nCount / (1000 / LOOP_PERIOD_MS));
		}

		m_Screen.Rotor (0, nCount);
		m_Timer.MsDelay (LOOP_PERIOD_MS);
	}

	return ShutdownHalt;
}

void CKernel::LogOptions (void)
{
	const char *pIgnore = m_Options.GetUSBIgnore ();

	m_Logger.Write (FromKernel, LogNotice,
			"USB options: fullspeed %u, boost 0x%X, powerdelay %u, ignore '%s'",
			(unsigned) m_Options.GetUSBFullSpeed (),
			m_Options.GetUSBBoost (),
			m_Options.GetUSBPowerDelay (),
			pIgnore != 0 && *pIgnore != '\0' ? pIgnore : "(none)");
}

void CKernel::TryBindKeyboards (void)
{
	for (unsigned i = 0; i < MAX_KEYBOARDS; i++)
	{
		if (m_Keyboard[i].pKeyboard != 0)
		{
			continue;
		}

		CUSBKeyboardDevice *pKeyboard =
			(CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd", i + 1, FALSE);
		if (pKeyboard == 0)
		{
			continue;
		}

		m_Keyboard[i].bHaveLast = FALSE;
		m_Keyboard[i].ucLastModifiers = 0;
		memset (m_Keyboard[i].LastKeys, 0, sizeof m_Keyboard[i].LastKeys);
		m_Keyboard[i].nReports = 0;
		m_Keyboard[i].nReportsPrev = 0;
		m_Keyboard[i].nQuietLoops = 0;
		m_Keyboard[i].bWasActive = FALSE;
		m_Keyboard[i].bSilenceLogged = FALSE;
		m_Keyboard[i].pKeyboard = pKeyboard;

		pKeyboard->RegisterRemovedHandler (KeyboardRemovedHandler, &m_Keyboard[i]);
		pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw, FALSE, &m_Keyboard[i]);

		m_Logger.Write (FromKernel, LogNotice, "ukbd%u connected", i + 1);
	}
}

void CKernel::TryBindGamePads (void)
{
	for (unsigned i = 0; i < MAX_GAMEPADS; i++)
	{
		if (m_GamePad[i].pGamePad != 0)
		{
			continue;
		}

		CUSBGamePadDevice *pGamePad =
			(CUSBGamePadDevice *) m_DeviceNameService.GetDevice ("upad", i + 1, FALSE);
		if (pGamePad == 0)
		{
			continue;
		}

		m_GamePad[i].bHaveLast = FALSE;
		memset (&m_GamePad[i].LastState, 0, sizeof m_GamePad[i].LastState);
		m_GamePad[i].nReports = 0;
		m_GamePad[i].nReportsPrev = 0;
		m_GamePad[i].nQuietLoops = 0;
		m_GamePad[i].bWasActive = FALSE;
		m_GamePad[i].bSilenceLogged = FALSE;
		m_GamePad[i].pGamePad = pGamePad;

		const TGamePadState *pState = pGamePad->GetInitialState ();
		if (pState != 0)
		{
			m_Logger.Write (FromKernel, LogNotice,
					"upad%u connected: %d axes, %d hat(s), %d button(s)%s",
					i + 1, pState->naxes, pState->nhats, pState->nbuttons,
					   pState->naxes == 0 && pState->nhats == 0 && pState->nbuttons == 0
					? " - no controls, looks like a non-joystick HID mis-bound as a pad"
					: "");
		}
		else
		{
			// A non-joystick HID interface can still be handed to us as a pad.
			m_Logger.Write (FromKernel, LogWarning,
					"upad%u connected but GetInitialState() is null", i + 1);
		}

		pGamePad->RegisterRemovedHandler (GamePadRemovedHandler, &m_GamePad[i]);
		pGamePad->RegisterStatusHandler (GamePadStatusHandler);
	}
}

void CKernel::ReportDeviceListChanges (void)
{
	CString List;

	for (unsigned i = 0; i < MAX_KEYBOARDS; i++)
	{
		if (m_Keyboard[i].pKeyboard != 0)
		{
			CString Item;
			Item.Format ("%sukbd%u", List.GetLength () > 0 ? " " : "", i + 1);
			List.Append (Item);
		}
	}

	for (unsigned i = 0; i < MAX_GAMEPADS; i++)
	{
		if (m_GamePad[i].pGamePad != 0)
		{
			CString Item;
			Item.Format ("%supad%u", List.GetLength () > 0 ? " " : "", i + 1);
			List.Append (Item);
		}
	}

	if (List.GetLength () == 0)
	{
		List = "(none)";
	}

	if (m_DeviceList.Compare (List) != 0)
	{
		m_DeviceList = List;

		m_Logger.Write (FromKernel, LogNotice, "Now connected: %s", (const char *) List);
	}
}

void CKernel::PollActivity (void)
{
	const unsigned nWarnMs = SILENCE_WARN_LOOPS * LOOP_PERIOD_MS;

	for (unsigned i = 0; i < MAX_KEYBOARDS; i++)
	{
		TKeyboardInfo *pInfo = &m_Keyboard[i];
		if (pInfo->pKeyboard == 0)
		{
			continue;
		}

		unsigned nNow = pInfo->nReports;
		if (nNow != pInfo->nReportsPrev)
		{
			pInfo->nReportsPrev = nNow;
			pInfo->nQuietLoops = 0;
			pInfo->bWasActive = TRUE;
			pInfo->bSilenceLogged = FALSE;
		}
		else
		{
			pInfo->nQuietLoops++;
		}
		// A boot keyboard only reports on change, so silence is normal here -
		// no warning for keyboards, just the count/age in the heartbeat.
	}

	for (unsigned i = 0; i < MAX_GAMEPADS; i++)
	{
		TGamePadInfo *pInfo = &m_GamePad[i];
		if (pInfo->pGamePad == 0)
		{
			continue;
		}

		unsigned nNow = pInfo->nReports;
		if (nNow != pInfo->nReportsPrev)
		{
			pInfo->nReportsPrev = nNow;
			pInfo->nQuietLoops = 0;
			pInfo->bWasActive = TRUE;
			pInfo->bSilenceLogged = FALSE;
		}
		else
		{
			pInfo->nQuietLoops++;

			// A standard gamepad reports every poll interval, so once it has
			// started, a gap of seconds means its interrupt endpoint stopped.
			if (   pInfo->bWasActive
			    && !pInfo->bSilenceLogged
			    && pInfo->nQuietLoops >= SILENCE_WARN_LOOPS)
			{
				pInfo->bSilenceLogged = TRUE;

				m_Logger.Write (FromKernel, LogWarning,
						"upad%u: no reports for %ums (was active, %u total) - endpoint stalled?",
						pInfo->nIndex, nWarnMs, pInfo->nReportsPrev);
			}
		}
	}
}

void CKernel::Heartbeat (unsigned nUptimeSeconds)
{
	m_Logger.Write (FromKernel, LogNotice, "alive %us, connected: %s",
			nUptimeSeconds,
			m_DeviceList.GetLength () > 0 ? (const char *) m_DeviceList : "(none)");

	for (unsigned i = 0; i < MAX_KEYBOARDS; i++)
	{
		TKeyboardInfo *pInfo = &m_Keyboard[i];
		if (pInfo->pKeyboard == 0)
		{
			continue;
		}

		unsigned nAgeMs = pInfo->nQuietLoops * LOOP_PERIOD_MS;
		m_Logger.Write (FromKernel, LogNotice, "  ukbd%u: %u reports, last %u.%us ago",
				pInfo->nIndex, pInfo->nReportsPrev, nAgeMs / 1000, (nAgeMs % 1000) / 100);
	}

	for (unsigned i = 0; i < MAX_GAMEPADS; i++)
	{
		TGamePadInfo *pInfo = &m_GamePad[i];
		if (pInfo->pGamePad == 0)
		{
			continue;
		}

		unsigned nAgeMs = pInfo->nQuietLoops * LOOP_PERIOD_MS;
		m_Logger.Write (FromKernel, LogNotice, "  upad%u: %u reports, last %u.%us ago",
				pInfo->nIndex, pInfo->nReportsPrev, nAgeMs / 1000, (nAgeMs % 1000) / 100);
	}
}

void CKernel::KeyStatusHandlerRaw (unsigned char ucModifiers,
				   const unsigned char RawKeys[6], void *pArg)
{
	TKeyboardInfo *pInfo = (TKeyboardInfo *) pArg;
	assert (pInfo != 0);

	pInfo->nReports++;

	// A keyboard may report the same state repeatedly - only log on a change.
	if (   pInfo->bHaveLast
	    && pInfo->ucLastModifiers == ucModifiers
	    && memcmp (pInfo->LastKeys, RawKeys, sizeof pInfo->LastKeys) == 0)
	{
		return;
	}

	pInfo->ucLastModifiers = ucModifiers;
	memcpy (pInfo->LastKeys, RawKeys, sizeof pInfo->LastKeys);
	pInfo->bHaveLast = TRUE;

	CString Msg;
	Msg.Format ("ukbd%u: modifiers 0x%02X keys", pInfo->nIndex, (unsigned) ucModifiers);

	boolean bAnyKey = FALSE;
	for (unsigned i = 0; i < 6; i++)
	{
		if (RawKeys[i] != 0)
		{
			CString Key;
			Key.Format (" %02X", (unsigned) RawKeys[i]);
			Msg.Append (Key);
			bAnyKey = TRUE;
		}
	}

	if (!bAnyKey)
	{
		Msg.Append (" (none)");
	}

	CLogger::Get ()->Write (FromKernel, LogNotice, Msg);
}

void CKernel::KeyboardRemovedHandler (CDevice *pDevice, void *pContext)
{
	TKeyboardInfo *pInfo = (TKeyboardInfo *) pContext;
	assert (pInfo != 0);

	CLogger::Get ()->Write (FromKernel, LogNotice, "ukbd%u disconnected (%u reports total)",
				pInfo->nIndex, pInfo->nReports);

	pInfo->pKeyboard = 0;
	pInfo->bHaveLast = FALSE;
	pInfo->ucLastModifiers = 0;
	memset (pInfo->LastKeys, 0, sizeof pInfo->LastKeys);
	pInfo->bWasActive = FALSE;
	pInfo->bSilenceLogged = FALSE;
}

void CKernel::GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState)
{
	assert (s_pThis != 0);

	if (nDeviceIndex >= MAX_GAMEPADS)
	{
		return;
	}

	TGamePadInfo *pInfo = &s_pThis->m_GamePad[nDeviceIndex];

	pInfo->nReports++;

	// The gamepad reports on every poll interval - only log on a change.
	if (   pInfo->bHaveLast
	    && memcmp (&pInfo->LastState, pState, sizeof *pState) == 0)
	{
		return;
	}

	pInfo->LastState = *pState;
	pInfo->bHaveLast = TRUE;

	CString Msg;
	Msg.Format ("upad%u: buttons 0x%X", nDeviceIndex + 1, pState->buttons);

	CString Value;

	if (pState->naxes > 0)
	{
		Msg.Append (" axes");

		for (int i = 0; i < pState->naxes; i++)
		{
			Value.Format (" %d", pState->axes[i].value);
			Msg.Append (Value);
		}
	}

	if (pState->nhats > 0)
	{
		Msg.Append (" hats");

		for (int i = 0; i < pState->nhats; i++)
		{
			Value.Format (" %d", pState->hats[i]);
			Msg.Append (Value);
		}
	}

	CLogger::Get ()->Write (FromKernel, LogNotice, Msg);
}

void CKernel::GamePadRemovedHandler (CDevice *pDevice, void *pContext)
{
	TGamePadInfo *pInfo = (TGamePadInfo *) pContext;
	assert (pInfo != 0);

	CLogger::Get ()->Write (FromKernel, LogNotice, "upad%u disconnected (%u reports total)",
				pInfo->nIndex, pInfo->nReports);

	pInfo->pGamePad = 0;
	pInfo->bHaveLast = FALSE;
	memset (&pInfo->LastState, 0, sizeof pInfo->LastState);
	pInfo->bWasActive = FALSE;
	pInfo->bSilenceLogged = FALSE;
}
