#include <iostream>
#include <sstream>
#include <vector>


using namespace std;

#define INF 32001
#define MATE 32000
#define MAX_DEPTH 64
#define S64 signed __int64
#define U64 unsigned __int64
#define NAME "Gnome"
#define VERSION "2025-10-13"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "

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


typedef struct {
	bool post;
	bool stop;
	int depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}SearchInfo;

SearchInfo info;

// ascii pieces
string pieces = " ANBRQKanbrqk";
string promotedPieces = "__nbrq__nbrq_";

int material_score[13] = {
	  0,      // empty square score
	100,      // white pawn score
	310,      // white knight score
	320,      // white bishop score
	500,      // white rook score
   900,      // white queen score
  10000,      // white king score
   -100,      // black pawn score
   -310,      // black knight score
   -320,      // black bishop score
   -500,      // black rook score
  -900,      // black queen score
 -10000,      // black king score

};

// castling rights

/*
							 castle   move     in      in
							  right    map     binary  decimal

		white king  moved:     1111 & 1100  =  1100    12
  white king's rook moved:     1111 & 1110  =  1110    14
 white queen's rook moved:     1111 & 1101  =  1101    13

		 black king moved:     1111 & 0011  =  1011    3
  black king's rook moved:     1111 & 1011  =  1011    11
 black queen's rook moved:     1111 & 0111  =  0111    7

*/

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

// mirror positional score tables for opposite side
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

// chess board representation
int board[128] = {
	r, n, b, q, k, b, n, r,  o, o, o, o, o, o, o, o,
	p, p, p, p, p, p, p, p,  o, o, o, o, o, o, o, o,
	e, e, e, e, e, e, e, e,  o, o, o, o, o, o, o, o,
	e, e, e, e, e, e, e, e,  o, o, o, o, o, o, o, o,
	e, e, e, e, e, e, e, e,  o, o, o, o, o, o, o, o,
	e, e, e, e, e, e, e, e,  o, o, o, o, o, o, o, o,
	P, P, P, P, P, P, P, P,  o, o, o, o, o, o, o, o,
	R, N, B, Q, K, B, N, R,  o, o, o, o, o, o, o, o
};

// side to move
int side = white;

// enpassant square
int enpassant = no_sq;

// castling rights (dec 15 => bin 1111 => both kings can castle to both sides)
int castle = 15;

// kings' squares
int king_square[2] = { e1, e8 };

/*
	Move formatting

	0000 0000 0000 0000 0111 1111       source square
	0000 0000 0011 1111 1000 0000       target square
	0000 0011 1100 0000 0000 0000       promoted piece
	0000 0100 0000 0000 0000 0000       capture flag
	0000 1000 0000 0000 0000 0000       double pawn flag
	0001 0000 0000 0000 0000 0000       enpassant flag
	0010 0000 0000 0000 0000 0000       castling

*/

// encode move
#define encode_move(source, target, piece, capture, pawn, enpassant, castling) \
(                          \
    (source) |             \
    (target << 7) |        \
    (piece << 14) |        \
    (capture << 18) |      \
    (pawn << 19) |         \
    (enpassant << 20) |    \
    (castling << 21)       \
)

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

// convert board square indexes to coordinates
char* square_to_coords[] = {
	"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", "i8", "j8", "k8", "l8", "m8", "n8", "o8", "p8",
	"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "i7", "j7", "k7", "l7", "m7", "n7", "o7", "p7",
	"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "i6", "j6", "k6", "l6", "m6", "n6", "o6", "p6",
	"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "i5", "j5", "k5", "l5", "m5", "n5", "o5", "p5",
	"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "i4", "j4", "k4", "l4", "m4", "n4", "o4", "p4",
	"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "i3", "j3", "k3", "l3", "m3", "n3", "o3", "p3",
	"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "i2", "j2", "k2", "l2", "m2", "n2", "o2", "p2",
	"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "i1", "j1", "k1", "l1", "m1", "n1", "o1", "p1"
};

// piece move offsets
int knight_offsets[8] = { 33, 31, 18, 14, -33, -31, -18, -14 };
int bishop_offsets[4] = { 15, 17, -15, -17 };
int rook_offsets[4] = { 16, -16, 1, -1 };
int king_offsets[8] = { 16, -16, 1, -1, 15, 17, -15, -17 };

// move list structure
typedef struct {
	int count = 0;
	int moves[256];
	void AddMove(int move) {
		moves[count++] = move;
	}
} moves;

