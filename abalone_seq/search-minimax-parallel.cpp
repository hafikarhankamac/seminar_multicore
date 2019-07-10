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
    ParallelMinimaxStrategy() : SearchStrategy("ParallelMinimax")
    {

#pragma omp parallel // First touch
        {
            int tid = omp_get_thread_num();
            if (tid == 0)
                num_replicas = omp_get_num_threads();
            replicas[tid] = new Board();
            evals[tid] = new Evaluator();
        }
        num_evals = new int[num_replicas];
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
    Evaluator *evals[100];
    int minimax(Board *board, Evaluator *eval, int, bool, int &);
    void searchBestMove();
    Move move_buffer[1000];
    int *num_evals;
};

int ParallelMinimaxStrategy::minimax(Board *board, Evaluator *eval, int depth, bool max, int &eval_counter)
{
    if (depth == _maxDepth)
    {
        eval_counter++;
        return max ? -eval->calcEvaluation(board) : eval->calcEvaluation(board);
    }
    MoveList moves;
    board->generateMoves(moves);
    Move move;
    int bestVal = max ? minEvaluation() : maxEvaluation();
    int i;
    for (i = 0; moves.getNext(move); i++)
    {
        board->playMove(move);
        int val = minimax(board, eval, depth + 1, !max, eval_counter);
        board->takeBack();
        if ((max && val > bestVal) || (!max && val <= bestVal))
        {
            bestVal = val;
        }
    }
    if (i == 0)
    {
        eval_counter++;
        return max ? -eval->calcEvaluation(board) : eval->calcEvaluation(board);
    }
    return bestVal;
}

void ParallelMinimaxStrategy::searchBestMove()
{
    MoveList moves;
    generateMoves(moves);
    Move move;
    int bestVal = minEvaluation();
    int i;
    char *boardState = _board->getState();
    EvalScheme *evalScheme = _ev->evalScheme();
#pragma omp parallel
    {
        int tid = omp_get_thread_num();
        replicas[tid]->setState(boardState);
        evals[tid]->setEvalScheme(evalScheme);
        num_evals[tid] = 0;
    }
    for (i = 0; moves.getNext(move); i++)
    {
        move_buffer[i] = move;
    }
    std::vector<int> values(i);
    int moveId = 0;
#pragma omp parallel
    {
        Move localMove;
        int tid = omp_get_thread_num();
        while (1)
        {
            int localMoveId = -1;
#pragma omp critical
            {
                if (moveId < i)
                {
                    localMoveId = moveId; 
                    localMove = move_buffer[moveId];
                    moveId++;
                }
            }
            if (localMoveId < 0)
                break;
            replicas[tid]->playMove(localMove);
            values[localMoveId] = minimax(replicas[tid], evals[tid], 1, false, num_evals[tid]);
            replicas[tid]->takeBack();
        }
    }
    for (int i = 0; i < values.size(); i++)
    {
        if (values[i] > bestVal)
        {
            bestVal = values[i];
            foundBestMove(0, move_buffer[i], bestVal);
        }
    }
}

/* void ParallelMinimaxStrategy::searchBestMove()
{
    MoveList moves;
    generateMoves(moves);
    Move move;
    int bestVal = minEvaluation();
    int i;
    char *boardState = _board->getState();
    EvalScheme *evalScheme = _ev->evalScheme();
# pragma omp parallel
{
    int tid = omp_get_thread_num();
    replicas[tid]->setState(boardState); 
    evals[tid]->setEvalScheme(evalScheme);
    num_evals[tid] = 0;
}
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
    if (tid == (n_threads-1)) 
    {
        end += i % n_threads;
    }
    for (int j = start; j < end; j++)
    {
        Move m = move_buffer[j];
        replicas[tid]->playMove(m);
        int val = minimax(replicas[tid], evals[tid], 1, false, num_evals[tid]);
        replicas[tid]->takeBack();
        values[j] = val;
    }
}
    for (int i = 0; i < values.size(); i++)
    {
        if (values[i] > bestVal) 
        {
            bestVal = values[i];
            foundBestMove(0, move_buffer[i], bestVal);
        }
    }
    int total_evals = 0;
    for (int i = 0; i < num_replicas; i++)
    {
        total_evals += num_evals[i];
    }
    printf("Total number of evals: %d\n", total_evals);
}*/

// register ourselve as a search strategy
ParallelMinimaxStrategy parallelMinimaxStrategy;