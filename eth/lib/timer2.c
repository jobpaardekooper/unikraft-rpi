//
// timer.c
//
// USPi - An USB driver for Raspberry Pi written in C
// Copyright (C) 2014-2015  R. Stange <rsta2@o2online.de>
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
#include <uspienv/timer.h>
#include <uspienv/bcm2835.h>
#include <uspienv/memio.h>
#include <uspienv/synchronize.h>
#include <uspienv/sysconfig.h>
#include <uspienv/logger.h>
#include <uspienv/debug.h>
#include <uk/assert.h>
#include <stdlib.h>
#include <raspi/irq.h>
#include <uspios.h>

// void DelayLoop (unsigned nCount);
void TimerPollKernelTimers (TTimer *pThis);
void TimerInterruptHandler (void *pParam);
void TimerTuneMsDelay (TTimer *pThis);

static TTimer *s_pThis = 0;

void Timer (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	pThis->m_nTicks = 0;
	pThis->m_nTime = 0;
#ifdef ARM_DISABLE_MMU
	pThis->m_nMsDelay = 12500;
#else
	pThis->m_nMsDelay = 350000;
#endif
	pThis->m_nusDelay = pThis->m_nMsDelay / 1000;

	UK_ASSERT (s_pThis == 0);
	s_pThis = pThis;

	for (unsigned hTimer = 0; hTimer < KERNEL_TIMERS; hTimer++)
	{
		pThis->m_KernelTimer[hTimer].m_pHandler = 0;
	}
}

void _Timer (TTimer *pThis)
{
	s_pThis = 0;
}

boolean TimerInitialize (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	int rc = ukplat_irq_register(IRQ_ID_RASPI_ARM_SYSTEM_TIMER_IRQ_3, TimerInterruptHandler, pThis);
	if (rc < 0)
		UK_CRASH("Failed to register timer IRQ 3 interrupt handler\n");

	InterruptSystemEnableIRQ(3);

	DataMemBarrier ();

	write32 (ARM_SYSTIMER_CLO, -(30 * CLOCKHZ));	// timer wraps soon, to check for problems

	write32 (ARM_SYSTIMER_C3, read32 (ARM_SYSTIMER_CLO) + CLOCKHZ / HZ);
	
	TimerTuneMsDelay (pThis);

	DataMemBarrier ();

	return TRUE;
}

unsigned TimerGetClockTicks (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	DataMemBarrier ();

	unsigned nResult = read32 (ARM_SYSTIMER_CLO);

	DataMemBarrier ();

	return nResult;
}

unsigned TimerGetTicks (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	return pThis->m_nTicks;
}

unsigned TimerGetTime (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	return pThis->m_nTime;
}

TString *TimerGetTimeString (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	uspi_EnterCritical ();

	unsigned nTime = pThis->m_nTime;
	unsigned nTicks = pThis->m_nTicks;

	uspi_LeaveCritical ();

	if (nTicks == 0)
	{
		return 0;
	}

	unsigned nSecond = nTime % 60;
	nTime /= 60;
	unsigned nMinute = nTime % 60;
	nTime /= 60;
	unsigned nHours = nTime;

	nTicks %= HZ;
#if (HZ != 100)
	nTicks = nTicks * 100 / HZ;
#endif

	TString *pString = malloc (sizeof (TString));
	UK_ASSERT (pString != 0);
	String (pString);

	StringFormat (pString, "%02u:%02u:%02u.%02lu", nHours, nMinute, nSecond, nTicks);

	return pString;
}

unsigned TimerStartKernelTimer (TTimer *pThis, unsigned nDelay, TKernelTimerHandler *pHandler, void *pParam, void *pContext)
{
	UK_ASSERT (pThis != 0);

	uspi_EnterCritical ();

	unsigned hTimer;
	for (hTimer = 0; hTimer < KERNEL_TIMERS; hTimer++)
	{
		if (pThis->m_KernelTimer[hTimer].m_pHandler == 0)
		{
			break;
		}
	}

	if (hTimer >= KERNEL_TIMERS)
	{
		uspi_LeaveCritical ();

		LoggerWrite (LoggerGet (), LogPanic, "System limit of kernel timers exceeded");

		return 0;
	}

	UK_ASSERT (pHandler != 0);
	pThis->m_KernelTimer[hTimer].m_pHandler    = pHandler;
	pThis->m_KernelTimer[hTimer].m_nElapsesAt  = pThis->m_nTicks+nDelay;
	pThis->m_KernelTimer[hTimer].m_pParam      = pParam;
	pThis->m_KernelTimer[hTimer].m_pContext    = pContext;

	uspi_LeaveCritical ();

	return hTimer+1;
}

