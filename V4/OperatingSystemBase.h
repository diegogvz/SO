// V4-studentsCode
#ifndef OPERATINGSYSTEMBASE_H
#define OPERATINGSYSTEMBASE_H

#include "ComputerSystem.h"
#include "OperatingSystem.h"
#include "Heap.h"
#include <stdio.h>

// Prototypes of OS functions that students should not change
int OperatingSystem_ObtainAnEntryInTheProcessTable();
int OperatingSystem_ObtainProgramSize(FILE *);
int OperatingSystem_ObtainPriority(FILE *);
int OperatingSystem_LoadProgram(FILE *, int, int);
void OperatingSystem_ReadyToShutdown();
void OperatingSystem_PrepareDaemons(int);
int OperatingSystem_GetExecutingProcessID();
void OperatingSystem_PrintStatus();  // V2-studentsCode
void OperatingSystem_PrintReadyToRunQueue();  // V2-studentsCode
int OperatingSystem_IsThereANewProgram();	// V3-studentsCode
#ifdef MEMCONFIG
void OperatingSystem_InitializePartitionsAndHolesTable(int);	// V4-studentsCode
void OperatingSystem_ShowPartitionsAndHolesTable(char *);	// V4-studentsCode
int OperatingSystem_InsertIntopartitionsAndHolesTable(int, int, int, int); // V4-studentsCode
int OperatingSystem_RemovePartitionOrHole(int) ; // V4-studentsCode
#endif

#define EMPTYQUEUE -1
#define NO 0
#define YES 1

// These "extern" declaration enables other source code files to gain access
// to the variable listed
extern PCB * processTable;
extern int OS_address_base;
extern int sipID;
extern char DAEMONS_PROGRAMS_FILE[];
extern char USER_PROGRAMS_FILE[];

#ifdef SLEEPINGQUEUE
extern heapItem *sleepingProcessesQueue;  // V2-studentsCode
extern int numberOfSleepingProcesses;   // V2-studentsCode
#endif

#ifdef ARRIVALQUEUE
extern int numberOfProgramsInArrivalTimeQueue;	// V3-studentsCode
extern heapItem * arrivalTimeQueue;				// V3-studentsCode
#endif

#ifdef MEMCONFIG // V4-studentsCode

#define HOLE -100   // V4-studentsCode

typedef struct {	// V4-studentsCode
     int initAddress; // Lowest physical address of the partition
     int size; // Size of the partition in memory positions
     int PID; // PID of the process using the partition, or HOLE if it's free
} PARTITIONDATA;

extern int PARTITIONSANDHOLESTABLEMAXSIZE;        // V4-studentsCode
extern PARTITIONDATA *partitionsAndHolesTable;	// V4-studentsCode
extern int numberOfPartitionsAndHoles;            // V4-studentsCode
#endif    // V4-studentsCode

#endif
