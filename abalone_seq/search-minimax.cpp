/**
 * Very simple example strategy:
 * Search all possible positions reachable via one move,
 * and return the move leading to best position
 *
 * (c) 2006, Josef Weidendorfer
 */

#include "search.h"
#include "board.h"
#include "eval.h"
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

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
    MinimaxStrategy(): SearchStrategy("Minimax") { }

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new MinimaxStrategy(); }

 private:

    /**
     * Implementation of the strategy.
     */
    int minimax(int=0, bool=true);
    void searchBestMove();
    bool timedout = false;
    Move selectedMove;
    Move bestMove;
};



 int MinimaxStrategy::minimax(int depth, bool max) 
{
    if (depth == _maxDepth) 
    {
        return max ? -evaluate() : evaluate();
    }
    MoveList moves;
    generateMoves(moves);
    if (moves.getLength() == 0) 
    {
        return max ? -evaluate() : evaluate();
    }
    Move move;
    int bestVal = max ? minEvaluation() : maxEvaluation();
    while (moves.getNext(move)) 
    {
        gettimeofday(&_board->t2,0);

        int msecsPassed =
	    (1000* _board->t2.tv_sec + _board->t2.tv_usec / 1000) -
	    (1000* _board->t1.tv_sec + _board->t1.tv_usec / 1000);
        //printf("Time Passed: %d\n", msecsPassed);
        //printf("Threshold: %f\n", (_board->_init_time)*0.25*1000);
        if(msecsPassed > (_board->_init_time)*0.25*1000)
        {
            //printf("Reached if in while\n");
            timedout = true;
            break;
        }
        playMove(move);
        int val = minimax(depth + 1, !max);
        takeBack();
        //printf("Threshold Base: %f\n", _board->_init_time);
        //printf("Threshold: %f\n", (_board->_init_time)*0.25);

        // if(msecsPassed < (_board->_init_time)*0.25)
        // {
        //     timedout = true;
        //     takeBack();
        //     break;
        // }else{
        //     takeBack();
        // }

        //printf("Played supposed move.\n");  
        // play time left is smaller than best move time, then stop minimax search. 
        /*
        if (msecsToPlayactColor() > 0 && msecsPassedbestMove() > 0)
        {
            //printf("Time left = %d\n", msecsToPlayactColor());    
            if (msecsToPlayactColor() < msecsPassedbestMove())
            {
                timedout = true;
                takeBack();
                //printf("Timed out in while!\n");
                //foundBestMove(depth, move, bestVal);
                break;
            }
        }

        */
        //int val = minimax(depth + 1, !max);
        //takeBack();
        if ((max && val >= bestVal) || (!max && val <= bestVal)) 
        {
            bestVal = val;
            bestMove = move;
            foundBestMove(depth, bestMove, bestVal);
        }
    }
    return bestVal;
}

void MinimaxStrategy::searchBestMove()
{
    //_maxDepth = 0;
    gettimeofday(&_board->t1,0);
    int temp = _maxDepth;
    while (true)
    {
        auto eval = minimax();
        if (timedout) {
            //printf("Timed out!\n");
            break;
        }
        Move m = bestMove;
        //foundBestMove(_maxDepth, m, eval);
        //break;
        _maxDepth++;
        //printf("Reached here!\n");
        //printf("field %d, direction %d, type %d\n", bestMove.field, bestMove.direction, bestMove.type);
        //break;
    }
    timedout = false;
    _maxDepth = temp;
    
    //minimax();
}

// register ourselve as a search strategy
MinimaxStrategy miniMaxStrategy;
