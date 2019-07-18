/**
 * Computer player
 *
 * (1) Connects to a game communication channel,
 * (2) Waits for a game position requiring to draw a move,
 * (3) Does a best move search, and broadcasts the resulting position,
 *     Jump to (2)
 *
 * (C) 2005-2015, Josef Weidendorfer, GPLv2+
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "board.h"
#include "search.h"
#include "eval.h"
#include "network.h"
#include "mpi.h"

#define TAG_RESULT 0
#define TAG_RESULT_2 5
#define TAG_ASK_FOR_JOB 1
#define TAG_JOB_DATA 2
#define TAG_JOB_DATA_2 4
#define TAG_STOP 3
#define TAG_DO_NOTHING 6

#define MOVE_ARRAY_SIZE 150


/* Global, static vars */
NetworkLoop l;
Board myBoard;
Evaluator ev;

/* Which color to play? */
int myColor = Board::color1;

/* Which search strategy to use? */
int strategyNo = 0;

/* Max search depth */
int maxDepth = 0;

/* Maximal number of moves before terminating (negative for infinity) */
int maxMoves = -1;

/* to set verbosity of NetworkLoop implementation */
extern int verbose;

/* remote channel */
char* host = 0;       /* not used on default */
int rport = 23412;

/* local channel */
int lport = 23412;

/* change evaluation after move? */
bool changeEval = true;

int  numtasks, rank;

//generate moves for worker threads - global vars
bool g_first_generation;

int g_firts_move_number;
Move g_first_move, g_second_move;
MoveList g_first_moves_list, g_second_moves_list;


/**
 * MyDomain
 *
 * Class for communication handling for player:
 * - start search for best move if a position is received
 *   in which this player is about to draw
 */
class MyDomain: public NetworkDomain
{
public:
    MyDomain(int p) : NetworkDomain(p) { sent = 0; }

    void sendBoard(Board*);

protected:
    void received(char* str);
    void newConnection(Connection*);

private:
    Board* sent;
    bool generate_move();
};

void MyDomain::sendBoard(Board* b)
{
    if (b) {
	static char tmp[500];
	sprintf(tmp, "pos %s\n", b->getState());
	if (verbose) printf("%s", tmp+4);
	broadcast(tmp);
    }
    sent = b;
}

