#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace nyble {

enum class OpCode : uint8_t {
    NOP,
    PUSH_NULL, PUSH_UNDEFINED, PUSH_TRUE, PUSH_FALSE,
    PUSH_NUM, PUSH_STRING,
    POP, DUP, DUP2, SWAP,
    LOAD, STORE,
    GET_PROP, SET_PROP,
    GET_INDEX, SET_INDEX,
    NEW_OBJECT, NEW_ARRAY,
    NEGATE, NOT, TYPEOF, BIT_NOT,
    ADD, SUB, MUL, DIV, MOD, POW,
    BIT_AND, BIT_OR, BIT_XOR, SHL, SHR, USHR,
    EQ, NEQ, STRICT_EQ, STRICT_NEQ,
    LT, GT, LTE, GTE,
    JMP, JMP_IF_FALSE, JMP_IF_TRUE, LOOP,
    CALL, RETURN, MAKE_FUNCTION, MAKE_ARROW_FUNCTION,
    SCOPE_ENTER, SCOPE_EXIT,
    INC_PRE, INC_POST, DEC_PRE, DEC_POST,
    THROW, RETHROW, PUSH_TRY, POP_TRY,
    NEW, CALL_METHOD,
    HALT
};

struct BytecodeChunk {
    std::vector<uint8_t> code;
    std::vector<double> numConstants;
    std::vector<std::string> strConstants;
    std::vector<int> lines;
    std::vector<BytecodeChunk*> functions;
    std::vector<std::string> params;

    size_t addNum(double val);
    size_t addStr(const std::string& s);
    size_t addFunc(BytecodeChunk* f);
    void emit(OpCode op, int line);
    void emitByte(uint8_t b, int line);
    void emitShort(uint16_t s, int line);
    void patch(size_t offset, uint16_t val);
    size_t count() const;
};

}
