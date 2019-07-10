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
    if (depth == _maxDepth || !_board->isValid())
    {
        return max ? -(evaluate()-depth) : evaluate()-depth ;
    }

    MoveList list;
    generateMoves(list);
    Move move;
    int bestVal = max ? minEvaluation() : maxEvaluation() ;
    int i;
    for(i = 0; list.getNext(move); i++) {
        playMove(move);
        int val = minimax(depth+1);
        takeBack();
        if ((max && val > bestVal) || (!max && val < bestVal)) {
            bestVal = val;
            if (depth == 0)
            {
                bestVal = val;
                foundBestMove(depth, move, bestVal);
            }
	    }

    }

    return bestVal;
}

void MinimaxStrategy::searchBestMove()
{
    eval = minimax(startingDepth);
}

// register ourselve as a search strategy
MinimaxStrategy miniMaxStrategy;
