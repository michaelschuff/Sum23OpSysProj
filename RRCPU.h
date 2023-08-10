#ifndef RRCPU_H
#define RRCPU_H

#include "RRprocess.h"
#include <math.h>
#include <numeric>
#include <climits>

extern unsigned long CUTOFF;


class RRCPU {
public:
	//INFO
	vector<RRProcess*> processes;
	int ctxSwitchTime;


	//STATE
	unsigned long time=0;
	int readyQCounter = 0;


	//LOCATIONS
	priority_queue<RRProcess*,vector<RRProcess*>,RRArrivalTimeCompare> incoming;
	priority_queue<RRProcess*,vector<RRProcess*>,RRCompare> readyQ;
	priority_queue<RRProcess*, vector<RRProcess*>,RRIOBurstTimeCompare> IOBursts;
	RRProcess* cpu = NULL;
	RRProcess* cpuOut = NULL;
	int ctxOutTime = INT_MAX;
	RRProcess* cpuIn = NULL;
	int ctxInTime = INT_MAX;




	//METRICS
	int numIOCTXSwitches = 0;
	int numCPUCTXSwitches = 0;
	int numPreemptions = 0;

	int numCPUBoundProcesses = 0;
	int numIOBoundProcesses = 0;

	int cpuRunning = 0;



	RRCPU(vector<RRProcess*> procs, int switchTime) {
		ctxSwitchTime = switchTime;
		for (size_t i = 0; i < procs.size(); i++) {
			incoming.push(procs[i]);
			if (procs[i]->isCPUBound)
				numCPUBoundProcesses+=procs[i]->totalCPUBursts;
			else
				numIOBoundProcesses+=procs[i]->totalCPUBursts;
		}

		processes = procs;
	}

	int getNextEvent() {
		if (cpu == NULL && cpuIn == NULL && cpuOut == NULL && !readyQ.empty()) {
			int flag = 5;
			return flag;
		}


		int CPU_TIME = INT_MAX;
		if (cpu != NULL)
			CPU_TIME = cpu->nextFinish();

		int CPU_OUT_TIME = INT_MAX;
		if (cpuOut != NULL)
			CPU_OUT_TIME = ctxOutTime;

		int CPU_IN_TIME = INT_MAX;
		if (cpuIn != NULL)
			CPU_IN_TIME = ctxInTime;
		


		int IO_FINISH = INT_MAX;
		if (!IOBursts.empty())
			IO_FINISH = IOBursts.top()->nextFinish();
		

		int INCOMING_FINISH = INT_MAX;
		if (!incoming.empty())
			INCOMING_FINISH = incoming.top()->arrivalTime;
		

		int min = INT_MAX;
		int flag = -1;


		if (CPU_TIME < min) {
			min = CPU_TIME;
			flag = 0;
		} 

		if (CPU_OUT_TIME < min) {
			min = CPU_OUT_TIME;
			flag = 1;
		} 
		if (CPU_IN_TIME < min) {
			min = CPU_IN_TIME;
			flag = 2;
		} 
		if (IO_FINISH < min) {
			min = IO_FINISH;
			flag = 3;
		} 
		if (INCOMING_FINISH < min) {
			min = INCOMING_FINISH;
			flag = 4;
		}

		return flag;

	}


