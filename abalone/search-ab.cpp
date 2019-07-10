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
class ABStrategy: public SearchStrategy
{
 public:
    // Defines the name of the strategy
    ABStrategy(): SearchStrategy("AlphaBeta") {}

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new ABStrategy(); }

 private:
     int nodes_evaluated;
    /**
     * Implementation of the strategy.
     */
    int alphaBeta(int depth=0, int alpha = -999999, int beta = 999999);
    void searchBestMove();
};

int ABStrategy::alphaBeta(int depth, int alpha, int beta)
{
    bool max = (depth + 1) % 2;//0=min, 1=max
    if (depth == _maxDepth)
        //return evaluate();
        return max ? -evaluate() : evaluate();
    int bestVal = max ? minEvaluation() : maxEvaluation() ;
    MoveList list;
    generateMoves(list);
    Move move;
    int i;
    for(i = 0; list.getNext(move); i++) {
        playMove(move);
        int val;
        val = alphaBeta(depth+1, -beta, -alpha);
        // if(max)
        // {
        //     val = alphaBeta(depth+1, alpha, beta);
        // }
        // else
        // {
        //     val = -alphaBeta(depth+1, -beta, -alpha);
        // }
        takeBack();
        if ((max && val >= bestVal) || (!max && val <= bestVal)) {
            bestVal = val;
            if (depth == 0)
            {
                bestVal = val;
                foundBestMove(depth, move, bestVal);
            }
        }

        if(max && bestVal >= alpha)
        {
            alpha = bestVal;
        }
        else if(!max && bestVal <= beta) //minimizing player
        {
            beta = bestVal;
        }
        if(alpha >= beta)
        {
            break;
        }
    }
    if (i == 0) //if no possible moves
        //return evaluate();
        return max ? -evaluate() : evaluate();
    return bestVal;
}

void ABStrategy::searchBestMove()
{
    eval = alphaBeta(0, -9999999, 9999999);
}

// register ourselve as a search strategy
ABStrategy aBStrategy;
