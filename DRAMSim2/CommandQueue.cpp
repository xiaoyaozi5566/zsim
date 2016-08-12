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








//CommandQueue.cpp
//
//Class file for command queue object
//

#include "CommandQueue.h"
#include "MemoryController.h"
#include <assert.h>

using namespace DRAMSim;

CommandQueue::CommandQueue(vector< vector<BankState> > &states, ostream &dramsim_log_, unsigned num_pids_, unsigned turn_length_) :
		dramsim_log(dramsim_log_),
		bankStates(states),
        num_pids(num_pids_),
        turn_length(turn_length_),
		nextBank(0),
		nextRank(0),
		nextBankPRE(0),
		nextRankPRE(0),
		refreshRank(0),
		refreshWaiting(false),
		finish_refresh(100000),
        wait_latency(0),
        total_reqs(0),
        num_turns(0),
        num_reqs(0),
        num_issued(0),
        num_same_bank(0),
        queuing_delay(0),
        limit(1500),
        window_size(100000),
        transition(false),
        transition_counter(0),
        secure_mode(false),
        secure_rank(0),
        secure_domain(0),
        insecPolicy(schedulingPolicy),
        sendAct(true)
{
	//set here to avoid compile errors
	currentClockCycle = 0;

	//use numBankQueus below to create queue structure
	size_t numBankQueues;
	if (queuingStructure==PerRank)
	{
		numBankQueues = 1;
	}
	else if (queuingStructure==PerRankPerBank)
	{
		numBankQueues = NUM_BANKS;
	}
    else if (queuingStructure==PerRankPerDomain)
    {
        numBankQueues = num_pids;
    }
	else
	{
		ERROR("== Error - Unknown queuing structure");
		exit(0);
	}

	//vector of counters used to ensure rows don't stay open too long
	rowAccessCounters = vector< vector<unsigned> >(NUM_RANKS, vector<unsigned>(NUM_BANKS,0));

	//create queue based on the structure we want
	BusPacket1D actualQueue;
	BusPacket2D perBankQueue = BusPacket2D();
	queues = BusPacket3D();
	for (size_t rank=0; rank<NUM_RANKS; rank++)
	{
		//this loop will run only once for per-rank and NUM_BANKS times for per-rank-per-bank
		for (size_t bank=0; bank<numBankQueues; bank++)
		{
			actualQueue	= BusPacket1D();
			perBankQueue.push_back(actualQueue);
		}
		queues.push_back(perBankQueue);
	}

    for (size_t i=0;i<3;i++)
    {
        BusPacket1D perRankQueue = BusPacket1D();
        cmdBuffer.push_back(perRankQueue);
        vector<unsigned> perRankTime;
        issue_time.push_back(perRankTime);
    }
    
    for (size_t i=0;i<3;i++)
    {
        tempRanks[i] = 1000;
        previousRanks.push_back(1000);
        for (size_t j=0;j<3;j++)
            previousBanks[i][j] = 1000;
    }
    
    for (size_t i=0;i<8;i++)
    {
        previousRankBanks.push_back(make_pair(0,0));
        previousDomains.push_back(i);
    }
    
    for (size_t i=0;i<num_pids;i++)
    {
        vector<unsigned> perDomainQueue;
        for (size_t j=0;j<NUM_RANKS;j++)
            perDomainQueue.push_back(0);
        rankStats.push_back(perDomainQueue);
        vector<unsigned> conflictQueue;
        for (size_t j=0;j<num_pids;j++)
            conflictQueue.push_back(0);
        conflictStats.push_back(conflictQueue);
        perDomainTotal.push_back(0);
        vector< pair<unsigned, uint64_t> > historyQueue;
        issueHistory.push_back(historyQueue);
    }
    
    rankRequests = new pair<unsigned, unsigned>[NUM_RANKS];
        
        
	//FOUR-bank activation window
	//	this will count the number of activations within a given window
	//	(decrementing counter)
	//
	//countdown vector will have decrementing counters starting at tFAW
	//  when the 0th element reaches 0, remove it
	tFAWCountdown.reserve(NUM_RANKS);
	for (size_t i=0;i<NUM_RANKS;i++)
	{
		//init the empty vectors here so we don't seg fault later
		tFAWCountdown.push_back(vector<unsigned>());
	}
}
CommandQueue::~CommandQueue()
{
	//ERROR("COMMAND QUEUE destructor");
	size_t bankMax = NUM_RANKS;
	if (queuingStructure == PerRank) {
		bankMax = 1; 
	}
	for (size_t r=0; r< NUM_RANKS; r++)
	{
		for (size_t b=0; b<bankMax; b++) 
		{
			for (size_t i=0; i<queues[r][b].size(); i++)
			{
				delete(queues[r][b][i]);
			}
			queues[r][b].clear();
		}
	}
    for (size_t i=0;i<3;i++)
    {
        cmdBuffer[i].clear();
        issue_time[i].clear();
    }
}
//Adds a command to appropriate queue
void CommandQueue::enqueue(BusPacket *newBusPacket)
{
	unsigned rank = newBusPacket->rank;
	unsigned bank = newBusPacket->bank;
    unsigned srcId = newBusPacket->srcId;
    newBusPacket->arrivalTime = currentClockCycle;
    if (newBusPacket->busPacketType == ACTIVATE)
    {
        total_reqs++;
        perDomainTotal[srcId]++;
        rankStats[srcId][rank]++;
        unsigned period = turn_length*num_pids;
        unsigned period_start = (currentClockCycle/period)*period;
        unsigned next_turn;
        if (period_start + srcId*turn_length > currentClockCycle)
            next_turn = period_start + srcId*turn_length;
        else
            next_turn = period_start + srcId*turn_length + period;
        wait_latency += next_turn - currentClockCycle;
        if (total_reqs % 1000 == 0)
        {
            printf("total_reqs: %ld, wait_latency: %ld\n", total_reqs, wait_latency);
            printf("num_reqs: %f, num_issued: %f, num_same_bank: %f\n", num_reqs*1.0/num_turns, num_issued*1.0/num_turns, num_same_bank*1.0/num_turns);
            printf("queueing_delay: %f\n", queuing_delay*1.0/num_issued);           
        }
        if (schedulingPolicy == SideChannel || schedulingPolicy == Probability)
        {          
            // if (perDomainTotal[srcId] % 1000 == 0)
            // {
            //     printf("Domain %d: ", srcId);
            //     for (size_t j=0;j<NUM_RANKS;j++)
            //     {
            //         printf("%4d ", rankStats[srcId][j]);
            //         rankStats[srcId][j] = 0;
            //     }
            //     printf("Yao\n");
            // }
            if (total_reqs % 1000 == 0)
            {
                printf("currentClockCycle: %ld\n", currentClockCycle);
                for (size_t i=0;i<num_pids;i++)
                {
                    printf("Domain %ld conflict: ", i);
                    for (size_t j=0;j<num_pids;j++)
                    {
                        printf("%d ", conflictStats[i][j]);
                    }
                    printf("Yao\n");
                } 
            }             
        }         
    }
    
    // printf("enqueue packet %lx from domain %d @ cycle %ld, rank %d, bank %d\n", newBusPacket->physicalAddress, newBusPacket->srcId, currentClockCycle, newBusPacket->rank, newBusPacket->bank);
	if (queuingStructure==PerRank)
	{
		queues[rank][0].push_back(newBusPacket);
		if (queues[rank][0].size()>CMD_QUEUE_DEPTH)
		{
			ERROR("== Error - Enqueued more than allowed in command queue");
			ERROR("						Need to call .hasRoomFor(int numberToEnqueue, unsigned rank, unsigned bank) first");
			exit(0);
		}
	}
	else if (queuingStructure==PerRankPerBank)
	{
		queues[rank][bank].push_back(newBusPacket);
		if (queues[rank][bank].size()>CMD_QUEUE_DEPTH)
		{
			ERROR("== Error - Enqueued more than allowed in command queue");
			ERROR("						Need to call .hasRoomFor(int numberToEnqueue, unsigned rank, unsigned bank) first");
			exit(0);
		}
	}
    else if (queuingStructure==PerRankPerDomain)
    {
        queues[rank][srcId].push_back(newBusPacket);
		if (queues[rank][srcId].size()>CMD_QUEUE_DEPTH)
		{
			ERROR("== Error - Enqueued more than allowed in command queue");
			ERROR("						Need to call .hasRoomFor(int numberToEnqueue, unsigned rank, unsigned bank) first");
			exit(0);
		}
    }
	else
	{
		ERROR("== Error - Unknown queuing structure");
		exit(0);
	}
}