static int CharToPiece(char c) {
	return (int)pieces.find(c);
}

static char PieceToChar(int p) {
	return pieces[p];
}

static void PrintBoard() {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	cout << t;
	for (int rank = 0; rank < 8; rank++) {
		cout << s << " " << 8 - rank << " ";
		for (int file = 0; file < 16; file++) {
			int square = rank * 16 + file;
			if (!(square & 0x88))
				printf("| %c ", pieces[board[square]]);
		}
		cout << "| " << 8 - rank << endl;
	}
	cout << s;
	cout << t << endl;

	// print board stats
	printf("    Side:     %s\n", (side == white) ? "white" : "black");
	printf("    Castling:  %c%c%c%c\n", (castle & KC) ? 'K' : '-',
		(castle & QC) ? 'Q' : '-',
		(castle & kc) ? 'k' : '-',
		(castle & qc) ? 'q' : '-');
	printf("    Enpassant:   %s\n", (enpassant == no_sq) ? "no" : square_to_coords[enpassant]);
	printf("    King square: %s\n\n", square_to_coords[king_square[side]]);
}

// reset board
static void ResetBoard() {
	for (int rank = 0; rank < 8; rank++) {
		for (int file = 0; file < 16; file++) {
			int square = rank * 16 + file;
			if (!(square & 0x88))
				board[square] = e;
		}
	}
	side = white;
	castle = 0;
	enpassant = no_sq;
}

// is square attacked
static inline int is_square_attacked(int square, int side)
{
	// pawn attacks
	if (!side)
	{
		// if target square is on board and is white pawn
		if (!((square + 17) & 0x88) && (board[square + 17] == P))
			return 1;

		// if target square is on board and is white pawn
		if (!((square + 15) & 0x88) && (board[square + 15] == P))
			return 1;
	}

	else
	{
		// if target square is on board and is black pawn
		if (!((square - 17) & 0x88) && (board[square - 17] == p))
			return 1;

		// if target square is on board and is black pawn
		if (!((square - 15) & 0x88) && (board[square - 15] == p))
			return 1;
	}

	// knight attacks
	for (int index = 0; index < 8; index++)
	{
		// init target square
		int target_square = square + knight_offsets[index];

		// lookup target piece
		int target_piece = board[target_square];

		// if target square is on board
		if (!(target_square & 0x88))
		{
			if (!side ? target_piece == N : target_piece == n)
				return 1;
		}
	}

	// king attacks
	for (int index = 0; index < 8; index++)
	{
		// init target square
		int target_square = square + king_offsets[index];

		// lookup target piece
		int target_piece = board[target_square];

		// if target square is on board
		if (!(target_square & 0x88))
		{
			// if target piece is either white or black king
			if (!side ? target_piece == K : target_piece == k)
				return 1;
		}
	}

	// bishop & queen attacks
	for (int index = 0; index < 4; index++)
	{
		// init target square
		int target_square = square + bishop_offsets[index];

		// loop over attack ray
		while (!(target_square & 0x88))
		{
			// target piece
			int target_piece = board[target_square];

			// if target piece is either white or black bishop or queen
			if (!side ? (target_piece == B || target_piece == Q) : (target_piece == b || target_piece == q))
				return 1;

			// break if hit a piece
			if (target_piece)
				break;

			// increment target square by move offset
			target_square += bishop_offsets[index];
		}
	}

	// rook & queen attacks
	for (int index = 0; index < 4; index++)
	{
		// init target square
		int target_square = square + rook_offsets[index];

		// loop over attack ray
		while (!(target_square & 0x88))
		{
			// target piece
			int target_piece = board[target_square];

			// if target piece is either white or black bishop or queen
			if (!side ? (target_piece == R || target_piece == Q) : (target_piece == r || target_piece == q))
				return 1;

			// break if hit a piece
			if (target_piece)
				break;

			// increment target square by move offset
			target_square += rook_offsets[index];
		}
	}

	return 0;
}

