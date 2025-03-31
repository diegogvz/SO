#include "Simulator.h"
#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateExecutingProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
void OperatingSystem_BLOCK_Process();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun(int);
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_MoveToTheBLOCKEDState();
int OperatingSystem_ExtractFromSleepingQueue();
void OperatingSystem_WakeUpProcesses();
void checkPriorityAfterWakeUp();

// The process table
// PCB processTable[PROCESSTABLEMAXSIZE];
PCB * processTable;

// Size of the memory occupied for the OS
int OS_MEMORY_SIZE=32;

// Address base for OS code in this version
int OS_address_base; 

// Identifier of the current executing process
int executingProcessID=NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation (Not assigned)
int initialPID=-1;

// Begin indes for daemons in programList
// int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes
 heapItem *readyToRunQueue[NUMBEROFQUEUES];
// int numberOfReadyToRunProcesses[0]={0};
int numberOfReadyToRunProcesses[NUMBEROFQUEUES]={0,0};
char * queueNames [NUMBEROFQUEUES]={"USER","DAEMONS"};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

//Exercise 1-e V2
int numberOfClockInterrupts=0;

// char DAEMONS_PROGRAMS_FILE[MAXFILENAMELENGTH]="teachersDaemons";

int MAINMEMORYSECTIONSIZE = 60;

extern int MAINMEMORYSIZE;

int PROCESSTABLEMAXSIZE = 4;

char * statesNames[5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"};

// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sorted by when to wakeup
heapItem *sleepingProcessesQueue;
int numberOfSleepingProcesses=0;


// Initial set of tasks of the OS
void OperatingSystem_Initialize(int programsFromFileIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	
// In this version, with original configuration of memory size (300) and number of processes (4)
// every process occupies a 60 positions main memory chunk 
// and OS code and the system stack occupies 60 positions 

	OS_address_base = MAINMEMORYSIZE - OS_MEMORY_SIZE;

	MAINMEMORYSECTIONSIZE = OS_address_base / PROCESSTABLEMAXSIZE;

	if (initialPID<0) // if not assigned in options...
		initialPID=PROCESSTABLEMAXSIZE-1; 
	
	// Space for the processTable
	processTable = (PCB *) malloc(PROCESSTABLEMAXSIZE*sizeof(PCB));
	
	// Space for the ready to run queues (one queue initially...)
	 for (i = 0; i < NUMBEROFQUEUES; i++) {
        readyToRunQueue[i] = Heap_create(PROCESSTABLEMAXSIZE);
    }

	sleepingProcessesQueue = Heap_create(PROCESSTABLEMAXSIZE);


	programFile=fopen("OperatingSystemCode", "r");
	if (programFile==NULL){
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE,99,SHUTDOWN,"FATAL ERROR: Missing Operating System!\n");
		exit(1);		
	}

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++){
		processTable[i].busy=0;
		processTable[i].programListIndex=-1;
		processTable[i].initialPhysicalAddress=-1;
		processTable[i].processSize=-1;
		processTable[i].copyOfSPRegister=-1;
		processTable[i].state=-1;
		processTable[i].priority=-1;
		processTable[i].copyOfPCRegister=-1;
		processTable[i].copyOfPSWRegister=0;
		processTable[i].programListIndex=-1;
		processTable[i].copyOfAccumulator=0;
		processTable[i].copyOfRegisterA=0;
		processTable[i].copyOfRegisterB=0;
		processTable[i].whenToWakeUp=-1;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+2);
		
	// Include in program list all user or system daemon processes
	OperatingSystem_PrepareDaemons(programsFromFileIndex);
	
	// Create all user processes from the information given in the command line
	int userPrograms = OperatingSystem_LongTermScheduler();
		
	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")
		&& processTable[sipID].state==READY) {
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE,99,SHUTDOWN,"FATAL ERROR: Missing SIP program!\n");
		exit(1);		
	}

	if(userPrograms==0){
		OperatingSystem_ReadyToShutdown();
	}

	// At least, one process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	Processor_SetSSP(MAINMEMORYSIZE-1);

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);

	if (executingProcessID == sipID) {
        Processor_SetSSP(MAINMEMORYSIZE - 1);
        Processor_PushInSystemStack(OS_address_base + 1);
        Processor_PushInSystemStack(Processor_GetPSW());
        executingProcessID = NOPROCESS;
        ComputerSystem_DebugMessage(TIMED_MESSAGE, 99, SHUTDOWN, "The system will shut down now...\n");
        return;
    }

	OperatingSystem_PrintStatus();
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler() {
  
	int PID, i,
		numberOfSuccessfullyCreatedProcesses=0;
	
	for (i=0; programList[i]!=NULL && i<PROGRAMSMAXNUMBER ; i++) {
		PID=OperatingSystem_CreateProcess(i);

		if (PID<0)
		{
			switch (PID)
			{
			case NOFREEENTRY:
				ComputerSystem_DebugMessage(NO_TIMED_MESSAGE,50,ERROR,programList[i]->executableName);
				break;
			case PROGRAMDOESNOTEXIST:
				ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 51, ERROR,programList[i]->executableName,"it does not exist");
				break;
			case PROGRAMNOTVALID:
			    ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 51, ERROR,programList[i]->executableName,"invalid priority or size");
				break;
			case TOOBIGPROCESS:
				ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 52, ERROR,programList[i]->executableName);
				break;
			default:
				break;
			}
			continue;
		}
		numberOfSuccessfullyCreatedProcesses++;
		if (programList[i]->type==USERPROGRAM) 
			numberOfNotTerminatedUserProcesses++;
		// Move process to the ready state
		ComputerSystem_DebugMessage(TIMED_MESSAGE,53, SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,statesNames[0],statesNames[1]);
		OperatingSystem_MoveToTheREADYState(PID);
	
	}
	if(numberOfSuccessfullyCreatedProcesses>0){
		OperatingSystem_PrintStatus();
	}

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();
	if (PID==NOFREEENTRY)
	{
		return NOFREEENTRY;
	}

	// Check if programFile exists
	programFile=fopen(executableProgram->executableName, "r");
	if(programFile==NULL) {return PROGRAMDOESNOTEXIST;}

	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(programFile);	

	if(processSize==PROGRAMNOTVALID){
		return PROGRAMNOTVALID;
	}

	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);
	
	if(processSize==PROGRAMNOTVALID){
		return PROGRAMNOTVALID;
	}
	
	// Obtain enough memory space
 	loadingPhysicalAddress=OperatingSystem_ObtainMainMemory(processSize, PID);
	if (loadingPhysicalAddress==TOOBIGPROCESS)
	{
		return PROGRAMNOTVALID;
	}
	

	// Load program in the allocated memory
	
	if (OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize)==TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	
	// Show message "Process [2] created into the [NEW] state, from program[progName]"
	ComputerSystem_DebugMessage(TIMED_MESSAGE,54,SYSPROC,PID,statesNames[0],executableProgram->executableName);
	
	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {

 	if (processSize>MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;
	
 	return PID*MAINMEMORYSECTIONSIZE;
}


// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {
    processTable[PID].busy = 1;
    processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
    processTable[PID].processSize = processSize;
    processTable[PID].copyOfSPRegister = initialPhysicalAddress + processSize;
    processTable[PID].state = NEW;
    processTable[PID].priority = priority;
    processTable[PID].programListIndex = processPLIndex;

    // Asignar la cola dependiendo del tipo de proceso
    if (programList[processPLIndex]->type == DAEMONPROGRAM) {
        processTable[PID].queueID = DAEMONSQUEUE;
        processTable[PID].copyOfPCRegister = initialPhysicalAddress;
        processTable[PID].copyOfPSWRegister = ((unsigned int)1) << EXECUTION_MODE_BIT;
    } else {
        processTable[PID].queueID = USERPROCESSQUEUE;
        processTable[PID].copyOfPCRegister = 0;
        processTable[PID].copyOfPSWRegister = 0;
    }
}



// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	int queueID = processTable[PID].queueID;
	
	if (Heap_add(PID, readyToRunQueue[queueID],QUEUE_PRIORITY ,&(numberOfReadyToRunProcesses[queueID]))>=0) {
		processTable[PID].state=READY;
	} 

}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess;

	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0) {
        selectedProcess = OperatingSystem_ExtractFromReadyToRun(USERPROCESSQUEUE);
    } else if (numberOfReadyToRunProcesses[DAEMONSQUEUE] > 0) {
        selectedProcess = OperatingSystem_ExtractFromReadyToRun(DAEMONSQUEUE);
    }
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun(int queueID) {
  
	int selectedProcess=NOPROCESS;

	selectedProcess=Heap_poll(readyToRunQueue[queueID],QUEUE_PRIORITY ,&(numberOfReadyToRunProcesses[queueID]));
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	ComputerSystem_DebugMessage(TIMED_MESSAGE,53, SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,statesNames[1],statesNames[2]);
	processTable[PID].state=EXECUTING;
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_PushInSystemStack(processTable[PID].copyOfPCRegister);
	Processor_PushInSystemStack(processTable[PID].copyOfPSWRegister);
	Processor_SetRegisterSP(processTable[PID].copyOfSPRegister);

	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);

	ComputerSystem_DebugMessage(TIMED_MESSAGE,53, SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[2],statesNames[1]);	
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_PopFromSystemStack();
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_PopFromSystemStack();
	
	// Save RegisterSP 
	processTable[PID].copyOfSPRegister=Processor_GetRegisterSP();

	processTable[PID].copyOfAccumulator=Processor_GetAccumulator();
}


