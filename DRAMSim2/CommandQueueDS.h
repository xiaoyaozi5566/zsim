#include "CommandQueue.h"

#define BLOCK_TIME 12
// #define DEBUG_TP

using namespace std;

namespace DRAMSim
{
    class CommandQueueDS : public CommandQueue
    {
        public:
            CommandQueueDS(vector< vector<BankState> > &states,
                    ostream &dramsim_log_, unsigned num_pids);
            virtual void enqueue(BusPacket *newBusPacket);
            virtual void print();
    };
}
