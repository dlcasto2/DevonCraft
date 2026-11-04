#include "platform_time.h"

#ifdef _TINSPIRE

#include <libndls.h>

// The CX and original monochrome ClickPad use different timer register
// layouts. Keep the CX SP804 path separate from the classic timer path.
//
// Classic timer reference: Ndless' classic get_ticks backend uses the
// 0x900C0000 counter, 0x900C0004 divider and 0x900C0008 control registers.
// The counter is sampled and cleared so elapsed ticks can be accumulated
// without depending on an interrupt handler.
static volatile uint32_t *const timer_load    = reinterpret_cast<volatile uint32_t*>(0x900C0000);
static volatile uint32_t *const timer_value   = reinterpret_cast<volatile uint32_t*>(0x900C0004);
static volatile uint32_t *const timer_control = reinterpret_cast<volatile uint32_t*>(0x900C0008);
static volatile uint32_t *const timer_clock   = reinterpret_cast<volatile uint32_t*>(0x900C0080);
static volatile uint32_t *const clock_gates   = reinterpret_cast<volatile uint32_t*>(0x900B0018);
static volatile uint32_t *const rtc_seconds   = reinterpret_cast<volatile uint32_t*>(0x90090000);

// CX timer state.
static uint32_t saved_load, saved_control, saved_clock;

// Classic timer state. On the classic model, 0x900C0000 is the counter
// itself, not the CX SP804 LOAD register.
static uint32_t classic_saved_counter;
static uint32_t classic_saved_divider;
static uint32_t classic_saved_control;
static uint32_t classic_tick_sum;
static bool classic_timer_active;

void platformTimeInit()
{
    if(is_classic)
    {
        classic_saved_counter = *timer_load;
        classic_saved_divider = *timer_value;
        classic_saved_control = *timer_control;
        classic_tick_sum = 0;

        // Classic timer 0: free-running upward counter, with a 32768 Hz
        // time base. Ndless uses divider 1 and control 0x0F for this timer.
        // Bit 4 freezes the classic timer while its registers are changed.
        *timer_control = 0x10;
        *timer_value = 1;
        *timer_load = 0;
        *timer_control = 0x0F;
        classic_timer_active = true;
        return;
    }

    saved_load = *timer_load;
    saved_control = *timer_control;
    saved_clock = *timer_clock;

    *clock_gates &= ~(1u << 11);  // open the timer's clock gate (CX)
    *timer_clock = 0xA;           // 32768 Hz (CX)
    *timer_control = 0;           // stop it while reprogramming
    *timer_load = 0xFFFFFFFF;
    *timer_control = 0xC2;        // enabled, periodic, 32-bit, no interrupt
    classic_timer_active = false;
}

void platformTimeDeinit()
{
    if(classic_timer_active)
    {
        // Freeze, restore the original divider/counter, then restore control.
        *timer_control = 0x10;
        *timer_value = classic_saved_divider;
        *timer_load = classic_saved_counter;
        *timer_control = classic_saved_control;
        classic_timer_active = false;
        return;
    }

    *timer_control = 0;
    *timer_load = saved_load;
    *timer_clock = saved_clock;
    *timer_control = saved_control;
    // The CX clock gate stays open: closing it again could stop timers the OS uses.
}

uint32_t platformTicks()
{
    if(classic_timer_active)
    {
        // The classic counter is cleared after each sample; accumulate its
        // elapsed counts so the public counter remains monotonic modulo 2^32.
        classic_tick_sum += *timer_load;
        *timer_load = 0;
        return classic_tick_sum;
    }

    // CX SP804 counts down; flip it so it counts up.
    return 0xFFFFFFFF - *timer_value;
}

uint32_t platformTickRate()
{
    return 32768;
}

uint32_t platformRtcSeconds()
{
    return *rtc_seconds;
}

#else

#include <ctime>
#include <SDL/SDL.h>

void platformTimeInit() {}
void platformTimeDeinit() {}

uint32_t platformTicks()
{
    return SDL_GetTicks();
}

uint32_t platformTickRate()
{
    return 1000;
}

uint32_t platformRtcSeconds()
{
    return static_cast<uint32_t>(time(nullptr));
}

#endif