// Exception management routine
void OperatingSystem_HandleException() {
  
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(TIMED_MESSAGE,71,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
	
	OperatingSystem_TerminateExecutingProcess();
	OperatingSystem_PrintStatus();
}

// All tasks regarding the removal of the executing process
void OperatingSystem_TerminateExecutingProcess() {
    ComputerSystem_DebugMessage(TIMED_MESSAGE, 53, SYSPROC, executingProcessID,
        programList[processTable[executingProcessID].programListIndex]->executableName,
        statesNames[2], statesNames[4]);

    processTable[executingProcessID].state = EXIT;

    // If the terminated process is SystemIdleProcess, shut down the system
    if (executingProcessID == sipID) {
        Processor_SetSSP(MAINMEMORYSIZE - 1);
        Processor_PushInSystemStack(OS_address_base + 1);
        Processor_PushInSystemStack(Processor_GetPSW());
        executingProcessID = NOPROCESS;
        ComputerSystem_DebugMessage(TIMED_MESSAGE, 99, SHUTDOWN, "The system will shut down now...\n");
        return;
    }

    Processor_SetSSP(Processor_GetSSP() + 2); // Unstack PC and PSW

    if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM) {
        numberOfNotTerminatedUserProcesses--;
    }

    // If no user processes remain, shutdown the system
    if (numberOfNotTerminatedUserProcesses == 0) {
        OperatingSystem_ReadyToShutdown();
    }

    // Select next process
    int selectedProcess = OperatingSystem_ShortTermScheduler();
    OperatingSystem_Dispatch(selectedProcess);
}


// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID;
	int queueID;
	int executingPriority;
	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterC();
	
	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			ComputerSystem_DebugMessage(TIMED_MESSAGE,72,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,Processor_GetRegisterA(),Processor_GetRegisterB());
			break;
		
		case SYSCALL_YIELD: 
			queueID = processTable[executingProcessID].queueID;  // Get executing process queue
    		executingPriority = processTable[executingProcessID].priority;

			// Check if there's another process in the same queue with the same priority
			if (numberOfReadyToRunProcesses[queueID] > 0) {
				int frontPID = readyToRunQueue[queueID][0].info;
				int frontPriority = processTable[frontPID].priority;

				// The process at the front must have the same priority
				if (frontPriority == executingPriority) {
					// Print message 55 - process relinquishing control
					ComputerSystem_DebugMessage(TIMED_MESSAGE, 55, SHORTTERMSCHEDULE,
						executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName,
						frontPID, programList[processTable[frontPID].programListIndex]->executableName);

					// Preempt and give CPU to the front process
					OperatingSystem_PreemptRunningProcess();
					int selectedProcess = OperatingSystem_ShortTermScheduler();
					OperatingSystem_Dispatch(selectedProcess);
					OperatingSystem_PrintStatus();
					return;
				}
			
    		}
			// No eligible process found, print message 56 - no transfer
			ComputerSystem_DebugMessage(TIMED_MESSAGE, 56, SHORTTERMSCHEDULE,
				executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
   
			break;
		case SYSCALL_END:
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			ComputerSystem_DebugMessage(TIMED_MESSAGE,73,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateExecutingProcess();
			OperatingSystem_PrintStatus();
			break;
		case SYSCALL_SLEEP:
			ComputerSystem_DebugMessage(TIMED_MESSAGE,53, SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[2],statesNames[3]);
			OperatingSystem_BLOCK_Process(Processor_GetRegisterD());
			int selectedProcess = OperatingSystem_ShortTermScheduler();
			OperatingSystem_Dispatch(selectedProcess);
			OperatingSystem_PrintStatus();
			break;
	}
}
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
		case CLOCKINT_BIT:
			OperatingSystem_HandleClockInterrupt();
			break;
	}

}

void OperatingSystem_PrintReadyToRunQueue() {
    // Print the header for the ready-to-run queues
    ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 108, SHORTTERMSCHEDULE);

    // Iterate through the queues
    for (int q = 0; q < NUMBEROFQUEUES; q++) {
        // Print queue name
        if (numberOfReadyToRunProcesses[q] == 0) {
            // If queue is empty, print only the name followed by ":"
            ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 109, SHORTTERMSCHEDULE, queueNames[q]);
            continue;
        }

        // Print queue name followed by first process
        int PID = readyToRunQueue[q][0].info;
        int priority = processTable[PID].priority;
        ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 111, SHORTTERMSCHEDULE, queueNames[q], PID, priority);

        // Print remaining processes in the queue
        for (int i = 1; i < numberOfReadyToRunProcesses[q]; i++) {
            PID = readyToRunQueue[q][i].info;
            priority = processTable[PID].priority;
            ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 112, SHORTTERMSCHEDULE, PID, priority);
        }

        // Print newline at the end of the queue list
        ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 113, SHORTTERMSCHEDULE);
    }
}

