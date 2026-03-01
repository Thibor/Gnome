#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

#define INF 32001
#define MATE 32000
#define MAX_PLY 64
#define S64 signed __int64
#define U64 unsigned __int64
#define NAME "Gnome"
#define VERSION "2026-02-08"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
// decode move's source square
#define get_move_from(move) (move & 0x7f)
// decode move's target square
#define get_move_to(move) ((move >> 7) & 0x7f)
// decode move's promoted piece
#define get_move_promo(move) ((move >> 14) & 0xf)
// decode move's capture flag
#define get_move_capture(move) ((move >> 18) & 0x1)
// decode move's double pawn push flag
#define get_move_pawn(move) ((move >> 19) & 0x1)
// decode move's enpassant flag
#define get_move_enpassant(move) ((move >> 20) & 0x1)
// decode move's castling flag
#define get_move_castling(move) ((move >> 21) & 0x1)

// piece encoding
enum pieces { e, P, N, B, R, Q, K, p, n, b, r, q, k, o };

// square encoding
enum squares {
	a8 = 0, b8, c8, d8, e8, f8, g8, h8,
	a7 = 16, b7, c7, d7, e7, f7, g7, h7,
	a6 = 32, b6, c6, d6, e6, f6, g6, h6,
	a5 = 48, b5, c5, d5, e5, f5, g5, h5,
	a4 = 64, b4, c4, d4, e4, f4, g4, h4,
	a3 = 80, b3, c3, d3, e3, f3, g3, h3,
	a2 = 96, b2, c2, d2, e2, f2, g2, h2,
	a1 = 112, b1, c1, d1, e1, f1, g1, h1, no_sq
};

enum capture_flags { all_moves, only_captures };
enum castling { KC = 1, QC = 2, kc = 4, qc = 8 };

// sides to move
enum sides { white, black };

struct SearchInfo {
	bool post;
	bool stop;
	int depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}info;

struct Position {
	int side;
	int castle;
	int enpassant;
	int king_square[2];
	int board[128];
};

struct SMoves {
	int count = 0;
	int moves[256]{};
	void AddMove(int source, int target, int piece, int capture, int pawn, int enpassant, int castling) {
		moves[count++] = source | (target << 7) | (piece << 14) | (capture << 18) | (pawn << 19) | (enpassant << 20) | (castling << 21);
	}
};

string pieces = " ANBRQKanbrqk";
string promotedPieces = "__nbrq__nbrq_";

const int material_score[13] = { 0,100,320,330,500,900,0,-100,-320,-330,-500,-900,0 };
const int piece_values[13] = { 0, 100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500,600 };

int castling_rights[128] = {
	 7, 15, 15, 15,  3, 15, 15, 11,  o, o, o, o, o, o, o, o,
	15, 15, 15, 15, 15, 15, 15, 15,  o, o, o, o, o, o, o, o,
	15, 15, 15, 15, 15, 15, 15, 15,  o, o, o, o, o, o, o, o,
	15, 15, 15, 15, 15, 15, 15, 15,  o, o, o, o, o, o, o, o,
	15, 15, 15, 15, 15, 15, 15, 15,  o, o, o, o, o, o, o, o,
	15, 15, 15, 15, 15, 15, 15, 15,  o, o, o, o, o, o, o, o,
	15, 15, 15, 15, 15, 15, 15, 15,  o, o, o, o, o, o, o, o,
	13, 15, 15, 15, 12, 15, 15, 14,  o, o, o, o, o, o, o, o
};

// pawn positional score
const int pawn_score[128] =
{
	90,  90,  90,  90,  90,  90,  90,  90,    o, o, o, o, o, o, o, o,
	30,  30,  30,  40,  40,  30,  30,  30,    o, o, o, o, o, o, o, o,
	20,  20,  20,  30,  30,  30,  20,  20,    o, o, o, o, o, o, o, o,
	10,  10,  10,  20,  20,  10,  10,  10,    o, o, o, o, o, o, o, o,
	 5,   5,  10,  20,  20,   5,   5,   5,    o, o, o, o, o, o, o, o,
	 0,   0,   0,   5,   5,   0,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   0, -10, -10,   0,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   0,   0,   0,   0,   0,   0,    o, o, o, o, o, o, o, o
};

