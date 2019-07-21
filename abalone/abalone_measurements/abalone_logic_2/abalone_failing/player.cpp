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
#include <vector>
#include <tuple>
#include <iostream>

#include "board.h"
#include "search.h"
#include "eval.h"
#include "network.h"
#include "mpi.h"

#define TAG_RESULT 0
#define TAG_RESULT_2 1
#define TAG_ASK_FOR_JOB 2
#define TAG_JOB_DATA 3
#define TAG_JOB_DATA_2 4
#define TAG_STOP 5
#define TAG_DO_NOTHING 6
#define TAG_TERMINATE_COMPUTATION 7

#define TERMINATED_BEST_VAL 987654321
#define MOVE_ARRAY_SIZE 150
#define BOARD_SIZE 1024
#define MAX_EVAL_VALUE 99999

#define TIME_TO_PLAY (3 * 1000)


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

bool first_turn = true;
int mymsecsToPlay = 0;

int g_time_to_play = TIME_TO_PLAY;


//generate moves for worker threads - global vars
bool g_first_generation, g_sort_moves = false, g_all_child_moves_generated[MOVE_ARRAY_SIZE];
int g_first_move_index, g_first_moves_total, g_number_child_moves_total[MOVE_ARRAY_SIZE], g_number_child_moves_processed[MOVE_ARRAY_SIZE];
Move g_first_move, g_second_move;
MoveList g_first_moves_list, g_second_moves_list;
Move g_first_move_array[MOVE_ARRAY_SIZE];

std::vector<Move> g_first_move_vector;
using SecondMove = struct
{
    int parent;
    Move move;
};
std::vector<SecondMove> g_second_move_vector;
int g_second_move_index = 0;
int g_numSamples = 16;
int g_threshold = 50;


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
    void sampleMoves(std::vector<Move> &output);
    Move calculate_best_move(char* str, struct timeval start_time);
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


