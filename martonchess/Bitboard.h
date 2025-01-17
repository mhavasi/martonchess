
#ifndef BITBOARD_H
#define BITBOARD_H

#include <array>
#include <cstdint>

/**
 * Bitboard stores squares as bits in a 64-bit long. We provide methods to
 * convert bit squares to 0x88 squares and vice versa.
 */
class Bitboard {
public:
    uint64_t squares = 0;

    bool operator==(const Bitboard& bitboard) const;
    bool operator!=(const Bitboard& bitboard) const;

    static int numberOfTrailingZeros(uint64_t b);
    static int bitCount(uint64_t b);
    static int next(uint64_t squares);
    static uint64_t remainder(uint64_t);
    int size();
    void add(int square);
    void addFile(int file);
    void remove(int square);

private:
    static const uint64_t DEBRUIJN64 = 0x03F79D71B4CB0A89ULL;
    static const std::array<int, 64> lsbTable;

    static int toX88Square(int square);
    static int toBitSquare(int square);
};


#endif /* BITBOARD_H */

