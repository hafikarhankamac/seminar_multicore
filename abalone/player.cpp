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
#define MAX_EVAL_VALUE 99999


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
bool g_first_generation, g_sort_moves = false, g_all_child_moves_generated[MOVE_ARRAY_SIZE];
int g_first_move_number, g_number_child_moves_generated[MOVE_ARRAY_SIZE], g_number_child_moves_calculated[MOVE_ARRAY_SIZE];
Move g_first_move, g_second_move;
MoveList g_first_moves_list, g_second_moves_list;
Move g_first_move_array[MOVE_ARRAY_SIZE];



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
        int best_eval = -MAX_EVAL_VALUE; // is equal to alpha
        bool is_next_move = true;
        g_first_moves_list.clear();
        myBoard.generateMoves(g_first_moves_list);
        //here sort the moves
        g_first_generation = true;
        g_first_move_number = 0;

        //for each move, store its best move (min round ->store minimum)
        int best_eval_array[MOVE_ARRAY_SIZE]; // is equal to beta for given branch
        Move percieved_second_move_array[MOVE_ARRAY_SIZE], bestMove, percieved_second_move;
        int i;
        for(i=0; i< MOVE_ARRAY_SIZE; i++ )
        {
            best_eval_array[i]=MAX_EVAL_VALUE;

            g_all_child_moves_generated[i]=false;
            g_number_child_moves_generated[i]=0;
            g_number_child_moves_calculated[i]=0;
        }
        int top_level_alpha = -MAX_EVAL_VALUE;

        MPI_Status status, status2;
        MPI_Request message_type, data_send_1, data_send_2, data_recv_1, data_recv_2, request;
        char tmp_buf [2];
        int tasks_created = 0;
        int tasks_completed = 0;
        int send_move_data[9];//0=first move index; 1-3 = first move, 4-6 = second move, 7=alpha, 8=beta,
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
                        send_move_data[0] = g_first_move_number;
                        send_move_data[1] = (int)g_first_move.field;
                        send_move_data[2] = (int)g_first_move.direction;
                        send_move_data[3] = g_first_move.type;
                        send_move_data[4] = (int)g_second_move.field;
                        send_move_data[5] = (int)g_second_move.direction;
                        send_move_data[6] = g_second_move.type;
                        send_move_data[7] = best_eval;//current alpha
                        send_move_data[8] = best_eval_array[g_first_move_number];//cuttent beta
                        MPI_Isend(send_move_data, 9, MPI_INT, slave_rank, TAG_JOB_DATA_2, MPI_COMM_WORLD, &data_send_2);
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
                if (verbose>1){
                  printf("receiving data no.%d from proc %d \n", tasks_completed, slave_rank);
                }

                int return_vals[5];//0=first move index; 1-3 = second move, 4 = eval;
                MPI_Irecv(return_vals, 5, MPI_INT, slave_rank, TAG_RESULT, MPI_COMM_WORLD, &data_recv_1);
                MPI_Wait(&data_recv_1, &status2);

                int move_index = return_vals[0];
                g_number_child_moves_calculated[move_index]++;


                //printf("index  %d  all_generated  %d  ,_generated  %d  _calculated  %d \n", move_index, g_all_child_moves_generated[move_index],  g_number_child_moves_generated[move_index], g_number_child_moves_calculated[move_index] );

                if (return_vals[4] < best_eval_array[move_index]) // search for min in each subtree
                {
                    best_eval_array[move_index] = return_vals[4];
                    percieved_second_move_array[move_index] = Move((short)return_vals[1], (unsigned char)return_vals[2], (Move::MoveType)return_vals[3]);
                    if (verbose>1){
                        printf("found new best eval %d from %d - %s \n", return_vals[4], slave_rank, g_first_move_array[move_index].name());
                        //myBoard.print();
                    }
                }

                //if all sub-moves of this move are computed, check if best move can be updated
                //here if(g_all_child_moves_generated[move_index] && g_number_child_moves_generated[move_index] == g_number_child_moves_calculated[move_index])
                if(g_number_child_moves_generated[move_index] == g_number_child_moves_calculated[move_index])
                {
                    //printf(" best eval %d from %d - move %d \n", best_eval_array[move_index], slave_rank, move_index);

                    if(best_eval_array[move_index] > best_eval )
                    {
                        best_eval = best_eval_array[move_index];
                        bestMove = g_first_move_array[move_index];
                        percieved_second_move = percieved_second_move_array[move_index];
                        //printf("found new GLOBAL best eval %d from %d - move %d \n", best_eval_array[move_index], slave_rank, move_index);
                    }
                }
                tasks_completed++;
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