// knight positional score
const int knight_score[128] =
{
	-5,   0,   0,   0,   0,   0,   0,  -5,    o, o, o, o, o, o, o, o,
	-5,   0,   0,  10,  10,   0,   0,  -5,    o, o, o, o, o, o, o, o,
	-5,   5,  20,  20,  20,  20,   5,  -5,    o, o, o, o, o, o, o, o,
	-5,  10,  20,  30,  30,  20,  10,  -5,    o, o, o, o, o, o, o, o,
	-5,  10,  20,  30,  30,  20,  10,  -5,    o, o, o, o, o, o, o, o,
	-5,   5,  20,  10,  10,  20,   5,  -5,    o, o, o, o, o, o, o, o,
	-5,   0,   0,   0,   0,   0,   0,  -5,    o, o, o, o, o, o, o, o,
	-5, -10,   0,   0,   0,   0, -10,  -5,    o, o, o, o, o, o, o, o
};

// bishop positional score
const int bishop_score[128] =
{
	 0,   0,   0,   0,   0,   0,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   0,   0,   0,   0,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   0,  10,  10,   0,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,  10,  20,  20,  10,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,  10,  20,  20,  10,   0,   0,    o, o, o, o, o, o, o, o,
	 0,  10,   0,   0,   0,   0,  10,   0,    o, o, o, o, o, o, o, o,
	 0,  30,   0,   0,   0,   0,  30,   0,    o, o, o, o, o, o, o, o,
	 0,   0, -10,   0,   0, -10,   0,   0,    o, o, o, o, o, o, o, o

};

// rook positional score
const int rook_score[128] =
{
	50,  50,  50,  50,  50,  50,  50,  50,    o, o, o, o, o, o, o, o,
	50,  50,  50,  50,  50,  50,  50,  50,    o, o, o, o, o, o, o, o,
	 0,   0,  10,  20,  20,  10,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,  10,  20,  20,  10,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,  10,  20,  20,  10,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,  10,  20,  20,  10,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,  10,  20,  20,  10,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   0,  20,  20,   0,   0,   0,    o, o, o, o, o, o, o, o

};

// king positional score
const int king_score[128] =
{
	 0,   0,   0,   0,   0,   0,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   5,   5,   5,   5,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   5,   5,  10,  10,   5,   5,   0,    o, o, o, o, o, o, o, o,
	 0,   5,  10,  20,  20,  10,   5,   0,    o, o, o, o, o, o, o, o,
	 0,   5,  10,  20,  20,  10,   5,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   5,  10,  10,   5,   0,   0,    o, o, o, o, o, o, o, o,
	 0,   5,   5,  -5,  -5,   0,   5,   0,    o, o, o, o, o, o, o, o,
	 0,   0,   5,   0, -15,   0,  10,   0,    o, o, o, o, o, o, o, o

};

//mirror positional score tables for opposite side
const int mirror_score[128] =
{
	a1, b1, c1, d1, e1, f1, g1, h1,    o, o, o, o, o, o, o, o,
	a2, b2, c2, d2, e2, f2, g2, h2,    o, o, o, o, o, o, o, o,
	a3, b3, c3, d3, e3, f3, g3, h3,    o, o, o, o, o, o, o, o,
	a4, b4, c4, d4, e4, f4, g4, h4,    o, o, o, o, o, o, o, o,
	a5, b5, c5, d5, e5, f5, g5, h5,    o, o, o, o, o, o, o, o,
	a6, b6, c6, d6, e6, f6, g6, h6,    o, o, o, o, o, o, o, o,
	a7, b7, c7, d7, e7, f7, g7, h7,    o, o, o, o, o, o, o, o,
	a8, b8, c8, d8, e8, f8, g8, h8,    o, o, o, o, o, o, o, o
};

int knight_offsets[8] = { 33, 31, 18, 14, -33, -31, -18, -14 };
int bishop_offsets[4] = { 15, 17, -15, -17 };
int rook_offsets[4] = { 16, -16, 1, -1 };
int king_offsets[8] = { 16, -16, 1, -1, 15, 17, -15, -17 };
int hh_table[13][128];
int killer_moves[2][MAX_PLY];
int pv_table[MAX_PLY][MAX_PLY];
int pv_length[MAX_PLY];

static U64 GetTimeMs() {
	return (clock() * 1000) / CLOCKS_PER_SEC;
}

static string SquareToUci(const int sq) {
	string str;
	str += 'a' + (sq % 16);
	str += '1' + (7 - sq / 16);
	return str;
}

// most valuable victim _ less valuable attacker
inline static int MVV_LVA(int attacker, int victim) {
	return piece_values[victim] - piece_values[attacker] / 100;
}

