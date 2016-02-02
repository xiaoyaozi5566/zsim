#include "CommandQueueDS.h"

using namespace DRAMSim;

CommandQueueDS::CommandQueueDS(vector< vector<BankState> > &states, 
        ostream &dramsim_log_, unsigned num_pids_):
    CommandQueue(states,dramsim_log_,num_pids_)
{}

void CommandQueueDS::enqueue(BusPacket *newBusPacket)
{
    unsigned rank = newBusPacket->rank;
    // Yao: need to get the correct threadID
    unsigned pid = 0;
    queues[rank][pid].push_back(newBusPacket);
    if (queues[rank][pid].size()>CMD_QUEUE_DEPTH)
    {
        ERROR("== Error - Enqueued more than allowed in command queue");
        ERROR("						Need to call .hasRoomFor(int "
                "numberToEnqueue, unsigned rank, unsigned bank) first");
        exit(0);
    }
}

void CommandQueueDS::print()
{
    PRINT("\n== Printing Timing Partitioning Command Queue" );

    for (unsigned i=0;i<NUM_RANKS;i++)
    {
        PRINT(" = Rank " << i );
        for (unsigned j=0;j<num_pids;j++)
        {
            PRINT("    PID "<< j << "   size : " << queues[i][j].size() );

            for (unsigned k=0;k<queues[i][j].size();k++)
            {
                PRINTN("       " << k << "]");
                queues[i][j][k]->print();
            }
        }
    }
}
