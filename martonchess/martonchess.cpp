#include "MartonChess.h"
#include "CastlingType.h"
#include "File.h"
#include "Rank.h"

#include <cassert>
#include <iostream>
#include <string>
#include <sstream>
#include <cctype>

MartonChess::MartonChess(double stageRatio, double cutoffRatio, bool enableBetaThreshold): search(new Search(*this, stageRatio, cutoffRatio, enableBetaThreshold)) {};

void MartonChess::run() {
    std::cin.exceptions(std::iostream::eofbit | std::iostream::failbit | std::iostream::badbit);
    while (true) {
        std::string line;
        std::getline(std::cin, line);
        std::istringstream input(line);

        std::string token;
        input >> std::skipws >> token;
        if (token == "uci") {
            receiveInitialize();
        } else if (token == "isready") {
            receiveReady();
        } else if (token == "ucinewgame") {
            receiveNewGame();
        } else if (token == "position") {
            receivePosition(input);
        } else if (token == "go") {
            receiveGo(input);
        } else if (token == "stop") {
            receiveStop();
		} else if (token == "wait") {
			receiveWait(input);
		} else if (token == "eval") {
			receiveEval();
        } else if (token == "quit") {
            receiveQuit();
            break;
        }
    }
}

void MartonChess::receiveQuit() {
    // We received a quit command. Stop calculating now and
    // cleanup!
    search->exit();
	std::cout << "evaluated: " << evalcount << " malformatted: " << malcount
		<< " time: " << myclock << std::endl;
}

void MartonChess::receiveEval() {
	if (malformatPosition) {
		malcount++;
	}
	else {
		auto begin = std::chrono::high_resolution_clock::now();
		evaluation.evaluate(*currentPosition, true, Value::INFINITE, true);
		auto end = std::chrono::high_resolution_clock::now();
		myclock += std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
		evalcount++;
		std::cout << std::endl;
	}
}

void MartonChess::receiveWait(std::istringstream& input) {
	uint64_t waitTime;
	if (!(input >> waitTime)) {
		throw std::exception();
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
}

void MartonChess::receiveInitialize() {

    // We received an initialization request.

    // We could do some global initialization here. Probably it would be best
    // to initialize all tables here as they will exist until the end of the
    // program.

    // We must send an initialization answer back!
    std::cout << "id name MartonChess" << std::endl;
    std::cout << "id author MHavasi" << std::endl;
    std::cout << "uciok" << std::endl;
}

void MartonChess::receiveReady() {
    // We received a ready request. We must send the token back as soon as we
    // can. However, because we launch the search in a separate thread, our main
    // thread is able to handle the commands asynchronously to the search. If we
    // don't answer the ready request in time, our engine will probably be
    // killed by the GUI.
    std::cout << "readyok" << std::endl;
}

void MartonChess::receiveNewGame() {
    search->suspend();

    // We received a new game command.

    // Initialize per-game settings here.
    *currentPosition = FenString::toPosition(FenString::STANDARDPOSITION);
}

void MartonChess::receivePosition(std::istringstream& input) {
    search->suspend();
	malformatPosition = false;
    // We received an position command. Just setup the position.

    std::string token;
    input >> token;
    if (token == "startpos") {
        *currentPosition = FenString::toPosition(FenString::STANDARDPOSITION);

        if (input >> token) {
            if (token != "moves") {
				malformatPosition = true;
            }
        }
    } else if (token == "fen") {
        std::string fen;

        while (input >> token) {
            if (token == "moves") {
                break;
            } else {
                fen += token + " ";
            }
        }

        *currentPosition = FenString::toPosition(fen);
    } else {
		malformatPosition = true;
    }

    MoveGenerator moveGenerator;

    while (input >> token) {
        // Verify moves
        MoveList<MoveEntry>& moves = moveGenerator.getLegalMoves(*currentPosition, 1, currentPosition->isCheck());
        bool found = false;
        for (int i = 0; i < moves.size; ++i) {
            int move = moves.entries[i]->move;
            if (fromMove(move) == token) {
                currentPosition->makeMove(move);
                found = true;
                break;
            }
        }

        if (!found) {
            malformatPosition = true;
        }
    }
}

void MartonChess::receiveGo(std::istringstream& input) {
    search->suspend();

    // We received a start command. Extract all parameters from the
    // command and start the search.
    std::string token;
    input >> token;
    uint64_t whiteTimeLeft = 1;
    uint64_t whiteTimeIncrement = 0;
    uint64_t blackTimeLeft = 1;
    uint64_t blackTimeIncrement = 0;
	uint64_t searchTime = 0;
    int searchMovesToGo = 50;
    bool ponder = false;

    do {
        if (token == "wtime") {
            if (!(input >> whiteTimeLeft)) {
                throw std::exception();
            }
        } else if (token == "winc") {
            if (!(input >> whiteTimeIncrement)) {
                throw std::exception();
            }
        } else if (token == "btime") {
            if (!(input >> blackTimeLeft)) {
                throw std::exception();
            }
        } else if (token == "binc") {
            if (!(input >> blackTimeIncrement)) {
                throw std::exception();
            }
        } else if (token == "movestogo") {
            if (!(input >> searchMovesToGo)) {
                throw std::exception();
            }
        } else if (token == "movetime") {
			if (!(input >> searchTime)) {
				throw std::exception();
			}
        }
    } while (input >> token);

	if (searchTime == 0) {
		if (currentPosition->activeColor == Color::WHITE) {
			searchTime = (whiteTimeLeft - 1000) / searchMovesToGo;
		} else {
			searchTime = (blackTimeLeft - 1000) / searchMovesToGo;
		}

		if (searchTime < 1000) {
			if (currentPosition->activeColor == Color::WHITE) {
				searchTime = (whiteTimeLeft / 10);
			}
			else {
				searchTime = (blackTimeLeft / 10);
			}
		}
	}

    search->newSearch(*currentPosition, searchTime);
    search->startTimer();

    search->resume();
    startTime = std::chrono::system_clock::now();
    statusStartTime = startTime;
}

void MartonChess::receivePonderHit() {
    search->startTimer();
    
}

void MartonChess::receiveStop() {
    // We received a stop command. If a search is running, stop it.
    search->suspend();
}

void MartonChess::sendBestMove(int bestMove, int ponderMove) {
    std::cout << "bestmove ";

    if (bestMove != Move::NOMOVE) {
        std::cout << fromMove(bestMove);

        if (ponderMove != Move::NOMOVE) {
            std::cout << " ponder " << fromMove(ponderMove);
        }
    } else {
        std::cout << "nomove";
    }

    std::cout << std::endl;
}

void MartonChess::sendStatus(
        int currentDepth, int currentMaxDepth, uint64_t totalNodes, int currentMove, int currentMoveNumber) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - statusStartTime).count() >= 1000) {
        sendStatus(false, currentDepth, currentMaxDepth, totalNodes, currentMove, currentMoveNumber);
    }
}