static int CharToPiece(char c) {
	return (int)pieces.find(c);
}

static char PieceToChar(int p) {
	return pieces[p];
}

//reset board
static void ResetBoard(Position* pos) {
	for (int rank = 0; rank < 8; rank++) {
		for (int file = 0; file < 16; file++) {
			int square = rank * 16 + file;
			pos->board[square] = square & 0x88 ? o : e;
		}
	}
	pos->side = white;
	pos->castle = 0;
	pos->enpassant = no_sq;
}

//is square attacked
static inline int IsSquareAttacked(Position* pos, int square, int side) {
	if (!side) {
		if (!((square + 17) & 0x88) && (pos->board[square + 17] == P))
			return 1;
		if (!((square + 15) & 0x88) && (pos->board[square + 15] == P))
			return 1;
	}
	else {
		if (!((square - 17) & 0x88) && (pos->board[square - 17] == p))
			return 1;
		if (!((square - 15) & 0x88) && (pos->board[square - 15] == p))
			return 1;
	}
	for (int index = 0; index < 8; index++) {
		int target_square = square + knight_offsets[index];
		int target_piece = pos->board[target_square];
		if (!(target_square & 0x88)) {
			if (!side ? target_piece == N : target_piece == n)
				return 1;
		}
	}
	for (int index = 0; index < 8; index++) {
		int target_square = square + king_offsets[index];
		int target_piece = pos->board[target_square];
		if (!(target_square & 0x88)) {
			if (!side ? target_piece == K : target_piece == k)
				return 1;
		}
	}
	for (int index = 0; index < 4; index++) {
		int target_square = square + bishop_offsets[index];
		while (!(target_square & 0x88)) {
			int target_piece = pos->board[target_square];
			if (!side ? (target_piece == B || target_piece == Q) : (target_piece == b || target_piece == q))
				return 1;
			if (target_piece)
				break;
			target_square += bishop_offsets[index];
		}
	}
	for (int index = 0; index < 4; index++) {
		int target_square = square + rook_offsets[index];
		while (!(target_square & 0x88)) {
			int target_piece = pos->board[target_square];
			if (!side ? (target_piece == R || target_piece == Q) : (target_piece == r || target_piece == q))
				return 1;
			if (target_piece)
				break;
			target_square += rook_offsets[index];
		}
	}
	return 0;
}