// move generator
static inline void generate_moves(moves* move_list)
{
	// reset move count
	move_list->count = 0;

	// loop over all board squares
	for (int square = 0; square < 128; square++)
	{
		// check if the square is on board
		if (!(square & 0x88))
		{
			// white pawn and castling moves
			if (!side)
			{
				// white pawn moves
				if (board[square] == P)
				{
					// init target square
					int to_square = square - 16;

					// quite white pawn moves (check if target square is on board)
					if (!(to_square & 0x88) && !board[to_square])
					{
						// pawn promotions
						if (square >= a7 && square <= h7)
						{
							move_list->AddMove(encode_move(square, to_square, Q, 0, 0, 0, 0));
							move_list->AddMove(encode_move(square, to_square, R, 0, 0, 0, 0));
							move_list->AddMove(encode_move(square, to_square, B, 0, 0, 0, 0));
							move_list->AddMove(encode_move(square, to_square, N, 0, 0, 0, 0));
						}

						else
						{
							// one square ahead pawn move
							move_list->AddMove(encode_move(square, to_square, 0, 0, 0, 0, 0));

							// two squares ahead pawn move
							if ((square >= a2 && square <= h2) && !board[square - 32])
								move_list->AddMove(encode_move(square, square - 32, 0, 0, 1, 0, 0));
						}
					}

					// white pawn capture moves
					for (int index = 0; index < 4; index++)
					{
						// init pawn offset
						int pawn_offset = bishop_offsets[index];

						// white pawn offsets
						if (pawn_offset < 0)
						{
							// init target square
							int to_square = square + pawn_offset;

							// check if target square is on board
							if (!(to_square & 0x88))
							{
								// capture pawn promotion
								if (
									(square >= a7 && square <= h7) &&
									(board[to_square] >= 7 && board[to_square] <= 12)
									)
								{
									move_list->AddMove(encode_move(square, to_square, Q, 1, 0, 0, 0));
									move_list->AddMove(encode_move(square, to_square, R, 1, 0, 0, 0));
									move_list->AddMove(encode_move(square, to_square, B, 1, 0, 0, 0));
									move_list->AddMove(encode_move(square, to_square, N, 1, 0, 0, 0));
								}

								else
								{
									// casual capture
									if (board[to_square] >= 7 && board[to_square] <= 12)
										move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 0, 0));

									// enpassant capture
									if (to_square == enpassant)
										move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 1, 0));
								}
							}
						}
					}
				}

				// white king castling
				if (board[square] == K)
				{
					// if king side castling is available
					if (castle & KC)
					{
						// make sure there are empty squares between king & rook
						if (!board[f1] && !board[g1])
						{
							// make sure king & next square are not under attack
							if (!is_square_attacked(e1, black) && !is_square_attacked(f1, black))
								move_list->AddMove(encode_move(e1, g1, 0, 0, 0, 0, 1));
						}
					}

					// if queen side castling is available
					if (castle & QC)
					{
						// make sure there are empty squares between king & rook
						if (!board[d1] && !board[b1] && !board[c1])
						{
							// make sure king & next square are not under attack
							if (!is_square_attacked(e1, black) && !is_square_attacked(d1, black))
								move_list->AddMove(encode_move(e1, c1, 0, 0, 0, 0, 1));
						}
					}
				}
			}

			// black pawn and castling moves
			else
			{
				// black pawn moves
				if (board[square] == p)
				{
					// init target square
					int to_square = square + 16;

					// quite black pawn moves (check if target square is on board)
					if (!(to_square & 0x88) && !board[to_square])
					{
						// pawn promotions
						if (square >= a2 && square <= h2)
						{
							move_list->AddMove(encode_move(square, to_square, q, 0, 0, 0, 0));
							move_list->AddMove(encode_move(square, to_square, r, 0, 0, 0, 0));
							move_list->AddMove(encode_move(square, to_square, b, 0, 0, 0, 0));
							move_list->AddMove(encode_move(square, to_square, n, 0, 0, 0, 0));
						}

						else
						{
							// one square ahead pawn move
							move_list->AddMove(encode_move(square, to_square, 0, 0, 0, 0, 0));

							// two squares ahead pawn move
							if ((square >= a7 && square <= h7) && !board[square + 32])
								move_list->AddMove(encode_move(square, square + 32, 0, 0, 1, 0, 0));
						}
					}

					// black pawn capture moves
					for (int index = 0; index < 4; index++)
					{
						// init pawn offset
						int pawn_offset = bishop_offsets[index];

						// white pawn offsets
						if (pawn_offset > 0)
						{
							// init target square
							int to_square = square + pawn_offset;

							// check if target square is on board
							if (!(to_square & 0x88))
							{
								// capture pawn promotion
								if (
									(square >= a2 && square <= h2) &&
									(board[to_square] >= 1 && board[to_square] <= 6)
									)
								{
									move_list->AddMove(encode_move(square, to_square, q, 1, 0, 0, 0));
									move_list->AddMove(encode_move(square, to_square, r, 1, 0, 0, 0));
									move_list->AddMove(encode_move(square, to_square, b, 1, 0, 0, 0));
									move_list->AddMove(encode_move(square, to_square, n, 1, 0, 0, 0));
								}

								else
								{
									// casual capture
									if (board[to_square] >= 1 && board[to_square] <= 6)
										move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 0, 0));

									// enpassant capture
									if (to_square == enpassant)
										move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 1, 0));
								}
							}
						}
					}
				}

				// black king castling
				if (board[square] == k)
				{
					// if king side castling is available
					if (castle & kc)
					{
						// make sure there are empty squares between king & rook
						if (!board[f8] && !board[g8])
						{
							// make sure king & next square are not under attack
							if (!is_square_attacked(e8, white) && !is_square_attacked(f8, white))
								move_list->AddMove(encode_move(e8, g8, 0, 0, 0, 0, 1));
						}
					}

					// if queen side castling is available
					if (castle & qc)
					{
						// make sure there are empty squares between king & rook
						if (!board[d8] && !board[b8] && !board[c8])
						{
							// make sure king & next square are not under attack
							if (!is_square_attacked(e8, white) && !is_square_attacked(d8, white))
								move_list->AddMove(encode_move(e8, c8, 0, 0, 0, 0, 1));
						}
					}
				}
			}

			// knight moves
			if (!side ? board[square] == N : board[square] == n)
			{
				// loop over knight move offsets
				for (int index = 0; index < 8; index++)
				{
					// init target square
					int to_square = square + knight_offsets[index];

					// init target piece
					int piece = board[to_square];

					// make sure target square is onboard
					if (!(to_square & 0x88))
					{
						//
						if (
							!side ?
							(!piece || (piece >= 7 && piece <= 12)) :
							(!piece || (piece >= 1 && piece <= 6))
							)
						{
							// on capture
							if (piece)
								move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 0, 0));

							// on empty square
							else
								move_list->AddMove(encode_move(square, to_square, 0, 0, 0, 0, 0));
						}
					}
				}
			}

			// king moves
			if (!side ? board[square] == K : board[square] == k)
			{
				// loop over king move offsets
				for (int index = 0; index < 8; index++)
				{
					// init target square
					int to_square = square + king_offsets[index];

					// init target piece
					int piece = board[to_square];

					// make sure target square is onboard
					if (!(to_square & 0x88))
					{
						//
						if (
							!side ?
							(!piece || (piece >= 7 && piece <= 12)) :
							(!piece || (piece >= 1 && piece <= 6))
							)
						{
							// on capture
							if (piece)
								move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 0, 0));

							// on empty square
							else
								move_list->AddMove(encode_move(square, to_square, 0, 0, 0, 0, 0));
						}
					}
				}
			}

			// bishop & queen moves
			if (
				!side ?
				(board[square] == B) || (board[square] == Q) :
				(board[square] == b) || (board[square] == q)
				)
			{
				// loop over bishop & queen offsets
				for (int index = 0; index < 4; index++)
				{
					// init target square
					int to_square = square + bishop_offsets[index];

					// loop over attack ray
					while (!(to_square & 0x88))
					{
						// init target piece
						int piece = board[to_square];

						// if hits own piece
						if (!side ? (piece >= 1 && piece <= 6) : ((piece >= 7 && piece <= 12)))
							break;

						// if hits opponent's piece
						if (!side ? (piece >= 7 && piece <= 12) : ((piece >= 1 && piece <= 6)))
						{
							move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 0, 0));
							break;
						}

						// if steps into an empty squre
						if (!piece)
							move_list->AddMove(encode_move(square, to_square, 0, 0, 0, 0, 0));

						// increment target square
						to_square += bishop_offsets[index];
					}
				}
			}

			// rook & queen moves
			if (
				!side ?
				(board[square] == R) || (board[square] == Q) :
				(board[square] == r) || (board[square] == q)
				)
			{
				// loop over bishop & queen offsets
				for (int index = 0; index < 4; index++)
				{
					// init target square
					int to_square = square + rook_offsets[index];

					// loop over attack ray
					while (!(to_square & 0x88))
					{
						// init target piece
						int piece = board[to_square];

						// if hits own piece
						if (!side ? (piece >= 1 && piece <= 6) : ((piece >= 7 && piece <= 12)))
							break;

						// if hits opponent's piece
						if (!side ? (piece >= 7 && piece <= 12) : ((piece >= 1 && piece <= 6)))
						{
							move_list->AddMove(encode_move(square, to_square, 0, 1, 0, 0, 0));
							break;
						}

						// if steps into an empty squre
						if (!piece)
							move_list->AddMove(encode_move(square, to_square, 0, 0, 0, 0, 0));

						// increment target square
						to_square += rook_offsets[index];
					}
				}
			}
		}
	}
}

