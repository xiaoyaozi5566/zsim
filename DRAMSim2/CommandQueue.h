/*********************************************************************************
*  Copyright (c) 2010-2011, Elliott Cooper-Balis
*                             Paul Rosenfeld
*                             Bruce Jacob
*                             University of Maryland 
*                             dramninjas [at] gmail [dot] com
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/






#ifndef CMDQUEUE_H
#define CMDQUEUE_H

//CommandQueue.h
//
//Header
//

#include "BusPacket.h"
#include "BankState.h"
#include "Transaction.h"
#include "SystemConfiguration.h"
#include "SimulatorObject.h"
#include <set>
#include <utility>
#include <stdlib.h>

using namespace std;

namespace DRAMSim
{
class CommandQueue : public SimulatorObject
{
	CommandQueue();
public:
    ostream &dramsim_log;
	//typedefs
	typedef vector<BusPacket *> BusPacket1D;
	typedef vector<BusPacket1D> BusPacket2D;
	typedef vector<BusPacket2D> BusPacket3D;

	//functions
	CommandQueue(vector< vector<BankState> > &states, ostream &dramsim_log, unsigned num_pids, unsigned turn_length);
	virtual ~CommandQueue(); 

	virtual void enqueue(BusPacket *newBusPacket);
	bool pop(BusPacket **busPacket);
	bool hasRoomFor(unsigned numberToEnqueue, unsigned rank, unsigned bank_or_domain);
	bool isIssuable(BusPacket *busPacket);
	bool isEmpty(unsigned rank);
	void needRefresh(unsigned rank);
    unsigned getRefreshRank();
	virtual void print();
	void update(); //SimulatorObject requirement
	virtual vector<BusPacket *> &getCommandQueue(unsigned rank, unsigned bank_or_domain);
    unsigned getCurrentDomain();
    unsigned* selectRanks(pair <unsigned, unsigned> * rankRequests, unsigned num_ranks);
    void delay(unsigned domain, unsigned adjust_delay);
    uint64_t calWorstTime(BusPacket *busPacket);
    uint64_t calExpectTime(BusPacket *busPacket);
    void resetMonitoring(unsigned domain);
    void increaseD(unsigned domain);
    void updateCurD(unsigned domain);

	//fields
	
	BusPacket3D queues; // 3D array of BusPacket pointers
	vector< vector<BankState> > &bankStates;
protected:
    unsigned num_pids;
    unsigned turn_length;
    
	void nextRankAndBank(unsigned &rank, unsigned &bank);
	//fields
	unsigned nextBank;
	unsigned nextRank;

	unsigned nextBankPRE;
	unsigned nextRankPRE;

	unsigned refreshRank;
	bool refreshWaiting;

	vector< vector<unsigned> > tFAWCountdown;
	vector< vector<unsigned> > rowAccessCounters;
    vector< vector<BusPacket *> > cmdBuffer;
    vector< vector<uint64_t> > issue_time;
    vector< vector<unsigned> > rankStats;
    vector< vector<unsigned> > conflictStats;
    vector< vector< pair<unsigned, uint64_t> > > issueHistory;
    vector<unsigned> previousRanks;
    vector<unsigned> perDomainTotal;
    vector< pair<unsigned, unsigned> > issuableRankBanks;
    vector< pair<unsigned, unsigned> > previousRankBanks;
    vector<unsigned> previousDomains;
    unsigned previousBanks[3][3];
    unsigned tempRanks[3];
    pair<unsigned, unsigned> *rankRequests;
    unsigned finish_refresh;
    uint64_t wait_latency;
    uint64_t total_reqs;
    uint64_t num_turns;
    uint64_t num_reqs;
    uint64_t num_issued;
    uint64_t num_same_bank;
    uint64_t queuing_delay;
    uint64_t limit;
    uint64_t window_size;
    bool transition;
    uint64_t transition_counter;
    bool secure_mode;
    uint64_t secure_rank;
    uint64_t secure_domain;
    SchedulingPolicy insecPolicy;
    
    vector<uint64_t> lastIssueTime;
    vector<uint64_t> lastWorstTime;
    vector<uint64_t> perDomainTrans;
    vector<uint64_t> perDomainVios;
    vector<uint64_t> perDomainB;
    vector<uint64_t> perDomainD;
    vector<uint64_t> perDomainCurD;
    vector<bool> RP_status;

	bool sendAct;
};
}

#endif