//move generator
static inline void GenerateMoves(Position* pos, SMoves* move_list) {
	move_list->count = 0;
	for (int square = 0; square < 128; square++) {
		if (square & 0x88)
			continue;
		if (!pos->side) {
			if (pos->board[square] == P) {
				int to_square = square - 16;
				if (!(to_square & 0x88) && !pos->board[to_square]) {
					if (square >= a7 && square <= h7) {
						move_list->AddMove(square, to_square, Q, 0, 0, 0, 0);
						move_list->AddMove(square, to_square, R, 0, 0, 0, 0);
						move_list->AddMove(square, to_square, B, 0, 0, 0, 0);
						move_list->AddMove(square, to_square, N, 0, 0, 0, 0);
					}
					else {
						move_list->AddMove(square, to_square, 0, 0, 0, 0, 0);
						if ((square >= a2 && square <= h2) && !pos->board[square - 32])
							move_list->AddMove(square, square - 32, 0, 0, 1, 0, 0);
					}
				}
				for (int index = 0; index < 4; index++) {
					int pawn_offset = bishop_offsets[index];
					if (pawn_offset < 0) {
						int to_square = square + pawn_offset;
						if (!(to_square & 0x88))
						{
							if ((square >= a7 && square <= h7) &&
								(pos->board[to_square] >= 7 && pos->board[to_square] <= 12)) {
								move_list->AddMove(square, to_square, Q, 1, 0, 0, 0);
								move_list->AddMove(square, to_square, R, 1, 0, 0, 0);
								move_list->AddMove(square, to_square, B, 1, 0, 0, 0);
								move_list->AddMove(square, to_square, N, 1, 0, 0, 0);
							}
							else {
								if (pos->board[to_square] >= 7 && pos->board[to_square] <= 12)
									move_list->AddMove(square, to_square, 0, 1, 0, 0, 0);
								if (to_square == pos->enpassant)
									move_list->AddMove(square, to_square, 0, 1, 0, 1, 0);
							}
						}
					}
				}
			}
			if (pos->board[square] == K) {
				if (pos->castle & KC)
					if (!pos->board[f1] && !pos->board[g1])
						if (!IsSquareAttacked(pos, e1, black) && !IsSquareAttacked(pos, f1, black))
							move_list->AddMove(e1, g1, 0, 0, 0, 0, 1);
				if (pos->castle & QC)
					if (!pos->board[d1] && !pos->board[b1] && !pos->board[c1])
						if (!IsSquareAttacked(pos, e1, black) && !IsSquareAttacked(pos, d1, black))
							move_list->AddMove(e1, c1, 0, 0, 0, 0, 1);
			}
		}
		else {
			if (pos->board[square] == p) {
				int to_square = square + 16;
				if (!(to_square & 0x88) && !pos->board[to_square]) {
					if (square >= a2 && square <= h2) {
						move_list->AddMove(square, to_square, q, 0, 0, 0, 0);
						move_list->AddMove(square, to_square, r, 0, 0, 0, 0);
						move_list->AddMove(square, to_square, b, 0, 0, 0, 0);
						move_list->AddMove(square, to_square, n, 0, 0, 0, 0);
					}
					else {
						move_list->AddMove(square, to_square, 0, 0, 0, 0, 0);
						if ((square >= a7 && square <= h7) && !pos->board[square + 32])
							move_list->AddMove(square, square + 32, 0, 0, 1, 0, 0);
					}
				}
				for (int index = 0; index < 4; index++)
				{
					int pawn_offset = bishop_offsets[index];
					if (pawn_offset > 0)
					{
						int to_square = square + pawn_offset;
						if (!(to_square & 0x88)) {
							if ((square >= a2 && square <= h2) &&
								(pos->board[to_square] >= 1 && pos->board[to_square] <= 6)) {
								move_list->AddMove(square, to_square, q, 1, 0, 0, 0);
								move_list->AddMove(square, to_square, r, 1, 0, 0, 0);
								move_list->AddMove(square, to_square, b, 1, 0, 0, 0);
								move_list->AddMove(square, to_square, n, 1, 0, 0, 0);
							}
							else {
								if (pos->board[to_square] >= 1 && pos->board[to_square] <= 6)
									move_list->AddMove(square, to_square, 0, 1, 0, 0, 0);
								if (to_square == pos->enpassant)
									move_list->AddMove(square, to_square, 0, 1, 0, 1, 0);
							}
						}
					}
				}
			}
			if (pos->board[square] == k)
			{
				if (pos->castle & kc)
				{
					if (!pos->board[f8] && !pos->board[g8])
					{
						if (!IsSquareAttacked(pos, e8, white) && !IsSquareAttacked(pos, f8, white))
							move_list->AddMove(e8, g8, 0, 0, 0, 0, 1);
					}
				}
				if (pos->castle & qc)
				{
					if (!pos->board[d8] && !pos->board[b8] && !pos->board[c8])
					{
						if (!IsSquareAttacked(pos, e8, white) && !IsSquareAttacked(pos, d8, white))
							move_list->AddMove(e8, c8, 0, 0, 0, 0, 1);
					}
				}
			}
		}
		if (!pos->side ? pos->board[square] == N : pos->board[square] == n) {
			for (int index = 0; index < 8; index++)
			{
				int to_square = square + knight_offsets[index];
				int piece = pos->board[to_square];
				if (!(to_square & 0x88))
				{
					if (
						!pos->side ?
						(!piece || (piece >= 7 && piece <= 12)) :
						(!piece || (piece >= 1 && piece <= 6))
						)
					{
						if (piece)
							move_list->AddMove(square, to_square, 0, 1, 0, 0, 0);
						else
							move_list->AddMove(square, to_square, 0, 0, 0, 0, 0);
					}
				}
			}
		}
		if (!pos->side ? pos->board[square] == K : pos->board[square] == k) {
			for (int index = 0; index < 8; index++) {
				int to_square = square + king_offsets[index];
				int piece = pos->board[to_square];
				if (!(to_square & 0x88)) {
					if (!pos->side ?
						(!piece || (piece >= 7 && piece <= 12)) :
						(!piece || (piece >= 1 && piece <= 6))) {
						if (piece)
							move_list->AddMove(square, to_square, 0, 1, 0, 0, 0);
						else
							move_list->AddMove(square, to_square, 0, 0, 0, 0, 0);
					}
				}
			}
		}
		if (!pos->side ?
			(pos->board[square] == B) || (pos->board[square] == Q) :
			(pos->board[square] == b) || (pos->board[square] == q)) {
			for (int index = 0; index < 4; index++) {
				int to_square = square + bishop_offsets[index];
				while (!(to_square & 0x88)) {
					int piece = pos->board[to_square];
					if (!pos->side ? (piece >= 1 && piece <= 6) : ((piece >= 7 && piece <= 12)))
						break;
					if (!pos->side ? (piece >= 7 && piece <= 12) : ((piece >= 1 && piece <= 6))) {
						move_list->AddMove(square, to_square, 0, 1, 0, 0, 0);
						break;
					}
					if (!piece)
						move_list->AddMove(square, to_square, 0, 0, 0, 0, 0);
					to_square += bishop_offsets[index];
				}
			}
		}
		if (!pos->side ?
			(pos->board[square] == R) || (pos->board[square] == Q) :
			(pos->board[square] == r) || (pos->board[square] == q)) {
			for (int index = 0; index < 4; index++) {
				int to_square = square + rook_offsets[index];
				while (!(to_square & 0x88)) {
					int piece = pos->board[to_square];
					if (!pos->side ? (piece >= 1 && piece <= 6) : ((piece >= 7 && piece <= 12)))
						break;
					if (!pos->side ? (piece >= 7 && piece <= 12) : ((piece >= 1 && piece <= 6))) {
						move_list->AddMove(square, to_square, 0, 1, 0, 0, 0);
						break;
					}
					if (!piece)
						move_list->AddMove(square, to_square, 0, 0, 0, 0, 0);
					to_square += rook_offsets[index];
				}
			}
		}
	}
}

