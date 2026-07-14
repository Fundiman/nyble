#include "bytecode.h"

namespace nyble {

size_t BytecodeChunk::addNum(double val) {
    numConstants.push_back(val);
    return numConstants.size() - 1;
}

size_t BytecodeChunk::addStr(const std::string& s) {
    strConstants.push_back(s);
    return strConstants.size() - 1;
}

size_t BytecodeChunk::addFunc(BytecodeChunk* f) {
    functions.push_back(f);
    return functions.size() - 1;
}

void BytecodeChunk::emit(OpCode op, int line) {
    code.push_back(static_cast<uint8_t>(op));
    lines.push_back(line);
}

void BytecodeChunk::emitByte(uint8_t b, int line) {
    code.push_back(b);
    lines.push_back(line);
}

void BytecodeChunk::emitShort(uint16_t s, int line) {
    code.push_back(s & 0xFF);
    lines.push_back(line);
    code.push_back((s >> 8) & 0xFF);
    lines.push_back(line);
}

void BytecodeChunk::patch(size_t offset, uint16_t val) {
    code[offset] = val & 0xFF;
    code[offset + 1] = (val >> 8) & 0xFF;
}

size_t BytecodeChunk::count() const { return code.size(); }

}
