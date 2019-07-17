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
#include <iostream>

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
        integerValues.reserve(100);
        indices.reserve(100);
    }
    // Factory method: just return a new instance of this class
    SearchStrategy *clone() { return new SamplingStrategy(); }

private:
    /**
     * Implementation of the strategy.
     */
    std::vector<double> values;
    std::vector<int> integerValues;
    std::vector<Move> moves;
    std::vector<int> indices;
    const int threshold = 40;
    const int numSamples = 8;
    const int maxSamples = 16;
    int minimax(int = 0, bool = true);
    std::vector<Move> sampleMoves();
    void searchBestMove() override;
};

std::vector<Move> SamplingStrategy::sampleMoves()
{
    std::vector<Move> sampledMoves; //(numSamples);
    // values.clear();
    moves.clear();
    integerValues.clear();
    indices.clear();
    MoveList list;
    generateMoves(list);
    Move m;
    int i = 0;
    while (list.getNext(m))
    {
        moves.push_back(m);
        playMove(m);
        integerValues.push_back(evaluate());
        takeBack();
        indices.push_back(i++);
    }
    std::stable_sort(indices.begin(), indices.end(), [this](int i1, int i2) {
        return this->integerValues[i1] > this->integerValues[i2];
    });
    /* for (auto idx: indices) {
        std::cout<<integerValues[idx]<<", ";
    }*/
    //std::cout<<"\n\n\n";
    for (i = 0; i < std::min(numSamples, (int)indices.size()); ++i) {
        auto idx = indices[i];
        sampledMoves.push_back(moves[idx]);
    }
    auto bestVal = integerValues[indices[0]];
    while (true) {
        if (i >= indices.size()) break;
        auto idx = indices[i];
        auto val = integerValues[idx];
        if ((bestVal - val) > threshold) break;
        sampledMoves.push_back(moves[idx]);
        i++;
    }
    /*sampledMoves.push_back(moves[indices[0]]);
    for (i=1; (
            i < integerValues.size()) && 
            (integerValues[indices[i - 1]] <= (integerValues[indices[i]] + threshold)
        ); i++)
    //for (auto idx: indices)
    {
        sampledMoves.push_back(moves[indices[i]]);
    }*/
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
    for (auto move : moves)
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