//make move
static inline int MakeMove(Position* pos, int move, int capture_flag) {
	if (capture_flag == all_moves)
	{
		int sq_from = get_move_from(move);
		int sq_to = get_move_to(move);
		int promoted_piece = get_move_promo(move);
		int enpass = get_move_enpassant(move);
		int double_push = get_move_pawn(move);
		int castling = get_move_castling(move);
		pos->board[sq_to] = pos->board[sq_from];
		pos->board[sq_from] = e;
		if (promoted_piece)
			pos->board[sq_to] = promoted_piece;
		if (enpass)
			!pos->side ? (pos->board[sq_to + 16] = e) : (pos->board[sq_to - 16] = e);
		pos->enpassant = no_sq;
		if (double_push)
			!pos->side ? (pos->enpassant = sq_to + 16) : (pos->enpassant = sq_to - 16);
		if (castling) {
			switch (sq_to) {
			case g1:
				pos->board[f1] = pos->board[h1];
				pos->board[h1] = e;
				break;
			case c1:
				pos->board[d1] = pos->board[a1];
				pos->board[a1] = e;
				break;
			case g8:
				pos->board[f8] = pos->board[h8];
				pos->board[h8] = e;
				break;
			case c8:
				pos->board[d8] = pos->board[a8];
				pos->board[a8] = e;
				break;
			}
		}
		if (pos->board[sq_to] == K || pos->board[sq_to] == k)
			pos->king_square[pos->side] = sq_to;
		pos->castle &= castling_rights[sq_from];
		pos->castle &= castling_rights[sq_to];
		pos->side ^= 1;
		if (IsSquareAttacked(pos, !pos->side ? pos->king_square[pos->side ^ 1] : pos->king_square[pos->side ^ 1], pos->side))
			return 0;
		else
			return 1;
	}
	else if (get_move_capture(move))
		return MakeMove(pos, move, all_moves);
	else
		return 0;
}


//performance test driver
static inline void PerftDriver(Position* pos, int depth) {
	SMoves move_list[1];
	GenerateMoves(pos, move_list);
	for (int move_count = 0; move_count < move_list->count; move_count++)
	{
		Position npos = *pos;
		if (!MakeMove(&npos, move_list->moves[move_count], all_moves))
			continue;
		if (depth)
			PerftDriver(&npos, depth - 1);
		else
			info.nodes++;
	}
}

static int ShrinkNumber(U64 n) {
	if (n < 10000)
		return 0;
	if (n < 10000000)
		return 1;
	if (n < 10000000000)
		return 2;
	return 3;
}

//displays a summary
static void PrintSummary(U64 time, U64 nodes) {
	U64 nps = (nodes * 1000) / max(time, 1ull);
	const char* units[] = { "", "k", "m", "g" };
	int sn = ShrinkNumber(nps);
	U64 p = (int)pow(10, sn * 3);
	printf("-----------------------------\n");
	printf("Time        : %llu\n", time);
	printf("Nodes       : %llu\n", nodes);
	printf("Nps         : %llu (%llu%s/s)\n", nps, nps / p, units[sn]);
	printf("-----------------------------\n");
}

