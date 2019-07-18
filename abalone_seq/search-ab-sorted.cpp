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
// #include "mpi.h"


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
    ABStrategySorted(): SearchStrategy("AlphaBetaSorted") {}

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new ABStrategySorted(); }

    int startingDepth = 0;
    int eval = 0;
 private:
     int nodes_evaluated;
     int branches_cut_off[20];
     int numSamples=8;
     int threshold=50;
    /**
     * Implementation of the strategy.
     */
    int alphaBeta(int depth=0, int alpha = -999999, int beta = 999999);
    void searchBestMove();
    std::vector<Move> sampleMoves();
};

std::vector<Move> ABStrategySorted::sampleMoves() 
{
    std::vector<Move> sortedMoves;
    MoveList list;
    Move move;
    using Value = int;
    std::vector<std::tuple<Move, Value>> actionValues;
    generateMoves(list);
    while (list.getNext(move))
    {
        playMove(move);
        actionValues.push_back(std::make_tuple(move, evaluate()));
        takeBack();
    }
    std::stable_sort(actionValues.begin(), actionValues.end(), [](auto a, auto b) {
        return std::get<Value>(a) > std::get<Value>(b);
    });
    int i;
    for (i = 0; i < std::min(numSamples, (int)actionValues.size()); i++) 
    {
        sortedMoves.push_back(std::get<Move>(actionValues[i]));
    }
    auto firstVal = std::get<Value>(actionValues[0]);
    while (true) 
    {
        if (i >= actionValues.size()) break;
        Move m; Value val; std::tie(m, val) = actionValues[i];
        if ((firstVal - val) > threshold) break;
        sortedMoves.push_back(m);
        i++;
    }
    return sortedMoves;
}

int ABStrategySorted::alphaBeta(int depth, int alpha, int beta)
{
    bool max = (depth + 1) % 2;//0=min, 1=max
    if (depth == _maxDepth || !_board->isValid())
    {
        return -(evaluate()-depth);
    }
    
    int bestVal = minEvaluation();
    auto moves = sampleMoves();
    for (auto move: moves)
    {
        playMove(move);
        int val = -alphaBeta(depth+1, -beta, -alpha);
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
            //printf("moving  alpha  from %d to %d at depth %d.  Beta is %d\n", alpha, bestVal, depth, beta);
            alpha = bestVal;
        }

        if(alpha >= beta)
        {
            branches_cut_off[depth]++;
            break;
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
    eval = alphaBeta(startingDepth, -9999999, 9999999);
    std::cout<<"Eval for move: "<<eval<<"\n";
    // for(int i = 0; i<_maxDepth; i++)
    // {
    //     printf("     cut off %d branches at depth %d \n",branches_cut_off[i], i);
    // }

}

// register ourselve as a search strategy
ABStrategySorted aBStrategySorted;
