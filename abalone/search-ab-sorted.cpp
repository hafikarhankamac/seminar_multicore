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
#include <vector>
#include <algorithm>
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
class ABSortedStrategy: public SearchStrategy
{
 public:
    // Defines the name of the strategy
    ABSortedStrategy(): SearchStrategy("AlphaBetaSorted")
    {
      indices.reserve(100);
      values.reserve(100);
      moveVector.reserve(100);
      sortedMoveVector.reserve(100);
    }

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new ABSortedStrategy(); }

 private:
     int nodes_evaluated;
     int branches_cut_off[20];
    /**
     * Implementation of the strategy.
     */
    int alphaBeta(int depth=0, int alpha = -999999, int beta = 999999);
    void searchBestMove();

    void getMoves(bool);
    std::vector<int> indices;
    std::vector<int> values;
    std::vector<Move> moveVector;
    std::vector<Move> sortedMoveVector;


};

void ABSortedStrategy::getMoves(bool sort)
{
  sortedMoveVector.clear();

  MoveList list;
  Move move;
  generateMoves(list);
  // if (!sort) {
  //   while (list.getNext(move)) {
  //     sortedMoveVector.push_back(move);
  //   }
  //   return;
  // }

  indices.clear();
  values.clear();
  moveVector.clear();

  int i = 0;
  while (list.getNext(move)) {
    moveVector.push_back(move);
    playMove(move);
    values.push_back(evaluate());
    takeBack();
    indices.push_back(i++);
  }
  std::stable_sort(indices.begin(), indices.end(), [this](int i1, int i2) {
    return this->values[i1] > this->values[i2];
  });
  for (auto i: indices) {
    sortedMoveVector.push_back(moveVector[i]);
  }
}

int ABSortedStrategy::alphaBeta(int depth, int alpha, int beta)
{
    bool max = (depth + 1) % 2;//0=min, 1=max
    if (depth == _maxDepth || !_board->isValid())
    {
        return -(evaluate()-depth);
    }

    //MoveList list;
    //generateMoves(list);
    //Move move;
    int bestVal = minEvaluation();
    int i;
    if(depth <= (_maxDepth-1))
    {
      getMoves(true);
      for (auto move: sortedMoveVector)
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
    }
    else
    {
      MoveList list;
      generateMoves(list);
      Move move;
      for(i = 0; list.getNext(move); i++)
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
    }

    return bestVal;
}

void ABSortedStrategy::searchBestMove()
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
ABSortedStrategy aBSortedStrategy;
