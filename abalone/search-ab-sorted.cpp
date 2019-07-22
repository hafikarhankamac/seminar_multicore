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
#include <vector>
#include <algorithm>
#include <tuple>
#include <iostream>
#include <unistd.h>

#include "mpi.h"

#define TAG_TERMINATE_COMPUTATION 7

#define TERMINATED_BEST_VAL 987654321

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
class ABStrategySorted: public SearchStrategy
{
 public:
    // Defines the name of the strategy
    ABStrategySorted(): SearchStrategy("AlphaBetaSorted/Sampling") {}

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new ABStrategySorted(); }

 private:
     int rank;
     int counter = 0;

     int nodes_evaluated;
     int branches_cut_off[20];

     int numSamples=8;
     int threshold=50;

     int updatedAlpha = -99999;
     int updatedBeta = 99999;

    /**
     * Implementation of the strategy.
     */
    int alphaBeta(int depth=0, int alpha = -999999, int beta = 999999);
    void searchBestMove();

    std::vector<Move> sampleMoves();
};


std::vector<Move> ABStrategySorted::sampleMoves()
{
    using avtype = std::tuple<Move, int>;
    const int valueOf = 1;
    const int moveOf = 0;
    std::vector<Move> sortedMoves;
    MoveList list;
    Move move;
    std::vector<avtype> actionValues;
    generateMoves(list);
    while (list.getNext(move))
    {
        playMove(move);
        actionValues.push_back(std::make_tuple(move, evaluate()));
        takeBack();
    }
    std::stable_sort(actionValues.begin(), actionValues.end(), [](avtype a, avtype b) {
        return std::get<valueOf>(a) > std::get<valueOf>(b);
    });
    int i;
    for (i = 0; i < std::min(numSamples, (int)actionValues.size()); i++)
    {
        sortedMoves.push_back(std::get<moveOf>(actionValues[i]));
    }
    auto firstVal = std::get<valueOf>(actionValues[0]);
    while (true)
    {
        if (i >= actionValues.size()) break;
        Move m; int val; std::tie(m, val) = actionValues[i];
        if ((firstVal - val) > threshold) break;
        sortedMoves.push_back(m);
        i++;
    }
    return sortedMoves;
}


int ABStrategySorted::alphaBeta(int depth, int alpha, int beta)
{
    counter++;
    if(counter % 4 == 0)
    {
        MPI_Status status;
        int flag;
        MPI_Test(unexpected_receive_request_ptr, &flag, &status);
        if(flag)
        {
            MPI_Irecv(unexpected_receive_array, 4, MPI_INT, 0, TAG_TERMINATE_COMPUTATION, MPI_COMM_WORLD, unexpected_receive_request_ptr);

            if(unexpected_receive_array[0] == 1)
            {
                //printf("!!!!!!!!!!!!!!!!!!!cutting off worker %d\n", rank);
                return TERMINATED_BEST_VAL;
            }
            else
            {
                if(unexpected_receive_array[1] > updatedAlpha)
                {
                    updatedAlpha = unexpected_receive_array[1];
                }
                //printf("received alpha %d beta %d     old alpha %d \n", unexpected_receive_array[1], startingBeta, startingAlpha);
                if(unexpected_receive_array[1] >= startingBeta)
                {
                    //printf("!!!!!!!!!proc %d cutting off the whole tree!!!!received alpha %d beta %d     old alpha %d \n", rank, unexpected_receive_array[1], startingBeta, startingAlpha);
                    return TERMINATED_BEST_VAL;
                }
                else
                {
                }
                //MPI_Request_free(unexpected_receive_request_ptr);

            }

        }
    }

    if (depth == _maxDepth || !_board->isValid())
    {
        return -(evaluate()-depth);
    }
    int bestVal = minEvaluation();

    if(depth <= (_maxDepth-2))
    {
        auto moves = sampleMoves();
        for (auto move: moves)
        {
            if(depth % 2 == 0 && updatedAlpha > alpha)//maximising
            {
                alpha = updatedAlpha;
            }
            else if(depth % 2 == 1 && -updatedAlpha < beta) //-updatedAlpha = beta
            {
                beta = updatedAlpha;
            }
            playMove(move);
            int val = -alphaBeta(depth+1, -beta, -alpha);
            if(val == TERMINATED_BEST_VAL || val == -TERMINATED_BEST_VAL)
            { //if this value returned, exit asap
                return TERMINATED_BEST_VAL;
            }
            takeBack();
            if (val > bestVal) {
                bestVal = val;
                if (depth == startingDepth)
                {
                    foundBestMove(depth, move, bestVal);
                }
            }
            if(bestVal > alpha)
            {
                alpha = bestVal;
            }
            if(alpha >= beta)
            {
                branches_cut_off[depth]++;
                break;
            }
        }
    }
    else
    {
        MoveList list;
        generateMoves(list);
        Move move;
        int i;
        for(i = 0; list.getNext(move); i++) {
            playMove(move);
            int val = -alphaBeta(depth+1, -beta, -alpha);
            if(val == TERMINATED_BEST_VAL || val == -TERMINATED_BEST_VAL)
            { //if this value returned, exit asap
                return TERMINATED_BEST_VAL;
            }
            takeBack();
            if (val > bestVal) {
                bestVal = val;
                if (depth == startingDepth)
                {
                    foundBestMove(depth, move, bestVal);
                }
            }

            if(bestVal > alpha)
            {
                alpha = bestVal;
            }
            if(alpha >= beta)
            {
                branches_cut_off[depth]++;
                break;
            }
        }
    }
    return bestVal;
}

void ABStrategySorted::searchBestMove()
{
    for(int i = 0; i<_maxDepth; i++)
    {
        branches_cut_off[i] = 0;
    }
    counter = 0;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    //MPI_Irecv(unexpected_receive_array, 4, MPI_INT, 0, TAG_TERMINATE_COMPUTATION, MPI_COMM_WORLD, unexpected_receive_request_ptr);
    eval = alphaBeta(startingDepth, startingAlpha, startingBeta);


    // for(int i = 0; i<_maxDepth; i++)
    // {
    //     printf("     cut off %d branches at depth %d \n",branches_cut_off[i], i);
    // }

}

// register ourselve as a search strategy
ABStrategySorted aBStrategySorted;