static void PrintMoves() {
	moves move_list[1];
	generate_moves(move_list);
	printf("\n    Move     Capture  Double   Enpass   Castling\n\n");

	// loop over moves in a movelist
	for (int index = 0; index < move_list->count; index++)
	{
		int move = move_list->moves[index];
		printf("    %s%s", square_to_coords[get_move_from(move)], square_to_coords[get_move_to(move)]);
		printf("%c    ", get_move_promo(move) ? promotedPieces[get_move_promo(move)] : ' ');
		printf("%d        %d        %d        %d\n", get_move_capture(move), get_move_pawn(move), get_move_enpassant(move), get_move_castling(move));
	}

	printf("\n    Total moves: %d\n\n", move_list->count);
}

// copy/restore board position macros
#define copy_board()                                \
    int board_copy[128], king_square_copy[2];       \
    int side_copy, enpassant_copy, castle_copy;     \
    memcpy(board_copy, board, 512);                 \
    side_copy = side;                               \
    enpassant_copy = enpassant;                     \
    castle_copy = castle;                           \
    memcpy(king_square_copy, king_square,8);        \

#define take_back()                                 \
    memcpy(board, board_copy, 512);                 \
    side = side_copy;                               \
    enpassant = enpassant_copy;                     \
    castle = castle_copy;                           \
    memcpy(king_square, king_square_copy,8);        \

