
//#include <bit>
#include <iostream>
#include <random>
#include <array>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <vector>
#include <sstream> 
#include<nmmintrin.h>

#include <algorithm>
#include <chrono>

#define MATE_SCORE (1 << 15)
#define MAX_DEPTH 128
#define INF (1 << 16)
#define U16 unsigned __int16
#define S32 signed __int32
#define S64 signed __int64
#define U64 unsigned __int64

#define NAME "Gnome"
#define VERSION "2025-10-13"

S64 start_time = 0;

using namespace std;

string defFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
//string tstFen = "1k6/1pp1R1p1/4P3/4b1P1/5p2/3q4/1P2R1PK/8 b - - 0 1";

struct SOptions {
	int elo = 2500;
	int eloMin = 0;
	int eloMax = 2500;
	U64 hash = 64ULL << 15;

	string bishop = "32 55 -36 -4";
	string defense = "11 14 11 20 -6 18 -3 13 -62 13 -46 20";
	string king = "52 39";
	string material = "-27 13 22 -34 33";
	string mobility = "8 5 3 7 3 5 3 2";
	string outFile = "2 -6 -7 -5 -58 -4 -6 -8 -4 -1 12 -15";
	string outpost = "80 8 11 4";
	string outRank = "1 57 -17 5 -17 2 3 5 -10 11 16 -22";
	string passed = "-5 8 -49 -4 4";
	string pawn = "3 7 -28 -26 -8 -21 -10 3";
	string rook = "72 1 30 12";
	string tempo = "16 8";

}options;

enum Term { PASSED = 6, STRUCTURE, TERM_NB };

int scores[TERM_NB][2];

enum PieceType
{
	PAWN,
	KNIGHT,
	BISHOP,
	ROOK,
	QUEEN,
	KING,
	PT_NB
};

constexpr U64 FileABB = 0x0101010101010101ULL;
constexpr U64 FileBBB = FileABB << 1;
constexpr U64 FileCBB = FileABB << 2;
constexpr U64 FileDBB = FileABB << 3;
constexpr U64 FileEBB = FileABB << 4;
constexpr U64 FileFBB = FileABB << 5;
constexpr U64 FileGBB = FileABB << 6;
constexpr U64 FileHBB = FileABB << 7;

constexpr U64 Rank1BB = 0xFF;
constexpr U64 Rank2BB = Rank1BB << (8 * 1);
constexpr U64 Rank3BB = Rank1BB << (8 * 2);
constexpr U64 Rank4BB = Rank1BB << (8 * 3);
constexpr U64 Rank5BB = Rank1BB << (8 * 4);
constexpr U64 Rank6BB = Rank1BB << (8 * 5);
constexpr U64 Rank7BB = Rank1BB << (8 * 6);
constexpr U64 Rank8BB = Rank1BB << (8 * 7);

const U64 bbOutpostRanks = Rank4BB | Rank5BB | Rank6BB;

enum File : int { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB };

enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB };

const U64 bbLight = 0xaa55aa55aa55aa55ull;
const U64 bbDark = 0x55aa55aa55aa55aaull;

static S64 GetTimeMs() {
	return (clock() * 1000) / CLOCKS_PER_SEC;
}

struct Position {
	array<int, 4> castling = { true, true, true, true };
	array<U64, 2> color = { 0xFFFFULL, 0xFFFF000000000000ULL };
	array<U64, 6> pieces = { 0xFF00000000FF00ULL,
							0x4200000000000042ULL,
							0x2400000000000024ULL,
							0x8100000000000081ULL,
							0x800000000000008ULL,
							0x1000000000000010ULL };
	U64 ep = 0x0ULL;
	bool flipped = false;
}pos;

struct Move {
	int from = 0;
	int to = 0;
	int promo = 0;
};

const Move no_move{};

struct Stack {
	Move moves[218];
	Move move;
	Move killer;
	int score;
};

struct TT_Entry {
	U64 key;
	Move move;
	int score;
	int depth;
	U16 flag;
};

struct SSearchInfo {
	bool stop = false;
	int depthLimit = MAX_DEPTH;
	S64 timeStart = 0;
	S64 timeLimit = 0;
	U64 nodes = 0;
	U64 nodesLimit = 0;
}info;

int phase = 0;

static int S(const int mg, const int eg) {
	return (eg << 16) + mg;
}

const int phases[] = { 0, 1, 1, 2, 4, 0 };
int materialValOrg[PT_NB] = { 100,320,330,500,900,0 };
int max_material[PT_NB] = {};
int material[PT_NB] = {};
int mobility[PT_NB] = {};
int pawnProtection[PT_NB] = {};
int pawnConnected = 0;
int pawnDoubled = 0;
int pawnIsolated = 0;
int pawnBehind = 0;
int passedFile = 0;
int passedRank = 0;
int passedBlocked = 0;
int passedKU = 0;
int passedKE = 0;
int bishopPair = 0;
int bishopBad = 0;
int rook_open = 0;
int rook_semi_open = 0;
int kingShield1 = 0;
int kingShield2 = 0;
int outsideFile[PT_NB] = {};
int outsideRank[PT_NB] = {};
int bonus[PT_NB][RANK_NB][FILE_NB] = {};
int bonusMax[PT_NB][RANK_NB][FILE_NB] = {};
int outpost[2][2] = {};
int tempo = 0;
int contempt = 0;
vector<U64> hash_history;

const auto keys = []() {
	mt19937_64 r;

	// pieces from 1-12 multiplied the square + ep squares + castling rights
	// 12 * 64 + 64 + 16 = 848
	array<U64, 848> values;
	for (auto& val : values) {
		val = r();
	}

	return values;
	}();

// Engine options
auto num_tt_entries = 64ULL << 15;  // The first value is the size in megabytes

vector<TT_Entry> transposition_table;

static void PrintWelcome() {
	cout << NAME << " " << VERSION << endl;
}

void TranspositionClear() {
	memset(transposition_table.data(), 0, sizeof(TT_Entry) * transposition_table.size());
}


static U64 flip(const U64 bb) {
	return _byteswap_uint64(bb);
}

static U64 lsb(const U64 bb) {
	return _tzcnt_u64(bb);
}

static U64 count(const U64 bb) {
	return _mm_popcnt_u64(bb);
}

static U64 East(const U64 bb) {
	return (bb << 1) & ~0x0101010101010101ULL;
}

