#include "usb_transfer_window.h"

#ifdef _TINSPIRE
#include <libndls.h>

namespace {
bool transfer_window_active = false;
int previous_nucleus_interrupt_mask = 0;
bool previous_cpu_irq_masked = true;

void setCpuIrqMasked(const bool masked)
{
    unsigned int cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    if(masked)
        cpsr |= 0x80u;
    else
        cpsr &= ~0x80u;
    __asm__ volatile("msr cpsr_c, %0" : : "r"(cpsr) : "memory", "cc");
}
}
#endif

bool usbTransferWindowActive()
{
#ifdef _TINSPIRE
    return transfer_window_active;
#else
    return false;
#endif
}

bool enableUsbTransferWindow()
{
#ifdef _TINSPIRE
    if(transfer_window_active)
        return true;

    unsigned int cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    previous_cpu_irq_masked = (cpsr & 0x80u) != 0;

    // Ndless documents that this mask and the ARM CPSR IRQ bit are separate.
    // Save both states so Ctrl+Shift+Period can safely return to the game's
    // original interrupt-masked mode after the host has received the file.
    previous_nucleus_interrupt_mask = TCT_Local_Control_Interrupts(0);
    setCpuIrqMasked(false);
    transfer_window_active = true;
    return true;
#else
    return false;
#endif
}

void disableUsbTransferWindow()
{
#ifdef _TINSPIRE
    if(!transfer_window_active)
        return;

    // Keep the ARM CPU IRQ gate closed while restoring the Nucleus mask.
    // Reapply the previous CPU IRQ state only after both layers are consistent.
    setCpuIrqMasked(true);
    TCT_Local_Control_Interrupts(previous_nucleus_interrupt_mask);
    setCpuIrqMasked(previous_cpu_irq_masked);
    transfer_window_active = false;
#endif
}