// make move
static inline int MakeMove(int move, int capture_flag) {
	if (capture_flag == all_moves)
	{
		// copy board state
		copy_board();

		// parse move
		int sq_from = get_move_from(move);
		int sq_to = get_move_to(move);
		int promoted_piece = get_move_promo(move);
		int enpass = get_move_enpassant(move);
		int double_push = get_move_pawn(move);
		int castling = get_move_castling(move);

		// move piece
		board[sq_to] = board[sq_from];
		board[sq_from] = e;

		// pawn promotion
		if (promoted_piece)
			board[sq_to] = promoted_piece;

		// enpassant capture
		if (enpass)
			!side ? (board[sq_to + 16] = e) : (board[sq_to - 16] = e);

		// reset enpassant square
		enpassant = no_sq;

		// double pawn push
		if (double_push)
			!side ? (enpassant = sq_to + 16) : (enpassant = sq_to - 16);

		// castling
		if (castling)
		{
			// switch target square
			switch (sq_to) {
				// white castles king side
			case g1:
				board[f1] = board[h1];
				board[h1] = e;
				break;

				// white castles queen side
			case c1:
				board[d1] = board[a1];
				board[a1] = e;
				break;

				// black castles king side
			case g8:
				board[f8] = board[h8];
				board[h8] = e;
				break;

				// black castles queen side
			case c8:
				board[d8] = board[a8];
				board[a8] = e;
				break;
			}
		}

		// update king square
		if (board[sq_to] == K || board[sq_to] == k)
			king_square[side] = sq_to;

		// update castling rights
		castle &= castling_rights[sq_from];
		castle &= castling_rights[sq_to];

		// change side
		side ^= 1;

		// take move back if king is under the check
		if (is_square_attacked(!side ? king_square[side ^ 1] : king_square[side ^ 1], side))
		{
			// restore board state
			take_back();

			// illegal move
			return 0;
		}

		else
			// legal move
			return 1;
	}
	else
	{
		// if move is a capture
		if (get_move_capture(move))
			// make capture move
			MakeMove(move, all_moves);

		else
			// move is not a capture
			return 0;
	}
}


//perft driver
static inline void PerftDriver(int depth) {
	moves move_list[1];
	generate_moves(move_list);
	for (int move_count = 0; move_count < move_list->count; move_count++)
	{
		copy_board();
		if (!MakeMove(move_list->moves[move_count], all_moves))
			continue;
		if (depth)
			PerftDriver(depth - 1);
		else
			info.nodes++;
		take_back();
	}
}

//put thousands separators in the given integer
string ThousandSeparator(uint64_t n) {
	string ans = "";
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
	if (ans.size() % 4 == 0)
		ans.erase(ans.begin());
	return ans;
}