void MartonChess::sendStatus(
        bool force, int currentDepth, int currentMaxDepth, uint64_t totalNodes, int currentMove, int currentMoveNumber) {
    auto timeDelta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime);

    if (force || timeDelta.count() >= 1000) {
        std::cout << "info";
        std::cout << " depth " << currentDepth;
        std::cout << " seldepth " << currentMaxDepth;
        std::cout << " nodes " << totalNodes;
        std::cout << " time " << timeDelta.count();
        std::cout << " nps " << (timeDelta.count() >= 1000 ? (totalNodes * 1000) / timeDelta.count() : 0);

        if (currentMove != Move::NOMOVE) {
            std::cout << " currmove " << fromMove(currentMove);
            std::cout << " currmovenumber " << currentMoveNumber;
        }

        std::cout << std::endl;

        statusStartTime = std::chrono::system_clock::now();
    }
}

void MartonChess::sendMove(RootEntry entry, int currentDepth, int currentMaxDepth, uint64_t totalNodes) {
    auto timeDelta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime);

    std::cout << "info";
    std::cout << " depth " << currentDepth;
    std::cout << " seldepth " << currentMaxDepth;
    std::cout << " nodes " << totalNodes;
    std::cout << " time " << timeDelta.count();
    std::cout << " nps " << (timeDelta.count() >= 1000 ? (totalNodes * 1000) / timeDelta.count() : 0);

    if (std::abs(entry.value) >= Value::CHECKMATE_THRESHOLD) {
        // Calculate mate distance
        int mateDepth = Value::CHECKMATE - std::abs(entry.value);
        std::cout << " score mate " << ((entry.value > 0) - (entry.value < 0)) * (mateDepth + 1) / 2;
    } else {
        std::cout << " score cp " << entry.value;
    }

    /*if (entry.pv.size > 0) {
        std::cout << " pv";
        for (int i = 0; i < entry.pv.size; ++i) {
            std::cout << " " << fromMove(entry.pv.moves[i]);
        }
    }*/

    std::cout << std::endl;

    statusStartTime = std::chrono::system_clock::now();
}

std::string MartonChess::fromMove(int move) {
    std::string notation;

    notation += FenString::fromSquare(Move::getOriginSquare(move));
    notation += FenString::fromSquare(Move::getTargetSquare(move));

    int promotion = Move::getPromotion(move);
    if (promotion != PieceType::NOPIECETYPE) {
        notation += std::tolower(FenString::fromPieceType(promotion));
    }

    return notation;
}