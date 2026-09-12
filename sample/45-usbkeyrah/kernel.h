//
// kernel.h
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
// Combined USB keyboard + gamepad test harness. Binds every ukbd1..ukbd4 and
// upad1..upad4 device that USB plug-and-play reports, and logs one line per
// event only - device connect/disconnect, a key change, a control change -
// instead of one line per USB poll.
//
// It also counts reports per device and prints a periodic heartbeat with each
// device's report count and time since its last report, so a device that
// stops responding (report count frozen) can be told apart from one that is
// simply idle (keyboards only report on change).
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/usb/usbgamepad.h>
#include <circle/types.h>

#define MAX_KEYBOARDS		4		// ukbd1 .. ukbd4
#define MAX_GAMEPADS		4		// upad1 .. upad4

#define LOOP_PERIOD_MS		100		// Run() loop period
#define HEARTBEAT_LOOPS		50		// -> heartbeat every 5 s
#define SILENCE_WARN_LOOPS	30		// warn once a device that was active goes quiet 3 s

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);

	TShutdownMode Run (void);

private:
	struct TKeyboardInfo
	{
		unsigned			nIndex;		// 1 .. MAX_KEYBOARDS
		CUSBKeyboardDevice * volatile	pKeyboard;	// 0 if unbound
		boolean				bHaveLast;
		unsigned char			ucLastModifiers;
		unsigned char			LastKeys[6];

		volatile unsigned		nReports;	// bumped from the raw handler (IRQ)
		unsigned			nReportsPrev;	// snapshot taken in PollActivity()
		unsigned			nQuietLoops;	// loops since nReports last moved
		boolean				bWasActive;	// delivered >= 1 report since connect
		boolean				bSilenceLogged;	// SILENCE_WARN_LOOPS message already printed
	};

	struct TGamePadInfo
	{
		unsigned			nIndex;		// 1 .. MAX_GAMEPADS
		CUSBGamePadDevice * volatile	pGamePad;	// 0 if unbound
		boolean				bHaveLast;
		TGamePadState			LastState;

		volatile unsigned		nReports;	// bumped from the status handler (IRQ), every poll
		unsigned			nReportsPrev;
		unsigned			nQuietLoops;
		boolean				bWasActive;
		boolean				bSilenceLogged;
	};

	void LogOptions (void);
	void TryBindKeyboards (void);
	void TryBindGamePads (void);
	void ReportDeviceListChanges (void);
	void PollActivity (void);			// update per-device quiet-loop counters, warn on new silence
	void Heartbeat (unsigned nUptimeSeconds);	// per-device report count + age

	static void KeyStatusHandlerRaw (unsigned char ucModifiers,
					 const unsigned char RawKeys[6], void *pArg);
	static void KeyboardRemovedHandler (CDevice *pDevice, void *pContext);

	static void GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState);
	static void GamePadRemovedHandler (CDevice *pDevice, void *pContext);

private:
	// do not change this order
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CScreenDevice		m_Screen;
	CSerialDevice		m_Serial;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CUSBHCIDevice		m_USBHCI;

	TKeyboardInfo		m_Keyboard[MAX_KEYBOARDS];
	TGamePadInfo		m_GamePad[MAX_GAMEPADS];
	CString			m_DeviceList;		// last logged set of connected devices

	static CKernel *s_pThis;
};

#endif
