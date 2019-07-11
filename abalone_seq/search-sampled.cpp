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
#include <random>

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
class SamplingStrategy : public SearchStrategy
{
public:
    // Defines the name of the strategy
    SamplingStrategy() : SearchStrategy("SamplingStrategy") 
    {
        values.reserve(100);
        moves.reserve(100);
        roundedValues.reserve(100);
        indices.reserve(100);
    }
    // Factory method: just return a new instance of this class
    SearchStrategy *clone() { return new SamplingStrategy(); }

private:
    /**
     * Implementation of the strategy.
     */
    std::vector<double> values;
    std::vector<int> roundedValues;
    std::vector<Move> moves;
    std::vector<int> indices;
    int numSamples=8;
    int minimax(int = 0, bool = true);
    std::vector<Move> sampleMoves();
    void searchBestMove();
};

std::vector<Move> SamplingStrategy::sampleMoves()
{
    std::vector<Move> sampledMoves(numSamples);
    values.clear();
    moves.clear();
    roundedValues.clear();
    indices.clear();
    MoveList list;
    generateMoves(list);
    Move m;
    int i = 0;
    while (list.getNext(m)) {
        moves.push_back(m);
        playMove(m);
        roundedValues.push_back(evaluate());
        takeBack();
        indices.push_back(i++);
    }
    std::sort(indices.begin(), indices.end(), [this](int i1, int i2) {
        return this->roundedValues[i1] > this->roundedValues[i2];
    });
    for (int i = 0; i < numSamples; ++i) {
        sampledMoves[i] = moves[indices[i]];
    }
    return sampledMoves;
}

int SamplingStrategy::minimax(int depth, bool max)
{
    if (depth == _maxDepth)
        return max ? -evaluate() : evaluate();
    auto moves = sampleMoves();
    if (moves.empty())
        return max ? -evaluate() : evaluate();
    int bestVal = max ? minEvaluation() : maxEvaluation();
    for (auto move: moves) 
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

void SamplingStrategy::searchBestMove()
{
    minimax(0, true);
}

// register ourselve as a search strategy
SamplingStrategy samplingStrategy;