//Removes the next item from the command queue based on the system's
//command scheduling policy
bool CommandQueue::pop(BusPacket **busPacket)
{
	// Switch back to side channel protection
    if (USE_MIX)
    {
        if (currentClockCycle % 1000000 == 0 && schedulingPolicy == SecMem) 
        {
            transition = true;
        }
        if (currentClockCycle % 1000000 == 100)
        {
            printf("enter insecure mode @ cycle %ld\n", currentClockCycle);
            transition = false;
            schedulingPolicy = insecPolicy;
            for (size_t i=0;i<num_pids;i++)
                for (size_t j=0;j<num_pids;j++)
                    conflictStats[i][j] = 0;
        }
        if (transition_counter > 0) transition_counter = transition_counter - 1;
        if (transition_counter == 1) 
        {
            transition = false;  
            schedulingPolicy = SecMem;
        }  
    }
    
    //this can be done here because pop() is called every clock cycle by the parent MemoryController
	//	figures out the sliding window requirement for tFAW
	//
	//deal with tFAW book-keeping
	//	each rank has it's own counter since the restriction is on a device level
	for (size_t i=0;i<NUM_RANKS;i++)
	{
		//decrement all the counters we have going
		for (size_t j=0;j<tFAWCountdown[i].size();j++)
		{
			tFAWCountdown[i][j]--;
		}

		//the head will always be the smallest counter, so check if it has reached 0
		if (tFAWCountdown[i].size()>0 && tFAWCountdown[i][0]==0)
		{
			tFAWCountdown[i].erase(tFAWCountdown[i].begin());
		}
	}

	/* Now we need to find a packet to issue. When the code picks a packet, it will set
		 *busPacket = [some eligible packet]
		 
		 First the code looks if any refreshes need to go
		 Then it looks for data packets
		 Otherwise, it starts looking for rows to close (in open page)
	*/

	if (rowBufferPolicy==ClosePage)
	{
		if (schedulingPolicy == SecMem)
        {
            unsigned rel_time = currentClockCycle % turn_length;
            
            // pair<unsigned, unsigned> *rankRequests = new pair<unsigned, unsigned>[NUM_RANKS];
            // at the start of a turn, decide what transactions to issue
            if (rel_time == 0 && !transition)
            {
                num_turns++;
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    vector<BusPacket *> &queue = getCommandQueue(i, getCurrentDomain());
                    num_reqs += queue.size()/2;
                }
                // printf("enter scheduling cycle 0\n");
                // Find the number of requests to different banks for each rank
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    // Do not issue requests from the refreshing rank
                    // printf("refresh rank %d %d\n", refreshRank, getRefreshRank());
                    if (getRefreshRank() == i || refreshRank == i) 
                    {
                        rankRequests[i] = make_pair(0, 0);
                        continue;
                    }
                    set<unsigned> banksToAccess;
                    vector<BusPacket *> &queue = getCommandQueue(i, getCurrentDomain());
                    for (size_t j=0;j<queue.size();j++)
                    {
                        unsigned bank = queue[j]->bank;
                        if (banksToAccess.find(bank) == banksToAccess.end())
                        {
                            banksToAccess.insert(bank);
                        }
                        if (banksToAccess.size() == NUM_DIFF_BANKS) break;
                    }
                    rankRequests[i] = make_pair(banksToAccess.size(), queue.size());
                }
                // printf("enter scheduling cycle 1\n");
                unsigned* temp = selectRanks(rankRequests, NUM_RANKS);
                unsigned chosenRanks[3];
                for (size_t i=0;i<3;i++)
                {
                    chosenRanks[i] = temp[i];
                }
                    
                for (size_t j=0;j<3;j++)
                    tempRanks[j] = 1000;
                bool slot_status[3];
                for (size_t j=0;j<3;j++)
                    slot_status[j] = false;
                
                for (size_t i=0;i<previousRanks.size();i++)
                {
                    for (size_t j=0;j<3;j++)
                    {
                        if (previousRanks[i] == chosenRanks[j])
                        {
                            tempRanks[i] = chosenRanks[j];
                            slot_status[i] = true;
                            break;
                        }
                    }
                }
                for (size_t i=0;i<3;i++)
                {
                    bool issued = false;
                    // Found if the rank is already scheduled
                    for (size_t j=0;j<3;j++)
                    {
                        if (chosenRanks[i] == tempRanks[j])
                        {
                            issued = true;
                            break;
                        }
                    }
                    // If not scheduled, find the empty slot to schedule it
                    if (!issued)
                    {
                        for (size_t k=0;k<3;k++)
                        {
                            if (!slot_status[k])
                            {
                                tempRanks[k] = chosenRanks[i];
                                slot_status[k] = true;
                                break;
                            }
                        }
                    }
                }
                
                // for (size_t i=0;i<3;i++) printf("tempRanks[%ld]=%d\n", i, tempRanks[i]);
                
                // printf("enter scheduling cycle 2\n");
                // Add bus packet to command buffers to be issued.
                for (size_t i=0;i<3;i++)
                {
                    // printf("enter scheduling cycle 3\n");
                    vector<BusPacket *> &queue = getCommandQueue(tempRanks[i], getCurrentDomain());
                    // record packet index used to remove items from queue later
                    vector<unsigned> items_to_remove;
                    set<unsigned> banksToAccess;
                    set<unsigned> banksStats;
                    unsigned tempBanks[NUM_DIFF_BANKS];
                    for (size_t j=0;j<NUM_DIFF_BANKS;j++)
                        tempBanks[j] = 1000;
                    // array to track each slot is taken or not
                    // Yao: the algorithm here can be improved
                    bool slot_status[NUM_DIFF_BANKS];
                    for (size_t j=0;j<NUM_DIFF_BANKS;j++)
                        slot_status[j] = false;
                    // printf("enter scheduling cycle 4\n");
                    // printf("rank %d queue size %d\n", chosenRanks[i], queue.size());
                    for (size_t j=0;j<queue.size();j++)
                    {
                        unsigned bank = queue[j]->bank;
                        if (banksToAccess.find(bank) == banksToAccess.end() && queue[j]->busPacketType==ACTIVATE)
                        {
                            // decide which slot to issue this request
                            unsigned order = 0;
                            for (size_t k=0;k<NUM_DIFF_BANKS;k++)
                            {
                                if (bank == previousBanks[i][NUM_DIFF_BANKS-1-k])
                                {
                                    order = NUM_DIFF_BANKS-1-k;
                                    break;
                                }
                            }

                            for (size_t k=order;k<NUM_DIFF_BANKS;k++)
                            {
                                if (!slot_status[k])
                                {
                                    order = k;
                                    slot_status[k] = true;
                                    break;
                                }
                            }
                            tempBanks[order] = bank;
                            // printf("rank %d, tempBanks[%d]=%d\n", tempRanks[i], order, bank);
                            
                            unsigned activate_time = currentClockCycle + order*BTB_DELAY + i*BTR_DELAY;
                            unsigned rdwr_time = activate_time + tRCD;
                            banksToAccess.insert(bank);
                            // printf("will issue bank %d @ position %d\n", bank, j);
                            items_to_remove.push_back(j);
                            cmdBuffer[i].push_back(queue[j]);
                            cmdBuffer[i].push_back(queue[j+1]);
                            issue_time[i].push_back(activate_time);
                            issue_time[i].push_back(rdwr_time);
                        }
                        if (banksToAccess.size() == NUM_DIFF_BANKS) break;
                    }
                    // stats
                    for (size_t j=0;j<queue.size();j++)
                    {
                        unsigned bank = queue[j]->bank;
                        if (banksStats.find(bank) == banksStats.end() && queue[j]->busPacketType==ACTIVATE)
                        {
                            banksStats.insert(bank);
                        }
                        // requests that are going to the same bank/rank
                        else if (banksStats.find(bank) != banksStats.end() && queue[j]->busPacketType==ACTIVATE)
                        {
                            num_same_bank++;
                        }
                    }
                    // printf("enter scheduling cycle 5\n");
                    for (int j=items_to_remove.size()-1;j>=0;j--)
                    {
                        queue.erase(queue.begin()+items_to_remove[j]+1);
                        queue.erase(queue.begin()+items_to_remove[j]);
                    }
                    // printf("enter scheduling cycle 6\n");
                    // Update previous banks
                    for (size_t j=0;j<NUM_DIFF_BANKS;j++)
                    {
                        previousBanks[i][j] = tempBanks[j];
                        // printf("previousBanks[%d][%d] = %ld\n", i, j, previousBanks[i][j]);
                    }
                        
                }
                // for (size_t j=0;j<2;j++)
                //     for (size_t k=0;k<cmdBuffer[j].size();k++)
                //         printf("cmdBuffer[%ld][%ld] = %lx, rank %ld, issue time %d\n", j, k, cmdBuffer[j][k]->physicalAddress, cmdBuffer[j][k]->rank, issue_time[j][k]);
                // printf("cmdBuffer size %d %d\n", cmdBuffer[0].size(), cmdBuffer[1].size());
            }
            if (rel_time > 0 && rel_time < BTB_DELAY && USE_BETTER_SCHEDULE)
            {
                bool issueMore = false;
                // check to see if there are empty ranks that can be used to issue more requests
                for (size_t i=0;i<3;i++)
                {
                    if (rankRequests[tempRanks[i]].first == 0)
                        issueMore = true;
                }
                if (issueMore)
                {
                    for (size_t i=0;i<NUM_RANKS;i++)
                    {
                        if (getRefreshRank() == i || refreshRank == i) 
                        {
                            continue;
                        }
                        vector<BusPacket *> &queue = getCommandQueue(i, getCurrentDomain());
                        if (queue.size()==0) continue;
                        bool canIssue = false;
                        bool foundIssuable = false;
                        unsigned order = 0;
                        for (size_t j=0;j<3;j++)
                        {
                            if (i == tempRanks[j] && rankRequests[tempRanks[j]].first == 0)
                            {
                                canIssue = true;
                                order = j;
                                break;
                            } 
                        }
                        if (!canIssue) continue;                   
                        for (size_t j=0;j<queue.size();j++)
                        {
                            if (queue[j]->busPacketType==ACTIVATE)
                            {
                                unsigned activate_time = currentClockCycle - (currentClockCycle%turn_length) + BTB_DELAY + order*BTR_DELAY;
                                unsigned rdwr_time = activate_time + tRCD;
                                cmdBuffer[order].push_back(queue[j]);
                                cmdBuffer[order].push_back(queue[j+1]);
                                issue_time[order].push_back(activate_time);
                                issue_time[order].push_back(rdwr_time);
                                foundIssuable = true;
                                rankRequests[i].first = 1;
                                previousBanks[order][NUM_DIFF_BANKS-1] = queue[j]->bank;
                                queue.erase(queue.begin()+j+1);
                                queue.erase(queue.begin()+j);
                                break;                                
                            }
                        }
                        if (foundIssuable) break;
                    }
                }  
            }
            if (rel_time == BTB_DELAY)
            {
                // update previous ranks
                previousRanks.clear();
                for (size_t i=0;i<3;i++) previousRanks.push_back(tempRanks[i]);
            }
                
            bool foundIssuable = false;
            for (size_t i=0;i<3;i++)
            {
                for (size_t j=0;j<issue_time[i].size();j++)
                {
                    if (issue_time[i][j] < currentClockCycle)
                        printf("issue time smaller than current clock cycle!\n");
                    if (issue_time[i][j] == currentClockCycle)
                    {
                        *busPacket = cmdBuffer[i][j];
                        // printf("pop packet %lx from domain %d @ cycle %ld, rank %d, bank %d\n", cmdBuffer[i][j]->physicalAddress, cmdBuffer[i][j]->srcId, currentClockCycle, cmdBuffer[i][j]->rank, cmdBuffer[i][j]->bank);
                        assert(isIssuable(*busPacket));
                        cmdBuffer[i].erase(cmdBuffer[i].begin() + j);
                        issue_time[i].erase(issue_time[i].begin() + j);
                        foundIssuable = true;
                        break;
                    }
                }
                if (foundIssuable) break;
            }
            // Check to see if we can issue a pending refresh command
            if (!foundIssuable && refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);  				
    				refreshWaiting = false;
                    foundIssuable = true;
                    finish_refresh = currentClockCycle + tRFC;
                    // printf("issuing refresh to rank %d\n", refreshRank);
    			}
            }
            // Mark the end of refresh, so the refreshing rank can issue requests again
            if (currentClockCycle == finish_refresh)
                refreshRank = -1;
            if (!foundIssuable) return false;
        }
        else if (schedulingPolicy == FixedService)
        {
            unsigned rel_time = currentClockCycle % BTB_DELAY;
            // Group banks for triple alternation
            unsigned bank_group = (currentClockCycle/BTB_DELAY) % 3;
            unsigned current_domain = (currentClockCycle/BTB_DELAY) % num_pids;
            if (rel_time == 0)
            {
                bool foundIssuable = false;
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    if (getRefreshRank() == i || refreshRank == i) continue;
                    vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                    for (size_t j=0; j<queue.size();j++)
                    {
                        unsigned bank = queue[j]->bank;
                        if (bank % 3 == bank_group && queue[j]->busPacketType==ACTIVATE)
                        {
                            unsigned activate_time = currentClockCycle;
                            unsigned rdwr_time = activate_time + tRCD;
                            cmdBuffer[0].push_back(queue[j]);
                            cmdBuffer[0].push_back(queue[j+1]);
                            queue.erase(queue.begin() + j + 1);
                            queue.erase(queue.begin() + j);
                            issue_time[0].push_back(activate_time);
                            issue_time[0].push_back(rdwr_time);
                            foundIssuable = true;
                        }
                        if (foundIssuable) break;
                    }
                    if (foundIssuable) break;
                }
            }
            bool foundIssuable = false;
            for (size_t i=0;i<issue_time[0].size();i++)
            {
                if (issue_time[0][i] < currentClockCycle)
                    printf("issue time smaller than current clock cycle!\n");
                if (issue_time[0][i] == currentClockCycle)
                {
                    *busPacket = cmdBuffer[0][i];
                    assert(isIssuable(*busPacket));
                    cmdBuffer[0].erase(cmdBuffer[0].begin() + i);
                    issue_time[0].erase(issue_time[0].begin() + i);
                    foundIssuable = true;
                    break;
                }
                if (foundIssuable) break;
            }
            // Check to see if we can issue a pending refresh command
            if (!foundIssuable && refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);  				
    				refreshWaiting = false;
                    foundIssuable = true;
                    finish_refresh = currentClockCycle + tRFC;
                    // printf("issuing refresh to rank %d\n", refreshRank);
    			}
            }
            // Mark the end of refresh, so the refreshing rank can issue requests again
            if (currentClockCycle == finish_refresh)
                refreshRank = -1;
            if (!foundIssuable) return false;
        }
        else if (schedulingPolicy == BankPar)
        {
            unsigned rel_time = currentClockCycle % BTB_DELAY;
            unsigned current_domain = (currentClockCycle/BTB_DELAY) % num_pids;
            if (rel_time == 0)
            {
                bool foundIssuable = false;
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    if (getRefreshRank() == i || refreshRank == i) continue;
                    vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                    for (size_t j=0; j<queue.size();j++)
                    {
                        if (queue[j]->busPacketType==ACTIVATE)
                        {
                            unsigned activate_time = currentClockCycle;
                            unsigned rdwr_time = activate_time + tRCD;
                            cmdBuffer[0].push_back(queue[j]);
                            cmdBuffer[0].push_back(queue[j+1]);
                            queue.erase(queue.begin() + j + 1);
                            queue.erase(queue.begin() + j);
                            issue_time[0].push_back(activate_time);
                            issue_time[0].push_back(rdwr_time);
                            foundIssuable = true;
                        }
                        if (foundIssuable) break;
                    }
                    if (foundIssuable) break;
                }
            }
            bool foundIssuable = false;
            for (size_t i=0;i<issue_time[0].size();i++)
            {
                if (issue_time[0][i] < currentClockCycle)
                    printf("issue time smaller than current clock cycle!\n");
                if (issue_time[0][i] == currentClockCycle)
                {
                    *busPacket = cmdBuffer[0][i];
                    assert(isIssuable(*busPacket));
                    cmdBuffer[0].erase(cmdBuffer[0].begin() + i);
                    issue_time[0].erase(issue_time[0].begin() + i);
                    foundIssuable = true;
                    break;
                }
                if (foundIssuable) break;
            }
            // Check to see if we can issue a pending refresh command
            if (!foundIssuable && refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);  				
    				refreshWaiting = false;
                    foundIssuable = true;
                    finish_refresh = currentClockCycle + tRFC;
                    // printf("issuing refresh to rank %d\n", refreshRank);
    			}
            }
            // Mark the end of refresh, so the refreshing rank can issue requests again
            if (currentClockCycle == finish_refresh)
                refreshRank = -1;
            if (!foundIssuable) return false;
        }
        else if (schedulingPolicy == RankPar)
        {
            unsigned rel_time = currentClockCycle % BTR_DELAY;
            unsigned current_domain = (currentClockCycle/BTR_DELAY) % num_pids;
            if (rel_time == 0)
            {
                bool foundIssuable = false;
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    if (getRefreshRank() == i || refreshRank == i) continue;
                    vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                    for (size_t j=0; j<queue.size();j++)
                    {
                        if (queue[j]->busPacketType==ACTIVATE)
                        {
                            unsigned activate_time = currentClockCycle;
                            unsigned rdwr_time = activate_time + tRCD;
                            cmdBuffer[0].push_back(queue[j]);
                            cmdBuffer[0].push_back(queue[j+1]);
                            queue.erase(queue.begin() + j + 1);
                            queue.erase(queue.begin() + j);
                            issue_time[0].push_back(activate_time);
                            issue_time[0].push_back(rdwr_time);
                            foundIssuable = true;
                        }
                        if (foundIssuable) break;
                    }
                    if (foundIssuable) break;
                }
            }
            bool foundIssuable = false;
            for (size_t i=0;i<issue_time[0].size();i++)
            {
                if (issue_time[0][i] < currentClockCycle)
                    printf("issue time smaller than current clock cycle!\n");
                if (issue_time[0][i] == currentClockCycle)
                {
                    *busPacket = cmdBuffer[0][i];
                    assert(isIssuable(*busPacket));
                    cmdBuffer[0].erase(cmdBuffer[0].begin() + i);
                    issue_time[0].erase(issue_time[0].begin() + i);
                    foundIssuable = true;
                    break;
                }
                if (foundIssuable) break;
            }
            // Check to see if we can issue a pending refresh command
            if (!foundIssuable && refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);  				
    				refreshWaiting = false;
                    foundIssuable = true;
                    finish_refresh = currentClockCycle + tRFC;
                    // printf("issuing refresh to rank %d\n", refreshRank);
    			}
            }
            // Mark the end of refresh, so the refreshing rank can issue requests again
            if (currentClockCycle == finish_refresh)
                refreshRank = -1;
            if (!foundIssuable) return false;
        }
        else if (schedulingPolicy == SideChannel)
        {
            unsigned rel_time = currentClockCycle % BTR_DELAY;
            unsigned current_domain = (currentClockCycle/BTR_DELAY) % num_pids;
            if (rel_time == 0 && !transition)
            {
                bool foundIssuable = false;
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    // Cannot issue request to refreshing rank
                    if (getRefreshRank() == i || refreshRank == i) continue;
                    // Cannot issue the same rank as previous ranks
                    if (i == previousRankBanks[6].first || i == previousRankBanks[7].first) continue;
                    vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                    for (size_t j=0; j<queue.size();j++)
                    {
                        if (queue[j]->busPacketType==ACTIVATE)
                        {
                            bool canIssue = true;
                            for (size_t k=0;k<6;k++)
                            {
                                if (i == previousRankBanks[k].first && queue[j]->bank == previousRankBanks[k].second)
                                {
                                    canIssue = false;
                                    break;
                                }
                            }
                            if (!canIssue) continue;
                            unsigned activate_time = currentClockCycle;
                            unsigned rdwr_time = activate_time + tRCD;
                            cmdBuffer[0].push_back(queue[j]);
                            cmdBuffer[0].push_back(queue[j+1]);                        
                            issue_time[0].push_back(activate_time);
                            issue_time[0].push_back(rdwr_time);
                            previousRankBanks.erase(previousRankBanks.begin());
                            previousRankBanks.push_back(make_pair(i, queue[j]->bank));
                            queue.erase(queue.begin() + j + 1);
                            queue.erase(queue.begin() + j);
                            foundIssuable = true;
                        }
                        if (foundIssuable) break;
                    }
                    if (foundIssuable) break;
                }
                // create a fake reqeust
                if (!foundIssuable)
                {
        			// update conflict stats
                    for (size_t i=0; i<NUM_RANKS; i++)
                    {
                        if (getRefreshRank() == i || refreshRank == i) continue;
                        vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                        if (queue.size() == 0) continue;
                        // check rank conflict first
                        if (i == previousRankBanks[6].first)
                        {
                            unsigned previous_domain = (current_domain + num_pids - 2) % num_pids;
                            conflictStats[previous_domain][current_domain]++;
                            if (USE_MIX)
                            {
                                if (conflictStats[previous_domain][current_domain] > limit)
                                {
                                    transition = true;
                                    transition_counter = 100;
                                    printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                }                                     
                            }                           
                        }
                        else if (i == previousRankBanks[7].first)
                        {
                            unsigned previous_domain = (current_domain + num_pids - 1) % num_pids;
                            conflictStats[previous_domain][current_domain]++;
                            if (USE_MIX)
                            {
                                if (conflictStats[previous_domain][current_domain] > limit)
                                {
                                    transition = true;
                                    transition_counter = 100;
                                    printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                }                                     
                            }  
                        }
                        // next check bank conflict
                        for (size_t j=0; j<queue.size(); j++)
                        {
                            if (queue[j]->busPacketType==ACTIVATE)
                            {
                                for (size_t k=0;k<6;k++)
                                {
                                    if (i == previousRankBanks[k].first && queue[j]->bank == previousRankBanks[k].second)
                                    {
                                        unsigned previous_domain = (current_domain + num_pids - 8 + k) % num_pids;
                                        conflictStats[previous_domain][current_domain]++;
                                        if (USE_MIX)
                                        {
                                            if (conflictStats[previous_domain][current_domain] > limit)
                                            {
                                                transition = true;
                                                transition_counter = 100;
                                                printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                            }                                     
                                        } 
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    issuableRankBanks.clear();
                    for (size_t i=0; i<NUM_RANKS; i++)
                    {
                        if (getRefreshRank() == i || refreshRank == i) continue;
                        if (i == previousRankBanks[6].first || i == previousRankBanks[7].first) continue;
                        for (size_t j=0; j<NUM_BANKS; j++)
                        {
                            bool canIssue = true;
                            for (size_t k=0;k<6;k++)
                            {
                                if (i == previousRankBanks[k].first && j == previousRankBanks[k].second)
                                {
                                    canIssue = false;
                                    break;
                                }
                            }
                            if (!canIssue) continue;
                            issuableRankBanks.push_back(make_pair(i,j));
                        }
                    }
                    unsigned rankBank = rand() % issuableRankBanks.size();
                    unsigned r = issuableRankBanks[rankBank].first;
                    unsigned b = issuableRankBanks[rankBank].second;
                    previousRankBanks.erase(previousRankBanks.begin());
                    previousRankBanks.push_back(make_pair(r, b));
                }
            }
            bool foundIssuable = false;
            for (size_t i=0;i<issue_time[0].size();i++)
            {
                if (issue_time[0][i] < currentClockCycle)
                    printf("issue time smaller than current clock cycle!\n");
                if (issue_time[0][i] == currentClockCycle)
                {
                    *busPacket = cmdBuffer[0][i];
                    // printf("pop packet %lx from domain %d @ cycle %ld, rank %d, bank %d\n", cmdBuffer[0][i]->physicalAddress, cmdBuffer[0][i]->srcId, currentClockCycle, cmdBuffer[0][i]->rank, cmdBuffer[0][i]->bank);
                    assert(isIssuable(*busPacket));
                    cmdBuffer[0].erase(cmdBuffer[0].begin() + i);
                    issue_time[0].erase(issue_time[0].begin() + i);
                    foundIssuable = true;
                    break;
                }
                if (foundIssuable) break;
            }
            // Check to see if we can issue a pending refresh command
            if (!foundIssuable && refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);  				
    				refreshWaiting = false;
                    foundIssuable = true;
                    finish_refresh = currentClockCycle + tRFC;
                    // printf("issuing refresh to rank %d\n", refreshRank);
    			}
            }
            // Mark the end of refresh, so the refreshing rank can issue requests again
            if (currentClockCycle == finish_refresh)
                refreshRank = -1;
            if (!foundIssuable) return false;
        }
        else if (schedulingPolicy == Probability)
        {
            unsigned rel_time = currentClockCycle % BTR_DELAY;
            unsigned current_domain = rand() % num_pids;
            if (rel_time == 0 && !transition)
            {
                bool foundIssuable = false;
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    // Cannot issue request to refreshing rank
                    if (getRefreshRank() == i || refreshRank == i) continue;
                    // Cannot issue the same rank as previous ranks
                    if (i == previousRankBanks[6].first || i == previousRankBanks[7].first) continue;
                    vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                    for (size_t j=0; j<queue.size();j++)
                    {
                        if (queue[j]->busPacketType==ACTIVATE)
                        {
                            bool canIssue = true;
                            for (size_t k=0;k<6;k++)
                            {
                                if (i == previousRankBanks[k].first && queue[j]->bank == previousRankBanks[k].second)
                                {
                                    canIssue = false;
                                    break;
                                }
                            }
                            if (!canIssue) continue;
                            unsigned activate_time = currentClockCycle;
                            unsigned rdwr_time = activate_time + tRCD;
                            cmdBuffer[0].push_back(queue[j]);
                            cmdBuffer[0].push_back(queue[j+1]);                        
                            issue_time[0].push_back(activate_time);
                            issue_time[0].push_back(rdwr_time);
                            previousRankBanks.erase(previousRankBanks.begin());
                            previousRankBanks.push_back(make_pair(i, queue[j]->bank));
                            queue.erase(queue.begin() + j + 1);
                            queue.erase(queue.begin() + j);
                            foundIssuable = true;
                        }
                        if (foundIssuable) break;
                    }
                    if (foundIssuable) break;
                }
                // create a fake reqeust
                if (!foundIssuable)
                {                    
        			// update conflict stats
                    for (size_t i=0; i<NUM_RANKS; i++)
                    {
                        if (getRefreshRank() == i || refreshRank == i) continue;
                        vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                        if (queue.size() == 0) continue;
                        // check rank conflict first
                        if (i == previousRankBanks[6].first)
                        {
                            unsigned previous_domain = previousDomains[6];
                            conflictStats[previous_domain][current_domain]++;
                            if (USE_MIX)
                            {
                                if (conflictStats[previous_domain][current_domain] > limit)
                                {
                                    transition = true;
                                    transition_counter = 100;
                                    printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                }                                     
                            }                           
                        }
                        else if (i == previousRankBanks[7].first)
                        {
                            unsigned previous_domain = previousDomains[7];
                            conflictStats[previous_domain][current_domain]++;
                            if (USE_MIX)
                            {
                                if (conflictStats[previous_domain][current_domain] > limit)
                                {
                                    transition = true;
                                    transition_counter = 100;
                                    printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                }                                     
                            }  
                        }
                        // next check bank conflict
                        for (size_t j=0; j<queue.size(); j++)
                        {
                            if (queue[j]->busPacketType==ACTIVATE)
                            {
                                for (size_t k=0;k<6;k++)
                                {
                                    if (i == previousRankBanks[k].first && queue[j]->bank == previousRankBanks[k].second)
                                    {
                                        unsigned previous_domain = previousDomains[k];
                                        conflictStats[previous_domain][current_domain]++;
                                        if (USE_MIX)
                                        {
                                            if (conflictStats[previous_domain][current_domain] > limit)
                                            {
                                                transition = true;
                                                transition_counter = 100;
                                                printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                            }                                     
                                        } 
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    issuableRankBanks.clear();
                    for (size_t i=0; i<NUM_RANKS; i++)
                    {
                        if (getRefreshRank() == i || refreshRank == i) continue;
                        if (i == previousRankBanks[6].first || i == previousRankBanks[7].first) continue;
                        for (size_t j=0; j<NUM_BANKS; j++)
                        {
                            bool canIssue = true;
                            for (size_t k=0;k<6;k++)
                            {
                                if (i == previousRankBanks[k].first && j == previousRankBanks[k].second)
                                {
                                    canIssue = false;
                                    break;
                                }
                            }
                            if (!canIssue) continue;
                            issuableRankBanks.push_back(make_pair(i,j));
                        }
                    }
                    unsigned rankBank = rand() % issuableRankBanks.size();
                    unsigned r = issuableRankBanks[rankBank].first;
                    unsigned b = issuableRankBanks[rankBank].second;
                    previousRankBanks.erase(previousRankBanks.begin());
                    previousRankBanks.push_back(make_pair(r, b));
                    previousDomains.erase(previousDomains.begin());
                    previousDomains.push_back(current_domain);
                }
            }
            bool foundIssuable = false;
            for (size_t i=0;i<issue_time[0].size();i++)
            {
                if (issue_time[0][i] < currentClockCycle)
                    printf("issue time smaller than current clock cycle!\n");
                if (issue_time[0][i] == currentClockCycle)
                {
                    *busPacket = cmdBuffer[0][i];
                    // printf("pop packet %lx from domain %d @ cycle %ld, rank %d, bank %d\n", cmdBuffer[0][i]->physicalAddress, cmdBuffer[0][i]->srcId, currentClockCycle, cmdBuffer[0][i]->rank, cmdBuffer[0][i]->bank);
                    assert(isIssuable(*busPacket));
                    cmdBuffer[0].erase(cmdBuffer[0].begin() + i);
                    issue_time[0].erase(issue_time[0].begin() + i);
                    foundIssuable = true;
                    break;
                }
                if (foundIssuable) break;
            }
            // Check to see if we can issue a pending refresh command
            if (!foundIssuable && refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);  				
    				refreshWaiting = false;
                    foundIssuable = true;
                    finish_refresh = currentClockCycle + tRFC;
                    // printf("issuing refresh to rank %d\n", refreshRank);
    			}
            }
            // Mark the end of refresh, so the refreshing rank can issue requests again
            if (currentClockCycle == finish_refresh)
                refreshRank = -1;
            if (!foundIssuable) return false;
        }
        else if (schedulingPolicy == AccessLimit)
        {
            // first update the issue history
            if (currentClockCycle > window_size)
            {
                for (size_t i=0;i<num_pids;i++)
                {
                    if (issueHistory[i].empty()) continue;
                    if (issueHistory[i][0].first < (currentClockCycle - window_size))
                        issueHistory[i].erase(issueHistory[i].begin());      
                }
            }
            
            unsigned rel_time = currentClockCycle % BTR_DELAY;
            unsigned current_domain = rand() % num_pids;
            if (rel_time == 0 && !transition)
            {
                bool foundIssuable = false;
                for (size_t i=0;i<NUM_RANKS;i++)
                {
                    // Cannot issue request to refreshing rank
                    if (getRefreshRank() == i || refreshRank == i) continue;
                    // Cannot issue the same rank as previous ranks
                    if (i == previousRankBanks[6].first || i == previousRankBanks[7].first) continue;
                    vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                    for (size_t j=0; j<queue.size();j++)
                    {
                        if (queue[j]->busPacketType==ACTIVATE)
                        {
                            bool canIssue = true;
                            for (size_t k=0;k<6;k++)
                            {
                                if (i == previousRankBanks[k].first && queue[j]->bank == previousRankBanks[k].second)
                                {
                                    canIssue = false;
                                    break;
                                }
                            }
                            // check if the address is issued within N cycles
                            for (size_t k=0; k<issueHistory[current_domain].size();k++)
                            {
                                if (queue[j]->physicalAddress == issueHistory[current_domain][k].second)
                                {
                                    canIssue = false;
                                    break;
                                }
                            }
                            if (!canIssue) continue;
                            unsigned activate_time = currentClockCycle;
                            unsigned rdwr_time = activate_time + tRCD;
                            cmdBuffer[0].push_back(queue[j]);
                            cmdBuffer[0].push_back(queue[j+1]);
                            issueHistory[current_domain].push_back(make_pair(currentClockCycle, queue[j]->physicalAddress));
                            issue_time[0].push_back(activate_time);
                            issue_time[0].push_back(rdwr_time);
                            previousRankBanks.erase(previousRankBanks.begin());
                            previousRankBanks.push_back(make_pair(i, queue[j]->bank));
                            queue.erase(queue.begin() + j + 1);
                            queue.erase(queue.begin() + j);
                            
                            foundIssuable = true;
                        }
                        if (foundIssuable) break;
                    }
                    if (foundIssuable) break;
                }
                // create a fake reqeust
                if (!foundIssuable)
                {                    
        			// update conflict stats
                    for (size_t i=0; i<NUM_RANKS; i++)
                    {
                        if (getRefreshRank() == i || refreshRank == i) continue;
                        vector<BusPacket *> &queue = getCommandQueue(i, current_domain);
                        if (queue.size() == 0) continue;
                        // check rank conflict first
                        if (i == previousRankBanks[6].first)
                        {
                            unsigned previous_domain = previousDomains[6];
                            conflictStats[previous_domain][current_domain]++;
                            if (USE_MIX)
                            {
                                if (conflictStats[previous_domain][current_domain] > limit)
                                {
                                    transition = true;
                                    transition_counter = 100;
                                    printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                }                                     
                            }                           
                        }
                        else if (i == previousRankBanks[7].first)
                        {
                            unsigned previous_domain = previousDomains[7];
                            conflictStats[previous_domain][current_domain]++;
                            if (USE_MIX)
                            {
                                if (conflictStats[previous_domain][current_domain] > limit)
                                {
                                    transition = true;
                                    transition_counter = 100;
                                    printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                }                                     
                            }  
                        }
                        // next check bank conflict
                        for (size_t j=0; j<queue.size(); j++)
                        {
                            if (queue[j]->busPacketType==ACTIVATE)
                            {
                                for (size_t k=0;k<6;k++)
                                {
                                    if (i == previousRankBanks[k].first && queue[j]->bank == previousRankBanks[k].second)
                                    {
                                        unsigned previous_domain = previousDomains[k];
                                        conflictStats[previous_domain][current_domain]++;
                                        if (USE_MIX)
                                        {
                                            if (conflictStats[previous_domain][current_domain] > limit)
                                            {
                                                transition = true;
                                                transition_counter = 100;
                                                printf("enter secure mode @ cycle %ld\n", currentClockCycle);
                                            }                                     
                                        } 
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    issuableRankBanks.clear();
                    for (size_t i=0; i<NUM_RANKS; i++)
                    {
                        if (getRefreshRank() == i || refreshRank == i) continue;
                        if (i == previousRankBanks[6].first || i == previousRankBanks[7].first) continue;
                        for (size_t j=0; j<NUM_BANKS; j++)
                        {
                            bool canIssue = true;
                            for (size_t k=0;k<6;k++)
                            {
                                if (i == previousRankBanks[k].first && j == previousRankBanks[k].second)
                                {
                                    canIssue = false;
                                    break;
                                }
                            }
                            if (!canIssue) continue;
                            issuableRankBanks.push_back(make_pair(i,j));
                        }
                    }
                    unsigned rankBank = rand() % issuableRankBanks.size();
                    unsigned r = issuableRankBanks[rankBank].first;
                    unsigned b = issuableRankBanks[rankBank].second;
                    previousRankBanks.erase(previousRankBanks.begin());
                    previousRankBanks.push_back(make_pair(r, b));
                    previousDomains.erase(previousDomains.begin());
                    previousDomains.push_back(current_domain);
                }
            }
            bool foundIssuable = false;
            for (size_t i=0;i<issue_time[0].size();i++)
            {
                if (issue_time[0][i] < currentClockCycle)
                    printf("issue time smaller than current clock cycle!\n");
                if (issue_time[0][i] == currentClockCycle)
                {
                    *busPacket = cmdBuffer[0][i];
                    // printf("pop packet %lx from domain %d @ cycle %ld, rank %d, bank %d\n", cmdBuffer[0][i]->physicalAddress, cmdBuffer[0][i]->srcId, currentClockCycle, cmdBuffer[0][i]->rank, cmdBuffer[0][i]->bank);
                    assert(isIssuable(*busPacket));
                    cmdBuffer[0].erase(cmdBuffer[0].begin() + i);
                    issue_time[0].erase(issue_time[0].begin() + i);
                    foundIssuable = true;
                    break;
                }
                if (foundIssuable) break;
            }
            // Check to see if we can issue a pending refresh command
            if (!foundIssuable && refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);  				
    				refreshWaiting = false;
                    foundIssuable = true;
                    finish_refresh = currentClockCycle + tRFC;
                    // printf("issuing refresh to rank %d\n", refreshRank);
    			}
            }
            // Mark the end of refresh, so the refreshing rank can issue requests again
            if (currentClockCycle == finish_refresh)
                refreshRank = -1;
            if (!foundIssuable) return false;
        }
        else if (schedulingPolicy == Dynamic)
        {
            bool sendingREF = false;
            //if the memory controller set the flags signaling that we need to issue a refresh
            if (refreshWaiting)
            {
                bool foundActiveOrTooEarly = false;
                //look for an open bank
                for (size_t b=0;b<NUM_BANKS;b++)
                {
                    //checks to make sure that all banks are idle
                    if (bankStates[refreshRank][b].currentBankState == RowActive)
                    {
                        foundActiveOrTooEarly = true;
                        //if the bank is open, make sure there is nothing else
                        // going there before we close it
                        for (size_t i=0;i<num_pids;i++)
                        {
                            vector<BusPacket *> &queue = getCommandQueue(refreshRank,i);
                            if (!queue.empty())
                            {
                                BusPacket *packet = queue[0];
                                if (packet->row == bankStates[refreshRank][b].openRowAddress &&
                                        packet->bank == b)
                                {
                                    if (packet->busPacketType != ACTIVATE && isIssuable(packet))
                                    {
                                        *busPacket = packet;
                                        queue.erase(queue.begin());
                                        sendingREF = true;
                                    }
                                    break;
                                }
                            }
                        }
                        
                        break;
                    }
                    //    NOTE: checks nextActivate time for each bank to make sure tRP is being
                    //                satisfied.    the next ACT and next REF can be issued at the same
                    //                point in the future, so just use nextActivate field instead of
                    //                creating a nextRefresh field
                    else if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
                    {
                        foundActiveOrTooEarly = true;
                        break;
                    }
                }

                //if there are no open banks and timing has been met, send out the refresh
                //    reset flags and rank pointer
                if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
                {
                    *busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);
                    // printf("refresh rank %d @ cycle %ld\n", refreshRank, currentClockCycle);
                    refreshRank = -1;
                    refreshWaiting = false;
                    sendingREF = true;
                }
            } // refreshWaiting

            //if we're not sending a REF, proceed as normal
            if (!sendingREF)
            {
                bool foundIssuable = false;
                if (secure_mode)
                {
                    vector<BusPacket *> &queue = getCommandQueue(secure_rank, secure_domain);
                    // the rd/wr command has been issued by refreshwaiting
                    if (queue.empty()) secure_mode = false;
                    else if (isIssuable(queue[0]))
                    {
                        if (queue[0]->busPacketType != ACTIVATE)
                            secure_mode = false;
                        *busPacket = queue[0];
                        // printf("addr %lx issued in secure mode @ cycle %ld\n", queue[0]->physicalAddress, currentClockCycle);
                        queue.erase(queue.begin());
                        foundIssuable = true;
                    }
                }
                if (!foundIssuable)
                {
                    unsigned which_domain = 0;
                    unsigned min_issueTime_D = 0;
                    for (size_t i=0;i<num_pids;i++)
                    {
                        unsigned which_rank = 0;
                        unsigned min_issueTime_R = 0;
                        for (size_t j=0;j<NUM_RANKS;j++)
                        {
                            vector<BusPacket *> &queue = getCommandQueue(j, i);
                            if (!queue.empty())
                            {
                                if (min_issueTime_R == 0)
                                {
                                    min_issueTime_R = queue[0]->issueTime;
                                    which_rank = j;
                                }
                                else if (min_issueTime_R > queue[0]->issueTime)
                                {
                                    min_issueTime_R = queue[0]->issueTime;
                                    which_rank = j;
                                }
                            }
                        }
                        // there is a request from this domain
                        if (min_issueTime_R != 0)
                        {
                            vector<BusPacket *> &queue = getCommandQueue(which_rank, i);
                            if (queue[0]->busPacketType == ACTIVATE && secure_mode == false &&
                                    queue[0]->w_issueTime <= currentClockCycle + B_WORST)
                            {
                                secure_mode = true;
                                secure_rank = which_rank;
                                secure_domain = i;
                                printf("addr %lx from %d enter secure mode\n", queue[0]->physicalAddress, queue[0]->srcId);
                                printf("secure_rank %d, secure_domain %d\n", secure_rank, secure_domain);
                                printf("currentClockCycle: %d\n", currentClockCycle);
                                printf("expected issue time: %d\n", queue[0]->issueTime);
                                printf("worst issue time: %d\n", queue[0]->w_issueTime);
                            }
                            
                            if (!((which_rank == refreshRank) && refreshWaiting && queue[0]->busPacketType == ACTIVATE))
                            {
                                // In secure mode, only issue rd/wr requests
                                if (secure_mode)
                                {
                                    if (isIssuable(queue[0]) && queue[0]->busPacketType != ACTIVATE)
                                    {
                                        if (min_issueTime_D == 0)
                                        {
                                            min_issueTime_D = min_issueTime_R;
                                            which_domain = i;
                                        }
                                        else if (min_issueTime_D > min_issueTime_R)
                                        {
                                            min_issueTime_D = min_issueTime_R;
                                            which_domain = i;
                                        }
                                    }
                                }
                                else if (isIssuable(queue[0]))
                                {
                                    if (min_issueTime_D == 0)
                                    {
                                        min_issueTime_D = min_issueTime_R;
                                        which_domain = i;
                                    }
                                    else if (min_issueTime_D > min_issueTime_R)
                                    {
                                        min_issueTime_D = min_issueTime_R;
                                        which_domain = i;
                                    }
                                }
                            }                      
                        }
                    }
                    // there is some request being selected from all domains
                    if (min_issueTime_D != 0)
                    {
                        unsigned which_rank = 0;
                        unsigned min_issueTime_R = 0;
                        for (size_t j=0;j<NUM_RANKS;j++)
                        {
                            vector<BusPacket *> &queue = getCommandQueue(j, which_domain);
                            if (!queue.empty())
                            {
                                if (min_issueTime_R == 0)
                                {
                                    min_issueTime_R = queue[0]->issueTime;
                                    which_rank = j;
                                }
                                else if (min_issueTime_R > queue[0]->issueTime)
                                {
                                    min_issueTime_R = queue[0]->issueTime;
                                    which_rank = j;
                                }
                            }
                        }
                        vector<BusPacket *> &queue = getCommandQueue(which_rank, which_domain);
                        if (secure_mode)
                        {
                            if (queue[0]->busPacketType != ACTIVATE)
                            {
                                *busPacket = queue[0];
                                queue.erase(queue.begin());
                                foundIssuable = true;
                            }
                        }
                        else
                        {
                            *busPacket = queue[0];
                            queue.erase(queue.begin());
                            foundIssuable = true;
                        }                 
                    }
                }    

                //if we couldn't find anything to send, return false
                if (!foundIssuable) return false;
            }
        }
        else
        {
            bool sendingREF = false;
    		//if the memory controller set the flags signaling that we need to issue a refresh
    		if (refreshWaiting)
    		{
    			bool foundActiveOrTooEarly = false;
    			//look for an open bank
    			for (size_t b=0;b<NUM_BANKS;b++)
    			{
    				vector<BusPacket *> &queue = getCommandQueue(refreshRank,b);
    				//checks to make sure that all banks are idle
    				if (bankStates[refreshRank][b].currentBankState == RowActive)
    				{
    					foundActiveOrTooEarly = true;
    					//if the bank is open, make sure there is nothing else
    					// going there before we close it
    					for (size_t j=0;j<queue.size();j++)
    					{
    						BusPacket *packet = queue[j];
    						if (packet->row == bankStates[refreshRank][b].openRowAddress &&
    								packet->bank == b)
    						{
    							if (packet->busPacketType != ACTIVATE && isIssuable(packet))
    							{
    								*busPacket = packet;
    								queue.erase(queue.begin() + j);
    								sendingREF = true;
    							}
    							break;
    						}
    					}

    					break;
    				}
    				//	NOTE: checks nextActivate time for each bank to make sure tRP is being
    				//				satisfied.	the next ACT and next REF can be issued at the same
    				//				point in the future, so just use nextActivate field instead of
    				//				creating a nextRefresh field
    				else if (bankStates[refreshRank][b].nextActivate > currentClockCycle)
    				{
    					foundActiveOrTooEarly = true;
    					break;
    				}
    			}

    			//if there are no open banks and timing has been met, send out the refresh
    			//	reset flags and rank pointer
    			if (!foundActiveOrTooEarly && bankStates[refreshRank][0].currentBankState != PowerDown)
    			{
    				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);
    				refreshRank = -1;
    				refreshWaiting = false;
    				sendingREF = true;
    			}
    		} // refreshWaiting

    		//if we're not sending a REF, proceed as normal
    		if (!sendingREF)
    		{
    			bool foundIssuable = false;
    			unsigned startingRank = nextRank;
    			unsigned startingBank = nextBank;
    			do
    			{
    				vector<BusPacket *> &queue = getCommandQueue(nextRank, nextBank);
    				//make sure there is something in this queue first
    				//	also make sure a rank isn't waiting for a refresh
    				//	if a rank is waiting for a refesh, don't issue anything to it until the
    				//		refresh logic above has sent one out (ie, letting banks close)
    				if (!queue.empty() && !((nextRank == refreshRank) && refreshWaiting))
    				{
    					if (queuingStructure == PerRank)
    					{

    						//search from beginning to find first issuable bus packet
    						for (size_t i=0;i<queue.size();i++)
    						{
    							if (isIssuable(queue[i]))
    							{
    								//check to make sure we aren't removing a read/write that is paired with an activate
    								if (i>0 && queue[i-1]->busPacketType==ACTIVATE &&
    										queue[i-1]->physicalAddress == queue[i]->physicalAddress)
    									continue;

    								*busPacket = queue[i];
    								queue.erase(queue.begin()+i);
    								foundIssuable = true;
    								break;
    							}
    						}
    					}
    					else
    					{
    						if (isIssuable(queue[0]))
    						{

    							//no need to search because if the front can't be sent,
    							// then no chance something behind it can go instead
    							*busPacket = queue[0];
    							queue.erase(queue.begin());
    							foundIssuable = true;
    						}
    					}

    				}

    				//if we found something, break out of do-while
    				if (foundIssuable) break;

    				//rank round robin
    				if (queuingStructure == PerRank)
    				{
    					nextRank = (nextRank + 1) % NUM_RANKS;
    					if (startingRank == nextRank)
    					{
    						break;
    					}
    				}
    				else 
    				{
    					nextRankAndBank(nextRank, nextBank);
    					if (startingRank == nextRank && startingBank == nextBank)
    					{
    						break;
    					}
    				}
    			}
    			while (true);

    			//if we couldn't find anything to send, return false
    			if (!foundIssuable) return false;
    		}
        }  
	}
	else if (rowBufferPolicy==OpenPage)
	{
		bool sendingREForPRE = false;
		if (refreshWaiting)
		{
			bool sendREF = true;
			//make sure all banks idle and timing met for a REF
			for (size_t b=0;b<NUM_BANKS;b++)
			{
				//if a bank is active we can't send a REF yet
				if (bankStates[refreshRank][b].currentBankState == RowActive)
				{
					sendREF = false;
					bool closeRow = true;
					//search for commands going to an open row
					vector <BusPacket *> &refreshQueue = getCommandQueue(refreshRank,b);

					for (size_t j=0;j<refreshQueue.size();j++)
					{
						BusPacket *packet = refreshQueue[j];
						//if a command in the queue is going to the same row . . .
						if (bankStates[refreshRank][b].openRowAddress == packet->row &&
								b == packet->bank)
						{
							// . . . and is not an activate . . .
							if (packet->busPacketType != ACTIVATE)
							{
								closeRow = false;
								// . . . and can be issued . . .
								if (isIssuable(packet))
								{
									//send it out
									*busPacket = packet;
									refreshQueue.erase(refreshQueue.begin()+j);
									sendingREForPRE = true;
								}
								break;
							}
							else //command is an activate
							{
								//if we've encountered another act, no other command will be of interest
								break;
							}
						}
					}

					//if the bank is open and we are allowed to close it, then send a PRE
					if (closeRow && currentClockCycle >= bankStates[refreshRank][b].nextPrecharge)
					{
						rowAccessCounters[refreshRank][b]=0;
						*busPacket = new BusPacket(PRECHARGE, 0, 0, 0, refreshRank, b, 0, 0, dramsim_log);
						sendingREForPRE = true;
					}
					break;
				}
				//	NOTE: the next ACT and next REF can be issued at the same
				//				point in the future, so just use nextActivate field instead of
				//				creating a nextRefresh field
				else if (bankStates[refreshRank][b].nextActivate > currentClockCycle) //and this bank doesn't have an open row
				{
					sendREF = false;
					break;
				}
			}

			//if there are no open banks and timing has been met, send out the refresh
			//	reset flags and rank pointer
			if (sendREF && bankStates[refreshRank][0].currentBankState != PowerDown)
			{
				*busPacket = new BusPacket(REFRESH, 0, 0, 0, refreshRank, 0, 0, 0, dramsim_log);
				refreshRank = -1;
				refreshWaiting = false;
				sendingREForPRE = true;
			}
		}

		if (!sendingREForPRE)
		{
			unsigned startingRank = nextRank;
			unsigned startingBank = nextBank;
			bool foundIssuable = false;
			do // round robin over queues
			{
				vector<BusPacket *> &queue = getCommandQueue(nextRank,nextBank);
				//make sure there is something there first
				if (!queue.empty() && !((nextRank == refreshRank) && refreshWaiting))
				{
					//search from the beginning to find first issuable bus packet
					for (size_t i=0;i<queue.size();i++)
					{
						BusPacket *packet = queue[i];
						if (isIssuable(packet))
						{
							//check for dependencies
							bool dependencyFound = false;
							for (size_t j=0;j<i;j++)
							{
								BusPacket *prevPacket = queue[j];
								if (prevPacket->busPacketType != ACTIVATE &&
										prevPacket->bank == packet->bank &&
										prevPacket->row == packet->row)
								{
									dependencyFound = true;
									break;
								}
							}
							if (dependencyFound) continue;

							*busPacket = packet;

							//if the bus packet before is an activate, that is the act that was
							//	paired with the column access we are removing, so we have to remove
							//	that activate as well (check i>0 because if i==0 then theres nothing before it)
							if (i>0 && queue[i-1]->busPacketType == ACTIVATE)
							{
								rowAccessCounters[(*busPacket)->rank][(*busPacket)->bank]++;
								// i is being returned, but i-1 is being thrown away, so must delete it here 
								delete (queue[i-1]);

								// remove both i-1 (the activate) and i (we've saved the pointer in *busPacket)
								queue.erase(queue.begin()+i-1,queue.begin()+i+1);
							}
							else // there's no activate before this packet
							{
								//or just remove the one bus packet
								queue.erase(queue.begin()+i);
							}

							foundIssuable = true;
							break;
						}
					}
				}

				//if we found something, break out of do-while
				if (foundIssuable) break;

				//rank round robin
				if (queuingStructure == PerRank)
				{
					nextRank = (nextRank + 1) % NUM_RANKS;
					if (startingRank == nextRank)
					{
						break;
					}
				}
				else 
				{
					nextRankAndBank(nextRank, nextBank); 
					if (startingRank == nextRank && startingBank == nextBank)
					{
						break;
					}
				}
			}
			while (true);

			//if nothing was issuable, see if we can issue a PRE to an open bank
			//	that has no other commands waiting
			if (!foundIssuable)
			{
				//search for banks to close
				bool sendingPRE = false;
				unsigned startingRank = nextRankPRE;
				unsigned startingBank = nextBankPRE;

				do // round robin over all ranks and banks
				{
					vector <BusPacket *> &queue = getCommandQueue(nextRankPRE, nextBankPRE);
					bool found = false;
					//check if bank is open
					if (bankStates[nextRankPRE][nextBankPRE].currentBankState == RowActive)
					{
						for (size_t i=0;i<queue.size();i++)
						{
							//if there is something going to that bank and row, then we don't want to send a PRE
							if (queue[i]->bank == nextBankPRE &&
									queue[i]->row == bankStates[nextRankPRE][nextBankPRE].openRowAddress)
							{
								found = true;
								break;
							}
						}

						//if nothing found going to that bank and row or too many accesses have happend, close it
						if (!found || rowAccessCounters[nextRankPRE][nextBankPRE]==TOTAL_ROW_ACCESSES)
						{
							if (currentClockCycle >= bankStates[nextRankPRE][nextBankPRE].nextPrecharge)
							{
								sendingPRE = true;
								rowAccessCounters[nextRankPRE][nextBankPRE] = 0;
								*busPacket = new BusPacket(PRECHARGE, 0, 0, 0, nextRankPRE, nextBankPRE, 0, 0, dramsim_log);
								break;
							}
						}
					}
					nextRankAndBank(nextRankPRE, nextBankPRE);
				}
				while (!(startingRank == nextRankPRE && startingBank == nextBankPRE));

				//if no PREs could be sent, just return false
				if (!sendingPRE) return false;
			}
		}
	}

	//sendAct is flag used for posted-cas
	//  posted-cas is enabled when AL>0
	//  when sendAct is true, when don't want to increment our indexes
	//  so we send the column access that is paid with this act
	if (AL>0 && sendAct)
	{
		sendAct = false;
	}
	else
	{
		sendAct = true;
		nextRankAndBank(nextRank, nextBank);
	}
	//if its an activate, add a tfaw counter
	if ((*busPacket)->busPacketType==ACTIVATE)
	{
		tFAWCountdown[(*busPacket)->rank].push_back(tFAW);
        queuing_delay += currentClockCycle - (*busPacket)->arrivalTime;
        num_issued++;
	}
    
	return true;
}

//check if a rank/bank queue has room for a certain number of bus packets
bool CommandQueue::hasRoomFor(unsigned numberToEnqueue, unsigned rank, unsigned bank_or_domain)
{
	vector<BusPacket *> &queue = getCommandQueue(rank, bank_or_domain); 
	return (CMD_QUEUE_DEPTH - queue.size() >= numberToEnqueue);
}

//prints the contents of the command queue
void CommandQueue::print()
{
	if (queuingStructure==PerRank)
	{
		PRINT(endl << "== Printing Per Rank Queue" );
		for (size_t i=0;i<NUM_RANKS;i++)
		{
			PRINT(" = Rank " << i << "  size : " << queues[i][0].size() );
			for (size_t j=0;j<queues[i][0].size();j++)
			{
				PRINTN("    "<< j << "]");
				queues[i][0][j]->print();
			}
		}
	}
	else if (queuingStructure==PerRankPerBank)
	{
		PRINT("\n== Printing Per Rank, Per Bank Queue" );

		for (size_t i=0;i<NUM_RANKS;i++)
		{
			PRINT(" = Rank " << i );
			for (size_t j=0;j<NUM_BANKS;j++)
			{
				PRINT("    Bank "<< j << "   size : " << queues[i][j].size() );

				for (size_t k=0;k<queues[i][j].size();k++)
				{
					PRINTN("       " << k << "]");
					queues[i][j][k]->print();
				}
			}
		}
	}
	else if (queuingStructure==PerRankPerDomain)
	{
		PRINT("\n== Printing Per Rank, Per Domain Queue" );

		for (size_t i=0;i<NUM_RANKS;i++)
		{
			PRINT(" = Rank " << i );
			for (size_t j=0;j<num_pids;j++)
			{
				PRINT("    Domain "<< j << "   size : " << queues[i][j].size() );

				for (size_t k=0;k<queues[i][j].size();k++)
				{
					PRINTN("       " << k << "]");
					queues[i][j][k]->print();
				}
			}
		}
	}
}

/** 
 * return a reference to the queue for a given rank, bank. Since we
 * don't always have a per bank queuing structure, sometimes the bank
 * argument is ignored (and the 0th index is returned 
 */
vector<BusPacket *> &CommandQueue::getCommandQueue(unsigned rank, unsigned bank_or_domain)
{
	if (queuingStructure == PerRankPerBank)
	{
		return queues[rank][bank_or_domain];
	}
	else if (queuingStructure == PerRank)
	{
		return queues[rank][0];
	}
    else if (queuingStructure == PerRankPerDomain)
    {
        return queues[rank][bank_or_domain];
    }
	else
	{
		ERROR("Unknown queue structure");
		abort(); 
	}

}

//checks if busPacket is allowed to be issued
bool CommandQueue::isIssuable(BusPacket *busPacket)
{   
    switch (busPacket->busPacketType)
	{
	case REFRESH:

		break;
	case ACTIVATE:
		if ((bankStates[busPacket->rank][busPacket->bank].currentBankState == Idle ||
		        bankStates[busPacket->rank][busPacket->bank].currentBankState == Refreshing) &&
		        currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextActivate &&
		        tFAWCountdown[busPacket->rank].size() < 4)
		{
			return true;
		}
		else
		{
			return false;
		}
		break;
	case WRITE:
	case WRITE_P:
		if (bankStates[busPacket->rank][busPacket->bank].currentBankState == RowActive &&
		        currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextWrite &&
		        busPacket->row == bankStates[busPacket->rank][busPacket->bank].openRowAddress &&
                rowAccessCounters[busPacket->rank][busPacket->bank] < TOTAL_ROW_ACCESSES)
		{
			return true;
		}
		else
		{
			return false;
		}
		break;
	case READ_P:
	case READ:
		if (bankStates[busPacket->rank][busPacket->bank].currentBankState == RowActive &&
		        currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextRead &&
		        busPacket->row == bankStates[busPacket->rank][busPacket->bank].openRowAddress &&
                rowAccessCounters[busPacket->rank][busPacket->bank] < TOTAL_ROW_ACCESSES)
		{
			return true;
		}
		else
		{
			return false;
		}
		break;
	case PRECHARGE:
		if (bankStates[busPacket->rank][busPacket->bank].currentBankState == RowActive &&
		        currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextPrecharge)
		{
			return true;
		}
		else
		{
			return false;
		}
		break;
	default:
		ERROR("== Error - Trying to issue a crazy bus packet type : ");
		busPacket->print();
		exit(0);
	}
	return false;
}

//figures out if a rank's queue is empty
bool CommandQueue::isEmpty(unsigned rank)
{
	if (queuingStructure == PerRank)
	{
		return queues[rank][0].empty();
	}
	else if (queuingStructure == PerRankPerBank)
	{
		for (unsigned i=0;i<NUM_BANKS;i++)
		{
			if (!queues[rank][i].empty()) return false;
		}
		return true;
	}
    else if (queuingStructure == PerRankPerDomain)
    {
		for (unsigned i=0;i<num_pids;i++)
		{
			if (!queues[rank][i].empty()) return false;
		}
		return true;
    }
	else
	{
		DEBUG("Invalid Queueing Stucture");
		abort();
	}
}

//tells the command queue that a particular rank is in need of a refresh
void CommandQueue::needRefresh(unsigned rank)
{
	refreshWaiting = true;
	refreshRank = rank;
}

void CommandQueue::nextRankAndBank(unsigned &rank, unsigned &bank)
{
	if (schedulingPolicy == RankThenBankRoundRobin)
	{
		rank++;
		if (rank == NUM_RANKS)
		{
			rank = 0;
			bank++;
			if (bank == NUM_BANKS)
			{
				bank = 0;
			}
		}
	}
	//bank-then-rank round robin
	else if (schedulingPolicy == BankThenRankRoundRobin)
	{
		bank++;
		if (bank == NUM_BANKS)
		{
			bank = 0;
			rank++;
			if (rank == NUM_RANKS)
			{
				rank = 0;
			}
		}
	}
    else if (schedulingPolicy == SecMem || schedulingPolicy == FixedService)
    {
		bank++;
		if (bank == NUM_BANKS)
		{
			bank = 0;
			rank++;
			if (rank == NUM_RANKS)
			{
				rank = 0;
			}
		}
    }
    else if (schedulingPolicy == BankPar || schedulingPolicy == RankPar)
    {
		bank++;
		if (bank == NUM_BANKS)
		{
			bank = 0;
			rank++;
			if (rank == NUM_RANKS)
			{
				rank = 0;
			}
		}
    }
    else if (schedulingPolicy == SideChannel || schedulingPolicy == Dynamic)
    {
		bank++;
		if (bank == NUM_BANKS)
		{
			bank = 0;
			rank++;
			if (rank == NUM_RANKS)
			{
				rank = 0;
			}
		}
    }
    else if (schedulingPolicy == Probability)
    {
		bank++;
		if (bank == NUM_BANKS)
		{
			bank = 0;
			rank++;
			if (rank == NUM_RANKS)
			{
				rank = 0;
			}
		}
    }
    else if (schedulingPolicy == AccessLimit)
    {
		bank++;
		if (bank == NUM_BANKS)
		{
			bank = 0;
			rank++;
			if (rank == NUM_RANKS)
			{
				rank = 0;
			}
		}
    }
	else
	{
		ERROR("== Error - Unknown scheduling policy");
		exit(0);
	}

}

void CommandQueue::update()
{
	//do nothing since pop() is effectively update(),
	//needed for SimulatorObject
	//TODO: make CommandQueue not a SimulatorObject
}

unsigned CommandQueue::getCurrentDomain()
{
    return (currentClockCycle/turn_length)%num_pids;
}

unsigned* CommandQueue::selectRanks(pair <unsigned, unsigned> * rankRequests, unsigned num_ranks)
{
    static unsigned chosenRanks[3];
    pair<unsigned, unsigned> *rankRequests_ = new pair<unsigned, unsigned>[NUM_RANKS];
    for (size_t i=0;i<NUM_RANKS;i++) rankRequests_[i] = rankRequests[i];
    for (size_t i=0;i<3;i++) chosenRanks[i] = 0;
    // record the id the the ranks
    unsigned *id = new unsigned[num_ranks];
    for (size_t i=0;i<num_ranks;i++) id[i] = i;
    
    // bubble sort
    pair<unsigned, unsigned> temp;
    unsigned id_temp;
    for (size_t i=0;i<num_ranks;i++)
        for (size_t j=0;j<num_ranks-1-i;j++)
    {
        if (rankRequests_[j].first < rankRequests_[j+1].first)
        {
            temp = rankRequests_[j];
            rankRequests_[j] = rankRequests_[j+1];
            rankRequests_[j+1] = temp;
            // swap the ids
            id_temp = id[j];
            id[j] = id[j+1];
            id[j+1] = id_temp;
        }
        else if ((rankRequests_[j].first == rankRequests_[j+1].first) && (rankRequests_[j].second < rankRequests_[j+1].second))
        {
            temp = rankRequests_[j];
            rankRequests_[j] = rankRequests_[j+1];
            rankRequests_[j+1] = temp;
            // swap the ids
            id_temp = id[j];
            id[j] = id[j+1];
            id[j+1] = id_temp;
        }
    }
    
    //Yao: there must be at least four ranks
    unsigned lastPos = 0;
    for (size_t i=0;i<3;i++)
    {
        if (rankRequests_[i].first != 0) 
        {
            chosenRanks[i] = id[i];
            lastPos = i+1;
        }
        else
            break;
    }

    unsigned* r = new unsigned[NUM_RANKS-lastPos];
    for (size_t i=0;i<NUM_RANKS-lastPos;i++)
    {
        r[i]=i;
    }
    for (int i=NUM_RANKS-lastPos-1; i>=0;i--){
        //generate a random number [0, n-1]
        unsigned j = rand() % (i+1);

        //swap the last element with element at random index
        unsigned t = r[i];
        r[i] = r[j];
        r[j] = t;
    }

    unsigned curPos = 0;    
    for (size_t i=lastPos;i<3;i++)
    {
        for (size_t j=curPos;j<NUM_RANKS-lastPos;j++)
        {
            if (id[lastPos+r[j]] != getRefreshRank() && id[lastPos+r[j]] != refreshRank)
            {
                chosenRanks[i] = id[lastPos+r[j]];
                curPos = j+1;
                break;
            }
        }       
    }

    return chosenRanks;
}

unsigned CommandQueue::getRefreshRank()
{
    unsigned refresh_interval = (REFRESH_PERIOD/tCK)/NUM_RANKS;
    unsigned turn_start = currentClockCycle - (currentClockCycle%turn_length);
    unsigned in_refresh = (turn_start + WORST_CASE) % refresh_interval;
    // will be in refresh period of some rank
    if (in_refresh <= tRFC)
    {
        unsigned rank = (((turn_start + WORST_CASE) / refresh_interval) % NUM_RANKS + NUM_RANKS - 1) % NUM_RANKS;
        return rank;
    }
    else
        return 1000;
}

void CommandQueue::delay(unsigned domain, unsigned adjust_delay)
{
    for (size_t i=0;i<NUM_RANKS;i++)
    {
        vector<BusPacket *> &queue = getCommandQueue(i, domain);
        for (size_t j=0;j<queue.size();j++)
        {
            queue[j]->issueTime = queue[j]->issueTime + adjust_delay;
        }
    }
}