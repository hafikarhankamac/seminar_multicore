/*
 * Search the game play tree for move leading to
 * position with highest evaluation
 *
 * (c) 2005, Josef Weidendorfer
 */

#ifndef SEARCH_H
#define SEARCH_H

#include "move.h"
#include "mpi.h"

class Board;
class Evaluator;
class SearchStrategy;

class SearchCallbacks
{
 public:
    SearchCallbacks(int v = 0) { _verbose = v; }
    virtual ~SearchCallbacks() {}

    // called at beginning of new search. If <msecs> >0,
    // we will stop search (via afterEval) after that time
    virtual void start(int msecsForSearch);
    // called at beginning of new sub search
    virtual void substart(char*);
    // called after search is done
    virtual void finished(Move&);
    // called after each evaluation
    // returns true to request stop of search
    virtual bool afterEval();
    // called when a new best move is found at depth d
    virtual void foundBestMove(int d, const Move&, int value);
    // called before children are visited
    virtual void startedNode(int d, char*);
    /**
     * Called after needed children are visited
     * Second parameter gives array of best move sequence found
     */
    virtual void finishedNode(int d, Move*);

    int msecsPassed() { return _msecsPassed; }
    int verbose() { return _verbose; }

 private:
    int _verbose;
    int _leavesVisited, _nodesVisited;
    int _msecsPassed, _msecsForSearch;
};


/*
 * Base class for search strategies
 *
 * Implement searchBestMove !
 */
class SearchStrategy
{
 public:
    SearchStrategy(const char* n, int prio = 5);
    virtual ~SearchStrategy() {};

    /* get list of names of available strategies */
    static const char** strategies();
    /* factory for a named strategy */
    static SearchStrategy* create(char*);
    static SearchStrategy* create(int);
    const char* name() { return _name; }

    void registerCallbacks(SearchCallbacks* sc) { _sc = sc; }
    void setMaxDepth(int d) { _maxDepth = d; }
    void setStartingDepth(int d) { startingDepth = d; }
    void setStartingAlpha(int d) { startingAlpha = d; }
    void setStartingBeta(int d) { startingBeta = d; }
    void setCallReceive(int d) { callReceive = d; }
    void set_unexpected_receive_request_ptr(MPI_Request * req_ptr) { unexpected_receive_request_ptr = req_ptr; }
    void set_unexpected_receive_array_ptr(int* int_ptr) { unexpected_receive_array = int_ptr; }




    void setEvaluator(Evaluator* e) { _ev = e; }

    /* Start search and return best move. */
    Move& bestMove(Board*);

    /* return best move in depth 1 if last search got one */
    virtual Move& nextMove();

    /* factory method: should return instance of derived class */
    virtual SearchStrategy* clone() = 0;

    void stopSearch() { _stopSearch = true; }
    int eval;
    //MPI_Request request;

    /**
     * Overwrite this to implement your search strategy
     * and set _bestMove
    */
    virtual void searchBestMove() = 0;

 protected:

    /**
     * Some dispatcher methods for convenience.
     * Call these from your search function
     */
    int minEvaluation();
    int maxEvaluation();
    // see Board::generateMoves
    void generateMoves(MoveList& list);
    // see Board::playMove
    void playMove(const Move& m);
    // see Board::takeBack
    bool takeBack();
    // see SearchCallbacks::foundBestMove
    void foundBestMove(int d, const Move& m, int eval);
    // see SearchCallbacks::finishedNode
    void finishedNode(int d, Move* bestList);
    // see Evaluator::calcEvaluation
    int evaluate();


    int _maxDepth;
    Board* _board;
    bool _stopSearch;
    SearchCallbacks* _sc;
    Evaluator* _ev;
    Move _bestMove;
    int startingDepth = 0;
    int startingAlpha = -99999;
    int startingBeta = 99999;
    int callReceive = 1;
    int * unexpected_receive_array;
    MPI_Request * unexpected_receive_request_ptr;
 private:

    const char* _name;
    int _prio;
    SearchStrategy* _next;
};


#endif