static U64 West(const U64 bb) {
	return (bb >> 1) & ~0x8080808080808080ULL;
}

static U64 North(const U64 bb) {
	return bb << 8;
}

static U64 South(const U64 bb) {
	return bb >> 8;
}

static U64 nw(const U64 bb) {
	return North(West(bb));
}

static U64 ne(const U64 bb) {
	return North(East(bb));
}

static U64 sw(const U64 bb) {
	return South(West(bb));
}

static U64 se(const U64 bb) {
	return South(East(bb));
}

auto operator==(const Move& lhs, const Move& rhs) {
	return !memcmp(&rhs, &lhs, sizeof(Move));
}

static inline int OutsideFile(int file) {
	return abs(file * 2 - 7) / 2 - 2;
}

static inline int OutsideRank(int rank) {
	return abs(rank * 2 - 7) / 2 - 2;
}

static inline int PassedRank(int rank) {
	return rank * rank;
}

static constexpr Rank RankOf(int s) { return Rank(s >> 3); }
static constexpr File FileOf(int s) { return File(s & 0b111); }

static int Distance(int s1, int s2) {
	return max(abs(FileOf(s1) - FileOf(s2)), abs(RankOf(s1) - RankOf(s2))) - 4;
};

static int Mg(int score) {
	return (short)score;
}

static int Eg(int score) {
	return (score + 0x8000) >> 16;
}

static int Max(int score) {
	return max(Mg(score), Eg(score));
}

static string SquareToUci(const int sq, const int flip) {
	string str;
	str += 'a' + (sq % 8);
	str += '1' + (flip ? (7 - sq / 8) : (sq / 8));
	return str;
}

static auto MoveToUci(const Move& move, const int flip) {
	string str;
	str += 'a' + (move.from % 8);
	str += '1' + (flip ? (7 - move.from / 8) : (move.from / 8));
	str += 'a' + (move.to % 8);
	str += '1' + (flip ? (7 - move.to / 8) : (move.to / 8));
	if (move.promo != PT_NB) {
		str += "\0nbrq\0\0"[move.promo];
	}
	return str;
}

static int PieceTypeOn(const Position& pos, const int sq) {
	const U64 bb = 1ULL << sq;
	for (int i = 0; i < 6; ++i) {
		if (pos.pieces[i] & bb) {
			return i;
		}
	}
	return PT_NB;
}

static void flip(Position& pos) {
	pos.color[0] = flip(pos.color[0]);
	pos.color[1] = flip(pos.color[1]);
	for (int i = 0; i < 6; ++i) {
		pos.pieces[i] = flip(pos.pieces[i]);
	}
	pos.ep = flip(pos.ep);
	swap(pos.color[0], pos.color[1]);
	swap(pos.castling[0], pos.castling[2]);
	swap(pos.castling[1], pos.castling[3]);
	pos.flipped = !pos.flipped;
}

template <typename F>
U64 Ray(const int sq, const U64 blockers, F f) {
	U64 mask = f(1ULL << sq);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	return mask;
}

static U64 KnightAttack(const int sq, const U64) {
	const U64 bb = 1ULL << sq;
	return (((bb << 15) | (bb >> 17)) & 0x7F7F7F7F7F7F7F7FULL) | (((bb << 17) | (bb >> 15)) & 0xFEFEFEFEFEFEFEFEULL) |
		(((bb << 10) | (bb >> 6)) & 0xFCFCFCFCFCFCFCFCULL) | (((bb << 6) | (bb >> 10)) & 0x3F3F3F3F3F3F3F3FULL);
}

static U64 BishopAttack(const int sq, const U64 blockers) {
	return Ray(sq, blockers, nw) | Ray(sq, blockers, ne) | Ray(sq, blockers, sw) | Ray(sq, blockers, se);
}

static U64 RookAttack(const int sq, const U64 blockers) {
	return Ray(sq, blockers, North) | Ray(sq, blockers, East) | Ray(sq, blockers, South) | Ray(sq, blockers, West);
}

static U64 KingAttack(const int sq, const U64) {
	const U64 bb = 1ULL << sq;
	return (bb << 8) | (bb >> 8) |
		(((bb >> 1) | (bb >> 9) | (bb << 7)) & 0x7F7F7F7F7F7F7F7FULL) |
		(((bb << 1) | (bb << 9) | (bb >> 7)) & 0xFEFEFEFEFEFEFEFEULL);
}

static bool Attacked(const Position& pos, const int sq, const int them = true) {
	const U64 bb = 1ULL << sq;
	const U64 kt = pos.color[them] & pos.pieces[KNIGHT];
	const U64 BQ = pos.pieces[BISHOP] | pos.pieces[QUEEN];
	const U64 RQ = pos.pieces[ROOK] | pos.pieces[QUEEN];
	const U64 pawns = pos.color[them] & pos.pieces[PAWN];
	const U64 pawn_attacks = them ? sw(pawns) | se(pawns) : nw(pawns) | ne(pawns);
	return (pawn_attacks & bb) | (kt & KnightAttack(sq, 0)) |
		(BishopAttack(sq, pos.color[0] | pos.color[1]) & pos.color[them] & BQ) |
		(RookAttack(sq, pos.color[0] | pos.color[1]) & pos.color[them] & RQ) |
		(KingAttack(sq, 0) & pos.color[them] & pos.pieces[KING]);
}

static auto MakeMove(Position& pos, const Move& move) {
	const int piece = PieceTypeOn(pos, move.from);
	const int captured = PieceTypeOn(pos, move.to);
	const U64 to = 1ULL << move.to;
	const U64 from = 1ULL << move.from;

	// Move the piece
	pos.color[0] ^= from | to;
	pos.pieces[piece] ^= from | to;

	// En passant
	if (piece == PAWN && to == pos.ep) {
		pos.color[1] ^= to >> 8;
		pos.pieces[PAWN] ^= to >> 8;
	}

	pos.ep = 0x0ULL;

	// Pawn double move
	if (piece == PAWN && move.to - move.from == 16) {
		pos.ep = to >> 8;
	}

	// Captures
	if (captured != PT_NB) {
		pos.color[1] ^= to;
		pos.pieces[captured] ^= to;
	}

	// Castling
	if (piece == KING) {
		const U64 bb = move.to - move.from == 2 ? 0xa0ULL : move.to - move.from == -2 ? 0x9ULL : 0x0ULL;
		pos.color[0] ^= bb;
		pos.pieces[ROOK] ^= bb;
	}

	// Promotions
	if (piece == PAWN && move.to >= 56) {
		pos.pieces[PAWN] ^= to;
		pos.pieces[move.promo] ^= to;
	}

	// Update castling permissions
	pos.castling[0] &= !((from | to) & 0x90ULL);
	pos.castling[1] &= !((from | to) & 0x11ULL);
	pos.castling[2] &= !((from | to) & 0x9000000000000000ULL);
	pos.castling[3] &= !((from | to) & 0x1100000000000000ULL);

	flip(pos);

	// Return move legality
	return !Attacked(pos, lsb(pos.color[1] & pos.pieces[KING]), false);
}