bool MyDomain::generate_move()
{
    if(g_sort_moves)
    {

    }
    else
    {
      //play first move in the first round and generate list of second moves
      if(g_first_generation)
      {
          g_first_generation = false;
          g_first_move_number = 0;

          //bool move_available = g_first_moves_list.getNext(g_first_move);
          if(g_first_moves_list.getNext(g_first_move))
          {
              myBoard.playMove(g_first_move);
              g_first_move_array[g_first_move_number] = g_first_move;
              g_second_moves_list.clear();
              myBoard.generateMoves(g_second_moves_list);
              g_number_child_moves_generated[g_first_move_number] = g_second_moves_list.getLength(); //here
          }
          else
          {
              printf("!!!SHOULD NOT HAPPEN 2\n \n \n \n \n \n \n \n generate_move - false.  %s\n", g_first_move.name());
              return false;
          }
      }
      if(!g_second_moves_list.getNext(g_second_move)) //try playing next first move and generate second moves for it
      {
          //printf("generated all moves for %d \n", g_first_move_number);
          //hereg_all_child_moves_generated[g_first_move_number] = true;
          myBoard.takeBack();
          if(g_first_moves_list.getNext(g_first_move))
          {
              g_first_move_number++;
              //here g_number_child_moves_generated[g_first_move_number] = 0;

              myBoard.playMove(g_first_move);
              g_first_move_array[g_first_move_number] = g_first_move;

              g_second_moves_list.clear();
              myBoard.generateMoves(g_second_moves_list);
              g_number_child_moves_generated[g_first_move_number] = g_second_moves_list.getLength(); //here
              //printf("printing board after play_move %d \n", g_first_move_number);
              //myBoard.print();
              if(!g_second_moves_list.getNext(g_second_move))
              {
                  printf("!!!SHOULD NOT HAPPEN 1\n \n \n \n \n \n \n \n generate_move - no more first moves. %d  %s xxxxx %s \n",g_first_move_number,  g_first_move.name(), g_second_move.name());
                  return false;
              }
          }
          else //no more first move
          {
              g_first_move_number++;
              return false;
          }
          //now second move is generated
      }
      //here g_number_child_moves_generated[g_first_move_number]++;
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
            MPI_Send(board, 0 , MPI_CHAR, 0, TAG_ASK_FOR_JOB , MPI_COMM_WORLD ) ;
            MPI_Probe (0, MPI_ANY_TAG , MPI_COMM_WORLD , &status ) ;
            if( status.MPI_TAG == TAG_JOB_DATA )
            {
                Move m1, m2;
                if(cnt > 0)//after the first round, check that the previous sends were completed
                {
                    MPI_Wait(&data_send_1, &status2);
                }
                int recv_move_data[9];//0=first move index; 1-3=first move, 4-6=second move, 7=alpha, 8=beta
                MPI_Irecv(board, 1024, MPI_CHAR, 0, TAG_JOB_DATA, MPI_COMM_WORLD, &data_recv_1);
                MPI_Irecv(recv_move_data, 9, MPI_INT, 0, TAG_JOB_DATA_2, MPI_COMM_WORLD, &data_recv_2);
                MPI_Wait(&data_recv_2, &status2);
                m1 = Move((short)recv_move_data[1], (unsigned char)recv_move_data[2], (Move::MoveType)recv_move_data[3]);
                m2 = Move((short)recv_move_data[4], (unsigned char)recv_move_data[5], (Move::MoveType)recv_move_data[6]);
                MPI_Wait(&data_recv_1, &status2);
                myBoard.setState(board+4);
                myBoard.playMove(m1);
                myBoard.playMove(m2);
                    //myBoard.print();
                    //printf("data no.%d from proc 0 playing move %s \n", cnt, m.name());
                myBoard.setStartingAlpha(recv_move_data[7]);
                myBoard.setStartingBeta(recv_move_data[8]);
                myBoard.setStartingDepth(2);
                myBoard.bestMove();

                int return_vals[5];//0=first move index; 1-3=second move, 4=eval;
                return_vals[0] = recv_move_data[0];
                return_vals[1] = recv_move_data[4];
                return_vals[2] = recv_move_data[5];
                return_vals[3] = recv_move_data[6];
                return_vals[4] = myBoard.getBestEval();

                MPI_Isend(return_vals, 5, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD, &data_send_1);

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
