// V1
#include "Clock.h"
#include "Processor.h"
#include "ComputerSystemBase.h"
int tics=0;
void Clock_Update() {
	tics++;
	if((tics)%DEFAULT_INTERVAL_BETWEEN_INTERRUPTS==0){
		Processor_RaiseInterrupt(CLOCKINT_BIT);
	}
}


int Clock_GetTime() {

	return tics;
}