void PrintSummary(U64 time, U64 nodes) {
	if (time < 1)
		time = 1;
	U64 nps = (nodes * 1000) / time;
	printf("-----------------------------\n");
	cout << "Time        : " << ThousandSeparator(time) << endl;
	cout << "Nodes       : " << ThousandSeparator(nodes) << endl;
	cout << "Nps         : " << ThousandSeparator(nps) << endl;
	printf("-----------------------------\n");
}

void ResetInfo() {
	info.post = true;
	info.stop = false;
	info.nodes = 0;
	info.depthLimit = 64;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = clock();
}

// evaluation of the position
static inline int evaluate_position() {
	int score = 0;

	// loop over board squares
	for (int square = 0; square < 128; square++)
	{
		// make sure square is on board
		if (!(square & 0x88))
		{
			// init piece
			int piece = board[square];

			// material score evaluation
			score += material_score[piece];

			// pieces evaluation
			switch (piece)
			{
				// white pieces
			case P:
				// positional score
				score += pawn_score[square];

				// double panws penalty
				if (board[square - 16] == P)
					score -= 64;

				break;

			case N: score += knight_score[square]; break;
			case B: score += bishop_score[square]; break;
			case R: score += rook_score[square]; break;
			case K: score += king_score[square]; break;

				// black pieces
			case p:
				// positional score
				score -= pawn_score[mirror_score[square]];

				// double pawns penalty
				if (board[square + 16] == p)
					score += 64;

				break;
			case n: score -= knight_score[mirror_score[square]]; break;
			case b: score -= bishop_score[mirror_score[square]]; break;
			case r: score -= rook_score[mirror_score[square]]; break;
			case k: score -= king_score[mirror_score[square]]; break;
			}

		}
	}

	// return positive score for white & negative for black
	return !side ? score : -score;
}


/***********************************************\

				 SEARCH FUNCTIONS

\***********************************************/

// most valuable victim & less valuable attacker

/*

	(Victims) Pawn Knight Bishop   Rook  Queen   King
  (Attackers)
		Pawn   105    205    305    405    505    605
	  Knight   104    204    304    404    504    604
	  Bishop   103    203    303    403    503    603
		Rook   102    202    302    402    502    602
	   Queen   101    201    301    401    501    601
		King   100    200    300    400    500    600

*/

static int mvv_lva[13][13] = {
	0,   0,   0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,
	0, 105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	0, 104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	0, 103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	0, 102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	0, 101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	0, 100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600,

	0, 105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	0, 104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	0, 103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	0, 102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	0, 101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	0, 100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600
};

// killer moves [id][ply]
int killer_moves[2][64];

// history moves [piece][square]
int history_moves[13][128];

// PV moves
int pv_table[64][64];
int pv_length[64];

// score move for move ordering
static inline int score_move(int ply, int move) {
	if (pv_table[0][ply] == move)
		return 20000;
	int score;
	score = mvv_lva[board[get_move_from(move)]][board[get_move_to(move)]];

	// on capture
	if (get_move_capture(move))
	{
		// add 10000 to current score
		score += 10000;
	}

	// on quiete move
	else {
		// on 1st killer move
		if (killer_moves[0][ply] == move)
			// score 9000
			score = 9000;

		// on 2nd killer move
		else if (killer_moves[1][ply] == move)
			// score 8000
			score = 8000;

		// on history move (previous alpha's best score)
		else
			// score with history depth
			score = history_moves[board[get_move_from(move)]][get_move_to(move)] + 7000;
	}
	return score;
}

static inline void sort_moves(int ply, moves* move_list) {
	int move_scores[0xff];
	for (int mf = 0; mf < move_list->count; mf++)
		move_scores[mf] = score_move(ply, move_list->moves[mf]);
	for (int mf = 0; mf < move_list->count; mf++) {
		for (int next = mf + 1; next < move_list->count; next++)
		{
			// order moves descending
			if (move_scores[mf] < move_scores[next])
			{
				// swap scores
				int temp_score = move_scores[mf];
				move_scores[mf] = move_scores[next];
				move_scores[next] = temp_score;

				// swap corresponding moves
				int temp_move = move_list->moves[mf];
				move_list->moves[mf] = move_list->moves[next];
				move_list->moves[next] = temp_move;
			}
		}
	}
}