void ResetInfo() {
	info.post = true;
	info.stop = false;
	info.nodes = 0;
	info.depthLimit = MAX_PLY;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = GetTimeMs();
}

//evaluation of the position
static inline int EvalPosition(Position* pos) {
	int score = 0;
	for (int square = 0; square < 128; square++) {
		if (square & 0x88)continue;
		int piece = pos->board[square];
		score += material_score[piece];
		switch (piece) {
		case P:
			score += pawn_score[square];
			if (pos->board[square - 16] == P)
				score -= 64;
			break;
		case N: score += knight_score[square]; break;
		case B: score += bishop_score[square]; break;
		case R: score += rook_score[square]; break;
		case K: score += king_score[square]; break;
		case p:
			score -= pawn_score[mirror_score[square]];
			if (pos->board[square + 16] == p)
				score += 64;
			break;
		case n: score -= knight_score[mirror_score[square]]; break;
		case b: score -= bishop_score[mirror_score[square]]; break;
		case r: score -= rook_score[mirror_score[square]]; break;
		case k: score -= king_score[mirror_score[square]]; break;
		}
	}
	return !pos->side ? score : -score;
}

//score move for move ordering
static inline int EvalMove(Position* pos, int ply, int move) {
	if (pv_table[0][ply] == move)
		return 20000;
	int sou = get_move_from(move);
	int des = get_move_to(move);
	int ps = pos->board[sou];
	int pd = pos->board[des];
	int score = MVV_LVA(ps, pd);
	if (get_move_capture(move))
		score += 10000;
	else {
		if (killer_moves[0][ply] == move)
			score = 9000;
		else if (killer_moves[1][ply] == move)
			score = 8000;
		else
			score = hh_table[ps][des] + 7000;
	}
	return score;
}

static inline void SortMoves(Position* pos, int ply, SMoves* move_list) {
	int move_scores[0xff];
	for (int mf = 0; mf < move_list->count; mf++)
		move_scores[mf] = EvalMove(pos, ply, move_list->moves[mf]);
	for (int mf = 0; mf < move_list->count; mf++) {
		for (int next = mf + 1; next < move_list->count; next++)
			if (move_scores[mf] < move_scores[next]) {
				int temp_score = move_scores[mf];
				move_scores[mf] = move_scores[next];
				move_scores[next] = temp_score;
				int temp_move = move_list->moves[mf];
				move_list->moves[mf] = move_list->moves[next];
				move_list->moves[next] = temp_move;
			}
	}
}

static bool CheckUp() {
	if ((++info.nodes & 0xffff) == 0) {
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit)
			info.stop = true;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = true;
	}
	return info.stop;
}

static string MoveToUCI(int move) {
	string uci_move = "";
	uci_move += SquareToUci(get_move_from(move));
	uci_move += SquareToUci(get_move_to(move));
	char promo = promotedPieces[get_move_promo(move)];
	if (promo != '_')
		uci_move += promo;
	return uci_move;
}

//parse move from UCI
static int UciToMove(Position* pos, string uci) {
	SMoves move_list;
	GenerateMoves(pos, &move_list);
	for (int i = 0; i < move_list.count; i++) {
		int move = move_list.moves[i];
		if (MoveToUCI(move) == uci)
			return move;
	}
	return 0;
}

//quiescence search
static inline int SearchQuiescence(Position* pos, int alpha, int beta, int depth, int ply) {
	if (CheckUp())
		return 0;
	int eval = EvalPosition(pos);
	if (ply >= MAX_PLY)
		return eval;
	if (alpha < eval)
		alpha = eval;
	if (alpha >= beta)
		return beta;
	SMoves move_list[1];
	GenerateMoves(pos, move_list);
	SortMoves(pos, ply, move_list);
	for (int count = 0; count < move_list->count; count++) {
		Position npos = *pos;
		if (!MakeMove(&npos, move_list->moves[count], only_captures))
			continue;
		int score = -SearchQuiescence(&npos, -beta, -alpha, depth - 1, ply + 1);
		if (info.stop)
			return 0;
		if (alpha < score)
			alpha = score;
		if (alpha >= beta)
			return beta;
	}
	return alpha;
}

