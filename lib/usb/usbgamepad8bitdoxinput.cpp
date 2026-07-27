//
// usbgamepad8bitdoxinput.cpp
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
#include <circle/usb/usbgamepad8bitdoxinput.h>

static const char FromUSBPad8BitDoXInput[] = "usbpad8bitdoxinput";

CUSBGamePad8BitDoXInputDevice::CUSBGamePad8BitDoXInputDevice (CUSBFunction *pFunction)
: CUSBGamePad8bitdoDevice (pFunction),
	m_bInterfaceOK (SelectInterfaceByClass (0xFF, 0x5D, 0x01, 1))
{
}

CUSBGamePad8BitDoXInputDevice::~CUSBGamePad8BitDoXInputDevice (void)
{
}

boolean CUSBGamePad8BitDoXInputDevice::Configure (void)
{
	if (!m_bInterfaceOK)
	{
		ConfigurationError (FromUSBPad8BitDoXInput);

		return FALSE;
	}

	if (!CUSBGamePad8bitdoDevice::Configure ())
	{
		return FALSE;
	}

	DMA_BUFFER(u8, Command, 3) = {0x01, 0x03, 0x02};
	if (!SendToEndpointOut(Command, 3)) {
		CLogger::Get ()->Write (FromUSBPad8BitDoXInput, LogError, 
                                "Cannot initialize receiver");
		return FALSE;
	}

	return StartRequest ();
}