static string MoveToUCI(int move) {
	string uci_move = "";
	uci_move += square_to_coords[get_move_from(move)];
	uci_move += square_to_coords[get_move_to(move)];
	char promo = promotedPieces[get_move_promo(move)];
	if (promo != '_')
		uci_move += promo;
	return uci_move;
}

static void CheckUp() {
	if ((info.timeLimit && clock() - info.timeStart > info.timeLimit) || (info.nodesLimit && info.nodes > info.nodesLimit))
		info.stop = 1;
}

//quiescence search
static inline int SearchQuiescence(int alpha, int beta, int depth, int ply) {
	if (!(++info.nodes & 0xffff))
		CheckUp();
	if (info.stop)
		return 0;
	int eval = evaluate_position();
	if (eval >= beta)
		return beta;
	if (eval > alpha)
		alpha = eval;
	moves move_list[1];
	generate_moves(move_list);
	sort_moves(ply, move_list);
	for (int count = 0; count < move_list->count; count++)
	{
		// copy board state
		copy_board();
		if (!MakeMove(move_list->moves[count], only_captures))
			continue;

		// recursive call
		int score = -SearchQuiescence(-beta, -alpha, depth - 1, ply + 1);

		// restore board state
		take_back();
		if (info.stop)
			return 0;

		if (score >= beta)
			return beta;

		// alpha acts like max in MiniMax
		if (score > alpha)
			alpha = score;
	}
	return alpha;
}

//negamax search
static inline int SearchAlpha(int alpha, int beta, int depth, int ply) {
	int legal_moves = 0;
	pv_length[ply] = ply;
	int in_check = is_square_attacked(king_square[side], side ^ 1);
	if (in_check)
		depth++;
	if (depth <= 0)
		return SearchQuiescence(alpha, beta, depth, ply);
	if (!(++info.nodes & 0xffff))
		CheckUp();
	if (info.stop)
		return 0;
	moves move_list[1];
	generate_moves(move_list);
	sort_moves(ply, move_list);
	for (int count = 0; count < move_list->count; count++) {
		copy_board();
		if (!MakeMove(move_list->moves[count], all_moves))
			continue;
		legal_moves++;
		int score = -SearchAlpha(-beta, -alpha, depth - 1, ply + 1);
		take_back();
		if (info.stop)
			return 0;
		if (score >= beta)
		{
			// update killer moves
			killer_moves[1][ply] = killer_moves[0][ply];
			killer_moves[0][ply] = move_list->moves[count];

			return beta;
		}

		// alpha acts like max in MiniMax
		if (score > alpha)
		{
			// update history score
			history_moves[board[get_move_from(move_list->moves[count])]][get_move_to(move_list->moves[count])] += depth;

			// set alpha score
			alpha = score;
			pv_table[ply][ply] = move_list->moves[count];

			for (int i = ply + 1; i < pv_length[ply + 1]; i++)
				pv_table[ply][i] = pv_table[ply + 1][i];
			pv_length[ply] = pv_length[ply + 1];
			if (!ply && info.post) {
				clock_t elapsed = clock() - info.timeStart;
				cout << "info depth " << depth << "score ";
				if (abs(score) < MATE - MAX_DEPTH)
					cout << "cp " << score;
				else
					cout << "mate " << (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1);
				cout << " time " << elapsed << " nodes " << info.nodes << " pv ";
				for (int i = 0; i < pv_length[0]; i++)
					cout << MoveToUCI(pv_table[0][i]) << " ";
				cout << endl;
			}
		}
	}
	if (!legal_moves)
	{
		if (in_check)
			return -MATE + ply;
		else
			return 0;
	}
	return alpha;
}

//search position
static void SearchIterate() {
	memset(pv_table, 0, sizeof(pv_table));
	memset(killer_moves, 0, sizeof(killer_moves));
	memset(history_moves, 0, sizeof(history_moves));
	int score;
	for (int depth = 1; depth <= info.depthLimit; depth++) {
		score = SearchAlpha(-MATE, MATE, depth, 0);
		if (info.stop)
			break;
		if (info.timeLimit && clock() - info.timeStart > info.timeLimit / 2)
			break;
	}
	if (info.post)
		cout << "bestmove " << MoveToUCI(pv_table[0][0]) << endl;
}