// In OperatingSystem.c Exercise 1-b of V2
void OperatingSystem_HandleClockInterrupt(){ 
	numberOfClockInterrupts++;
	ComputerSystem_DebugMessage(TIMED_MESSAGE,57,INTERRUPT,numberOfClockInterrupts);

	OperatingSystem_WakeUpProcesses();
	
} 
void OperatingSystem_WakeUpProcesses(){
	int first=Heap_getFirst(sleepingProcessesQueue,PROCESSTABLEMAXSIZE);
	int process = NOPROCESS;
	while (first != NOPROCESS && processTable[first].whenToWakeUp == numberOfClockInterrupts){
		ComputerSystem_DebugMessage(TIMED_MESSAGE,53, SYSPROC,first,programList[processTable[first].programListIndex]->executableName,statesNames[3],statesNames[1]);
		processTable[first].whenToWakeUp=-1;
		processTable[first].state = READY;
		process = OperatingSystem_ExtractFromSleepingQueue();
		OperatingSystem_MoveToTheREADYState(process);
		first = Heap_getFirst(sleepingProcessesQueue,PROCESSTABLEMAXSIZE);
		OperatingSystem_PrintStatus();

	}

	if (process!=NOPROCESS)
	{
		checkPriorityAfterWakeUp();
	}

	
}

void checkPriorityAfterWakeUp(){
	
	if(processTable[executingProcessID].queueID == USERPROCESSQUEUE){
		if(processTable[executingProcessID].priority > 
			processTable[Heap_getFirst(readyToRunQueue[processTable[executingProcessID].queueID],PROCESSTABLEMAXSIZE)].priority){
				int aux = Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE],PROCESSTABLEMAXSIZE);
				ComputerSystem_DebugMessage(TIMED_MESSAGE,58,SHORTTERMSCHEDULE,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,
					aux,programList[processTable[aux].programListIndex]->executableName);
				OperatingSystem_PreemptRunningProcess();
				int selectedProcess = OperatingSystem_ShortTermScheduler();
				OperatingSystem_Dispatch(selectedProcess);
				OperatingSystem_PrintStatus();
			}
	}
	else
	{
		if(Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE],PROCESSTABLEMAXSIZE) != NOPROCESS){
			int aux = Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE],PROCESSTABLEMAXSIZE);
			ComputerSystem_DebugMessage(TIMED_MESSAGE,58,SHORTTERMSCHEDULE,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,
				aux,programList[processTable[aux].programListIndex]->executableName);
			OperatingSystem_PreemptRunningProcess();
			int selectedProcess = OperatingSystem_ShortTermScheduler();
			OperatingSystem_Dispatch(selectedProcess);
			OperatingSystem_PrintStatus();
		}
		else{
			if(processTable[executingProcessID].priority > 
				processTable[Heap_getFirst(readyToRunQueue[DAEMONSQUEUE],PROCESSTABLEMAXSIZE)].priority){
				int aux = Heap_getFirst(readyToRunQueue[DAEMONSQUEUE],PROCESSTABLEMAXSIZE);
				ComputerSystem_DebugMessage(TIMED_MESSAGE,58,SHORTTERMSCHEDULE,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,
					aux,programList[processTable[aux].programListIndex]->executableName);

				OperatingSystem_PreemptRunningProcess();
				int selectedProcess = OperatingSystem_ShortTermScheduler();
				OperatingSystem_Dispatch(selectedProcess);
			}
		}
	}
}

void OperatingSystem_BLOCK_Process(){
	OperatingSystem_SaveContext(executingProcessID);
	OperatingSystem_MoveToTheBLOCKEDState(Processor_GetRegisterD());
	executingProcessID = NOPROCESS;
}

void OperatingSystem_MoveToTheBLOCKEDState(int regD) {
	
	processTable[executingProcessID].state = BLOCKED;
	if(regD>0){
		processTable[executingProcessID].whenToWakeUp = regD + numberOfClockInterrupts + 1;
	}
	else{
		processTable[executingProcessID].whenToWakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;
	}
		
	Heap_add(executingProcessID,sleepingProcessesQueue, QUEUE_WAKEUP ,&(numberOfSleepingProcesses));

}

// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromSleepingQueue() {
  
	int selectedProcess=NOPROCESS;

	selectedProcess=Heap_poll(sleepingProcessesQueue,QUEUE_WAKEUP ,&(numberOfSleepingProcesses));
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


