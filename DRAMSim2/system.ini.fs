; COPY THIS FILE AND MODIFY IT TO SUIT YOUR NEEDS

NUM_CHANS=1								; number of *logically independent* channels (i.e. each with a separate memory controller); should be a power of 2
JEDEC_DATA_BUS_BITS=64 		 		; Always 64 for DDRx; if you want multiple *ganged* channels, set this to N*64
TRANS_QUEUE_DEPTH=32					; transaction queue, i.e., CPU-level commands such as:  READ 0xbeef
CMD_QUEUE_DEPTH=32						; command queue, i.e., DRAM-level commands such as: CAS 544, RAS 4
EPOCH_LENGTH=100000						; length of an epoch in cycles (granularity of simulation)
ROW_BUFFER_POLICY=close_page 		; close_page or open_page
ADDRESS_MAPPING_SCHEME=scheme2	;valid schemes 1-7; For multiple independent channels, use scheme7 since it has the most parallelism 
SCHEDULING_POLICY=fixed_service  ; bank_then_rank_round_robin or rank_then_bank_round_robin 
QUEUING_STRUCTURE=per_rank_per_domain			;per_rank or per_rank_per_bank

;for true/false, please use all lowercase
DEBUG_TRANS_Q=false
DEBUG_CMD_Q=false
DEBUG_ADDR_MAP=false
DEBUG_BUS=false
DEBUG_BANKSTATE=false
DEBUG_BANKS=false
DEBUG_POWER=false
VIS_FILE_OUTPUT=true

USE_LOW_POWER=false 					; go into low power mode when idle?
VERIFICATION_OUTPUT=false 			; should be false for normal operation
TOTAL_ROW_ACCESSES=4	; 				maximum number of open page requests to send to the same row before forcing a row close (to prevent starvation)
