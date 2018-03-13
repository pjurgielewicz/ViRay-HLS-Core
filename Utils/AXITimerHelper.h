#ifndef AXITIMERHELPER__H_
#define AXITIMERHELPER__H_

#include "xil_types.h"
#include "xtmrctr.h"
#include "xparameters.h"

class CAXITimerHelper
{
public:
	CAXITimerHelper(unsigned id, unsigned freq)
	{
		XTmrCtr_Initialize(&axiTimer, id);
		
		timerClockFrequency = freq;
		clockPeriodSeconds = (float)1.0/timerClockFrequency;
	}
	virtual ~CAXITimerHelper() 
	{
	}
	
	unsigned GetElapsedTicks()
	{
		return tickCounter2 - tickCounter1;
	}

	float GetElapsedSeconds()
	{
		return (float)(tickCounter2 - tickCounter1) * clockPeriodSeconds;
	}

	unsigned StartTimer()
	{
		XTmrCtr_Reset(&axiTimer, 0);
		tickCounter1 = XTmrCtr_GetValue(&axiTimer, 0);
		XTmrCtr_Start(&axiTimer, 0);
		
		return tickCounter1;
	}

	unsigned StopTimer()
	{
		XTmrCtr_Stop(&axiTimer, 0);
		tickCounter2 = XTmrCtr_GetValue(&axiTimer, 0);
		return tickCounter2 - tickCounter1;
	}

	float GetClockPeriod()
	{
		return clockPeriodSeconds;
	}

	float GetTimerClockFrequency()
	{
		return timerClockFrequency;
	}
	
private:
	XTmrCtr 	axiTimer;
	unsigned 	tickCounter1;
	unsigned 	tickCounter2;
	float 		clockPeriodSeconds;
	float 		timerClockFrequency;
};

#endif