Move MyDomain::calculate_best_move(char* str, struct timeval t1)
{
    struct timeval t2;
    MPI_Status status, status2;
    MPI_Request message_type, data_send_1, data_send_2, data_recv_1, data_recv_2, request;
    int i;
    char tmp_buf [2];

    Move bestMove, bestMoveInCurrentDepth, percieved_second_move;

    int currentMaxDepth = 3;
    bool next_depth = true;
    while(next_depth)
    {

        ////////temporary
        if(currentMaxDepth > 9)
        {
            break;
        }
        ////////temporary

        g_first_moves_list.clear();
        myBoard.generateMoves(g_first_moves_list);
        g_first_moves_total = g_first_moves_list.getLength();
        //here sort the moves
        g_first_generation = true;
        g_first_move_index = 0;

        //for each move, store its best move (min round ->store minimum)
        int best_eval_array[g_first_moves_total]; // is equal to beta for given branch
        int best_eval = -MAX_EVAL_VALUE; // is equal to alpha
        int debug_branches_skipped = 0;

        Move percieved_second_move_array[g_first_moves_total];

        for(i=0; i< g_first_moves_total; i++ )
        {
            best_eval_array[i]=MAX_EVAL_VALUE;
            g_all_child_moves_generated[i]=false;
            g_number_child_moves_total[i]=0;
            g_number_child_moves_processed[i]=0;
        }


        int tasks_created = 0, tasks_completed = 0;
        bool is_next_move = true;
        while(is_next_move || tasks_created > tasks_completed) //loop until there is no new task to create and all pending tasks are calculated
        {
            int message_available;
            //MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &message_available, &status);
            if(message_available)
            {
                int slave_rank = status.MPI_SOURCE;
                if (status.MPI_TAG == TAG_ASK_FOR_JOB)
                {
                    if(is_next_move)
                    {
                        //generate a move into global variables g_first_move g_second_move
                        is_next_move = generate_move();
                        if(is_next_move)
                        {
                            if(best_eval >= best_eval_array[g_first_move_index])//cut off - do not create a task
                            {
                                //printf("        cut off %d for move %d, alpha %d beta %d \n", g_first_move_index, g_number_child_moves_processed[g_first_move_index], best_eval, best_eval_array[g_first_move_index]);
                                g_number_child_moves_processed[g_first_move_index]++;
                                continue;
                            }
                            else
                            {
                                int send_move_data[10];//0=first move index; 1-3 = first move, 4-6 = second move, 7=alpha, 8=beta, 9=maxDepth
                                MPI_Irecv(tmp_buf, 0, MPI_CHAR, slave_rank, TAG_ASK_FOR_JOB, MPI_COMM_WORLD, &message_type);
                                MPI_Isend(str, BOARD_SIZE, MPI_CHAR, slave_rank, TAG_JOB_DATA, MPI_COMM_WORLD, &data_send_1);
                                send_move_data[0] = g_first_move_index;
                                send_move_data[1] = (int)g_first_move.field;
                                send_move_data[2] = (int)g_first_move.direction;
                                send_move_data[3] = g_first_move.type;
                                send_move_data[4] = (int)g_second_move.field;
                                send_move_data[5] = (int)g_second_move.direction;
                                send_move_data[6] = g_second_move.type;
                                send_move_data[7] = best_eval;//current alpha
                                send_move_data[8] = best_eval_array[g_first_move_index];//cuttent beta
                                send_move_data[9] = currentMaxDepth;
                                MPI_Isend(send_move_data, 10, MPI_INT, slave_rank, TAG_JOB_DATA_2, MPI_COMM_WORLD, &data_send_2);
                                if (verbose>2)
                                    printf("sending data no.%d to proc %d \n", tasks_created, slave_rank);
                                tasks_created++;
                            }
                        }
                    }

                    if(!is_next_move) //there is no next move but have to wait for other workers to finish the round
                    {
                        MPI_Irecv(tmp_buf, 0, MPI_CHAR, slave_rank, TAG_ASK_FOR_JOB, MPI_COMM_WORLD, &message_type);
                        MPI_Isend(str, 0, MPI_CHAR, slave_rank, TAG_DO_NOTHING, MPI_COMM_WORLD, &request ) ;
                        if (verbose>1)
                            printf("no further move to test in this round .. (total %d) \n", tasks_created);
                        is_next_move = false;
                    }
                }
                else if (status.MPI_TAG == TAG_RESULT)
                {
                    if (verbose>2)
                        printf("receiving data no.%d from proc %d \n", tasks_completed, slave_rank);

                    int return_vals[5];//0=first move index; 1-3 = second move, 4 = eval;
                    MPI_Irecv(return_vals, 5, MPI_INT, slave_rank, TAG_RESULT, MPI_COMM_WORLD, &data_recv_1);
                    MPI_Wait(&data_recv_1, &status2);

                    int move_index = return_vals[0];
                    g_number_child_moves_processed[move_index]++;

                    if (return_vals[4] < best_eval_array[move_index]) // search for min in each subtree
                    {
                        best_eval_array[move_index] = return_vals[4];
                        percieved_second_move_array[move_index] = Move((short)return_vals[1], (unsigned char)return_vals[2], (Move::MoveType)return_vals[3]);
                        if (verbose>1)
                            printf("found new best eval %d from %d - %s \n", return_vals[4], slave_rank, g_first_move_array[move_index].name());
                    }

                    //if all sub-moves of this move are computed, check if best move can be updated
                    if(g_number_child_moves_total[move_index] == g_number_child_moves_processed[move_index])
                    {
                        if (verbose>2)
                            printf(" best eval %d from %d - move %d \n", best_eval_array[move_index], slave_rank, move_index);

                        if(best_eval_array[move_index] > best_eval )
                        {
                            best_eval = best_eval_array[move_index];
                            bestMoveInCurrentDepth = g_first_move_array[move_index];
                            percieved_second_move = percieved_second_move_array[move_index];
                            if (verbose>0)
                                printf("     found new GLOBAL best eval %d from %d - move %d \n", best_eval_array[move_index], slave_rank, move_index);
                        }
                    }
                    tasks_completed++;
                }
                else
                {
                    MPI_Irecv(tmp_buf, 0, MPI_CHAR, slave_rank, MPI_ANY_TAG, MPI_COMM_WORLD, &message_type);
                    printf("!!!!!!!!!!!!!!received tag %d \n", status.MPI_TAG);
                    usleep(100000);
                }
            }
            else
            {
                gettimeofday(&t2,0);
                int msecsPassed = (1000* t2.tv_sec + t2.tv_usec / 1000) - (1000* t1.tv_sec + t1.tv_usec / 1000);
                if(msecsPassed > g_time_to_play)
                {
                    int terminate = 1;
                    for (i = 1; i < numtasks; i++) {
                        MPI_Irsend(&terminate, 1, MPI_INT, i, TAG_TERMINATE_COMPUTATION, MPI_COMM_WORLD, &request );
                    }
                    printf("Cutting at depth: %d \n", currentMaxDepth);

                    myBoard.takeBack();
                    return bestMove;

                }
            }

        }
        if (verbose>1)
            printf("all moves evaluated .. %d \n", tasks_completed);

        int msecsPassed = (1000* t2.tv_sec + t2.tv_usec / 1000) - (1000* t1.tv_sec + t1.tv_usec / 1000);
        printf("depth: %d BestEval: %d after %d.%03d secs \n", currentMaxDepth, best_eval, msecsPassed/1000, msecsPassed%1000);
        bestMove = bestMoveInCurrentDepth;
        currentMaxDepth++;
    }
    return bestMove;
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
      if (first_turn) {
        first_turn = false;
        mymsecsToPlay = myBoard.msecsToPlay(myColor);
      }
      struct timeval t1, t2;

    	gettimeofday(&t1,0);

        ////////////////////////////////MPI

        Move bestMove = calculate_best_move(str, t1);
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

      if (myBoard.msecsToPlay(myColor) > (mymsecsToPlay * 0.5) && myBoard.msecsToPlay(myColor) <= (mymsecsToPlay * 0.75)) {
        g_time_to_play = TIME_TO_PLAY * 0.75;
      }

      if (myBoard.msecsToPlay(myColor) > (mymsecsToPlay * 0.25) && myBoard.msecsToPlay(myColor) <= (mymsecsToPlay * 0.5)) {
        g_time_to_play = TIME_TO_PLAY * 0.5;
      }

      if (myBoard.msecsToPlay(myColor) <= (mymsecsToPlay * 0.25)) {
        g_time_to_play = TIME_TO_PLAY * 0.25;
      }

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
    	}

    	maxMoves--;
    	if (maxMoves == 0) {
    	    printf("Terminating because given number of moves drawn.\n");
            ////////////////////////////////MPI
            int i;
            for (i = 1; i < numtasks; i++) {
                MPI_Recv(str, 0, MPI_CHAR, i, TAG_ASK_FOR_JOB, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(str, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD);
            }
            ////////////////////////////////
    	    broadcast("quit\n");
    	    l.exit();
    	}
    }
}

