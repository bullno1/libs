#!/usr/bin/env python3
"""Generate bfmt_cldr.h, the plural rules of every language, from CLDR.

The rules come from plurals.json and ordinals.json of cldr-core, fetched from
the cldr-json repository at the given ref unless given as files:

    scripts/bfmt_cldr.py --ref 48.0.0 -o bfmt_cldr.h
    scripts/bfmt_cldr.py --plurals plurals.json --ordinals ordinals.json -o bfmt_cldr.h

The template is bfmt_cldr.h.in next to this script with ${RULES}, ${TABLE},
${CLDR_VERSION} and ${CLDR_REF} filled in.

Each rule is compiled into a C expression over bfmt_plural_num_t. The syntax
is the one of UTS #35. The samples after `@` are dropped. The compact decimal
exponent (c, e) is not among bfmt's operands, so it is taken as 0 and folded away.
"""

import argparse
import json
import os
import string
import re
import sys
import urllib.request

RAW_URL = "https://raw.githubusercontent.com/unicode-org/cldr-json/{ref}/cldr-json/cldr-core/supplemental/{name}.json"

CATEGORIES = ["zero", "one", "two", "few", "many"]
OPERANDS = {"n", "i", "v", "w", "f", "t", "c", "e"}

TOKEN = re.compile(r"\s*(?:(\d+)|(\.\.)|(,)|(!=)|(=)|(%)|([a-z]+))")


# Parsing {{{

class Relation:
    def __init__(self, operand, mod, negated, items):
        self.operand = operand
        self.mod = mod
        self.negated = negated
        self.items = items  # list of int or (lo, hi)


def tokenize(text):
    pos = 0
    tokens = []
    text = text.rstrip()
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if m is None:
            raise ValueError(f"cannot tokenize {text!r} at {pos}")
        pos = m.end()
        tokens.append(next(t for t in m.groups() if t is not None))
    return tokens


class Parser:
    def __init__(self, text):
        self.tokens = tokenize(text)
        self.pos = 0

    def peek(self):
        return self.tokens[self.pos] if self.pos < len(self.tokens) else None

    def take(self, expected=None):
        token = self.peek()
        if token is None or (expected is not None and token != expected):
            raise ValueError(f"expected {expected or 'a token'}, got {token!r} in {' '.join(self.tokens)!r}")
        self.pos += 1
        return token

    # condition = and_condition ('or' and_condition)*
    def condition(self):
        terms = [self.and_condition()]
        while self.peek() == "or":
            self.take()
            terms.append(self.and_condition())
        return terms

    # and_condition = relation ('and' relation)*
    def and_condition(self):
        relations = [self.relation()]
        while self.peek() == "and":
            self.take()
            relations.append(self.relation())
        return relations

    # relation = operand ('%' value)? ('=' | '!=') range_list
    def relation(self):
        operand = self.take()
        if operand not in OPERANDS:
            raise ValueError(f"unsupported operand {operand!r}")
        mod = None
        if self.peek() == "%":
            self.take()
            mod = int(self.take())
        op = self.take()
        if op not in ("=", "!="):
            raise ValueError(f"unsupported relation {op!r}")
        items = [self.range()]
        while self.peek() == ",":
            self.take()
            items.append(self.range())
        return Relation(operand, mod, op == "!=", items)

    # range = value ('..' value)?
    def range(self):
        lo = int(self.take())
        if self.peek() == "..":
            self.take()
            return (lo, int(self.take()))
        return lo


def parse_condition(text):
    """The condition of a rule, without its samples, as a list of and-lists"""
    text = text.split("@", 1)[0].strip()
    if text == "":
        return None
    parser = Parser(text)
    condition = parser.condition()
    if parser.peek() is not None:
        raise ValueError(f"trailing tokens in {text!r}")
    return condition

# }}}


# Code generation {{{

def member(expr, items):
    """C expression: `expr` is in `items`"""
    parts = []
    for item in items:
        if isinstance(item, tuple):
            if item[0] == 0:  # operands are unsigned
                parts.append(f"{expr} <= {item[1]}")
            else:
                parts.append(f"({item[0]} <= {expr} && {expr} <= {item[1]})")
        else:
            parts.append(f"{expr} == {item}")
    return "(" + " || ".join(parts) + ")"


def strip_parens(expr):
    """Without the outer parentheses, when they enclose the whole expression"""
    while expr.startswith("(") and expr.endswith(")"):
        depth = 0
        for i, c in enumerate(expr):
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0 and i != len(expr) - 1:
                    return expr
        expr = expr[1:-1]
    return expr


def fold_exponent(relation):
    """A relation on the exponent, evaluated at 0, as a constant"""
    value = 0 if relation.mod is None else 0 % relation.mod
    found = any(
        (item[0] <= value <= item[1]) if isinstance(item, tuple) else item == value
        for item in relation.items
    )
    return found != relation.negated


def compile_relation(relation):
    """A relation as a C expression, or a bool when it is constant"""
    if relation.operand in ("c", "e"):
        return fold_exponent(relation)

    if relation.operand == "n":
        # n is i with the fraction f: it is in a list of integers only when
        # the fraction is 0, and the modulus of a non-integer is never an integer
        expr = "num.i" if relation.mod is None else f"(num.i % {relation.mod})"
        if relation.negated:
            return f"(num.f != 0 || !{member(expr, relation.items)})"
        return f"(num.f == 0 && {member(expr, relation.items)})"

    expr = f"num.{relation.operand}"
    if relation.mod is not None:
        expr = f"({expr} % {relation.mod})"
    test = member(expr, relation.items)
    return f"!{test}" if relation.negated else test


