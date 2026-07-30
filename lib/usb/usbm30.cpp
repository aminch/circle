//
// usbm30.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
//
#include <circle/usb/usbm30.h>
#include <circle/usb/usbdevice.h>
#include <circle/logger.h>
#include <assert.h>

static const char FromM30[] = "usbm30";

CUSBM30Device::CUSBM30Device (CUSBFunction *pFunction)
:	CUSBFunction (pFunction)
{
}

CUSBM30Device::~CUSBM30Device (void)
{
}

boolean CUSBM30Device::Configure (void)
{
	if (!CUSBFunction::Configure ())
	{
		return FALSE;
	}

	// The receiver starts with Switch-compatible descriptors. Remaining
	// completely silent lets its host-detection timeout select 6B mode.
	CLogger::Get ()->Write (FromM30, LogNotice,
				  "M30 receiver detected; waiting for 6B re-enumeration");
	return TRUE;
}

boolean CUSBM30Device::IsM30Receiver (CUSBFunction *pFunction)
{
	assert (pFunction != 0);

	const TUSBConfigurationDescriptor *pConfigDesc =
		pFunction->GetDevice ()->GetConfigurationDescriptor ();
	assert (pConfigDesc != 0);

	const u8 *pDescriptor = (const u8 *) pConfigDesc;
	const u8 *pEnd = pDescriptor + pConfigDesc->wTotalLength;
	boolean bTargetInterface = FALSE;
	u8 ucInInterval = 0;
	u8 ucOutInterval = 0;

	for (; pDescriptor + 2 <= pEnd; pDescriptor += pDescriptor[0])
	{
		if (pDescriptor[0] < 2 || pDescriptor + pDescriptor[0] > pEnd)
		{
			return FALSE;
		}

		if (pDescriptor[1] == DESCRIPTOR_INTERFACE)
		{
			const TUSBInterfaceDescriptor *pInterface =
				(const TUSBInterfaceDescriptor *) pDescriptor;
			bTargetInterface =
				pInterface->bInterfaceNumber == pFunction->GetInterfaceNumber ();
			continue;
		}

		if (   !bTargetInterface
		    || pDescriptor[1] != DESCRIPTOR_ENDPOINT
		    || pDescriptor[0] != sizeof (TUSBEndpointDescriptor))
		{
			continue;
		}

		const TUSBEndpointDescriptor *pEndpoint =
			(const TUSBEndpointDescriptor *) pDescriptor;
		if ((pEndpoint->bmAttributes & 0x3F) != 0x03)
		{
			continue;
		}

		if (pEndpoint->bEndpointAddress & 0x80)
		{
			ucInInterval = pEndpoint->bInterval;
		}
		else
		{
			ucOutInterval = pEndpoint->bInterval;
		}
	}

	return ucInInterval == 4 && ucOutInterval == 10;
}