void MyDomain::sampleMoves(std::vector<Move> &output)
{
    Move m;
    using avtype = std::tuple<Move, int>;
    const int valueOf = 1;
    const int moveOf = 0;
    std::vector<avtype> actionValues;
    MoveList list;
    myBoard.generateMoves(list);
    while (list.getNext(m))
    {
        myBoard.playMove(m);
        auto v = ev.calcEvaluation(&myBoard);
        actionValues.push_back(std::make_tuple(m, v));
        myBoard.takeBack();
    }
    std::stable_sort(actionValues.begin(), actionValues.end(), [valueOf](avtype av1, avtype av2) {
        return std::get<valueOf>(av1) > std::get<valueOf>(av2);
    });
    int i;
    for (i = 0; i < std::min(g_numSamples, (int)actionValues.size()); i++)
    {
        output.push_back(std::get<moveOf>(actionValues[i]));
    }
    auto firstVal = std::get<valueOf>(actionValues[0]);
    while (true)
    {
        if (i >= actionValues.size())
            break;
        Move m;
        int val;
        std::tie(m, val) = actionValues[i];
        if ((firstVal - val) > g_threshold)
            break;
        output.push_back(m);
        i++;
    }
}

bool MyDomain::generate_move()
{
    if(g_sort_moves)
    {
        if (g_first_generation)
        {
            g_first_move_vector.clear();
            g_second_move_vector.clear();
            g_second_move_index = 0;
            g_first_generation = false;
            g_first_move_index = 0;
        }
        // Initializationx
        if (g_first_move_vector.empty())
        {
            sampleMoves(g_first_move_vector);
            std::vector<Move> temp;
            for (int i = 0; i < g_first_move_vector.size(); i++)
            {
                myBoard.playMove(g_first_move_vector[i]);
                sampleMoves(temp);
                g_number_child_moves_total[i] = temp.size();
                for (auto m: temp)
                    g_second_move_vector.push_back(SecondMove{i, m});
                myBoard.takeBack();
                temp.clear();
            }
        }
        if (g_second_move_index >= g_second_move_vector.size())
        {
            g_first_move_vector.clear();
            g_second_move_vector.clear();
            g_second_move_index = 0;
            return false;
        }
        // Everything is normal
        auto sm = g_second_move_vector[g_second_move_index];
        if (g_second_move_index >= 1) {
            auto sm_ = g_second_move_vector[g_second_move_index-1];
            if (sm.parent != sm_.parent) {
                myBoard.takeBack();
                myBoard.playMove(g_first_move_vector[sm.parent]);
            }
        } else if (g_second_move_index == 0) {
            myBoard.playMove(g_first_move_vector[sm.parent]);
        }
        g_first_move = g_first_move_vector[sm.parent];
        g_first_move_index = sm.parent;
        g_first_move_array[g_first_move_index] = g_first_move;
        g_second_move = sm.move;
        g_second_move_index++;
        return true;
    }
    else
    {
        //play first move in the first round and generate list of second moves
        if(g_first_generation)
        {
            g_first_generation = false;


            //bool move_available = g_first_moves_list.getNext(g_first_move);
            if(g_first_moves_list.getNext(g_first_move))
            {
                g_first_move_index = 0;
                myBoard.playMove(g_first_move);
                g_first_move_array[g_first_move_index] = g_first_move;

                g_second_moves_list.clear();
                myBoard.generateMoves(g_second_moves_list);
                g_number_child_moves_total[g_first_move_index] = g_second_moves_list.getLength(); //here
            }
            else
            {
                printf("!!!SHOULD NOT HAPPEN 2\n \n \n \n \n \n \n \n generate_move - false.  %s\n", g_first_move.name());
                return false;
            }
        }
        if(!g_second_moves_list.getNext(g_second_move)) //try playing next first move and generate second moves for it
        {
            //printf("generated all moves for %d \n", g_first_move_index);
            myBoard.takeBack();
            if(g_first_moves_list.getNext(g_first_move))
            {
                g_first_move_index++;
                myBoard.playMove(g_first_move);
                g_first_move_array[g_first_move_index] = g_first_move;

                g_second_moves_list.clear();
                myBoard.generateMoves(g_second_moves_list);
                g_number_child_moves_total[g_first_move_index] = g_second_moves_list.getLength(); //here

                if(!g_second_moves_list.getNext(g_second_move))
                {
                    printf("!!!SHOULD NOT HAPPEN 1\n \n \n \n \n \n \n \n generate_move - no more first moves. %d  %s xxxxx %s \n",g_first_move_index,  g_first_move.name(), g_second_move.name());
                    return false;
                }
            }
            else //no more first move
            {
                return false;
            }
        }
        //the two moves are in g_first_move, g_second_move
        return true;
    }
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

////////////////////////////////MPI
int worker_process()
{
    MPI_Status status, status2;
    MPI_Request data_send_1, data_send_2, data_recv_1, data_recv_2;
    int cnt = 0;
    int last_move_number = -1;
    while(true)
    {
        char board[BOARD_SIZE];
        MPI_Send(board, 0 , MPI_CHAR, 0, TAG_ASK_FOR_JOB , MPI_COMM_WORLD ) ;
        MPI_Probe (0, MPI_ANY_TAG , MPI_COMM_WORLD , &status ) ;
        if( status.MPI_TAG == TAG_JOB_DATA || status.MPI_TAG == TAG_JOB_DATA_2 )
        {
            Move m1, m2;
            if(cnt > 0)//after the first round, check that the previous sends were completed
            {
                MPI_Wait(&data_send_1, &status2);
            }
            int recv_move_data[10];//0=first move index; 1-3 = first move, 4-6 = second move, 7=alpha, 8=beta, 9=maxDepth
            MPI_Irecv(board, BOARD_SIZE, MPI_CHAR, 0, TAG_JOB_DATA, MPI_COMM_WORLD, &data_recv_1);
            MPI_Irecv(recv_move_data, 10, MPI_INT, 0, TAG_JOB_DATA_2, MPI_COMM_WORLD, &data_recv_2);
            MPI_Wait(&data_recv_2, &status2);
            m1 = Move((short)recv_move_data[1], (unsigned char)recv_move_data[2], (Move::MoveType)recv_move_data[3]);
            m2 = Move((short)recv_move_data[4], (unsigned char)recv_move_data[5], (Move::MoveType)recv_move_data[6]);
            MPI_Wait(&data_recv_1, &status2);

            myBoard.setState(board+4);
            if(myBoard.getMoveNo() == last_move_number)
            {
                myBoard.setCallReceive(0);
            }
            else
            {
                last_move_number = myBoard.getMoveNo();
                myBoard.setCallReceive(1);
            }
            myBoard.playMove(m1);
            myBoard.playMove(m2);
            myBoard.setStartingAlpha(recv_move_data[7]);
            myBoard.setStartingBeta(recv_move_data[8]);
            myBoard.setDepth(recv_move_data[9]);
            myBoard.setStartingDepth(2);
            myBoard.bestMove();

            int return_vals[5];//0=first move index; 1-3=second move, 4=eval;
            return_vals[0] = recv_move_data[0];
            return_vals[1] = recv_move_data[4];
            return_vals[2] = recv_move_data[5];
            return_vals[3] = recv_move_data[6];
            return_vals[4] = myBoard.getBestEval();

            //before sending the data back to master, check if the timeout has been sent - in that case the result is not relevant anymore
            //TODO update for master having sent it before the iprobe noticed it
            if(return_vals[4] != TERMINATED_BEST_VAL && return_vals[4] != -TERMINATED_BEST_VAL)
            {
                MPI_Isend(return_vals, 5, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD, &data_send_1);
            }
            else
            {
                //printf("rank %d being terminated\n", rank);
            }


            cnt ++;
        }
        else if( status.MPI_TAG == TAG_DO_NOTHING){
            //wait until other workers finish the tasks for this ound but still ask for next job as it comes in the next round
            MPI_Recv(board, 0, MPI_CHAR, 0, TAG_DO_NOTHING, MPI_COMM_WORLD, &status2) ;
            usleep(100);
        }
        else if( status.MPI_TAG == TAG_STOP ){
            MPI_Recv(board, 0, MPI_CHAR, 0, TAG_STOP, MPI_COMM_WORLD, &status2);
            return 0;
        }
        else if( status.MPI_TAG == TAG_TERMINATE_COMPUTATION ){
            printf("process %d received tag terminate %d \n", rank, status.MPI_TAG);
            MPI_Recv(board, 1, MPI_CHAR, 0, TAG_TERMINATE_COMPUTATION, MPI_COMM_WORLD, &status2);
            usleep(100);
        }
        else
        {
            printf("process %d received tag %d \n", rank, status.MPI_TAG);
            usleep(100000);
        }
    }
}
////////////////////////////////

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
    if(rank == 0) //process for networking
    {
    ////////////////////////////////
        MyDomain d(lport);
        l.install(&d);

        if (host) d.addConnection(host, rport);

        l.run();
    ////////////////////////////////MPI
    }
    // else if(rank == 1) //master process
    // {
    //     master_process();
    // }
    else
    {
        worker_process();
    }
    MPI_Finalize();
    ////////////////////////////////

}