def compile_condition(condition):
    """A condition as a list of C and-expressions, or a bool when constant"""
    terms = []
    for and_list in condition:
        relations = []
        constant = True
        for relation in and_list:
            compiled = compile_relation(relation)
            if compiled is False:
                constant = False
                break
            if compiled is not True:
                relations.append(compiled)
        if not constant:
            continue
        if not relations:
            return True
        terms.append(" && ".join(relations) if len(relations) == 1 else "(" + " && ".join(relations) + ")")
    if not terms:
        return False
    return terms


def rule_text(rules, category):
    return rules.get(f"pluralRule-count-{category}", "").split("@", 1)[0].strip()


def compile_rules(rules, indent):
    """The checks of one plural type as C lines"""
    lines = []
    for category in CATEGORIES:
        key = f"pluralRule-count-{category}"
        if key not in rules:
            continue
        condition = parse_condition(rules[key])
        if condition is None:
            raise ValueError(f"empty condition for {category}")
        compiled = compile_condition(condition)
        if compiled is True:
            raise ValueError(f"{category} folds to true: {rules[key]!r}")
        if compiled is False:
            lines.append(f"{indent}// {category}: {rule_text(rules, category)} (never, without an exponent)")
            continue
        lines.append(f"{indent}// {category}: {rule_text(rules, category)}")
        if len(compiled) == 1:
            lines.append(f"{indent}if ({strip_parens(compiled[0])}) {{")
        else:
            lines.append(f"{indent}if (")
            for i, term in enumerate(compiled):
                sep = "" if i == len(compiled) - 1 else " ||"
                lines.append(f"{indent}\t{term}{sep}")
            lines.append(f"{indent}) {{")
        lines.append(f"{indent}\treturn BFMT_PLURAL_{category.upper()};")
        lines.append(f"{indent}}}")
    return lines


def c_name(tag):
    return tag.replace("-", "_")


def generate_rules(functions, table):
    """The rule functions as C"""
    lines = []
    for key, name in functions.items():
        cardinal = json.loads(key[0])
        ordinal = json.loads(key[1])
        users = [tag for tag, fn in table if fn == name]
        lines.append(f"// {', '.join(users)}")
        lines.append("static bfmt_plural_t")
        lines.append(f"{name}(bfmt_plural_num_t num, bfmt_plural_type_t type) {{")
        lines.append("\t(void)num;")
        lines.append("\tif (type == BFMT_PLURAL_ORDINAL) {")
        lines.extend(compile_rules(ordinal, "\t\t"))
        lines.append("\t\treturn BFMT_PLURAL_OTHER;")
        lines.append("\t}")
        lines.append("")
        lines.extend(compile_rules(cardinal, "\t"))
        lines.append("\treturn BFMT_PLURAL_OTHER;")
        lines.append("}")
        lines.append("")
    return "\n".join(lines)


def generate(cardinals, ordinals, version, ref, template, out):
    # One function per distinct pair of rule sets, named after the first
    # language using it
    tags = sorted(cardinals, key=str.lower)
    functions = {}  # (cardinal json, ordinal json) -> function name
    table = []
    for tag in tags:
        key = (
            json.dumps(cardinals[tag], sort_keys=True),
            json.dumps(ordinals.get(tag, {}), sort_keys=True),
        )
        if key not in functions:
            functions[key] = f"bfmt_cldr__rule_{c_name(tag.lower())}"
        table.append((tag.lower(), functions[key]))
    table.sort(key=lambda entry: entry[0])

    out.write(string.Template(template).substitute(
        CLDR_VERSION=version,
        CLDR_REF=ref,
        RULES=generate_rules(functions, table),
        TABLE="\n".join(f'\t{{ "{tag}", {name} }},' for tag, name in table),
    ))

# }}}


def load(path, ref, name):
    if path is not None:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    with urllib.request.urlopen(RAW_URL.format(ref=ref, name=name)) as response:
        return json.load(response)


def main():
    args = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    args.add_argument("--ref", default="48.0.0", help="cldr-json git ref to fetch from (default: %(default)s)")
    args.add_argument("--plurals", help="plurals.json, instead of fetching it")
    args.add_argument("--ordinals", help="ordinals.json, instead of fetching it")
    args.add_argument("--template", help="the header template (default: bfmt_cldr.h.in next to this script)")
    args.add_argument("-o", "--output", help="the header to write (default: stdout)")
    opts = args.parse_args()

    plurals = load(opts.plurals, opts.ref, "plurals")
    ordinals = load(opts.ordinals, opts.ref, "ordinals")
    version = plurals["supplemental"]["version"]["_cldrVersion"]
    cardinals = plurals["supplemental"]["plurals-type-cardinal"]
    ordinal_rules = ordinals["supplemental"]["plurals-type-ordinal"]
    ref = opts.ref if opts.plurals is None else "local files"
    template_path = opts.template or os.path.join(os.path.dirname(os.path.abspath(__file__)), "bfmt_cldr.h.in")
    with open(template_path, encoding="utf-8") as f:
        template = f.read()

    if opts.output is None:
        generate(cardinals, ordinal_rules, version, ref, template, sys.stdout)
    else:
        with open(opts.output, "w", encoding="utf-8", newline="\n") as out:
            generate(cardinals, ordinal_rules, version, ref, template, out)


if __name__ == "__main__":
    main()