//parse move (from UCI)
int UciToMove(const char* move_str) {
	moves move_list[1];
	generate_moves(move_list);
	int parse_from = (move_str[0] - 'a') + (8 - (move_str[1] - '0')) * 16;
	int parse_to = (move_str[2] - 'a') + (8 - (move_str[3] - '0')) * 16;
	int prom_piece = 0;
	int move;

	// loop over generated moves
	for (int count = 0; count < move_list->count; count++)
	{
		// pick up move
		move = move_list->moves[count];

		// if input move is present in the move list
		if (get_move_from(move) == parse_from && get_move_to(move) == parse_to)
		{
			// init promoted piece
			prom_piece = get_move_promo(move);

			// if promoted piece is present compare it with promoted piece from user input
			if (prom_piece)
			{
				if ((prom_piece == N || prom_piece == n) && move_str[4] == 'n')
					return move;

				else if ((prom_piece == B || prom_piece == b) && move_str[4] == 'b')
					return move;

				else if ((prom_piece == R || prom_piece == r) && move_str[4] == 'r')
					return move;

				else if ((prom_piece == Q || prom_piece == q) && move_str[4] == 'q')
					return move;

				continue;
			}
			return move;
		}
	}
	return 0;
}

static void SetFen(string fen) {
	ResetBoard();
	stringstream ss(fen);
	string word;
	ss >> word;
	int sq = 0;
	for (char c : word) {
		switch (c) {
		case 'p':board[sq++] = p; break;
		case 'n':board[sq++] = n; break;
		case 'b':board[sq++] = b; break;
		case 'r':board[sq++] = r; break;
		case 'q':board[sq++] = q; break;
		case 'k':king_square[black] = sq; board[sq++] = k; break;
		case 'P':board[sq++] = P; break;
		case 'N':board[sq++] = N; break;
		case 'B':board[sq++] = B; break;
		case 'R':board[sq++] = R; break;
		case 'Q':board[sq++] = Q; break;
		case 'K':king_square[white] = sq; board[sq++] = K; break;
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
	side = (word == "w") ? white : black;
	ss >> word;
	for (char c : word)
		switch (c)
		{
		case 'K':
			castle |= KC;
			break;
		case 'Q':
			castle |= QC;
			break;
		case 'k':
			castle |= kc;
			break;
		case 'q':
			castle |= qc;
			break;
		}
	ss >> word;
	if (word.length() == 2) {
		int file = word[0] - 'a';
		int rank = 7 - (word[1] - '1');
		enpassant = rank * 16 + file;
	}
}

void PrintPerformanceHeader() {
	printf("-----------------------------\n");
	printf("ply      time        nodes\n");
	printf("-----------------------------\n");
}

//start benchmark
static void UciBench() {
	ResetInfo();
	PrintPerformanceHeader();
	SetFen(START_FEN);
	info.depthLimit = 0;
	info.post = false;
	U64 elapsed = 0;
	while (elapsed < 3000)
	{
		++info.depthLimit;
		SearchIterate();
		elapsed = clock() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

// start performance test
static inline void UciPerformance() {
	ResetInfo();
	PrintPerformanceHeader();
	SetFen(START_FEN);
	info.depthLimit = 0;
	info.nodes = 0;
	U64 elapsed = 0;
	while (elapsed < 3000) {
		PerftDriver(info.depthLimit++);
		elapsed = clock() - info.timeStart;
		printf("%4d %8d %12ld\n", info.depthLimit, clock() - info.timeStart, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

static void ParsePosition(string command) {
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
	SetFen(fen);
	while (ss >> token) {
		int move = UciToMove(token.c_str());
		MakeMove(move, all_moves);
	}
}

static void ParseGo(string command) {
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
	int time = side ? btime : wtime;
	int inc = side ? binc : winc;
	if (time)
		info.timeLimit = min(time / movestogo + inc, time / 2);
	SearchIterate();
}

void UciCommand(string command) {
	if (command == "uci")
		cout << "id name " << NAME << endl << "uciok" << endl;
	else if (command == "isready")
		cout << "readyok" << endl;
	else if (command.substr(0, 8) == "position")
		ParsePosition(command);
	else if (command.substr(0, 2) == "go")
		ParseGo(command);
	else if (command == "print")
		PrintBoard();
	else if (command == "perft")
		UciPerformance();
	else if (command == "bench")
		UciBench();
	else if (command == "quit")
		exit(0);
}

static void UciLoop() {
	string line;
	while (true) {
		getline(cin, line);
		UciCommand(line);
	}
}

// main driver
int main() {
	cout << NAME << " " << VERSION << endl;
	SetFen(START_FEN);
	UciLoop();
	return 0;
}