static void add_move(Move* const movelist, int& num_moves, const int from, const int to, const int promo = PT_NB) {
	movelist[num_moves++] = Move{ from, to, promo };
}

static void generate_pawn_moves(Move* const movelist, int& num_moves, U64 to_mask, const int offset) {
	while (to_mask) {
		const int to = lsb(to_mask);
		to_mask &= to_mask - 1;
		if (to >= 56) {
			add_move(movelist, num_moves, to + offset, to, QUEEN);
			add_move(movelist, num_moves, to + offset, to, ROOK);
			add_move(movelist, num_moves, to + offset, to, BISHOP);
			add_move(movelist, num_moves, to + offset, to, KNIGHT);
		}
		else {
			add_move(movelist, num_moves, to + offset, to);
		}
	}
}

static void generate_piece_moves(Move* const movelist,
	int& num_moves,
	const Position& pos,
	const int piece,
	const U64 to_mask,
	U64(*func)(int, U64)) {
	U64 copy = pos.color[0] & pos.pieces[piece];
	while (copy) {
		const int fr = lsb(copy);
		copy &= copy - 1;
		U64 moves = func(fr, pos.color[0] | pos.color[1]) & to_mask;
		while (moves) {
			const int to = lsb(moves);
			moves &= moves - 1;
			add_move(movelist, num_moves, fr, to);
		}
	}
}

static int MoveGen(const Position& pos, Move* const movelist, const bool only_captures) {
	int num_moves = 0;
	const U64 all = pos.color[0] | pos.color[1];
	const U64 to_mask = only_captures ? pos.color[1] : ~pos.color[0];
	const U64 pawns = pos.color[0] & pos.pieces[PAWN];
	generate_pawn_moves(
		movelist, num_moves, North(pawns) & ~all & (only_captures ? 0xFF00000000000000ULL : 0xFFFFFFFFFFFF0000ULL), -8);
	if (!only_captures) {
		generate_pawn_moves(movelist, num_moves, North(North(pawns & 0xFF00ULL) & ~all) & ~all, -16);
	}
	generate_pawn_moves(movelist, num_moves, nw(pawns) & (pos.color[1] | pos.ep), -7);
	generate_pawn_moves(movelist, num_moves, ne(pawns) & (pos.color[1] | pos.ep), -9);
	generate_piece_moves(movelist, num_moves, pos, KNIGHT, to_mask, KnightAttack);
	generate_piece_moves(movelist, num_moves, pos, BISHOP, to_mask, BishopAttack);
	generate_piece_moves(movelist, num_moves, pos, QUEEN, to_mask, BishopAttack);
	generate_piece_moves(movelist, num_moves, pos, ROOK, to_mask, RookAttack);
	generate_piece_moves(movelist, num_moves, pos, QUEEN, to_mask, RookAttack);
	generate_piece_moves(movelist, num_moves, pos, KING, to_mask, KingAttack);
	if (!only_captures && pos.castling[0] && !(all & 0x60ULL) && !Attacked(pos, 4) && !Attacked(pos, 5)) {
		add_move(movelist, num_moves, 4, 6);
	}
	if (!only_captures && pos.castling[1] && !(all & 0xEULL) && !Attacked(pos, 4) && !Attacked(pos, 3)) {
		add_move(movelist, num_moves, 4, 2);
	}
	return num_moves;
}

//Pretty-prints the position (including FEN and hash key)
static void PrintBoard(Position& pos) {
	bool r = pos.flipped;
	if (r)
		flip(pos);
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	cout << t;
	for (int i = 56; i >= 0; i -= 8) {
		cout << s << " " << i / 8 + 1 << " ";
		for (int j = 0; j < 8; j++) {
			int sq = i + j;
			int piece = PieceTypeOn(pos, sq);
			if (pos.color[0] & 1ull << sq)
				cout << "| " << "ANBRQK "[piece] << " ";
			else
				cout << "| " << "anbrqk "[piece] << " ";
		}
		cout << "| " << i / 8 + 1 << endl;
	}
	cout << s;
	cout << t << endl;
	if (r)
		flip(pos);
}

static int TotalScore(int c) {
	int score = 0;
	for (int n = 0; n < TERM_NB; n++)
		score += scores[n][c];
	return score;
}

static U64 Span(U64 bb) {
	return bb | bb >> 8 | bb >> 16 | bb >> 24 | bb >> 32;
}

static constexpr U64 Attacks(int pt, int sq, U64 blockers) {
	switch (pt) {
	case ROOK:
		return RookAttack(sq, blockers);
	case BISHOP:
		return BishopAttack(sq, blockers);
	case QUEEN:
		return RookAttack(sq, blockers) | BishopAttack(sq, blockers);
	case KNIGHT:
		return KnightAttack(sq, blockers);
	case KING:
		return KingAttack(sq, blockers);
	default:
		return 0;
	}
}

