//
// usbm30.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
//
#ifndef _circle_usb_usbm30_h
#define _circle_usb_usbm30_h

#include <circle/usb/usbfunction.h>

class CUSBM30Device : public CUSBFunction
{
public:
	CUSBM30Device (CUSBFunction *pFunction);
	~CUSBM30Device (void);

	boolean Configure (void);

	static boolean IsM30Receiver (CUSBFunction *pFunction);
};

#endif