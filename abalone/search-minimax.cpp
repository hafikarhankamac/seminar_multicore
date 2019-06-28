/**
 * Very simple example strategy:
 * Search all possible positions reachable via one move,
 * and return the move leading to best position
 *
 * (c) 2006, Josef Weidendorfer
 */
#include <stdio.h>
#include "search.h"
#include "board.h"
#include "eval.h"
#include <tuple>
#include "mpi.h"


/**
 * To create your own search strategy:
 * - copy this file into another one,
 * - change the class name one the name given in constructor,
 * - adjust clone() to return an instance of your class
 * - adjust last line of this file to create a global instance
 *   of your class
 * - adjust the Makefile to include your class in SEARCH_OBJS
 * - implement searchBestMove()
 *
 * Advises for implementation of searchBestMove():
 * - call foundBestMove() when finding a best move since search start
 * - call finishedNode() when finishing evaluation of a tree node
 * - Use _maxDepth for strength level (maximal level searched in tree)
 */
class MinimaxStrategy: public SearchStrategy
{
 public:
    // Defines the name of the strategy
    MinimaxStrategy(): SearchStrategy("Minimax") {}

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new MinimaxStrategy(); }

 private:
     int nodes_evaluated;
    /**
     * Implementation of the strategy.
     */
    int minimax(int depth=0);
    void searchBestMove();
};

int MinimaxStrategy::minimax(int depth)
{
    bool max = (depth + 1) % 2;//0=min, 1=max
    int bestEval = max ? minEvaluation() : maxEvaluation() ;
    int eval;
    if (depth == _maxDepth) {
        nodes_evaluated++;
        return evaluate();
    }
    MoveList list;
    Move m;
    generateMoves(list);
    int i;
    for(i = 0; list.getNext(m); i++) {
        playMove(m);
        eval = minimax(depth+1);
        takeBack();


        if ((max && eval > bestEval) || (!max && eval <= bestEval)) {
            bestEval = eval;
            //     printf(" depth: %d  iteration: %d    move: %s   bestEval: %d \n", depth, i, m.name(), eval);
            if (depth == 0)
            {
                //printf("\n");
                foundBestMove(0, m, eval);
            }
	    }

    }
    if (i == 0) {
        nodes_evaluated++;
        return evaluate();
    }

    return bestEval;
}

void MinimaxStrategy::searchBestMove()
{
    // generateMoves(list);


    // int  numtasks, rank, len;
    // char hostname[MPI_MAX_PROCESSOR_NAME];
    // MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
    // MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    // MPI_Get_processor_name(hostname, &len);
    //
    // if(numtasks <= 1)
    //     minimax(0);

    // nodes_evaluated = 0;

    eval = minimax(1);
    // finishedNode(0,0);
    //printf("Total nodes evaluated: %d \n", nodes_evaluated);
}

// register ourselve as a search strategy
MinimaxStrategy miniMaxStrategy;