static int Eval(Position& pos) {
	std::memset(scores, 0, sizeof(scores));
	int score = tempo;
	phase = 0;
	U64 blockers = pos.color[0] | pos.color[1];
	for (int c = 0; c < 2; ++c) {
		const U64 pawns[] = { pos.color[0] & pos.pieces[PAWN], pos.color[1] & pos.pieces[PAWN] };
		const U64 bbDefense = nw(pawns[0]) | ne(pawns[0]);
		const U64 bbAttack = se(pawns[1]) | sw(pawns[1]);
		const U64 bbSpan = Span(bbAttack);
		const U64 bbOutpost = ~bbSpan & bbOutpostRanks;
		const int sqKUs = lsb(pos.color[0] & pos.pieces[KING]);
		const int sqKEn = lsb(pos.color[1] & pos.pieces[KING]);
		U64 bbConnected = bbDefense | South(bbDefense);
		bbConnected |= South(bbConnected);
		for (int pt = 0; pt < 6; ++pt) {
			auto copy = pos.color[0] & pos.pieces[pt];
			while (copy) {
				phase += phases[pt];

				const int sq = lsb(copy);
				copy &= copy - 1;
				const int rank = sq / 8;
				const int file = sq % 8;
				scores[pt][pos.flipped] += bonus[pt][rank][file];
				const U64 bbPiece = 1ULL << sq;
				if (bbPiece & bbDefense) {
					scores[pt][pos.flipped] += pawnProtection[pt];
				}
				if (pt == PAWN) {
					// Passed pawns
					U64 bbFile = 0x101010101010101ULL << file;
					U64 bbForward = 0x101010101010100ULL << sq;
					U64 blockers = bbForward | West(bbForward) | East(bbForward);
					if (!(blockers & pawns[1])) {
						int passed = OutsideFile(file) * passedFile;
						passed += PassedRank(rank - 1) * passedRank;
						if (North(bbPiece) & pos.color[1])
							passed += passedBlocked;
						int sq2 = sq + 8;
						passed += Distance(sq2, sqKUs) * passedKU;
						passed += Distance(sq2, sqKEn) * passedKE;
						scores[PASSED][pos.flipped] += S(passed >> 1, passed);
					}
					int structure = 0;

					if (bbForward & pawns[0]) {
						structure += pawnDoubled;
					}
					if (bbConnected & bbPiece) {
						structure += pawnConnected;
					}
					else {
						U64 bbAdjacent = East(bbFile) | West(bbFile);
						if (!(bbAdjacent & pawns[0])) {
							structure += pawnIsolated;
						}
						else {
							bbAdjacent &= North(bbDefense);
							if (bbAdjacent & pawns[0] && bbAdjacent & pawns[1])
								structure += pawnBehind;
						}
					}
					scores[STRUCTURE][pos.flipped] += structure;
				}
				else if (pt == KING) {
					if ((file < 3 || file>4)) {
						U64 bbShield1 = North(bbPiece);
						bbShield1 |= East(bbShield1) | West(bbShield1);
						U64 bbShield2 = North(bbShield1);
						//PrintBitboard(pos.flipped,bbShield2);
						int v1 = kingShield1 * count(bbShield1 & pawns[0]);
						int v2 = kingShield2 * count(bbShield2 & pawns[0]);
						scores[pt][pos.flipped] += S(v1 + v2, 0);
					}
				}
				else {
					scores[pt][pos.flipped] += mobility[pt] * count(Attacks(pt, sq, blockers) & ~bbAttack);
					if (pt == ROOK) {
						// Rook on open or semi-open files
						const U64 file_bb = 0x101010101010101ULL << file;
						if (!(file_bb & pawns[0])) {
							if (!(file_bb & pawns[1])) {
								scores[pt][pos.flipped] += rook_open;
							}
							else {
								scores[pt][pos.flipped] += rook_semi_open;
							}
						}
					}
					else if ((pt == KNIGHT) || (pt == BISHOP)) {
						if (bbOutpost & bbPiece)
							scores[pt][pos.flipped] += outpost[pt == BISHOP][bbDefense && bbPiece] * 2;
						else {
							U64 bbMoves = (pt == KNIGHT) ? KnightAttack(sq, pos.color[0]) : BishopAttack(sq, pos.color[0] | pos.color[1]);
							U64 bb = bbMoves & bbOutpost & ~pos.color[0];
							if (bb)
								scores[pt][pos.flipped] += outpost[pt == BISHOP][bbDefense && bb];
						}
					}
				}
			}
		}


		U64 bbPieces = pos.pieces[BISHOP] & pos.color[0];
		if (bbPieces) {
			bool bw = bbPieces & bbLight;
			bool bb = bbPieces & bbDark;
			if (bw && bb)
				scores[BISHOP][pos.flipped] += bishopPair;
			else if (bw)
				scores[BISHOP][pos.flipped] += bishopBad * count(bbLight & pawns[0]);
			else
				scores[BISHOP][pos.flipped] += bishopBad * count(bbDark & pawns[0]);
		}

		score += TotalScore(pos.flipped);
		flip(pos);
		score = -score;
	}
	return (Mg(score) * phase + Eg(score) * (24 - phase)) / 24;
}

static auto GetHash(const Position& pos) {
	U64 hash = pos.flipped;

	// Pieces
	U64 copy = pos.color[0] | pos.color[1];
	while (copy) {
		const int sq = lsb(copy);
		copy &= copy - 1;
		hash ^= keys[(PieceTypeOn(pos, sq) + 6 * ((pos.color[pos.flipped] >> sq) & 1)) * 64 + sq];
	}

	// En passant square
	if (pos.ep) {
		hash ^= keys[768 + lsb(pos.ep)];
	}

	// Castling permissions
	hash ^= keys[832 + (pos.castling[0] | pos.castling[1] << 1 | pos.castling[2] << 2 | pos.castling[3] << 3)];

	return hash;
}

static void CheckUp() {
	if ((info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit) || (info.nodesLimit && info.nodes > info.nodesLimit))
		info.stop = true;
}

static int Permill() {
	int pm = 0;
	for (int n = 0; n < 1000; n++) {
		if (transposition_table[n].key)
			pm++;
	}
	return pm;
}

static bool IsPseudolegalMove(const Position& pos, const Move& move) {
	Move moves[256];
	const int num_moves = MoveGen(pos, moves, false);
	for (int i = 0; i < num_moves; ++i) {
		if (moves[i] == move) {
			return true;
		}
	}
	return false;
}


static void PrintPv(const Position& pos, const Move move, vector<U64>& hash_history) {
	// Check move pseudolegality
	if (!IsPseudolegalMove(pos, move)) {
		return;
	}

	// Check move legality
	auto npos = pos;
	if (!MakeMove(npos, move)) {
		return;
	}

	// Print current move
	cout << " " << MoveToUci(move, pos.flipped);

	// Probe the TT in the resulting position
	const U64 tt_key = GetHash(npos);
	const TT_Entry& tt_entry = transposition_table[tt_key % num_tt_entries];

	// Only continue if the move was valid and comes from a PV search
	if (tt_entry.key != tt_key || tt_entry.move == Move{} || tt_entry.flag != 0) {
		return;
	}

	// Avoid infinite recursion on a repetition
	for (const auto old_hash : hash_history) {
		if (old_hash == tt_key) {
			return;
		}
	}

	hash_history.emplace_back(tt_key);
	PrintPv(npos, tt_entry.move, hash_history);
	hash_history.pop_back();
}

