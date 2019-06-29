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
    void minimax_iter(bool max=true);
    void searchBestMove();
};

void MinimaxStrategy::minimax_iter(bool max)
{
    std::__throw_runtime_error("NotImplemented");
    std::vector<MoveList> lists(_maxDepth);
    // std::vector<std::vector<int>> values(_maxDepth);
    // generateMoves(lists[0]);
    int depth = 0;
    int reached_depth = 0;
    Move m;
    bool hasNext = false;
    while (true) {
        // We are not interleaving, so generate moves
        if (!hasNext) {
            generateMoves(lists[depth]);
            hasNext = lists[depth].getNext(m);
            if (!hasNext) {  // Level is full
                depth--;
            }
        }
        else {
            playMove(m);
            hasNext = lists[depth].getNext(m);
            depth++;
        }

    }
}

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
        playMove(move);
        int val = minimax(depth + 1, !max);
        takeBack();
        if ((max && val >= bestVal) || (!max && val <= bestVal)) 
        {
            bestVal = val;
            foundBestMove(depth, move, bestVal);
        }
    }
    return bestVal;
}

void MinimaxStrategy::searchBestMove()
{
    minimax();
}

// register ourselve as a search strategy
MinimaxStrategy miniMaxStrategy;