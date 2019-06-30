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
#include "omp.h"

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
class ParallelMinimaxStrategy : public SearchStrategy
{
public:
    // Defines the name of the strategy
    ParallelMinimaxStrategy() : SearchStrategy("ParallelMinimax") {
        num_replicas = omp_get_max_threads();
        for (int i = 0; i < num_replicas; i++)
        {
            replicas[i] = new Board();
        }
    }

    // Factory method: just return a new instance of this class
    SearchStrategy *clone() { return new ParallelMinimaxStrategy(); }

private:
    /**
     * Implementation of the strategy.
     */
    int max_threads;
    int num_replicas;
    Board *replicas[100];
    int minimax(Board *board, int = 0, bool = true);
    void searchBestMove();
    Move move_buffer[1000];
};

int ParallelMinimaxStrategy::minimax(Board *board, int depth, bool max)
{
    if (depth == _maxDepth)
        return max ? -_ev->calcEvaluation(board) : _ev->calcEvaluation(board);
    MoveList moves;
    board->generateMoves(moves);
    Move move;
    int bestVal = max ? minEvaluation() : maxEvaluation();
    int i;
    for (i = 0; moves.getNext(move); i++)
    {
        board->playMove(move);
        int val = minimax(board, depth + 1, !max);
        board->takeBack();
        if ((max && val >= bestVal) || (!max && val <= bestVal))
        {
            bestVal = val;
        }
    }
    if (i == 0)
        return max ? -_ev->calcEvaluation(board) : _ev->calcEvaluation(board);
    return bestVal;
}

void ParallelMinimaxStrategy::searchBestMove()
{
    MoveList moves;
    generateMoves(moves);
    Move move;
    int bestVal = minEvaluation();
    int i, bestI;
    for (i = 0; i < num_replicas; i++)
    {
        replicas[i]->setState(_board->getState()); 
    }
    Move bestMove;
    for (i = 0; moves.getNext(move); i++)
    {
        move_buffer[i] = move;
    }
    std::vector<int> values(i);
# pragma omp parallel
{
    int n_threads = omp_get_num_threads();
    int per_proc = i / n_threads;
    int tid = omp_get_thread_num();
    int start = tid * per_proc;
    int end = (tid + 1) * per_proc;
    if (tid == (n_threads-1)) {
        end += i % n_threads;
    }
    for (int j = start; j < end; j++)
    {
        Move m = move_buffer[j];
        replicas[tid]->playMove(m);
        int val = minimax(replicas[tid], 1, false);
        replicas[tid]->takeBack();
        values[j] = val;
        //# pragma omp critical
        /* {
            //printf("Critical %d\n", tid);
            if (val >= bestVal)
            {
                bestVal = val;
                bestMove = m;
            }
        }*/
    }
}
    for (int i = 0; i < values.size(); i++)
    {
        if (values[i] >= bestVal) {
            bestVal = values[i];
            bestMove = move_buffer[i];
        }
    }
    
    foundBestMove(0, bestMove, bestVal);
}

// register ourselve as a search strategy
ParallelMinimaxStrategy parallelMinimaxStrategy;