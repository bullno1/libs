#pragma description bserial

import type.leb128;
import std.io;

using Type;
using SymbolDef;

namespace impl {
  std::mem::Section symtab = std::mem::create_section("symtab");
  u32 num_symbols = 0;

  fn format_SymbolDef(ref auto sym) {
    return std::format("\"{}\"", sym.content);
  };
  
  fn format_SymbolRef(ref auto sym) {
    u32 section_addr = sym.id * sizeof(u32);
    u32 addr @ section_addr in symtab;
    SymbolDef def @ addr;
    return impl::format_SymbolDef(def);
  };
  
  fn format_Symbol(ref auto sym) {
    if (sym.type == Type::SYM_DEF) {
      return impl::format_SymbolDef(sym.value);
    } else {
      return impl::format_SymbolRef(sym.value);
    }
  };
  
  fn register_SymbolDef(auto addr) {
     u32 section_addr = num_symbols * sizeof(u32);
     u32 addr_copy @ section_addr in symtab;
     addr_copy = addr;
     num_symbols += 1;
  };
  
  fn LEB128_to_SInt(auto leb128) {
    s64 res = s64(leb128);
    if ((res & 1) != 0) {
      return ~(res >> 1);
    } else {
      return res >> 1;
    }
  };
  
  fn transform_SInt(ref auto num) {
    s64 res = impl::LEB128_to_SInt(num.internal);
    return res;
  };
  
  fn format_SInt(ref auto num) {
    s64 res = impl::LEB128_to_SInt(num.internal);
    return std::format("{} ({:#x})", res, res);
  };

  fn LEB128_to_UInt(auto leb128) {
    u64 res = u64(leb128);
    return res;
  };
  
  fn transform_UInt(ref auto num) {
    u64 res = impl::LEB128_to_UInt(num.internal);
    return res;
  };
  
  fn format_UInt(ref auto num) {
    u64 res = impl::LEB128_to_UInt(num.internal);
    return std::format("{} ({:#x})", res, res);
  };
  
  fn format_Element(ref auto element) {
    auto type = element.type;
    if (type == Type::UINT) {
      return std::format("UINT({})", element.value);
    } else if (type == Type::SINT) {
      return std::format("SINT({})", element.value);
    } else if (type == Type::F32) {
      return std::format("F32({:G})", element.value);
    } else if (type == Type::F64) {
      return std::format("F64({:G})", element.value);
    } else if (type == Type::BLOB) {
      return std::format("BLOB({})", element.value.len);
    } else if (type == Type::SYM_DEF) {
      return std::format("SYMBOL({})", format_SymbolDef(element.value));
    } else if (type == Type::SYM_REF) {
      return std::format("SYMBOL({})", format_SymbolRef(element.value));
    } else if (type == Type::RECORD) {
      return std::format("RECORD({})", element.value.width);
    } else if (type == Type::ARRAY) {
      return std::format("ARRAY({})", element.value.len);
    } else if (type == Type::TABLE) {
      return std::format("TABLE({}, {})", element.value.num_columns, element.value.num_rows);
    }
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

struct IntBase {
  type::uLEB128 internal;
} [[sealed]];

using SInt = IntBase [[format("impl::format_SInt"), transform("impl::transform_SInt")]];
using UInt = IntBase [[format("impl::format_UInt"), transform("impl::transform_UInt")]];

union BlobContent<auto len> {
  char text[len];
  u8 binary[len];
};

struct Blob {
  UInt len;
  BlobContent<len> content [[inline]];
};

struct SymbolDef {
  UInt len;
  char content[len];
} [[format("impl::format_SymbolDef")]];

struct SymbolRef {
  UInt id;
} [[format("impl::format_SymbolRef")]];

struct Symbol {
  Type type;
  if (type == Type::SYM_DEF) {
    impl::register_SymbolDef($);
    SymbolDef value [[inline]];
  } else {
    SymbolRef value [[inline]];
  }
} [[format("impl::format_Symbol")]];

using Element;

struct Array {
  UInt len;
  Element elements[len];
};

struct Record {
  UInt width;
  Symbol keys[width];
  Element values[width];
};

struct Row<auto num_columns> {
  Element cells[num_columns] [[inline]];
};

struct Table {
  UInt num_rows;
  UInt num_columns;
  Symbol columns[num_columns];
  Row<num_columns> rows[num_rows];
};

struct Element {
  Type type;
  
  if (type == Type::UINT) {
    UInt value;
  } else if (type == Type::SINT) {
    SInt value;
  } else if (type == Type::F32) {
    float value;
  } else if (type == Type::F64) {
    double value;
  } else if (type == Type::BLOB) {
    Blob value [[inline]];
  } else if (type == Type::SYM_DEF) {
    impl::register_SymbolDef($);
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
} [[format("impl::format_Element")]];

Element element @ 0x00;