	void run() {
		printf("time %ldms: Simulator started for RR ",time);
		printReady();
		while (!readyQ.empty() || !incoming.empty() || !IOBursts.empty() || cpu!=NULL || cpuIn!=NULL || cpuOut!=NULL ) {
			int flag = getNextEvent();
			// if (time < CUTOFF) printf("flag: %d\n",flag);

			// if (time < CUTOFF) printf("iobursts: ");
			// if (time < CUTOFF) printQueue(IOBursts);
			// if (time < CUTOFF) printf("incoming: ");
			// if (time < CUTOFF) printQueue(incoming);
			// if (time < CUTOFF) printf("ready: ");
			// if (time < CUTOFF) printQueue(readyQ);
			// if (time < CUTOFF) printf("cpu: ");
			// if (time < CUTOFF)  {
			// 	if (cpu == NULL) {
			// 		printf("null\n");
			// 	} else { 
			// 		printf("%c\n", (char)cpu->ID+65);
			// 	} 
			// }
			if (flag == 0) {
				// CPU FINISH
				elapseTime(cpu->nextFinish(),flag);

				//UPDATE METRICS
				if (cpu->isCPUBound)
					numCPUCTXSwitches++;
				else
					numIOCTXSwitches++;



				if (cpu->shouldTerminate()) {
					printTime();
					printf("Process %c terminated ", idtoc(cpu->ID));
					printReady();
					cpu->elapseTurnaroundTime(ctxSwitchTime/2);
				} else {
					cpuOut = cpu;
					ctxOutTime = ctxSwitchTime/2;
					if (time < CUTOFF) printTime();
					if (time < CUTOFF) printf("Process %c completed a CPU burst; %d bursts to go ", idtoc(cpu->ID), cpu->totalCPUBursts - cpu->completedCPUBursts);
					if (time < CUTOFF) printReady();
					if (time < CUTOFF) printTime();
					if (time < CUTOFF) printf("Process %c switching out of CPU; blocking on I/O until time %ldms ", idtoc(cpu->ID), cpu->nextFinish() + time + ctxSwitchTime/2);
					if (time < CUTOFF) printReady();
				}
				cpu = NULL;
				
			} 
			else if (flag == 1) {
				elapseTime(ctxOutTime,flag);

				if (!cpuOut->shouldTerminate())
					IOBursts.push(cpuOut);

				ctxOutTime = INT_MAX;
				cpuOut = NULL;
			} 
			else if (flag == 2) {
				elapseTime(ctxInTime,flag);

				cpu = cpuIn;
				cpuIn = NULL;
				ctxInTime = INT_MAX;

				cpu->priority = 0;

				if (time < CUTOFF) printTime();
				if (time < CUTOFF) printf("Process %c started using the CPU for %dms burst; ", idtoc(cpu->ID), cpu->nextFinish());
				if (time < CUTOFF) printReady();
			} 
			else if (flag == 3) {
				RRProcess* p = IOBursts.top();
				int t = p->nextFinish();
				IOBursts.pop();
				p->elapseTime(t);
				elapseTime(t,flag);

				p->priority = readyQCounter;
				readyQCounter++;
				readyQ.push(p);
				if (time < CUTOFF) printTime();
				if (time < CUTOFF) printf("Process %c completed I/O; added to ready queue ", idtoc(p->ID));
				if (time < CUTOFF) printReady();
			} 
			else if (flag == 4) {
				RRProcess* p = incoming.top();
				incoming.pop();
				int t = p->arrivalTime;
				p->elapseTime(t);
				elapseTime(t,flag);

				readyQ.push(p);
				if (time < CUTOFF) printTime();
				if (time < CUTOFF) printf("Process %c arrived; added to ready queue ", idtoc(p->ID));
				if (time < CUTOFF) printReady();
			} 
			else if (flag == 5) {
				RRProcess* p = readyQ.top();
				readyQ.pop();
				cpuIn = p;
				ctxInTime = ctxSwitchTime / 2;
			}
			// if (time < CUTOFF) printf("\n");
		}

		printTime();
		printf("Simulator ended for RR ");
		printReady();

		/*
		Algorithm RR
		-- CPU utilization: 84.253%
		-- average CPU burst time: 3067.776 ms (4071.000 ms/992.138 ms)
		-- average wait time: 779.663 ms (217.284 ms/1943.207 ms)
		-- average turnaround time: 3851.439 ms (4292.284 ms/2939.345 ms)
		-- number of context switches: 89 (60/29)
		-- number of preemptions: 0 (0/0)
		*/

		long CPUBOUND_cpu_burst_time = 0;
		long IOBOUND_cpu_burst_time = 0;
		for (size_t i = 0; i < processes.size(); i++) {
			if (processes[i]->isCPUBound)
				CPUBOUND_cpu_burst_time += processes[i]->time_using_cpu;
			else
				IOBOUND_cpu_burst_time += processes[i]->time_using_cpu;
		}

		long CPU_turnaround = 0;
		long IO_turnaround = 0;
		for (size_t i = 0; i < processes.size(); i++) {
			if (processes[i]->isCPUBound)
				CPU_turnaround += processes[i]->total_turnaround_time;
			else
				IO_turnaround += processes[i]->total_turnaround_time;
		}

		long CPU_wait = 0;
		long IO_wait = 0;
		for (size_t i = 0; i < processes.size(); i++) {
			if (processes[i]->isCPUBound)
				CPU_wait += processes[i]->total_wait_time;
			else
				IO_wait += processes[i]->total_wait_time;
		}


		printf("Algorithm RR\n");
		printf("-- CPU utilization: %.3f%%\n",100.0 * cpuRunning / time);
		printf("-- average CPU burst time: %.3f ms (%.3f ms/%.3f ms)\n",(IOBOUND_cpu_burst_time + CPUBOUND_cpu_burst_time)/(double)(numIOBoundProcesses+numCPUBoundProcesses),CPUBOUND_cpu_burst_time/(double)numCPUBoundProcesses,IOBOUND_cpu_burst_time/(double)numIOBoundProcesses);
		printf("-- average wait time: %.3f ms (%.3f ms/%.3f ms)\n",(CPU_wait + IO_wait)/(double)(numIOBoundProcesses+numCPUBoundProcesses),CPU_wait/(double)numCPUBoundProcesses,IO_wait/(double)numIOBoundProcesses);
		printf("-- average turnaround time: %.3f ms (%.3f ms/%.3f ms)\n",(CPU_turnaround + IO_turnaround)/(double)(numIOBoundProcesses+numCPUBoundProcesses),CPU_turnaround/(double)numCPUBoundProcesses,IO_turnaround/(double)numIOBoundProcesses);
		printf("-- number of context switches: %d (%d/%d)\n",numIOCTXSwitches+numCPUCTXSwitches,numCPUCTXSwitches,numIOCTXSwitches);
		printf("-- number of preemptions: 0 (0/0)\n");

		

	}
	void elapseTime(int t, int flag) {
		time += t;
		elapseTimeCPU(t);
		elapseTimeIO(t);
		elapseTimeIncoming(t);
		elapseWaitTimeReady(t);
		if (cpu != NULL)
			cpuRunning += t;
		
		if (flag == 0) {
			// CPU FINISH
			elapseTurnaroundTime(t);
		} 
		else if (flag == 1) {
			// CPU OUT
			elapseTurnaroundTime(t);
		} 
		else if (flag == 2) {
			// CPU IN
			elapseTurnaroundTime(t);
		} 
		else if (flag == 3) {
			// IOBurst finish
			elapseTurnaroundTime(t);
		} 
		else if (flag == 4) {
			// incoming finish
			elapseTurnaroundTime(t);
		}
		// else if (flag == 5) {

		// }
	}



