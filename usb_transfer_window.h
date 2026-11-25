#ifndef USB_TRANSFER_WINDOW_H
#define USB_TRANSFER_WINDOW_H

// Temporarily allow the TI-Nspire OS USB/file-transfer service to run while
// DevonCraft stays active. The mode is opt-in and restores the prior IRQ state
// when disabled or when the game exits.
bool usbTransferWindowActive();
bool enableUsbTransferWindow();
void disableUsbTransferWindow();

#endif // USB_TRANSFER_WINDOW_H