static int SearchAlpha(Position& pos,
	int alpha,
	const int beta,
	int depth,
	const int ply,
	Stack* const stack,
	int64_t(&hh_table)[2][64][64],
	vector<U64>& hash_history,
	const int do_null = true) {
	// Exit early if out of time
	if ((++info.nodes & 0xffff) == 0)
		CheckUp();
	if (info.stop)
		return 0;
	const int static_eval = Eval(pos);
	// Don't overflow the stack
	if (ply > 127) {
		return static_eval;
	}
	stack[ply].score = static_eval;
	// Check extensions
	const auto in_check = Attacked(pos, lsb(pos.color[0] & pos.pieces[KING]));
	depth = in_check ? max(1, depth + 1) : depth;
	const int improving = ply > 1 && static_eval > stack[ply - 2].score;
	int in_qsearch = depth <= 0;
	if (in_qsearch && static_eval > alpha) {
		if (static_eval >= beta) {
			return beta;
		}
		alpha = static_eval;
	}

	const U64 tt_key = GetHash(pos);

	if (ply > 0 && !in_qsearch) {
		// Repetition detection
		for (const auto old_hash : hash_history) {
			if (old_hash == tt_key) {
				return 0;
			}
		}

		if (!in_check && alpha == beta - 1) {

			// Reverse futility pruning
			if (depth < 8) {
				if (static_eval - 71 * (depth - improving) >= beta)
					return static_eval;
				in_qsearch = static_eval + 238 * depth < alpha;
			}

			// Null move pruning
			if (depth > 2 && static_eval >= beta && do_null && pos.color[0] & ~pos.pieces[PAWN] & ~pos.pieces[KING]) {
				Position npos = pos;
				flip(npos);
				npos.ep = 0;
				if (-SearchAlpha(npos,
					-beta,
					-alpha,
					depth - 4 - depth / 5 - min((static_eval - beta) / 196, 3),
					ply + 1,
					stack,
					hh_table,
					hash_history,
					false) >= beta) {
					return beta;
				}
			}

			// Razoring
			/*if (depth == 1 && static_eval + 200 < alpha) {
				return SearchAlpha(pos,
					alpha,
					beta,
					0,
					ply,
					// minify enable filter delete
					nodes,
					// minify disable filter delete
					stop_time,
					stop,
					stack,
					hh_table,
					hash_history,
					do_null);
			}*/
		}
	}

	// TT Probing
	TT_Entry& tt_entry = transposition_table[tt_key % num_tt_entries];
	Move tt_move{};
	if (tt_entry.key == tt_key) {
		tt_move = tt_entry.move;
		if (ply > 0 && tt_entry.depth >= depth) {
			if (tt_entry.flag == 0) {
				return tt_entry.score;
			}
			if (tt_entry.flag == 1 && tt_entry.score <= alpha) {
				return tt_entry.score;
			}
			if (tt_entry.flag == 2 && tt_entry.score >= beta) {
				return tt_entry.score;
			}
		}
	}
	// Internal iterative reduction
	else if (depth > 3) {
		depth--;
	}

	auto& moves = stack[ply].moves;
	const int num_moves = MoveGen(pos, moves, in_qsearch);

	// Score moves
	int64_t move_scores[256];
	for (int j = 0; j < num_moves; ++j) {
		const int capture = PieceTypeOn(pos, moves[j].to);
		if (moves[j] == tt_move) {
			move_scores[j] = 1LL << 62;
		}
		else if (capture != PT_NB) {
			move_scores[j] = ((capture + 1) * (1LL << 54)) - PieceTypeOn(pos, moves[j].from);
		}
		else if (moves[j] == stack[ply].killer) {
			move_scores[j] = 1LL << 50;
		}
		else {
			move_scores[j] = hh_table[pos.flipped][moves[j].from][moves[j].to];
		}
	}

	int quiet_moves_evaluated = 0;
	int moves_evaluated = 0;
	int best_score = -INF;
	Move best_move{};
	uint16_t tt_flag = 1;  // Alpha flag
	hash_history.emplace_back(tt_key);
	for (int i = 0; i < num_moves; ++i) {
		// Find best move remaining
		int best_move_index = i;
		for (int j = i; j < num_moves; ++j) {
			if (move_scores[j] > move_scores[best_move_index]) {
				best_move_index = j;
			}
		}

		const auto move = moves[best_move_index];
		const auto best_move_score = move_scores[best_move_index];

		moves[best_move_index] = moves[i];
		move_scores[best_move_index] = move_scores[i];

		// Material gain
		const S32 gain = max_material[move.promo] + max_material[PieceTypeOn(pos, move.to)];

		// Delta pruning
		if (in_qsearch && !in_check && static_eval + 50 + gain < alpha)
			break;

		// Forward futility pruning
		if (ply > 0 && depth < 8 && !in_qsearch && !in_check && moves_evaluated && static_eval + 105 * depth + gain < alpha)
			break;

		auto npos = pos;
		if (!MakeMove(npos, move)) {
			continue;
		}

		int score;
		if (in_qsearch || !moves_evaluated) {
		full_window:
			score = -SearchAlpha(npos,
				-beta,
				-alpha,
				depth - 1,
				ply + 1,
				stack,
				hh_table,
				hash_history);
		}
		else {
			// Late move reduction
			int reduction = max(0,
				depth > 3 && moves_evaluated > 3
				? 1 + moves_evaluated / 16 + depth / 10 + (alpha == beta - 1) - improving
				: 0);

		zero_window:
			score = -SearchAlpha(npos,
				-alpha - 1,
				-alpha,
				depth - reduction - 1,
				ply + 1,
				stack,
				hh_table,
				hash_history);

			if (reduction > 0 && score > alpha) {
				reduction = 0;
				goto zero_window;
			}

			if (score > alpha && score < beta) {
				goto full_window;
			}
		}
		moves_evaluated++;
		if (PieceTypeOn(pos, move.to) == PT_NB) {
			quiet_moves_evaluated++;
		}

		// Exit early if out of time
		if (info.stop) {hash_history.pop_back();return 0;}

		if (score > best_score) {
			best_score = score;
			best_move = move;
			if (score > alpha) {
				tt_flag = 0;  // Exact flag
				alpha = score;
				stack[ply].move = move;

				if (!ply) {
					cout << "info";
					cout << " depth " << depth;
					if (abs(score) < MATE_SCORE - MAX_DEPTH)
						cout << " score cp " << score;
					else
						cout << " score mate " << (score > 0 ? (MATE_SCORE - score + 1) >> 1 : -(MATE_SCORE + score) >> 1);
					const auto elapsed = GetTimeMs() - info.timeStart;
					cout << " alpha " << alpha;
					cout << " beta " << beta;
					cout << " time " << elapsed;
					cout << " nodes " << info.nodes;
					if (elapsed > 0) {
						cout << " nps " << info.nodes * 1000 / elapsed;
					}
					cout << " hashfull " << Permill();
					cout << " currmovenumber " << moves_evaluated;
					cout << " pv";
					PrintPv(pos, stack[0].move, hash_history);
					cout << endl;
				}
			}
		}
		else if (!in_qsearch && !in_check && alpha == beta - 1 && depth <= 3 && moves_evaluated >= (depth * 3) + 2 &&
			static_eval < alpha - (50 * depth) && best_move_score < (1LL << 50)) {
			best_score = alpha;
			break;
		}

		if (alpha >= beta) {
			tt_flag = 2;  // Beta flag
			const int capture = PieceTypeOn(pos, move.to);
			if (capture == PT_NB) {
				hh_table[pos.flipped][move.from][move.to] += depth * depth;
				stack[ply].killer = move;
			}
			break;
		}

		// Late move pruning based on quiet move count
		if (!in_check && alpha == beta - 1 && quiet_moves_evaluated > 3 + 2 * depth * depth) {
			break;
		}
	}
	hash_history.pop_back();

	// Return mate or draw scores if no moves found
	if (best_score == -INF) {
		return in_qsearch ? alpha : in_check ? ply - MATE_SCORE : 0;
	}

	// Save to TT
	if (tt_entry.key != tt_key || depth >= tt_entry.depth || tt_flag == 0) {
		tt_entry =
			TT_Entry{ tt_key, best_move == no_move ? tt_move : best_move, best_score, in_qsearch ? 0 : depth, tt_flag };
	}

	return alpha;
}