	void elapseTimeIO(int t) {
		vector<RRProcess*> procs;
		while (!IOBursts.empty()) {
			RRProcess* p = IOBursts.top();
			IOBursts.pop();
			p->elapseTime(t);
			procs.push_back(p);
		}
		for (size_t i = 0; i < procs.size(); i++) {
			IOBursts.push(procs[i]);
		}
	}
	void elapseTimeIncoming(int t) {
		vector<RRProcess*> procs;
		while (!incoming.empty()) {
			RRProcess* p = incoming.top();
			incoming.pop();
			p->elapseTime(t);
			procs.push_back(p);
		}
		for (size_t i = 0; i < procs.size(); i++) {
			incoming.push(procs[i]);
		}
	}
	void elapseTimeCPU(int t) {
		if (cpuOut != NULL) {
			ctxOutTime -= t;
		}

		if (cpu != NULL) {
			cpu->elapseTime(t);
			cpu->time_using_cpu += t;
		}

		if (cpuIn != NULL)
			ctxInTime -= t;
	}
	void elapseWaitTimeReady(int t) {
		vector<RRProcess*> procs;
		while (!readyQ.empty()) {
			RRProcess* p = readyQ.top();
			readyQ.pop();
			p->elapseWaitTime(t);
			procs.push_back(p);
		}
		for (size_t i = 0; i < procs.size(); i++) {
			readyQ.push(procs[i]);
		}
	}
	void elapseTurnaroundTime(int t) {
		vector<RRProcess*> procs;
		while (!readyQ.empty()) {
			RRProcess* p = readyQ.top();
			readyQ.pop();
			p->elapseTurnaroundTime(t);
			procs.push_back(p);
		}
		for (size_t i = 0; i < procs.size(); i++) {
			readyQ.push(procs[i]);
		}


		if (cpuOut != NULL)
			cpuOut->elapseTurnaroundTime(t);

		if (cpu != NULL) {
			cpu->elapseTurnaroundTime(t);
		}

		if (cpuIn != NULL)
			cpuIn->elapseTurnaroundTime(t);

	}


	void printTime() {
		printf("time %ldms: ", time);
	}

	void printReady() {
		priority_queue<RRProcess*,vector<RRProcess*>,RRCompare> copy = readyQ;
		if (copy.empty()) {
			printf("[Q <empty> ]\n");
		} else {
			printf("[Q ");
			while (!copy.empty()) {
				printf("%c ",idtoc(copy.top()->ID));
				copy.pop();
				
			}
			printf("]\n");
		}

	}
	template<class S>
	void printQueue(priority_queue<RRProcess*, vector<RRProcess*>, S> queue) {
		priority_queue<RRProcess*,vector<RRProcess*>,S> copy = queue;
		
		while (!copy.empty()) {
			printf("%c ",idtoc(copy.top()->ID));
			copy.pop();
		}
		printf("\n");

	}
};

#endif