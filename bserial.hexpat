#pragma description bserial

import type.leb128;
import std.io;

namespace impl {
  fn format_Symbol(auto sym) {
    return std::format("\"{}\"", sym.content);
  };
}

enum Type: u8 {
  UINT    =  1,
  SINT    =  2,
  F32     =  3,
  F64     =  4,
  BLOB    =  5,
  SYM_DEF =  6,
  SYM_REF =  7,
  ARRAY   =  8,
  TABLE   =  9,
  RECORD  = 10,
};

using Element;

struct Array {
  type::uLEB128 len;
  Element elements[len];
};

struct SymbolDef {  
  type::uLEB128 len;
  char content[len];
}[[format("impl::format_Symbol")]];

struct SymbolRef {
  type::uLEB128 id;
};

struct Symbol {
  Type type;
  if (type == Type::SYM_DEF) {
    SymbolDef value [[inline]];
  } else {
    SymbolRef value [[inline]];
  }
};

struct Record {
  type::uLEB128 width;
  Symbol keys[width];
  Element values[width];
};

struct Blob {
  type::uLEB128 len;
  char content[len];
};

struct Row<auto num_columns> {
  Element cells[num_columns] [[inline]];
};

struct Table {
  type::uLEB128 num_rows;
  type::uLEB128 num_columns;
  Symbol columns[num_columns];
  Row<num_columns> rows[num_rows];
};

struct Element {
  Type type;
  
  if (type == Type::UINT) {
    type::uLEB128 value;
  } else if (type == Type::SINT) {
    le type::sLEB128 value;
  } else if (type == Type::F32) {
    float value;
  } else if (type == Type::F64) {
    double value;
  } else if (type == Type::BLOB) {
    Blob value [[inline]];
  } else if (type == Type::SYM_DEF) {
    SymbolDef value [[inline]];
  } else if (type == Type::SYM_REF) {
    SymbolRef value [[inline]];
  } else if (type == Type::RECORD) {
    Record value [[inline]];
  } else if (type == Type::ARRAY) {
    Array value [[inline]];
  } else if (type == Type::TABLE) {
    Table value [[inline]];
  }
};

Element element @ 0x00;
