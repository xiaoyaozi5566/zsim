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

    for (size_t i=0;i<2;i++)
    {
        BusPacket1D perRankQueue = BusPacket1D();
        cmdBuffer.push_back(perRankQueue);
        vector<unsigned> perRankTime;
        issue_time.push_back(perRankTime);
    }
    
    for (size_t i=0;i<2;i++)
        for (size_t j=0;j<3;j++)
            previousBanks[i][j] = 1000;
        
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
    for (size_t i=0;i<2;i++)
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
    // printf("enqueue packet %lx @ cycle %ld\n", newBusPacket->physicalAddress, currentClockCycle);
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
		if (queuingStructure == PerRankPerDomain)
        {
            unsigned rel_time = currentClockCycle % turn_length;
            
            // at the start of a turn, decide what transactions to issue
            if (rel_time == 0)
            {
                // printf("enter scheduling cycle 0\n");
                pair<unsigned, unsigned> *rankRequests = new pair<unsigned, unsigned>[NUM_RANKS];
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
                        if (banksToAccess.size() == 3) break;
                    }
                    rankRequests[i] = make_pair(banksToAccess.size(), queue.size());
                }
                // printf("enter scheduling cycle 1\n");
                pair<unsigned, unsigned> temp = selectRanks(rankRequests, NUM_RANKS);
                unsigned chosenRanks[2];
                chosenRanks[0] = temp.first;
                chosenRanks[1] = temp.second;
                if (previousRanks.size() > 1)
                {
                    if (previousRanks[1] == chosenRanks[0] || previousRanks[0] == chosenRanks[1])
                    // we need to swap the ranks to meet the timing constraints
                    {
                        unsigned temp = chosenRanks[0];
                        chosenRanks[0] = chosenRanks[1];
                        chosenRanks[1] = temp;
                    }
                }
                // update previous ranks
                previousRanks.clear();
                for (size_t i=0;i<2;i++) previousRanks.push_back(chosenRanks[i]);
                
                // printf("enter scheduling cycle 2\n");
                // Add bus packet to command buffers to be issued.
                for (size_t i=0;i<2;i++)
                {
                    // printf("enter scheduling cycle 3\n");
                    vector<BusPacket *> &queue = getCommandQueue(chosenRanks[i], getCurrentDomain());
                    // record packet index used to remove items from queue later
                    vector<unsigned> items_to_remove;
                    set<unsigned> banksToAccess;
                    unsigned tempBanks[3];
                    for (size_t j=0;j<3;j++)
                        tempBanks[j] = 1000;
                    // array to track each slot is taken or not
                    // Yao: the algorithm here can be improved
                    bool slot_status[3];
                    for (size_t j=0;j<3;j++)
                        slot_status[j] = false;
                    // printf("enter scheduling cycle 4\n");
                    // printf("rank %d queue size %d\n", chosenRanks[i], queue.size());
                    for (size_t j=0;j<queue.size();j++)
                    {
                        unsigned bank = queue[j]->bank;
                        if (banksToAccess.find(bank) == banksToAccess.end() && queue[j]->busPacketType==ACTIVATE)
                        {
                            // decide which slot to issue this request
                            unsigned order;
                            if (bank == previousBanks[i][2]) 
                            {
                                order = 2;
                            }   
                            else if (bank == previousBanks[i][1]) 
                            {
                                order = 1;
                            }
                            else order = 0;

                            for (size_t k=order;k<3;k++)
                            {
                                if (!slot_status[k])
                                {
                                    order = k;
                                    slot_status[k] = true;
                                    break;
                                }
                            }
                            tempBanks[order] = bank;
                            
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
                        if (banksToAccess.size() == 3) break;
                    }
                    // printf("enter scheduling cycle 5\n");
                    for (int j=items_to_remove.size()-1;j>=0;j--)
                    {
                        queue.erase(queue.begin()+items_to_remove[j]+1);
                        queue.erase(queue.begin()+items_to_remove[j]);
                    }
                    // printf("enter scheduling cycle 6\n");
                    // Update previous banks
                    for (size_t j=0;j<3;j++)
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
            unsigned foundIssuable = false;
            for (size_t i=0;i<2;i++)
            {
                for (size_t j=0;j<issue_time[i].size();j++)
                {
                    if (issue_time[i][j] < currentClockCycle)
                        printf("issue time smaller than current clock cycle!\n");
                    if (issue_time[i][j] == currentClockCycle)
                    {
                        *busPacket = cmdBuffer[i][j];
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

pair<unsigned, unsigned> CommandQueue::selectRanks(pair <unsigned, unsigned> * rankRequests, unsigned num_ranks)
{
    // record the id the the ranks
    unsigned *id = new unsigned[num_ranks];
    unsigned firstRank = 0, secondRank = 0;
    for (size_t i=0;i<num_ranks;i++) id[i] = i;
    
    // bubble sort
    pair <unsigned, unsigned> temp;
    unsigned id_temp;
    for (size_t i=0;i<num_ranks;i++)
        for (size_t j=0;j<num_ranks-1-i;j++)
    {
        if (rankRequests[j].first < rankRequests[j+1].first)
        {
            temp = rankRequests[j];
            rankRequests[j] = rankRequests[j+1];
            rankRequests[j+1] = temp;
            // swap the ids
            id_temp = id[j];
            id[j] = id[j+1];
            id[j+1] = id_temp;
        }
        else if ((rankRequests[j].first == rankRequests[j+1].first) && (rankRequests[j].second < rankRequests[j+1].second))
        {
            temp = rankRequests[j];
            rankRequests[j] = rankRequests[j+1];
            rankRequests[j+1] = temp;
            // swap the ids
            id_temp = id[j];
            id[j] = id[j+1];
            id[j+1] = id_temp;
        }
    }
    
    unsigned firstPos = 0;
    for (size_t i=0;i<num_ranks;i++)
    {
        if (id[i] != getRefreshRank() && id[i] != refreshRank)
        {
            firstRank = id[i];
            firstPos = i;
            break;
        }
    }
    for (size_t i=firstPos+1;i<num_ranks;i++)
    {
        if (id[i] != getRefreshRank() && id[i] != refreshRank)
        {
            secondRank = id[i];
            break;
        }
    }
    return make_pair(firstRank, secondRank);
}

unsigned CommandQueue::getRefreshRank()
{
    unsigned refresh_interval = (REFRESH_PERIOD/tCK)/NUM_RANKS;
    unsigned in_refresh = (currentClockCycle + WORST_CASE) % refresh_interval;
    // will be in refresh period of some rank
    if (in_refresh <= tRFC)
    {
        unsigned rank = (((currentClockCycle + WORST_CASE) / refresh_interval) % NUM_RANKS + NUM_RANKS - 1) % NUM_RANKS;
        return rank;
    }
    else
        return 1000;
}