static Move SearchIteratively(Position& pos, vector<U64>& hash_history) {
	info.stop = false;
	info.nodes = 0;
	info.timeStart = GetTimeMs();
	TranspositionClear();
	Stack stack[128] = {};
	int64_t hh_table[2][64][64] = {};

	int score = 0;
	for (int i = 1; i <= info.depthLimit; ++i) {
		auto window = 40;
		auto research = 0;
	research:
		int alpha = score - window;
		int beta = score + window;
		const int newscore = SearchAlpha(pos,
			alpha,
			beta,
			i,
			0,
			stack,
			hh_table,
			hash_history);
		if (info.stop)
			break;
		if (newscore >= score + window || newscore <= score - window) {
			window <<= ++research;
			score = newscore;
			goto research;
		}

		score = newscore;

		// Early exit after completed ply
		if (!research && GetTimeMs() - info.timeStart > info.timeLimit / 2) {
			break;
		}
	}
	return stack[0].move;
}

static void SetFen(Position& pos, const string& fen) {
	if (fen == "startpos") {
		pos = Position();
		return;
	}

	// Clear
	pos.color = {};
	pos.pieces = {};
	pos.castling = {};

	stringstream ss{ fen };
	string word;

	ss >> word;
	int i = 56;
	for (const auto c : word) {
		if (c >= '1' && c <= '8') {
			i += c - '1' + 1;
		}
		else if (c == '/') {
			i -= 16;
		}
		else {
			const int side = c == 'p' || c == 'n' || c == 'b' || c == 'r' || c == 'q' || c == 'k';
			const int piece = (c == 'p' || c == 'P') ? PAWN
				: (c == 'n' || c == 'N') ? KNIGHT
				: (c == 'b' || c == 'B') ? BISHOP
				: (c == 'r' || c == 'R') ? ROOK
				: (c == 'q' || c == 'Q') ? QUEEN
				: KING;
			pos.color.at(side) ^= 1ULL << i;
			pos.pieces.at(piece) ^= 1ULL << i;
			i++;
		}
	}

	// Side to move
	ss >> word;
	const bool black_move = word == "b";

	// Castling permissions
	ss >> word;
	for (const auto c : word) {
		pos.castling[0] |= c == 'K';
		pos.castling[1] |= c == 'Q';
		pos.castling[2] |= c == 'k';
		pos.castling[3] |= c == 'q';
	}

	// En passant
	ss >> word;
	if (word != "-") {
		const int sq = word[0] - 'a' + 8 * (word[1] - '1');
		pos.ep = 1ULL << sq;
	}

	// Flip the board if necessary
	if (black_move) {
		flip(pos);
	}
}

static void SplitStr(const std::string& txt, std::vector<std::string>& vStr, char ch) {
	vStr.clear();
	if (txt == "")
		return;
	size_t pos = txt.find(ch);
	size_t initialPos = 0;
	while (pos != std::string::npos) {
		vStr.push_back(txt.substr(initialPos, pos - initialPos));
		initialPos = pos + 1;
		pos = txt.find(ch, initialPos);
	}
	vStr.push_back(txt.substr(initialPos, std::min(pos, txt.size()) - initialPos + 1));
}

static void SplitInt(const string& txt, vector<int>& vInt, char ch) {
	vInt.clear();
	vector<string> vs;
	SplitStr(txt, vs, ch);
	for (string s : vs)
		vInt.push_back(stoi(s));
}

static int GetVal(vector<int> v, int i) {
	if (i >= 0 && i < v.size())
		return v[i];
	return 0;
}

