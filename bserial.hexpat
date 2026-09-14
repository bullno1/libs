#pragma description bserial

import type.leb128;
import std.io;

using Type;
using SymbolDef;

namespace impl {
  std::mem::Section symtab = std::mem::create_section("symtab");
  u32 num_symbols = 0;

  // Record schemas are interned like symbols. Only the width is needed to
  // parse a reference.
  std::mem::Section schematab = std::mem::create_section("schematab");
  u32 num_schemas = 0;

  fn register_Schema(auto width) {
    u32 section_addr = num_schemas * sizeof(u32);
    u32 width_copy @ section_addr in schematab;
    width_copy = width;
    num_schemas += 1;
  };

  fn schema_width(auto id) {
    u32 section_addr = id * sizeof(u32);
    u32 width @ section_addr in schematab;
    return width;
  };

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
      return std::format("SYMBOL({})", impl::format_SymbolDef(element.value));
    } else if (type == Type::SYM_REF) {
      return std::format("SYMBOL({})", impl::format_SymbolRef(element.value));
    } else if (type == Type::RECORD_DEF) {
      return std::format("RECORD({})", element.value.width);
    } else if (type == Type::RECORD_REF) {
      return std::format("RECORD(#{})", element.value.id);
    } else if (type == Type::ARRAY) {
      return std::format("ARRAY({})", element.value.len);
    } else if (type == Type::TABLE) {
      return std::format("TABLE({})", element.value.len);
    }
  };
}

enum Type: u8 {
  UINT       =  1,
  SINT       =  2,
  F32        =  3,
  F64        =  4,
  BLOB       =  5,
  SYM_DEF    =  6,
  SYM_REF    =  7,
  RECORD_DEF =  9,
  RECORD_REF =  9,
  ARRAY      = 10,
  TABLE      = 11,
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

struct RecordDef {
  UInt width;
  Symbol keys[width];
  impl::register_Schema(width);
  Element values[width];
};

struct RecordRef {
  UInt id;
  Element values[impl::schema_width(id)];
};

struct Row<auto Width> {
  Element values[Width];
};

// The schema is stored once with the first row.
// The remaining rows only contain values.
struct Table {
  UInt len;
  if (len > 0) {
    Type schema_type;
    if (schema_type == Type::RECORD_DEF) {
      UInt width;
      Symbol keys[width];
      impl::register_Schema(width);
      Row<width> rows[len];
    } else if (schema_type == Type::RECORD_REF) {
      UInt id;
      Row<impl::schema_width(id)> rows[len];
    }
  }
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
  } else if (type == Type::RECORD_DEF) {
    RecordDef value [[inline]];
  } else if (type == Type::RECORD_REF) {
    RecordRef value [[inline]];
  } else if (type == Type::ARRAY) {
    Array value [[inline]];
  } else if (type == Type::TABLE) {
    Table value [[inline]];
  }
} [[format("impl::format_Element")]];

Element element @ 0x00;
