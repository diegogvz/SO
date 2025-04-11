#include "MMU.h"
#include "Buses.h"
#include "Processor.h"
#include "Simulator.h"

// The base register
int registerBase_MMU;

// The limit register
int registerLimit_MMU;

// The MAR register
int registerMAR_MMU;

// The CTRL register
int registerCTRL_MMU;

// Logical address is in registerMAR_MMU. If correct, physical address is produced
// by adding logical address and base register
void MMU_SetCTRL(int ctrl) {
	registerCTRL_MMU = ctrl & 0x3;
	int logicalAddress = registerMAR_MMU;
	int valid = 0;

	if (Processor_PSW_BitState(EXECUTION_MODE_BIT)) {
		// Protected mode: valid if address ∈ [0, MAINMEMORYSIZE)
		valid = (logicalAddress >= 0 && logicalAddress < MAINMEMORYSIZE);
	} else {
		// User mode: valid if address ∈ [0, registerLimit_MMU)
		valid = (logicalAddress >= 0 && logicalAddress < registerLimit_MMU);
		if (valid) {
			// Translate logical to physical address
			registerMAR_MMU += registerBase_MMU;
		}
	}

	switch (registerCTRL_MMU) {
		case CTRLREAD:
		case CTRLWRITE:
			if (valid) {
				Buses_write_AddressBus_From_To(MMU, MAINMEMORY);
				Buses_write_ControlBus_From_To(MMU, MAINMEMORY);
				registerCTRL_MMU |= CTRL_SUCCESS;
			} else {
				registerCTRL_MMU |= CTRL_FAIL;
				Processor_RaiseException(INVALIDADDRESS);
			}
			break;

		default:
			registerCTRL_MMU |= CTRL_FAIL;
			Processor_RaiseException(INVALIDADDRESS);
			break;
	}

	Buses_write_ControlBus_From_To(MMU, CPU);
}


// Getter for registerCTRL_MMU
int MMU_GetCTRL () {
  return registerCTRL_MMU;
}

// Setter for registerMAR_MMU
void MMU_SetMAR (int newMAR) {
  registerMAR_MMU = newMAR;
}

// Getter for registerMAR_MMU
int MMU_GetMAR () {
  return registerMAR_MMU;
}

// Setter for registerBase_MMU
void MMU_SetBase (int newBase) {
  registerBase_MMU = newBase;
}

// Getter for registerBase_MMU
int MMU_GetBase () {
  return registerBase_MMU;
}

// Setter for registerLimit_MMU
void MMU_SetLimit (int newLimit) {
  registerLimit_MMU = newLimit;
}

// Getter for registerLimit_MMU
int MMU_GetLimit () {
  return registerLimit_MMU;
}