//negamax search
static inline int SearchAlpha(Position* pos, int alpha, int beta, int depth, int ply) {
	if (ply >= MAX_PLY - 1)
		return EvalPosition(pos);
	pv_length[ply] = ply;
	int legal_moves = 0;
	int in_check = IsSquareAttacked(pos, pos->king_square[pos->side], pos->side ^ 1);
	if (in_check)
		depth++;
	if (depth < 1)
		return SearchQuiescence(pos, alpha, beta, depth, ply);
	if (CheckUp())
		return 0;
	int  mate_value = MATE - ply;
	if (alpha < -mate_value) alpha = -mate_value;
	if (beta > mate_value - 1) beta = mate_value - 1;
	if (alpha >= beta) return alpha;
	SMoves move_list[1];
	GenerateMoves(pos, move_list);
	SortMoves(pos, ply, move_list);
	for (int n = 0; n < move_list->count; n++) {
		int move = move_list->moves[n];
		Position npos = *pos;
		if (!MakeMove(&npos, move, all_moves))
			continue;
		legal_moves++;
		int score = -SearchAlpha(&npos, -beta, -alpha, depth - 1, ply + 1);
		if (info.stop)
			return 0;
		if (alpha < score) {
			alpha = score;
			pv_table[ply][ply] = move;
			for (int j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv_table[ply][j] = pv_table[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];
			if (!ply && info.post) {
				U64 elapsed = GetTimeMs() - info.timeStart;
				cout << "info depth " << depth << " score ";
				if (abs(score) < MATE - MAX_PLY)
					cout << "cp " << score;
				else
					cout << "mate " << (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1);
				cout << " time " << elapsed << " nodes " << info.nodes << " pv";
				for (int i = 0; i < pv_length[0]; i++)
					cout << " " << MoveToUCI(pv_table[0][i]);
				cout << endl;
			}
			if (alpha >= beta) {
				if (get_move_capture(move) == 0) {
					hh_table[pos->board[get_move_from(move)]][get_move_to(move)] += depth;
					killer_moves[1][ply] = killer_moves[0][ply];
					killer_moves[0][ply] = move;
				}
				return beta;
			}

		}
	}
	if (!legal_moves)
		return in_check ? ply - MATE : 0;
	return alpha;
}

static void UciNewGame() {
	memset(killer_moves, 0, sizeof(killer_moves));
	memset(hh_table, 0, sizeof(hh_table));
	memset(pv_length, 0, sizeof(pv_length));
	memset(pv_table, 0, sizeof(pv_table));
}

//search position
static void SearchIterate(Position* pos) {
	UciNewGame();
	int score;
	for (int depth = 1; depth <= info.depthLimit; depth++) {
		score = SearchAlpha(pos, -MATE, MATE, depth, 0);
		if (info.stop)
			break;
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit / 2)
			break;
	}
	if (info.post)
		cout << "bestmove " << MoveToUCI(pv_table[0][0]) << endl;
}

static void SetFen(Position* pos, string fen) {
	ResetBoard(pos);
	stringstream ss(fen);
	string word;
	ss >> word;
	int sq = 0;
	for (char c : word) {
		switch (c) {
		case 'p':pos->board[sq++] = p; break;
		case 'n':pos->board[sq++] = n; break;
		case 'b':pos->board[sq++] = b; break;
		case 'r':pos->board[sq++] = r; break;
		case 'q':pos->board[sq++] = q; break;
		case 'k':pos->king_square[black] = sq; pos->board[sq++] = k; break;
		case 'P':pos->board[sq++] = P; break;
		case 'N':pos->board[sq++] = N; break;
		case 'B':pos->board[sq++] = B; break;
		case 'R':pos->board[sq++] = R; break;
		case 'Q':pos->board[sq++] = Q; break;
		case 'K':pos->king_square[white] = sq; pos->board[sq++] = K; break;
		case '1': sq += 1; break;
		case '2': sq += 2; break;
		case '3': sq += 3; break;
		case '4': sq += 4; break;
		case '5': sq += 5; break;
		case '6': sq += 6; break;
		case '7': sq += 7; break;
		case '8': sq += 8; break;
		case '/': sq += 8; break;
		}
	}
	ss >> word;
	pos->side = (word == "w") ? white : black;
	ss >> word;
	for (char c : word)
		switch (c)
		{
		case 'K':
			pos->castle |= KC;
			break;
		case 'Q':
			pos->castle |= QC;
			break;
		case 'k':
			pos->castle |= kc;
			break;
		case 'q':
			pos->castle |= qc;
			break;
		}
	ss >> word;
	if (word.length() == 2) {
		int file = word[0] - 'a';
		int rank = 7 - (word[1] - '1');
		pos->enpassant = rank * 16 + file;
	}
}