void TimerCancelKernelTimer (TTimer *pThis, unsigned hTimer)
{
	UK_ASSERT (pThis != 0);

	UK_ASSERT (1 <= hTimer && hTimer <= KERNEL_TIMERS);
	pThis->m_KernelTimer[hTimer-1].m_pHandler = 0;
}

// void TimerMsDelay (TTimer *pThis, unsigned nMilliSeconds)
// {
// 	UK_ASSERT (pThis != 0);

// 	if (nMilliSeconds > 0)
// 	{
// 		unsigned nCycles =  pThis->m_nMsDelay * nMilliSeconds;

// 		DelayLoop (nCycles);
// 	}
// }

// void TimerusDelay (TTimer *pThis, unsigned nMicroSeconds)
// {
// 	UK_ASSERT (pThis != 0);

// 	if (nMicroSeconds > 0)
// 	{
// 		unsigned nCycles =  pThis->m_nusDelay * nMicroSeconds;

// 		DelayLoop (nCycles);
// 	}
// }

TTimer *TimerGet (void)
{
	UK_ASSERT (s_pThis != 0);
	return s_pThis;
}

void TimerSimpleMsDelay (unsigned nMilliSeconds)
{
	if (nMilliSeconds > 0)
	{
		TimerSimpleusDelay (nMilliSeconds * 1000);
	}
}

void TimerSimpleusDelay (unsigned nMicroSeconds)
{
	if (nMicroSeconds > 0)
	{
		unsigned nTicks = nMicroSeconds * (CLOCKHZ / 1000000);

		DataMemBarrier ();

		unsigned nStartTicks = read32 (ARM_SYSTIMER_CLO);
		while (read32 (ARM_SYSTIMER_CLO) - nStartTicks < nTicks)
		{
			// do nothing
		}

		DataMemBarrier ();
	}
}

void TimerPollKernelTimers (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	uspi_EnterCritical ();

	for (unsigned hTimer = 0; hTimer < KERNEL_TIMERS; hTimer++)
	{
		volatile TKernelTimer *pTimer = &pThis->m_KernelTimer[hTimer];

		TKernelTimerHandler *pHandler = pTimer->m_pHandler;
		if (pHandler != 0)
		{
			if ((int) (pTimer->m_nElapsesAt - pThis->m_nTicks) <= 0)
			{
				pTimer->m_pHandler = 0;

				(*pHandler) (hTimer+1, pTimer->m_pParam, pTimer->m_pContext);
			}
		}
	}

	uspi_LeaveCritical ();
}

void TimerInterruptHandler (void *pParam)
{
	TTimer *pThis = (TTimer *) pParam;
	UK_ASSERT (pThis != 0);

	DataMemBarrier ();

	UK_ASSERT (read32 (ARM_SYSTIMER_CS) & (1 << 3));
	
	u32 nCompare = read32 (ARM_SYSTIMER_C3) + CLOCKHZ / HZ;
	write32 (ARM_SYSTIMER_C3, nCompare);
	if (nCompare < read32 (ARM_SYSTIMER_CLO))			// time may drift
	{
		nCompare = read32 (ARM_SYSTIMER_CLO) + CLOCKHZ / HZ;
		write32 (ARM_SYSTIMER_C3, nCompare);
	}

	write32 (ARM_SYSTIMER_CS, 1 << 3);

	DataMemBarrier ();

	if (++pThis->m_nTicks % HZ == 0)
	{
		pThis->m_nTime++;
	}

	TimerPollKernelTimers (pThis);
}

void TimerTuneMsDelay (TTimer *pThis)
{
	UK_ASSERT (pThis != 0);

	unsigned nTicks = TimerGetTicks (pThis);
	MsDelay (1000);
	nTicks = TimerGetTicks (pThis) - nTicks;

	unsigned nFactor = 100 * HZ / nTicks;

	pThis->m_nMsDelay = pThis->m_nMsDelay * nFactor / 100;
	pThis->m_nusDelay = (pThis->m_nMsDelay + 500) / 1000;

	//LoggerWrite (LoggerGet (), "timer", LogNotice, "SpeedFactor is %u.%02u", nFactor / 100, nFactor % 100);
}
