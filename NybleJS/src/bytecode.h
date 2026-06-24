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
    POP, DUP, SWAP,
    LOAD, STORE,
    GET_PROP, SET_PROP,
    GET_INDEX, SET_INDEX,
    NEW_OBJECT, NEW_ARRAY,
    NEGATE, NOT, TYPEOF,
    ADD, SUB, MUL, DIV, MOD, POW,
    EQ, NEQ, STRICT_EQ, STRICT_NEQ,
    LT, GT, LTE, GTE,
    JMP, JMP_IF_FALSE, JMP_IF_TRUE, LOOP,
    CALL, RETURN, MAKE_FUNCTION,
    SCOPE_ENTER, SCOPE_EXIT,
    INC_PRE, INC_POST, DEC_PRE, DEC_POST,
    HALT
};

struct BytecodeChunk {
    std::vector<uint8_t> code;
    std::vector<double> numConstants;
    std::vector<std::string> strConstants;
    std::vector<int> lines;
    std::vector<BytecodeChunk*> functions;
    std::vector<std::string> params;

    size_t addNum(double val) {
        numConstants.push_back(val);
        return numConstants.size() - 1;
    }

    size_t addStr(const std::string& s) {
        strConstants.push_back(s);
        return strConstants.size() - 1;
    }

    size_t addFunc(BytecodeChunk* f) {
        functions.push_back(f);
        return functions.size() - 1;
    }

    void emit(OpCode op, int line) {
        code.push_back(static_cast<uint8_t>(op));
        lines.push_back(line);
    }

    void emitByte(uint8_t b, int line) {
        code.push_back(b);
        lines.push_back(line);
    }

    void emitShort(uint16_t s, int line) {
        code.push_back(s & 0xFF);
        lines.push_back(line);
        code.push_back((s >> 8) & 0xFF);
        lines.push_back(line);
    }

    void patch(size_t offset, uint16_t val) {
        code[offset] = val & 0xFF;
        code[offset + 1] = (val >> 8) & 0xFF;
    }

    size_t count() const { return code.size(); }
};

}