static void InitEval() {
	int mg, eg;
	vector<int> split{};
	SplitInt(options.material, split, ' ');
	int eloMod = 600 - (600 * options.elo) / 2500;
	for (int pt = PAWN; pt < PT_NB; pt++) {
		int md = GetVal(split, pt);
		mg = materialValOrg[pt] + md - eloMod;
		eg = materialValOrg[pt] - md;
		material[pt] = S(mg, eg);
		max_material[pt] = max(mg, eg);
	}

	SplitInt(options.mobility, split, ' ');
	for (int pt = KNIGHT; pt < KING; pt++) {
		mg = GetVal(split, (pt - 1) * 2);
		eg = GetVal(split, (pt - 1) * 2 + 1);
		mobility[pt] = S(mg, eg);
	}

	SplitInt(options.outFile, split, ' ');
	for (int pt = PAWN; pt < PT_NB; pt++) {
		mg = GetVal(split, pt * 2);
		eg = GetVal(split, pt * 2 + 1);
		outsideFile[pt] = S(mg, eg);
	}
	SplitInt(options.outRank, split, ' ');
	for (int pt = PAWN; pt < PT_NB; pt++) {
		mg = GetVal(split, pt * 2);
		eg = GetVal(split, pt * 2 + 1);
		outsideRank[pt] = S(mg, eg);
	}
	SplitInt(options.defense, split, ' ');
	for (int pt = PAWN; pt < PT_NB; pt++) {
		mg = GetVal(split, pt * 2);
		eg = GetVal(split, pt * 2 + 1);
		pawnProtection[pt] = S(mg, eg);
	}

	SplitInt(options.bishop, split, ' ');
	mg = GetVal(split, 0);
	eg = GetVal(split, 1);
	bishopPair = S(mg, eg);
	mg = GetVal(split, 2);
	eg = GetVal(split, 3);
	bishopBad = S(mg, eg);

	SplitInt(options.pawn, split, ' ');
	mg = GetVal(split, 0);
	eg = GetVal(split, 1);
	pawnConnected = S(mg, eg);
	mg = GetVal(split, 2);
	eg = GetVal(split, 3);
	pawnDoubled = S(mg, eg);
	mg = GetVal(split, 4);
	eg = GetVal(split, 5);
	pawnIsolated = S(mg, eg);
	mg = GetVal(split, 6);
	eg = GetVal(split, 7);
	pawnBehind = S(mg, eg);

	SplitInt(options.king, split, ' ');
	kingShield1 = GetVal(split, 0);
	kingShield2 = GetVal(split, 1);

	SplitInt(options.rook, split, ' ');
	mg = GetVal(split, 0);
	eg = GetVal(split, 1);
	rook_open = S(mg, eg);
	mg = GetVal(split, 2);
	eg = GetVal(split, 3);
	rook_semi_open = S(mg, eg);

	SplitInt(options.passed, split, ' ');
	passedFile = GetVal(split, 0);
	passedRank = GetVal(split, 1);
	passedBlocked = GetVal(split, 2);
	passedKU = GetVal(split, 3);
	passedKE = GetVal(split, 4);

	SplitInt(options.outpost, split, ' ');
	for (int s = 0; s < 2; s++) {
		mg = GetVal(split, s * 2);
		eg = GetVal(split, s * 2 + 1);
		int score = S(mg, eg);
		outpost[0][s] = score * 2;
		outpost[1][s] = score;
	}

	SplitInt(options.tempo, split, ' ');
	mg = GetVal(split, 0);
	eg = GetVal(split, 1);
	tempo = S(mg, eg);

	for (int pt = PAWN; pt < PT_NB; ++pt)
		for (int r = RANK_1; r < RANK_NB; ++r)
			for (int f = FILE_A; f < FILE_NB; ++f)
			{
				bonus[pt][r][f] = material[pt];
				bonus[pt][r][f] += outsideFile[pt] * OutsideFile(f);
				if (pt == PAWN) {
					bonus[pt][r][f] += outsideRank[pt] * (r - 4);
				}
				else
				{
					bonus[pt][r][f] += outsideRank[pt] * OutsideRank(r);
				}
				bonusMax[pt][r][f] = Max(bonus[pt][r][f]);
			}
}