void MyDomain::received(char* str)
{
    if (strncmp(str, "quit", 4)==0) {
        ////////////////////////////////MPI
        int i;
        for (i = 1; i < numtasks; i++) {
            MPI_Recv(str, 0, MPI_CHAR, i, TAG_ASK_FOR_JOB, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(str, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD);
        }
        ////////////////////////////////
    	l.exit();
    	return;
    }

    if (strncmp(str, "pos ", 4)!=0) return;

    // on receiving remote position, do not broadcast own board any longer
    sent = 0;

    myBoard.setState(str+4);
    if (verbose) {
	       printf("\n\n==========================================\n%s", str+4);
    }

    int state = myBoard.validState();
    if ((state != Board::valid1) && (state != Board::valid2)) {
    	printf("%s\n", Board::stateDescription(state));
    	switch(state) {
    	    case Board::timeout1:
    	    case Board::timeout2:
    	    case Board::win1:
    	    case Board::win2:
            ////////////////////////////////MPI
            int i;
            for (i = 1; i < numtasks; i++) {
                MPI_Recv(str, 0, MPI_CHAR, i, TAG_ASK_FOR_JOB, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(str, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD);
            }
            ////////////////////////////////
    		l.exit();
    	    default:
    		break;
    	}
    	return;
    }

    if (myBoard.actColor() & myColor) {
    	struct timeval t1, t2;

    	gettimeofday(&t1,0);

        ////////////////////////////////MPI

        bool is_next_move = true;
        g_first_moves_list.clear();
        myBoard.generateMoves(g_first_moves_list);
        g_first_generation = true;
        g_firts_move_number = 0;

        //for each move, store its best move (min round ->store minimum)
        int best_eval_array[MOVE_ARRAY_SIZE];
        Move first_move_array[MOVE_ARRAY_SIZE];
        Move percieved_second_move_array[MOVE_ARRAY_SIZE];
        int i;
        for(i=0; i< MOVE_ARRAY_SIZE; i++ )
        {
            best_eval_array[i]=99999;
        }

        MPI_Status status, status2;
        MPI_Request message_type, data_send_1, data_send_2, data_recv_1, data_recv_2, request;
        char tmp_buf [2];
        int tasks_created = 0;
        int tasks_completed = 0;
        int send_move_data[7], recv_move_data[7];//0-2 = first move, 3-5 = second move, 6=first move index
        while(true)
        {

            if(tasks_created == tasks_completed && !is_next_move)
            {
                if (verbose>1){printf("all moves evaluated .. %d \n", tasks_completed);}
                break;
            }
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            int slave_rank = status.MPI_SOURCE;
            if (status.MPI_TAG == TAG_ASK_FOR_JOB)
            {
                MPI_Irecv(tmp_buf, 0, MPI_CHAR, slave_rank, TAG_ASK_FOR_JOB, MPI_COMM_WORLD, &message_type);
                if(is_next_move)
                {
                    //generate a move into global variables g_first_move g_second_move
                    is_next_move = generate_move();
                    if(is_next_move)
                    {
                        MPI_Isend(str, 1024, MPI_CHAR, slave_rank, TAG_JOB_DATA, MPI_COMM_WORLD, &data_send_1);
                        send_move_data[0] = (int)g_first_move.field;
                        send_move_data[1] = (int)g_first_move.direction;
                        send_move_data[2] = g_first_move.type;
                        send_move_data[3] = (int)g_second_move.field;
                        send_move_data[4] = (int)g_second_move.direction;
                        send_move_data[5] = g_second_move.type;
                        send_move_data[6] = g_firts_move_number;//??const void *
                        MPI_Isend(send_move_data, 7, MPI_INT, slave_rank, TAG_JOB_DATA_2, MPI_COMM_WORLD, &data_send_2);
                        if (verbose>1){printf("sending data no.%d to proc %d \n", tasks_created, slave_rank);}
                        tasks_created++;
                    }
                }

                if(!is_next_move) //there is no next move but have to wait for other workers to finish the round
                {
                    MPI_Isend(str, 0, MPI_CHAR, slave_rank, TAG_DO_NOTHING, MPI_COMM_WORLD, &request ) ;
                    if (verbose>1)
                        printf("no further move to test in this round .. (total %d) \n", tasks_created);
                    is_next_move = false;
                }
            }
            else if (status.MPI_TAG == TAG_RESULT)
            {
                if (verbose>1){printf("receiving data no.%d from proc %d \n", tasks_completed, slave_rank);}

                int worker_eval;
                MPI_Irecv(&worker_eval, 1, MPI_INT, slave_rank, TAG_RESULT, MPI_COMM_WORLD, &data_recv_1);
                MPI_Irecv(recv_move_data, 7, MPI_INT, slave_rank, TAG_RESULT_2, MPI_COMM_WORLD, &data_recv_2);

                MPI_Wait(&data_recv_1, &status2);
                MPI_Wait(&data_recv_2, &status2);
                if (worker_eval < best_eval_array[recv_move_data[6]]) // search for min in each subtree
                {
                    best_eval_array[recv_move_data[6]] = worker_eval;
                    first_move_array[recv_move_data[6]] = Move((short)recv_move_data[0], (unsigned char)recv_move_data[1], (Move::MoveType)recv_move_data[2]);
                    percieved_second_move_array[recv_move_data[6]] = Move((short)recv_move_data[3], (unsigned char)recv_move_data[4], (Move::MoveType)recv_move_data[5]);
                    if (verbose>1){
                        printf("found new best eval %d from %d - %s ; %s \n", worker_eval, slave_rank, first_move_array[recv_move_data[6]].name(), percieved_second_move_array[recv_move_data[6]].name());
                        //myBoard.print();
                    }
                }
                tasks_completed++;
            }
        }

        int bestEval = -99999;
        Move bestMove, percieved_second_move;
        for(i=0;i<g_firts_move_number; i++)
        {
            if(best_eval_array[i] > bestEval)
            {
                bestEval = best_eval_array[i];
                bestMove = first_move_array[i];
                percieved_second_move = percieved_second_move_array[i];
            }
        }
        ////////////////////////////////

        ////////////////////////////////Sequential
        //Move bestMove = myBoard.bestMove();
        ////////////////////////////////

        gettimeofday(&t2,0);

    	int msecsPassed =
    	    (1000* t2.tv_sec + t2.tv_usec / 1000) -
    	    (1000* t1.tv_sec + t1.tv_usec / 1000);

    	printf("%s ", (myColor == Board::color1) ? "O":"X");
    	if (bestMove.type == Move::none) {
    	    printf(" can not draw any move ?! Sorry.\n");
    	    return;
    	}
    	printf("draws '%s' (after %d.%03d secs)...\n",
    	       bestMove.name(), msecsPassed/1000, msecsPassed%1000);

    	myBoard.playMove(bestMove, msecsPassed);
    	sendBoard(&myBoard);

    	if (changeEval)
    	    ev.changeEvaluation();

    	/* stop player at win position */
    	int state = myBoard.validState();
    	if ((state != Board::valid1) &&
    	    (state != Board::valid2)) {
    	    printf("%s\n", Board::stateDescription(state));
    	    switch(state) {
    		case Board::timeout1:
    		case Board::timeout2:
    		case Board::win1:
    		case Board::win2:
    		    l.exit();
    		default:
    		    break;
    	    }
    	}

    	maxMoves--;
    	if (maxMoves == 0) {
    	    printf("Terminating because given number of moves drawn.\n");
    	    broadcast("quit\n");
    	    l.exit();
    	}
    }
}

MoveList * tmp_g_first_moves_list;
Move tmp_move_array [150];

bool MyDomain::generate_move()
{
    // Move g_first_move, g_second_move;
    // MoveList g_first_moves_list, g_second_moves_list;


    //play first move in the first round and generate list of second moves
    if(g_first_generation)
    {
        g_first_generation = false;
        g_firts_move_number = 0;

        //bool move_available = g_first_moves_list.getNext(g_first_move);
        if(g_first_moves_list.getNext(g_first_move))
        {
            myBoard.playMove(g_first_move);
            g_second_moves_list.clear();
            myBoard.generateMoves(g_second_moves_list);
        }
        else
        {
            printf("!!!SHOULD NOT HAPPEN 2\n \n \n \n \n \n \n \n generate_move - false.  %s\n", g_first_move.name());
            return false;
        }
    }
    if(!g_second_moves_list.getNext(g_second_move)) //try playing next first move and generate second moves for it
    {
        myBoard.takeBack();
        if(g_first_moves_list.getNext(g_first_move))
        {
            myBoard.playMove(g_first_move);
            g_second_moves_list.clear();
            myBoard.generateMoves(g_second_moves_list);
            //printf("printing board after play_move %d \n", g_firts_move_number);
            //myBoard.print();
            g_firts_move_number++;
            if(!g_second_moves_list.getNext(g_second_move))
            {
                printf("!!!SHOULD NOT HAPPEN 1\n \n \n \n \n \n \n \n generate_move - no more first moves. %d  %s xxxxx %s \n",g_firts_move_number,  g_first_move.name(), g_second_move.name());
                return false;
            }
        }
        else //no more first move
        {
            return false;
        }
        //now second move is generated
    }
    //the two moves are in g_first_move, g_second_move
    return true;
}

void MyDomain::newConnection(Connection* c)
{
    NetworkDomain::newConnection(c);

    if (sent) {
	static char tmp[500];
	int len = sprintf(tmp, "pos %s\n", sent->getState());
	c->sendString(tmp, len);
    }
}

/*
 * Main program
 */

void printHelp(char* prg, bool printHeader)
{
    if (printHeader)
	printf("Computer player V 0.2\n"
	       "Search for a move on receiving a position in which we are expected to draw.\n\n");

    printf("Usage: %s [options] [X|O] [<strength>]\n\n"
	   "  X                Play side X\n"
	   "  O                Play side O (default)\n"
	   "  <strength>       Playing strength, depending on strategy\n"
	   "                   A time limit can reduce this\n\n" ,
	   prg);
    printf(" Options:\n"
	   "  -h / --help      Print this help text\n"
	   "  -v / -vv         Be verbose / more verbose\n"
	   "  -s <strategy>    Number of strategy to use for computer (see below)\n"
	   "  -n               Do not change evaluation function after own moves\n"
	   "  -<integer>       Maximal number of moves before terminating\n"
	   "  -p [host:][port] Connection to broadcast channel\n"
	   "                   (default: 23412)\n\n");

    printf(" Available search strategies for option '-s':\n");

    const char** strs = SearchStrategy::strategies();
    for(int i = 0; strs[i]; i++)
	printf("  %2d : Strategy '%s'%s\n", i, strs[i],
	       (i==strategyNo) ? " (default)":"");
    printf("\n");

    exit(1);
}

void parseArgs(int argc, char* argv[])
{
    int arg=0;
    while(arg+1<argc) {
	arg++;
	if (strcmp(argv[arg],"-h")==0 ||
	    strcmp(argv[arg],"--help")==0) printHelp(argv[0], true);
	if (strncmp(argv[arg],"-v",2)==0) {
	    verbose = 1;
	    while(argv[arg][verbose+1] == 'v') verbose++;
	    continue;
	}
	if (strcmp(argv[arg],"-n")==0)	{
	    changeEval = false;
	    continue;
	}
	if ((strcmp(argv[arg],"-s")==0) && (arg+1<argc)) {
	    arg++;
	    if (argv[arg][0]>='0' && argv[arg][0]<='9')
               strategyNo = argv[arg][0] - '0';
            continue;
        }

	if ((argv[arg][0] == '-') &&
	    (argv[arg][1] >= '0') &&
	    (argv[arg][1] <= '9')) {
	    int pos = 2;

	    maxMoves = argv[arg][1] - '0';
	    while((argv[arg][pos] >= '0') &&
		  (argv[arg][pos] <= '9')) {
		maxMoves = maxMoves * 10 + argv[arg][pos] - '0';
		pos++;
	    }
	    continue;
	}

	if ((strcmp(argv[arg],"-p")==0) && (arg+1<argc)) {
	    arg++;
	    if (argv[arg][0]>'0' && argv[arg][0]<='9') {
		lport = atoi(argv[arg]);
		continue;
	    }
	    char* c = strrchr(argv[arg],':');
	    int p = 0;
	    if (c != 0) {
		*c = 0;
		p = atoi(c+1);
	    }
	    host = argv[arg];
	    if (p) rport = p;
	    continue;
	}

	if (argv[arg][0] == 'X') {
	    myColor = Board::color2;
	    continue;
	}
	if (argv[arg][0] == 'O') {
	    myColor = Board::color1;
	    continue;
	}

	int strength = atoi(argv[arg]);
	if (strength == 0) {
	    printf("ERROR - Unknown option %s\n", argv[arg]);
	    printHelp(argv[0], false);
	}

	maxDepth = strength;
    }
}

int main(int argc, char* argv[])
{
    ////////////////////////////////MPI
    //initialize MPI
    int len;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Get_processor_name(hostname, &len);
    ////////////////////////////////

    parseArgs(argc, argv);

    SearchStrategy* ss = SearchStrategy::create(strategyNo);
    ss->setMaxDepth(maxDepth);
    printf("Using strategy '%s' (depth %d) ...\n", ss->name(), maxDepth);

    myBoard.setSearchStrategy( ss );
    ss->setEvaluator(&ev);
    ss->registerCallbacks(new SearchCallbacks(verbose));

    ////////////////////////////////MPI
    MPI_Barrier(MPI_COMM_WORLD);
    if(rank == 0)
    {
    ////////////////////////////////
        MyDomain d(lport);
        l.install(&d);

        if (host) d.addConnection(host, rport);

        l.run();
    ////////////////////////////////MPI
    }
    else
    {
        MPI_Status status, status2;
        MPI_Request data_send_1, data_send_2, data_recv_1, data_recv_2;
        int cnt = 0;
        while(true)
        {
            char board[1024];
            int recv_move_data[7];
            MPI_Send(board, 0 , MPI_CHAR, 0, TAG_ASK_FOR_JOB , MPI_COMM_WORLD ) ;
            MPI_Probe (0, MPI_ANY_TAG , MPI_COMM_WORLD , &status ) ;
            if ( status.MPI_TAG == TAG_JOB_DATA ) {
                Move m1, m2, best_move;
                if(cnt > 0)//after the first round, check that the previous sends were completed
                {
                    MPI_Wait(&data_send_1, &status2);
                    MPI_Wait(&data_send_2, &status2);
                }
                MPI_Irecv(board, 1024, MPI_CHAR, 0, TAG_JOB_DATA, MPI_COMM_WORLD, &data_recv_1);
                MPI_Irecv(recv_move_data, 7, MPI_INT, 0, TAG_JOB_DATA_2, MPI_COMM_WORLD, &data_recv_2);
                MPI_Wait(&data_recv_2, &status2);
                m1 = Move((short)recv_move_data[0], (unsigned char)recv_move_data[1], (Move::MoveType)recv_move_data[2]);
                m2 = Move((short)recv_move_data[3], (unsigned char)recv_move_data[4], (Move::MoveType)recv_move_data[5]);
                MPI_Wait(&data_recv_1, &status2);
                myBoard.setState(board+4);
                myBoard.playMove(m1);
                myBoard.playMove(m2);
                    //myBoard.print();
                    //printf("data no.%d from proc 0 playing move %s \n", cnt, m.name());
                myBoard.setStartingDepth(2);
                myBoard.bestMove();
                int my_eval = myBoard.getBestEval();

                MPI_Isend(&my_eval, 1, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD, &data_send_1);
                MPI_Isend(recv_move_data, 7, MPI_INT, 0, TAG_RESULT_2, MPI_COMM_WORLD, &data_send_2);

                cnt ++;
            }
            else if( status.MPI_TAG == TAG_DO_NOTHING){
                //wait until other workers finish the tasks for this ound but still ask for next job as it comes in the next round
                MPI_Recv(board, 0, MPI_CHAR, 0, TAG_DO_NOTHING, MPI_COMM_WORLD, &status2) ;
                usleep(100);
            }
            else if( status.MPI_TAG == TAG_STOP ){
                MPI_Recv(board, 0, MPI_CHAR, 0, TAG_STOP, MPI_COMM_WORLD, &status2);
                break;
            }
        }
    }
    MPI_Finalize();
    ////////////////////////////////


}