static void PrintBoard(Position* pos) {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	cout << t;
	for (int rank = 0; rank < 8; rank++) {
		cout << s << " " << 8 - rank << " ";
		for (int file = 0; file < 16; file++) {
			int square = rank * 16 + file;
			if (!(square & 0x88))
				printf("| %c ", pieces[pos->board[square]]);
		}
		cout << "| " << 8 - rank << endl;
	}
	cout << s;
	cout << t << endl;
	printf("    Side:     %s\n", (pos->side == white) ? "white" : "black");
	printf("    Castling:  %c%c%c%c\n",
		(pos->castle & KC) ? 'K' : '-',
		(pos->castle & QC) ? 'Q' : '-',
		(pos->castle & kc) ? 'k' : '-',
		(pos->castle & qc) ? 'q' : '-');
	printf("    Enpassant:   %s\n", (pos->enpassant == no_sq) ? "no" : SquareToUci(pos->enpassant).c_str());
	printf("    King square: %s\n\n", SquareToUci(pos->king_square[pos->side]).c_str());
}

static void PrintPerformanceHeader() {
	printf("-----------------------------\n");
	printf("ply      time        nodes\n");
	printf("-----------------------------\n");
}

//start benchmark
static void UciBench(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	info.post = false;
	U64 elapsed = 0;
	while (elapsed < 3000) {
		++info.depthLimit;
		SearchIterate(pos);
		elapsed = GetTimeMs() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

//start performance test
static void UciPerformance(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	info.nodes = 0;
	U64 elapsed = 0;
	while (elapsed < 3000) {
		PerftDriver(pos, info.depthLimit++);
		elapsed = GetTimeMs() - info.timeStart;
		printf("%4d %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

static void ParsePosition(Position* pos, string command) {
	string fen = START_FEN;
	stringstream ss(command);
	string token;
	ss >> token;
	if (token != "position")
		return;
	ss >> token;
	if (token == "startpos")
		ss >> token;
	else if (token == "fen") {
		fen = "";
		while (ss >> token && token != "moves")
			fen += token + " ";
		fen.pop_back();
	}
	SetFen(pos, fen);
	while (ss >> token) {
		int move = UciToMove(pos, token);
		MakeMove(pos, move, all_moves);
	}
}

static void ParseGo(Position* pos, string command) {
	stringstream ss(command);
	string token;
	ss >> token;
	if (token != "go")
		return;
	ResetInfo();
	int wtime = 0;
	int btime = 0;
	int winc = 0;
	int binc = 0;
	int movestogo = 32;
	char* argument = NULL;
	while (ss >> token) {
		if (token == "wtime")
			ss >> wtime;
		else if (token == "btime")
			ss >> btime;
		else if (token == "winc")
			ss >> winc;
		else if (token == "binc")
			ss >> binc;
		else if (token == "movestogo")
			ss >> movestogo;
		else if (token == "movetime")
			ss >> info.timeLimit;
		else if (token == "depth")
			ss >> info.depthLimit;
		else if (token == "nodes")
			ss >> info.nodesLimit;
	}
	int time = pos->side ? btime : wtime;
	int inc = pos->side ? binc : winc;
	if (time)
		info.timeLimit = min(time / movestogo + inc, time / 2);
	SearchIterate(pos);
}

static void UciCommand(Position* pos, string command) {
	if (command == "uci")cout << "id name " << NAME << endl << "uciok" << endl;
	else if (command == "isready")cout << "readyok" << endl;
	else if (command == "ucinewgame")UciNewGame();
	else if (command == "print")PrintBoard(pos);
	else if (command == "perft")UciPerformance(pos);
	else if (command == "bench")UciBench(pos);
	else if (command == "quit")exit(0);
	else if (command.substr(0, 8) == "position")ParsePosition(pos, command);
	else if (command.substr(0, 2) == "go")ParseGo(pos, command);
}

static void UciLoop(Position* pos) {
	string line;
	while (true) {
		getline(cin, line);
		UciCommand(pos, line);
	}
}

int main() {
	cout << NAME << " " << VERSION << endl;
	Position pos;
	SetFen(&pos, START_FEN);
	UciLoop(&pos);
	return 0;
}