static string StrToLower(string s) {
	transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

static std::string trim(const std::string& s)
{
	if (s.empty())
		return s;
	auto start = s.begin();
	while (start != s.end() && std::isspace(*start)) {
		start++;
	}

	auto end = s.end();
	do {
		end--;
	} while (std::distance(start, end) > 0 && std::isspace(*end));

	return std::string(start, end + 1);
}

//Get next word after uci command
static bool UciValue(vector<string> list, string command, string& value) {
	value = "";
	for (size_t n = 0; n < list.size() - 1; n++)
		if (list[n] == command) {
			value = list[n + 1];
			return true;
		}
	return false;
}

static bool UciValues(vector<string> list, string command, string& value) {
	bool result = false;
	value = "";
	for (size_t n = 0; n < list.size(); n++) {
		if (result)
			value += " " + list[n];
		else if (list[n] == command)
			result = true;
	}
	value = trim(value);
	return result;
}

static int ScoreToValue(int score) {
	int mgWeight = phase;
	int egWeight = 24 - mgWeight;
	return (mgWeight * Mg(score) + egWeight * Eg(score)) / 24;
}

static string ShowScore(string result) {
	int len = 16 - result.length();
	if (len < 0)
		len = 0;
	result.append(len, ' ');
	return result;
}

static string ShowScore(int s) {
	int v = ScoreToValue(s);
	return ShowScore(to_string(v) + " (" + to_string(Mg(s)) + " " + to_string((int)Eg(s)) + ")");
}

static void PrintTerm(string name, int idx) {
	int sw = scores[idx][0];
	int sb = scores[idx][1];
	std::cout << ShowScore(name) << ShowScore(sw) << " " << ShowScore(sb) << " " << ShowScore(sw - sb) << endl;
}

// Function to put thousands
// separators in the given integer
string ThousandSeparator(uint64_t n)
{
	string ans = "";

	// Convert the given integer
	// to equivalent string
	string num = to_string(n);

	// Initialise count
	int count = 0;

	// Traverse the string in reverse
	for (int i = (int)num.size() - 1; i >= 0; i--) {
		ans.push_back(num[i]);

		// If three characters
		// are traversed
		if (++count == 3) {
			ans.push_back(' ');
			count = 0;
		}
	}

	// Reverse the string to get
	// the desired output
	reverse(ans.begin(), ans.end());

	// If the given string is
	// less than 1000
	if (ans.size() % 4 == 0) {

		// Remove ','
		ans.erase(ans.begin());
	}

	return ans;
}

//Displays a summary
static void ShowInfo(uint64_t time, uint64_t nodes) {
	if (time < 1)
		time = 1;
	uint64_t nps = (nodes * 1000) / time;
	printf("-----------------------------\n");
	cout << "Time        : " << ThousandSeparator(time) << endl;
	cout << "Nodes       : " << ThousandSeparator(nodes) << endl;
	cout << "Nps         : " << ThousandSeparator(nps) << endl;
	printf("-----------------------------\n");
}

static void UciBench() {
	info.depthLimit = MAX_DEPTH;
	info.nodesLimit = 0;
	info.timeLimit = 10000;
	SearchIteratively(pos, hash_history);
	ShowInfo(GetTimeMs() - info.timeStart, info.nodes);
}

static void UciEval() {
	SetFen(pos, "1k6/1pp1R1p1/4PN2/4b1P1/5p2/3q1n2/1P2R1PK/8 b - - 0 1");
	PrintBoard(pos);
	cout << "side " << (pos.flipped ? "black" : "white") << endl;
	int score = Eval(pos);
	PrintTerm("Pawn", PAWN);
	PrintTerm("Knight", KNIGHT);
	PrintTerm("Bishop", BISHOP);
	PrintTerm("Rook", ROOK);
	PrintTerm("Queen", QUEEN);
	PrintTerm("King", KING);
	PrintTerm("Passed", PASSED);
	PrintTerm("Structure", STRUCTURE);
	cout << "phase " << phase << endl;
	cout << "score " << score << endl;
}

static void UciQuit() {
	exit(0);
}

static void UciCommand(string str) {
	str = trim(str);
	string value;
	vector<string> split{};
	SplitStr(str, split, ' ');
	if (split.empty())
		return;
	string command = split[0];
	if (command == "uci")
	{
		cout << "id name " << NAME << endl;
		cout << "option name UCI_Elo type spin default " << options.eloMax << " min " << options.eloMin << " max " << options.eloMax << endl;
		cout << "option name hash type spin default " << (options.hash >> 15) << " min 1 max 65536" << endl;
		cout << "option name bishop type string default " << options.bishop << endl;
		cout << "option name material type string default " << options.material << endl;
		cout << "option name mobility type string default " << options.mobility << endl;
		cout << "option name outFile type string default " << options.outFile << endl;
		cout << "option name outRank type string default " << options.outRank << endl;
		cout << "option name pawn type string default " << options.pawn << endl;
		cout << "option name defense type string default " << options.defense << endl;
		cout << "option name rook type string default " << options.rook << endl;
		cout << "uciok" << endl;
	}
	else if (command == "setoption")
	{
		string name;
		bool isValue = UciValues(split, "value", value);
		if (isValue && UciValue(split, "name", name)) {
			name = StrToLower(name);
			if (name == "uci_elo") {
				options.elo = stoi(value);
				InitEval();
			}
			else if (name == "hash") {
				options.hash = stoi(value);
				options.hash = min(max(options.hash, 1ULL), 0xffffULL) * 1000000 / sizeof(TT_Entry);
				transposition_table.clear();
				transposition_table.resize(options.hash);
				transposition_table.shrink_to_fit();
			}
			else if (name == "bishop")
				options.bishop = value;
			else if (name == "king")
				options.king = value;
			else if (name == "material")
				options.material = value;
			else if (name == "mobility")
				options.mobility = value;
			else if (name == "outFile")
				options.outFile = value;
			else if (name == "outRank")
				options.outRank = value;
			else if (name == "outpost")
				options.outpost = value;
			else if (name == "pawn")
				options.pawn = value;
			else if (name == "defense")
				options.defense = value;
			else if (name == "rook")
				options.rook = value;
		}
	}
	else if (command == "isready") {
		cout << "readyok" << endl;
	}
	else if (command == "ucinewgame") {
		memset(transposition_table.data(), 0, sizeof(TT_Entry) * transposition_table.size());
	}
	else if (command == "position") {
		pos = Position();
		hash_history.clear();
		int mark = 0;
		string pFen = "";
		vector<string> pMoves = {};
		for (int i = 1; i < split.size(); i++) {
			if (mark == 1)
				pFen += ' ' + split[i];
			if (mark == 2)
				pMoves.push_back(split[i]);
			if (split[i] == "fen")
				mark = 1;
			else if (split[i] == "moves")
				mark = 2;
		}
		pFen = trim(pFen);
		if (!pFen.empty())
			SetFen(pos, pFen == "" ? defFen : pFen);
		Move moves[256];
		for (string uci : pMoves) {
			const int num_moves = MoveGen(pos, moves, false);
			for (int i = 0; i < num_moves; ++i) {
				if (uci == MoveToUci(moves[i], pos.flipped)) {
					if (PieceTypeOn(pos, moves[i].to) != PT_NB || PieceTypeOn(pos, moves[i].from) == PAWN) {
						hash_history.clear();
					}
					else {
						hash_history.emplace_back(GetHash(pos));
					}
					MakeMove(pos, moves[i]);
					break;
				}
			}
		}
	}
	else if (command == "go") {
		int depth = MAX_DEPTH;
		int nodes = 0;
		int wtime = 0;
		int btime = 0;
		int winc = 0;
		int binc = 0;
		int movetime = 0;
		int movestogo = 32;
		if (UciValue(split, "depth", value))
			depth = stoi(value);
		if (UciValue(split, "nodes", value))
			nodes = stoi(value);
		if (UciValue(split, "wtime", value))
			wtime = stoi(value);
		if (UciValue(split, "btime", value))
			btime = stoi(value);
		if (UciValue(split, "winc", value))
			winc = stoi(value);
		if (UciValue(split, "binc", value))
			binc = stoi(value);
		if (UciValue(split, "movetime", value))
			movetime = stoi(value);
		if (UciValue(split, "movestogo", value))
			movestogo = stoi(value);
		int ct = pos.flipped ? btime : wtime;
		int inc = pos.flipped ? binc : winc;
		int st = min(ct / movestogo + inc, ct / 2);
		info.depthLimit = depth;
		info.nodesLimit = nodes;
		info.timeLimit = movetime ? movetime : st;
		const Move best_move = SearchIteratively(pos, hash_history);
		cout << "bestmove " << MoveToUci(best_move, pos.flipped) << endl << flush;
	}
	else if (command == "bench")
		UciBench();
	else if (command == "print")
		PrintBoard(pos);
	else if (command == "eval")
		UciEval();
	else if (command == "quit")
		UciQuit();
}

//main uci loop
static void UciLoop() {
	string line;
	while (true) {
		getline(cin, line);
		UciCommand(line);
	}
}

int main(const int argc, const char** argv) {
	PrintWelcome();
	InitEval();
	transposition_table.resize(options.hash);
	UciLoop();
}
