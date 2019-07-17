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
};

int ABStrategySorted::alphaBeta(int depth, int alpha, int beta)
{
    bool max = (depth + 1) % 2;//0=min, 1=max
    if (depth == _maxDepth || !_board->isValid())
    {
        return -(evaluate()-depth);
    }
    MoveList list;
    generateMoves(list);
    Move move;
    std::vector<Move> moves;
    std::vector<Move> sortedMoves;
    std::vector<int> values;
    std::vector<int> indices;
    int i = 0;
    while (list.getNext(move)) 
    {
        indices.push_back(i++);
        playMove(move);
        values.push_back(evaluate());
        takeBack();
        moves.push_back(move);
    }
    std::stable_sort(indices.begin(), indices.end(), [&values](auto i1, auto i2) {
        return values[i1] > values[i2];
    });
    for (i = 0; i < std::min(numSamples, (int)indices.size()); i++) {
        sortedMoves.push_back(moves[indices[i]]);
    }
    auto firsVal = values[indices[0]];
    while (true) {
        if (i >= indices.size()) break;
        auto idx = indices[i];
        auto val = values[idx];
        if ((firsVal - val) > threshold) break;
        sortedMoves.push_back(moves[idx]);
        i++;
    }
    /* for (auto idx: indices) {
        sortedMoves.push_back(moves[idx]);
    }*/
    int bestVal = minEvaluation();
    //int i;
    //for(i = 0; list.getNext(move); i++) {
    for (auto move: sortedMoves)
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
    // for(int i = 0; i<_maxDepth; i++)
    // {
    //     printf("     cut off %d branches at depth %d \n",branches_cut_off[i], i);
    // }

}

// register ourselve as a search strategy
ABStrategySorted aBStrategySorted;
