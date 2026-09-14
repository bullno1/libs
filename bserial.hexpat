#pragma description bserial

import type.leb128;
import std.io;
import std.core;

using Type;
using SymbolDef;

namespace impl {
  std::mem::Section symtab = std::mem::create_section("symtab");
  u32 num_symbols = 0;

  // Record schemas are interned like symbols.
  // schematab holds [width, first key index] per schema and schemakeys holds
  // the address of the SymbolDef of every key so that a reference can be
  // displayed with its key names.
  std::mem::Section schematab = std::mem::create_section("schematab");
  std::mem::Section schemakeys = std::mem::create_section("schemakeys");
  u32 num_schemas = 0;
  u32 num_schema_keys = 0;

  fn symbol_def_addr(ref auto sym) {
    if (sym.type == Type::SYM_DEF) {
      return addressof(sym.value);
    } else {
      u32 section_addr = sym.value.id * sizeof(u32);
      u32 addr @ section_addr in symtab;
      return addr;
    }
  };

  // Returns the id of the new schema
  fn register_Schema(ref auto keys, auto width) {
    u32 id = num_schemas;
    u32 width_copy @ (id * 2 * sizeof(u32)) in schematab;
    width_copy = width;
    u32 first_key_copy @ ((id * 2 + 1) * sizeof(u32)) in schematab;
    first_key_copy = num_schema_keys;

    for (u32 i = 0, i < width, i += 1) {
      u32 addr_copy @ (num_schema_keys * sizeof(u32)) in schemakeys;
      addr_copy = impl::symbol_def_addr(keys[i]);
      num_schema_keys += 1;
    }

    num_schemas += 1;
    return id;
  };

  fn schema_width(auto id) {
    u32 width @ (id * 2 * sizeof(u32)) in schematab;
    return width;
  };

  fn schema_key_addr(auto id, auto index) {
    u32 first_key @ ((id * 2 + 1) * sizeof(u32)) in schematab;
    u32 addr @ ((first_key + index) * sizeof(u32)) in schemakeys;
    return addr;
  };

  fn schema_key_name(auto id, auto index) {
    SymbolDef def @ impl::schema_key_addr(id, index);
    return def.content;
  };

  // "{x, y}"
  fn format_Schema(auto id) {
    str keys = "";
    u32 width = impl::schema_width(id);
    for (u32 i = 0, i < width, i += 1) {
      if (i > 0) {
        keys = std::format("{}, ", keys);
      }
      keys = std::format("{}{}", keys, impl::schema_key_name(id, i));
    }
    return std::format("{{{}}}", keys);
  };

  // "{x: F32(1), y: F32(2)}"
  fn format_Fields(auto id, ref auto fields) {
    str body = "";
    u32 width = impl::schema_width(id);
    for (u32 i = 0, i < width, i += 1) {
      if (i > 0) {
        body = std::format("{}, ", body);
      }
      body = std::format("{}{}: {}", body, impl::schema_key_name(id, i), impl::format_Element(fields[i].value));
    }
    return std::format("{{{}}}", body);
  };

  fn format_Row(ref auto row) {
    return impl::format_Fields(row.schema_id, row.values);
  };

  fn format_Field(ref auto field) {
    return impl::format_Element(field.value);
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
  
  // Nested records are formatted recursively. Each branch returns on its own
  // instead of chaining else-if so that the nesting stays within the
  // evaluation depth limit.
  fn format_Element(ref auto element) {
    auto type = element.type;
    if (type == Type::UINT) {
      return std::format("UINT({})", element.value);
    }
    if (type == Type::SINT) {
      return std::format("SINT({})", element.value);
    }
    if (type == Type::F32) {
      return std::format("F32({:G})", element.value);
    }
    if (type == Type::F64) {
      return std::format("F64({:G})", element.value);
    }
    if (type == Type::BLOB) {
      return std::format("BLOB({})", element.value.len);
    }
    if (type == Type::SYM_DEF) {
      return std::format("SYMBOL({})", impl::format_SymbolDef(element.value));
    }
    if (type == Type::SYM_REF) {
      return std::format("SYMBOL({})", impl::format_SymbolRef(element.value));
    }
    if (type == Type::RECORD_DEF) {
      return std::format("RECORD{}", impl::format_Fields(element.value.schema_id, element.value.values));
    }
    if (type == Type::RECORD_REF) {
      return std::format("RECORD{}", impl::format_Fields(element.value.id, element.value.values));
    }
    if (type == Type::ARRAY) {
      return std::format("ARRAY({})", element.value.len);
    }
    if (type == Type::TABLE) {
      if (element.value.len > 0) {
        return std::format("TABLE({}) {}", element.value.len, impl::format_Schema(element.value.schema_id));
      }
      return "TABLE(0)";
    }
    return "?";
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
  ARRAY      =  8,
  TABLE      =  9,
  RECORD_DEF = 10,
  RECORD_REF = 11,
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

// A value named after its key.
// The key is shown where its symbol was defined so that interned records
// still display their keys.
struct Field<auto SchemaId> {
  SymbolDef key @ impl::schema_key_addr(SchemaId, std::core::array_index());
  Element value;
} [[name(key.content), format("impl::format_Field")]];

struct RecordDef {
  UInt width;
  Symbol keys[width];
  u32 schema_id = impl::register_Schema(keys, width);
  Field<schema_id> values[width];
};

struct RecordRef {
  UInt id;
  Field<id> values[impl::schema_width(id)];
};

struct Row<auto SchemaId> {
  u32 schema_id = SchemaId;
  Field<SchemaId> values[impl::schema_width(SchemaId)];
} [[format("impl::format_Row")]];

// The schema is stored once with the first row.
// The remaining rows only contain values.
struct Table {
  UInt len;
  if (len > 0) {
    Type schema_type;
    if (schema_type == Type::RECORD_DEF) {
      UInt width;
      Symbol header[width];
      u32 schema_id = impl::register_Schema(header, width);
    } else if (schema_type == Type::RECORD_REF) {
      UInt schema_id;
    }
    Row<schema_id> rows[len];
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
