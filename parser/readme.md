# minidb — SQL Parser

This document explains the design and current state of minidb's SQL parsing
pipeline: **lexer → parser → AST**.

## Pipeline overview

```
SQL string  ──tokenize()──>  vector<Token>  ──parseStatement()──>  ASTNode tree
```

Two independent stages:

- **Lexer** (`tokenize`) — turns the raw SQL string into a flat sequence of
  tokens. Knows nothing about SQL grammar or structure — just character
  classification.
- **Parser** (`parseStatement` and friends) — walks the token sequence and
  builds a tree (`ASTNode`), enforcing grammar rules and structure. Knows
  nothing about individual characters — only tokens.

An interactive REPL (`tests/repl.cpp` or similar — see **REPL** below)
exercises both stages together against arbitrary queries typed at a prompt.
A future `parseSQL(sql)` function will wrap both stages into one public
entry point, so callers never need to know lexing and parsing are separate.

---

## AST design

### Tag types

The AST node "kind" is represented as one of seven closed enums, combined
into a `std::variant`:

```cpp
enum class StatementType  { SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP };
enum class Clauses        { FROM, WHERE, GROUP_BY, HAVING, ORDER_BY, LIMIT, IF_EXISTS, DISTINCT, SET };
enum class Relations      { TABLE, JOIN, LEFT_JOIN, RIGHT_JOIN };
enum class ExpressionType { EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL, AND, OR };
enum class ValueType      { COLUMN, LITERAL, POSITION, ALIAS, FUNCTION, STAR, INDEX };
enum class OrderDirection { ASC, DESC };
enum class InternalNode   { ORDER_ITEM, VALUE_TUPLE, ARG_LIST, QUALIFIER, INDEX_LIST };

using ASTTag = std::variant<StatementType, Clauses, Relations, ExpressionType,
                             ValueType, OrderDirection, InternalNode>;
```

We deliberately use a **flat variant of closed enums**, not a generic
`template<EnumType T>` — the AST only ever produces nodes of these kinds,
so the type should reflect that closed set rather than accepting any enum.

### Node structure

```cpp
struct ASTNode {
    ASTTag type;
    std::string value;                                // e.g. "salary", "5000", "employees"
    std::vector<std::unique_ptr<ASTNode>> children;
};
```

A single node type with a tag + optional value + children list, rather
than a class hierarchy per node kind. Simpler for this scope; revisit if
node-specific fields are ever needed beyond a tag/value/children.

### Not every grammar symbol becomes a node

Some symbols carry meaning the tree needs to keep (a column name, an
operator, a literal value, an alias) and become nodes. Others are pure
structure — they tell the *parser* something, but contribute nothing the
AST needs to remember once parsing is done: `SELECT`, `FROM`, `WHERE`,
`,`, `;`, and `AS` are all consumed via `expect()` and discarded, never
wrapped in an `ASTNode`. When adding a new grammar rule, this is the
first question to ask: does this symbol's *identity* matter downstream,
or only its presence during parsing?

---

## Extending the tag set

The original five enums (`StatementType`, `Clauses`, `Relations`,
`ExpressionType`, `ValueType`) didn't have a home for everything the
grammar grew into. Each addition below follows the same test: does this
concept already fit an existing enum's *category*, or does forcing it in
misrepresent what that enum means?

### Sort direction and `ORDER_ITEM` (`ORDER BY`)

**Sort direction (`ASC`/`DESC`)** — not a value, not an expression
operator, not a clause:
```cpp
enum class OrderDirection { ASC, DESC };
```

**`ORDER_ITEM`, a wrapper node with no SQL keyword of its own** — each
`ORDER BY` entry (a column/position plus optional direction) needs to be
grouped as one unit so multiple order items don't collide:
```
ORDER BY
├─> ORDER_ITEM
│   ├─> COLUMN: department
│   └─> DESC
└─> ORDER_ITEM
    ├─> COLUMN: salary
    └─> ASC
```
`ORDER_ITEM` doesn't correspond to anything a user writes in SQL — it's
purely a tree-shape convenience the parser introduces. Putting it in
`Clauses` would be wrong: `Clauses` represents real SQL clauses (`WHERE`,
`GROUP_BY`, etc.), and `ORDER_ITEM` isn't one. It goes in a new enum
reserved for internal, non-SQL bookkeeping nodes:
```cpp
enum class InternalNode { ORDER_ITEM };
```
Any future wrapper/grouping node that exists only for tree structure
(not because a keyword demands it) belongs in `InternalNode`. (`VALUE_TUPLE`,
added later for `INSERT`, is the second member of this enum — see below.
`QUALIFIER`, added for qualified star support, is the third — see
**`SELECT *` and qualified star** below.)

**Position-based ordering (`ORDER BY 2`)** gets its own `ValueType`
member rather than reusing `LITERAL`:
```cpp
enum class ValueType { COLUMN, LITERAL, POSITION };
```
A bare number in `ORDER BY 2` and a bare number in `WHERE age = 30`
mean fundamentally different things — one is a column position, one is
a comparison value. Tagging them identically as `LITERAL` would force a
semantic analyzer to re-derive the distinction later by walking back up
the tree. Tagging the node itself as `POSITION` makes its meaning
self-evident — the same reasoning that justified separating `COLUMN`
from `LITERAL` in the first place.

### `LEFT_JOIN` / `RIGHT_JOIN` (`JOIN ... ON`)

Explicit joins needed a way to distinguish join *kind*, and to bundle
the joined table with its own `ON` condition — a flat list of children
under `FROM` can't express "this condition belongs to this join, not
some other one" once there's more than one join.

```cpp
enum class Relations { TABLE, JOIN, LEFT_JOIN, RIGHT_JOIN };
```

Distinct tags per join kind were chosen over one generic `JOIN` tag with
kind carried as a child — consistent with how comparators (`GREATER`,
`GREATER_EQUAL`, etc.) are separate tags rather than one `COMPARISON`
tag with a child specifying which one. Join kind is always known,
always present, and a fixed small set — exactly the shape that favors a
distinct tag over an optional child.

A `JOIN`'s wrapper node bundles the joined relation and its `ON`
condition as children:
```
FROM
├─> TABLE: employees
└─> LEFT_JOIN
    ├─> TABLE: departments
    └─> EQUALS
        ├─> COLUMN: dept_id
        │   └─> QUALIFIER: employees
        └─> COLUMN: id
            └─> QUALIFIER: departments
```
Comma-separated relations (`FROM a, b`) need no such wrapper — they
become flat `TABLE` siblings under `FROM`, since there's no per-item
data to bundle, unlike a join's condition.

**`ON` now accepts compound `AND`/`OR` conditions.** `ON` originally
called `parseComparison` directly — a single `column OP column` only.
Fixed by pointing `ON` at the same `parseOrExpr` entry point `WHERE` and
`HAVING` already use, so `ON a.id = b.id AND a.active = true` (and
arbitrary `AND`/`OR` nesting) now parses with the same precedence rules
as everywhere else in the grammar:
```sql
SELECT emp.name, mang.name FROM employees emp LEFT JOIN manager mang
  ON emp.dept_id = mang.dept_id AND emp.location = mang.location;
```
```
FROM
├─> TABLE: employees
│   └─> ALIAS: emp
└─> LEFT_JOIN
    ├─> TABLE: manager
    │   └─> ALIAS: mang
    └─> AND
        ├─> EQUALS
        │   ├─> COLUMN: dept_id
        │   │   └─> QUALIFIER: emp
        │   └─> COLUMN: dept_id
        │       └─> QUALIFIER: mang
        └─> EQUALS
            ├─> COLUMN: location
            │   └─> QUALIFIER: emp
            └─> COLUMN: location
                └─> QUALIFIER: mang
```
`OR` binds looser than `AND` inside `ON`, same as everywhere else, since
this is the exact same `or_expr`/`and_expr` chain, not a parallel
implementation:
```sql
... ON emp.id = mang.id OR emp.backup_manager_id = mang.id AND emp.active = 1;
```
```
OR
├─> EQUALS
│   ├─> COLUMN: id
│   │   └─> QUALIFIER: emp
│   └─> COLUMN: id
│       └─> QUALIFIER: mang
└─> AND
    ├─> EQUALS
    │   ├─> COLUMN: backup_manager_id
    │   │   └─> QUALIFIER: emp
    │   └─> COLUMN: id
    │       └─> QUALIFIER: mang
    └─> EQUALS
        ├─> COLUMN: active
        │   └─> QUALIFIER: emp
        └─> LITERAL: 1
```
No grammar *text* change was needed for this — the written grammar
always said `ON condition` with `condition := or_expr`. The bug was that
the code didn't match its own grammar: `ON` was wired to `parseComparison`
instead of `parseOrExpr`. Parenthesized grouping was the next gap and
is now closed — see **Parenthesized expressions** below.

### Column and table aliases (`AS`)

Aliasing needed a new `ValueType` member, but *not* a tag for `AS`
itself:
```cpp
enum class ValueType { COLUMN, LITERAL, POSITION, ALIAS };
```

The alias name becomes its own child node under the `COLUMN`/`TABLE`
node it renames — not merged into that node's `.value` as a combined
string (`"salary AS s"`), so the real name and the alias stay
independently addressable:
```
COLUMN: salary
└─> ALIAS: s
```

`AS` the keyword itself gets **no `ASTTag` at all** — it's a pure
syntactic marker, consumed via `expect(tokens, pos, TokenType::AS)` and
discarded, exactly like `SEMICOLON`/`FROM`/`WHERE`. It carries no
meaning the tree needs to keep; only the alias name that follows it
does. This mirrors the earlier decision to consume `SEMICOLON` without
emitting a node for it.

Grammar:
```
column   := IDENTIFIER alias?
relation := IDENTIFIER alias?
alias    := AS? IDENTIFIER
```
Both share the same alias-attachment pattern: parse the base
`IDENTIFIER`, check for `AS` or a bare following `IDENTIFIER`; either
way, consume the alias `IDENTIFIER` and attach it as an `ALIAS` child of
the node just built. The no-`AS` form (`salary s`, `employees e`) is
supported alongside the `AS` form — `alias := AS? IDENTIFIER` makes `AS`
itself optional, not the alias.

**Who attaches the alias (v6 rule).** The alias is attached by the
*caller* that owns the grammar slot, never by the function that builds
the base node: the select-list item, `parseColumn(allow_alias)`,
`parseFunctionCall(allow_alias)` and `parseRelation`. This is the
"node builder must not consume tokens belonging to the enclosing rule"
rule — see **Alias rules and the node-builder rule** below for the three
bugs that taught it.

Aliases compose correctly with joins: once a table is aliased, its
alias — not its original name — is what appears in qualified column
references inside `ON`:
```
FROM employees AS emp LEFT JOIN departments AS dept ON emp.dept_id = dept.id
```
```
FROM
├─> TABLE: employees
│   └─> ALIAS: emp
└─> LEFT_JOIN
    ├─> TABLE: departments
    │   └─> ALIAS: dept
    └─> EQUALS
        ├─> COLUMN: dept_id
        │   └─> QUALIFIER: emp
        └─> COLUMN: id
            └─> QUALIFIER: dept
```

### `HAVING`

Structurally identical to `WHERE` — `parseHavingClause` mirrors
`parseWhereClause` exactly, delegating the whole condition to
`parseOrExpr`:
```
having_clause := HAVING condition
```
```cpp
enum class Clauses { FROM, WHERE, GROUP_BY, HAVING, ORDER_BY, LIMIT };
```

`HAVING` without a preceding `GROUP BY` is rejected. This required a
`hasGroupBy` boolean tracked in `parseSelectStatement`, set at the
point `GROUP BY` is actually parsed:

```cpp
bool hasGroupBy = false;
if (check(tokens, pos, TokenType::GROUP)) {
    auto groupByNode = parseGroupByClause(tokens, pos);
    root->children.push_back(std::move(groupByNode));
    hasGroupBy = true;
}
// ...
if (check(tokens, pos, TokenType::HAVING)) {
    if (hasGroupBy) {
        auto havingNode = parseHavingClause(tokens, pos);
        root->children.push_back(std::move(havingNode));
    } else {
        parseError(tokens, pos, "HAVING requires GROUP BY");
    }
}
```

The naive version of this check — re-peeking the token stream for
`GROUP` at the point `HAVING` is being checked — doesn't work, because
`GROUP BY`'s tokens (if present) were already consumed earlier in the
same function call; `pos` no longer points at them. There is no way to
retroactively ask the token stream "did `GROUP BY` happen," so the flag
has to be captured live, at the moment it's true.

`HAVING`'s condition reuses `operand`, so once `operand` grew to accept
function calls (see below), `HAVING SUM(salary) > 5000` came along for
free without any change to `parseHavingClause` itself.

One ordering bug surfaced while wiring `HAVING` up alongside `ORDER BY`:
`parseSelectStatement` originally checked for `ORDER BY` *before*
`HAVING`, the reverse of the grammar's `having_clause? order_by_clause?`
sequence. For a query with both clauses present, the `ORDER` check ran
first, saw `HAVING` sitting at the current position (not `ORDER`), and
silently skipped — then after `HAVING` was parsed, nothing checked for
`ORDER BY` again, leaving it unconsumed and breaking the trailing
`expect(SEMICOLON)`. Fixed by checking `HAVING` before `ORDER BY`,
matching the grammar's own clause order.

### `INSERT` (`INSERT INTO ... VALUES ...`)

```
InsertStat  := INSERT INTO IDENTIFIER (LPAREN column_list RPAREN)? VALUES
               value_tuple (COMMA value_tuple)* SEMICOLON
value_tuple := LPAREN value_list RPAREN
value_list  := literal (COMMA literal)*
literal     := NUMBER | STRING
```

**The target table is a plain `IDENTIFIER`, not `relation`.** `relation`
(used by `FROM`/`JOIN`) carries an optional alias, because something
downstream — a qualified column in the same query — consumes that alias.
`INSERT`'s target has no such consumer: there's no `RETURNING`, no `ON
CONFLICT`, nothing in this grammar that would ever reference an alias on
the insert target. Reusing `relation` here would let the parser silently
accept `INSERT INTO employees AS e ...` and build an `ALIAS` node no
reader of the tree would ever look at. `relation`'s alias earns its
place in `FROM`/`JOIN` specifically because there's a consumer; here
there isn't one, so the target stays a bare `IDENTIFIER`.

**`value_list` introduces its own `literal` rule instead of reusing
`operand`.** `operand := IDENTIFIER | NUMBER | STRING | function_call`
treats `IDENTIFIER` as a column *reference* — meaningful in a
comparison, meaningless inside `VALUES`, where every slot is a literal
being stored, not something being looked up. Reusing `operand` would let
`INSERT INTO t (name) VALUES (some_column)` parse successfully, pushing
a checkable mistake downstream to semantic analysis instead of
rejecting it at parse time. `literal := NUMBER | STRING` makes that
input unrepresentable in the first place.

**Column list is optional.** Both the explicit form
(`INSERT INTO t (a, b) VALUES (...)`) and the schema-implied form
(`INSERT INTO t VALUES (...)`, columns matched by table order) are
accepted structurally. The parser was never going to validate the
positional match against a real table schema either way — it has no
schema access — so accepting the shorter form costs nothing at this
layer; that validation belongs to semantic analysis regardless of which
form is used.

**The column list is plain identifiers.** `parseColumn` used to accept an
optional alias, and `INSERT` reused it, so `INSERT INTO t (a x) VALUES (1)`
parsed and hung `ALIAS: x` under `COLUMN: a`. `parseColumn` now takes an
`allow_alias` flag: the select list passes `true`; the `INSERT` column
list and `GROUP BY` pass `false`.
```
INSERT INTO t (a x) VALUES (1);       Syntax error: at or near "x", expected ')'
INSERT INTO t (a AS x) VALUES (1);    Syntax error: at or near "AS", expected ')'
SELECT a FROM t GROUP BY a x;         Syntax error: at or near "x", expected ';'
```
The last message is accurate but terse (a `,`, `HAVING`, `ORDER` or
`LIMIT` could also follow); the trailing-token check planned for
`parseSQL` is the right place to improve it. `VALUES` items go through
`parseLiteral` (`NUMBER | STRING` only):
```
INSERT INTO t VALUES (a);       Syntax error: at or near "a", expected a number or string
INSERT INTO t VALUES (1, );     Syntax error: at or near ")", expected a number or string
```

**Multi-row `VALUES` and `VALUE_TUPLE`.** `value_tuple`'s
`(COMMA value_tuple)*` repetition is supported from the start. Each row
becomes its own wrapper node grouping that row's literals — the same
role `ORDER_ITEM` plays for `ORDER BY` entries — added to `InternalNode`:
```cpp
enum class InternalNode { ORDER_ITEM, VALUE_TUPLE };
```
This was initially named `VALUE_TYPE`, one word-order away from the
existing `ValueType` enum (`COLUMN, LITERAL, POSITION, ALIAS, FUNCTION`)
— a completely different concept (what *kind* of value a node holds, vs.
a wrapper grouping one row's literals). Renamed to `VALUE_TUPLE` before
it spread across `ast.hpp`/`parser.cpp`/`printer.cpp`, to avoid the kind
of near-miss name that causes a mis-read or a typo later.

```
INSERT INTO employees (name, salary) VALUES ('Alice', 5000), ('Bob', 6000);
```
```
Insert
├─> TABLE: employees
├─> COLUMN: name
├─> COLUMN: salary
├─> VALUE_TUPLE
│   ├─> LITERAL: Alice
│   └─> LITERAL: 5000
└─> VALUE_TUPLE
    ├─> LITERAL: Bob
    └─> LITERAL: 6000
```

### Function calls (`COUNT(*)`, `SUM(x)`, and any future function)

```
projection_item  := (column | function_call)
function_call    := IDENTIFIER LPAREN arg_list RPAREN alias?
arg_list         := STAR | arg_item (COMMA arg_item)*
arg_item         := function_call | IDENTIFIER | literal
operand          := function_call | IDENTIFIER | NUMBER | STRING
```

**Function names are plain `IDENTIFIER`s, not dedicated keyword
tokens.** `COUNT`, `SUM`, etc. tokenize as ordinary `IDENTIFIER`s;
`parseProjectionItem`/`parseOperand` recognize a function call purely by
lookahead — an `IDENTIFIER` immediately followed by `LPAREN`. Dedicating
a keyword token per function name was considered and rejected: it would
mean touching the lexer every time a new function is added, working
directly against letting users eventually define their own functions.
The function name itself just becomes the node's `.value`, same as a
column name does:
```cpp
enum class ValueType { COLUMN, LITERAL, POSITION, ALIAS, FUNCTION };
```

**The lookahead needed no new primitive.** Deciding "is this a column or
a function call" requires looking one token past the current
`IDENTIFIER`. `check()` already takes `pos` by value (unlike `expect()`,
which takes it by reference), so `check(tokens, pos + 1, TokenType::LPAREN)`
just works — no offset parameter, no generalized signature, no
consume-then-check dance needed.

**`COUNT(*)`'s star argument.** No children on the `FUNCTION` node
signals `*` (the star token is consumed via `expect()` and discarded,
same as `AS`); real arguments live under an `ARG_LIST` child (see
below):
```
FUNCTION: COUNT          FUNCTION: SUM
(no children = *)        └─> ARG_LIST
                             └─> COLUMN: salary
```
`STAR` sits as an alternative to the *entire* `arg_list`, not inside
`arg_item` — `arg_list := STAR | arg_item (COMMA arg_item)*` — so
`COUNT(*, x)` is not even syntactically reachable, rather than being
accepted by the grammar and rejected later in semantic analysis. `*`
never coexists with real arguments in any SQL dialect worth targeting,
so the grammar itself rules it out.

**Two aliasing bugs, both the same shape, both caught before commit.**
`function_call`'s own grammar includes `alias?` (`SUM(salary) AS total`
is a real, useful `SELECT`-list construct) — but `operand`'s
`function_call` alternative should *not* accept one; `HAVING SUM(salary)
AS s > 5000` isn't valid SQL, since aliasing only makes sense for
something appearing in the projection list, never inside a comparison.

1. `parseArg`'s `IDENTIFIER` branch originally called `parseColumn`,
   which does its own alias-check internally. `arg_item := IDENTIFIER |
   literal` has no alias in the grammar, so a function argument could
   silently absorb one it was never given a rule for. Fixed by having
   `parseArg` build the `COLUMN` node directly, without going through
   `parseColumn`.
2. The same shape recurred one level up: `parseOperand`'s function-call
   branch was reusing `parseFunctionCall` wholesale, which meant
   `HAVING SUM(salary) AS s > 5000` parsed successfully with an `ALIAS`
   child nothing in `operand`'s grammar authorized. Fixed with a
   `bool allowAlias = true` parameter on `parseFunctionCall`:
   `parseProjectionItem` calls it with the default; `parseOperand` calls
   it with `false`, skipping the trailing alias-check entirely. Verified
   by confirming the alias case now throws:
   ```
   HAVING SUM(salary) AS s > 5000;
   -- Error at the token AS (originally "Expected a comparator at position 12";
   --  since moved to the parseError format — see Syntax error messages)
   -- (AS is left unconsumed after the function call, since allowAlias=false
   --  never checks for it, so the next token a comparator-check sees is AS)
   ```

Both bugs came from the same root cause — reusing a parser function for
its convenience without checking whether the grammar rule at the call
site actually authorized everything that function does.

**Nested calls and the `ARG_LIST` wrapper.** An argument can itself be a
function call (`COALESCE(SUM(salary), 0)`), hence
`arg_item := function_call | IDENTIFIER | literal`. Arguments are
collected under an `InternalNode::ARG_LIST` child of `FUNCTION`.

`ARG_LIST` is the first `InternalNode` wrapper needed for a *different*
reason than `ORDER_ITEM`/`VALUE_TUPLE`. Those exist because several
same-shaped items would otherwise collide as flat siblings. `FUNCTION`
instead combines a variable-length argument list with another, distinct
optional child (`ALIAS`) at the same level. Without a wrapper, a consumer
would have to know "everything except the trailing `ALIAS` is an
argument". With it, `FUNCTION` has the fixed shape
`[ARG_LIST?, ALIAS?]`:

```
FUNCTION: SUM
├─> ARG_LIST
│   └─> COLUMN: salary
│       └─> QUALIFIER: emp
└─> ALIAS: total_salary        <- sibling of ARG_LIST, not a child
```

- **`*` has no `ARG_LIST`.** `COUNT(*)` is a `FUNCTION` with no children
  (or just `ALIAS`), so "no `ARG_LIST`" is the star signal.
- Nesting simply puts a `FUNCTION` node inside the `ARG_LIST`:
  ```
  FUNCTION: COALESCE
  └─> ARG_LIST
      ├─> FUNCTION: SUM
      │   └─> ARG_LIST
      │       └─> COLUMN: salary
      └─> LITERAL: 0
  ```
  (shape derived from the rules above; replace with your REPL output.)

### Syntax error messages

Parse failures use a Postgres-style format and are produced in **one
place**, the `parseError` helper:

```cpp
[[noreturn]] void parseError(const std::vector<Token>& tokens, size_t idx,
                             const std::string& detail);
// throws std::runtime_error("Syntax error: " + syntaxErrorLocation(tokens, idx)
//                           + ", " + detail)
```
`syntaxErrorLocation` builds the `at or near "X"` / `unexpected end of
input` part (bounds-safe: an index past the end reads as end of input),
and `tokenTypeToString` turns a `TokenType` into the name shown after
`expected`. The whole parser contains exactly **one** `throw
std::runtime_error` — the one inside `parseError`
(`grep -n "runtime_error" parser.cpp` should show a single line). Every
other failure path calls `parseError`.

```
Syntax error: at or near "e", expected ';'
Syntax error: at or near "Employee", expected TABLE
Syntax error: unexpected end of input, expected ';'
```

**Conventions**
- **`idx` is the offending token.** Validate *before* consuming
  (`parseError(tokens, pos, ...)`), so `pos` is the culprit and no
  `pos - 1` arithmetic is needed. Two sites originally ran the check
  after `pos++` and quoted the *next* token (`parseAlias`, the relation
  `*` check); both were fixed.
- **`expect()` is the generic fallback.** Every mandatory token gets a
  precise `expected X` message for free, with no per-call-site strings.
  Specific messages exist only where the generic one would mislead.
- **`detail` is plain text, never a formatted message.** Passing an
  already-built `Syntax error: ...` string as `detail` produced doubled
  prefixes on the `DROP` paths
  (`Syntax error: at or near ";", Syntax error: at or near ";", expected ON`).
  Let the first exception propagate; do not catch and re-wrap.
- **Wording:** lowercase, no trailing punctuation, no `at pos:N` — the
  location is the helper's job.

**Messages currently produced (all through `parseError`)**

| Where | Message |
|---|---|
| `parseStatement` | `expected SELECT, INSERT, UPDATE, DELETE or DROP` |
| `parseStatement`, `CREATE` / `ALTER` | `CREATE is not supported yet` / `ALTER is not supported yet` |
| `parseDropStatement` | `expected TABLE or INDEX` |
| `parseLiteral` / `expectLiteral` | `expected a number or string` |
| `parseOperand`, last branch | `expected a value (column, literal or function call)` |
| `parseProjectionList` | `expected a column or function after ','` |
| `parseOrderItem` | `expected column or position for ORDER BY` / `... after ','` |
| `parseSelectStatement` | `HAVING requires GROUP BY` |
| `parseQualifier` | `malformed qualified name`, `star is only allowed in the select list` |
| `parseAlias` | `alias cannot contain '.'` |
| `parseIndex` | `index name cannot contain '.'` |
| `parseRelation` | `relation name cannot contain '*'` |
| `expect()` | `expected <token>` / `unexpected end of input, expected <token>` |

Errors fall in three buckets, and only the first two exist yet:

| Kind | Raised by | Example |
|---|---|---|
| Lexical | `tokenize()` | unterminated string, bad character |
| Syntax | parser (`parseError`) | `DELETE employees WHERE ...` (expected FROM) |
| Semantic | future binder | unknown table, `HAVING` on a non-grouped column |

### Parenthesized expressions

```
primary := LPAREN or_expr RPAREN | comparison
```

`and_expr` now calls `parsePrimary` instead of `parseComparison`:

```cpp
std::unique_ptr<ASTNode> parsePrimary(const std::vector<Token>& tokens, size_t& pos) {
    if (check(tokens, pos, TokenType::LPAREN)) {
        expect(tokens, pos, TokenType::LPAREN);
        auto node = parseOrExpr(tokens, pos);
        expect(tokens, pos, TokenType::RPAREN);
        return node;
    }
    return parseComparison(tokens, pos);
}
```

**Parens create no node.** Same test as `AS` and `;`: does the symbol's
identity matter downstream? The grouping is fully encoded in the tree's
shape, so a `PAREN` node would only force every later phase to unwrap it.
Because `WHERE`, `HAVING` and `ON` all go through `parseOrExpr`, one
change gave all three paren support.

**Test choice matters.** `(a AND b) OR c` is redundant with default
precedence and would pass even if parens were ignored. The meaningful
check is one that *changes* the tree, e.g. `ON (a OR b) AND c`:

```
AND
├─> OR
│   ├─> (a)
│   └─> (b)
└─> (c)
```

Without parens the same text parses as `a OR (b AND c)`. Redundant and
deeply nested forms (`((x) AND (y))`) were also verified, as was a large
multi-line query combining joins, nested parens, `GROUP BY`/`HAVING` and
`ORDER BY`.

### `DELETE`

```
delete := DELETE FROM relation where_clause? SEMICOLON
```

```
SQL> DELETE FROM employees WHERE id = 5;
Delete
├─> FROM
│   └─> TABLE: employees
└─> WHERE
    └─> EQUALS
        ├─> COLUMN: id
        └─> LITERAL: 5
```

- **Target is a `relation`**, not a bare `IDENTIFIER`: `WHERE` can refer
  to the table by alias (`DELETE FROM employees e WHERE e.id = 5`), so
  the alias must be consumable. Contrast `INSERT`, whose target is a
  plain `IDENTIFIER` because nothing after it uses an alias.
- **`FROM` wrapper kept** around the target, matching `SELECT`'s tree so
  later phases resolve the table the same way for both statements.
- `WHERE` is optional and reuses `parseWhereClause` unchanged.
- The printer's root label is `Delete`, matching `Select`/`Insert`
  (it originally printed `DELETE`; fixed).

Verified errors:
```
DELETE employees WHERE id = 5;
Error: Syntax error: at or near "employees", expected FROM
```
The statement must end in `;` — `parseDeleteStatement` calls
`expect(SEMICOLON)` after the optional `WHERE`, so trailing junk is caught:
```
DELETE FROM t WHERE id = 1 foo;
Error: Syntax error: at or near "foo", expected ';'
```

### `DROP` (`TABLE`, `INDEX`, `IF EXISTS`)

```
DropStat   := DROP TABLE (IF EXISTS)? IDENTIFIER SEMICOLON
            | DROP INDEX (IF EXISTS)? index_list ON IDENTIFIER SEMICOLON
index_list := IDENTIFIER (COMMA IDENTIFIER)*
```

`parseDropStatement` consumes `DROP`, then branches on `TABLE` or `INDEX`.
Anything else throws `expected TABLE or INDEX after DROP, but found ...`.
There is deliberately a final `else`: without it `DROP;` fell through to
`expect(SEMICOLON)` and parsed as a valid empty `Drop`.

**`DROP TABLE`**
```
SQL> Drop table Employee;
Drop
└─> TABLE: Employee
```
- Target is a plain `IDENTIFIER`: an alias on `DROP` is meaningless and is
  rejected, not silently absorbed (`DROP TABLE Employee e;` fails at `"e"`,
  expected `';'`).
- The `TABLE:` node reuses `Relations::TABLE`, same as in `FROM`.

**`DROP INDEX`**
```
SQL> DROP INDEX a, b ON t;
Drop
├─> INDEX_LIST
│   ├─> INDEX: a
│   └─> INDEX: b
└─> TABLE: t
```
- **`INDEX_LIST` wrapper.** Several same-shaped index names need a
  container rather than colliding as flat siblings, the same reason as
  `ARG_LIST` and `VALUE_TUPLE`. A single index still gets the wrapper, so
  consumers never branch on count.
- **`ON <table>` is required** (MySQL / SQL Server style). Postgres omits
  it because index names are schema-global; this is a deliberate dialect
  choice, not an oversight.
- **`TABLE` means different things in the two forms.** Under `DROP TABLE`
  it is the object being dropped; under `DROP INDEX` it is the table that
  owns the indexes. `INDEX_LIST` is the discriminator: a binder must check
  for it rather than just looking for a `TABLE` child.
- The owner table is a plain `IDENTIFIER`, so an alias is rejected
  (`DROP INDEX EMP_ID ON EMPLOYEE e;` fails at `"e"`, expected `';'`).

**`IF EXISTS`**
```
SQL> DROP TABLE IF EXISTS t;
Drop
├─> IF_EXISTS
└─> TABLE: t

SQL> DROP INDEX IF EXISTS a, b ON t;
Drop
├─> IF_EXISTS
├─> INDEX_LIST
│   ├─> INDEX: a
│   └─> INDEX: b
└─> TABLE: t
```
- **A marker node, `Clauses::IF_EXISTS`, no value.** Same test as every
  other symbol: its identity matters downstream (the executor must not
  error when the object is missing), unlike `AS` or `;`.
- **It is the first child of `Drop`**, so the tree reads like the SQL.
- **`IF` and `EXISTS` are reserved words**, added to `TokenType` *and*
  `keywordTable`. With only the enum entry, `IF` lexed as an `IDENTIFIER`
  and `DROP TABLE IF EXISTS t` failed at `EXISTS`, expected `';'`.
- **Position matters.** The clause is parsed right after `TABLE`/`INDEX`,
  *before* the name. Checking after the name made the parser read `IF` as
  the table name; in the `INDEX` branch it would have accepted
  `DROP INDEX a, b IF EXISTS ON t`.
- The check requires `IF` and `EXISTS` together (`pos + 1` lookahead), so
  a lone `IF` reports `expected an identifier` rather than `expected EXISTS`.
  Since `IF` is reserved, checking `IF` alone and then `expect(EXISTS)`
  would give the better message.

**`parseIndex`.** Index names are parsed by their own function
(`expect(IDENTIFIER)` → `ValueType::INDEX` node). Because the lexer folds
`.` and `*` into IDENTIFIER-shaped tokens, a plain `expect(IDENTIFIER)`
accepted `idx.name` and `emp.*` as index names and only failed later at
`ON`. `parseIndex` rejects them at the token:
```
DROP INDEX idx.name ON t;   Syntax error: at or near "idx.name", index name cannot contain '.'
DROP INDEX emp.* ON t;      Syntax error: at or near "emp.*", index name cannot contain '.'
```

**Verified errors**
```
DROP INDEX idx;                  at or near ";", expected ON
DROP INDEX ;                     at or near ";", expected an identifier
DROP INDEX EMP_ID ON;            at or near ";", expected an identifier
DROP INDEX ON EMPLOYEE;          at or near "ON", expected an identifier
DROP INDEX EMP_ID ON EMPLOYEE e; at or near "e", expected ';'
DROP TABLE IF t;                 at or near "IF", expected an identifier
DROP TABLE IF EXISTS;            at or near ";", expected an identifier
DROP;                            at or near ";", expected TABLE or INDEX
DROP Employee;                   at or near "Employee", expected TABLE or INDEX
```
All of these go through `parseError`. (A doubled
`Syntax error: ..., Syntax error: ...` prefix on the `DROP INDEX` paths
was fixed; see **Syntax error messages**.)

**Untested so far:** `DROP TABLE;`, `DROP TABLE a, b;`, and for `DROP INDEX`
a trailing comma, a missing comma and a three-item list. `DROP FUNCTION`
would need `CREATE FUNCTION` first.

### `SELECT *` and qualified star (`table.*`)

```
projection_item := STAR | function_call | column
```
A bare `*` and a table-qualified `table.*` both surface structurally as
a `STAR` node — `*` alone, `table.*` with a `QUALIFIER` child:
```
select id, * from employee;
```
```
├─> COLUMN: id
└─> STAR
```
```
select emp.* from employee emp;
```
```
└─> STAR
    └─> QUALIFIER: emp
```

**Tokenization: no new token type.** `emp.*` is lexed exactly the same
way `emp.name` already was — the lexer's identifier scan swallows
letters, digits, `_`, `.`, and `*` into one greedy token, so `emp.*`
arrives as a single `IDENTIFIER`-shaped token with lexeme `"emp.*"`.
A dedicated `DOT` token (splitting `emp.*` into three tokens at lex
time) was considered and rejected — it would mean the lexer forking its
scanning behavior based on *what* follows a dot, which is backwards for
a stage that's supposed to only classify characters, not interpret
content. Disambiguating "is this plain, qualified, or a qualified star"
stays entirely a parse-time decision, made once, uniformly, for both
`STAR` and `COLUMN`.

**Qualifier handling in `parseQualifier` (v6).** The caller checks that
the current token is an `IDENTIFIER` *by type* and that its lexeme contains
a `.`, then calls `parseQualifier(tokens, pos, allow_star)`, which **always
consumes that token** and builds one node:

- Validates the shape up front and rejects `emp..id`, `emp.`, `.id`
  (multiple dots, or an empty side): `malformed qualified name`, reported at
  the token.
- Suffix `*` and `allow_star == true`: a `STAR` node with a `QUALIFIER`
  child (`emp.*`).
- Suffix `*` and `allow_star == false`: `star is only allowed in the
  select list`. Only the select list passes `true`; `ORDER BY`, `GROUP BY`,
  function arguments, `WHERE`, `ON` and `SET` pass `false`.
- Otherwise: `COLUMN: id` with a `QUALIFIER: emp` child.
- It does **not** look for an alias. Callers attach aliases (see below).

**Dispatch is on token type, not lexeme text.** `3.14` (a `NUMBER`) and
`'a.b'` (a `STRING`) both contain a dot. An early version dispatched on
`lexeme.find('.')`, so `SELECT 3.14` became a bogus qualified column. The
guard is `tokens[pos].type == IDENTIFIER` first, dot second.

The v5 version returned `nullptr` *without* consuming for plain qualified
columns and consumed only for stars. That asymmetry is gone: the function
now has one contract (consume, build, return), and the callers decide
whether to call it. The earlier decoy-vector / `const_cast` approach is
long gone too; `tokens` is `const&` everywhere and nothing mutates it.

**Design decision (reversed from v5): `QUALIFIER` is attached to plain
`COLUMN` as well as `STAR`.** v5 left `emp.id` folded into the `COLUMN`
node's `.value` string and noted that the binder would have to re-split it,
duplicating `parseQualifier`'s logic. That was the wrong side of the
trade-off: resolving `emp.id = mang.id` needs the table qualifier exactly
as much as `mang.*` does. The parser already knows the split, so it
records it once, structurally, and the binder reads a child instead of
re-parsing a string.
```
SELECT emp.name FROM employees emp WHERE emp.id = 5;
```
```
COLUMN: name
└─> QUALIFIER: emp
```
The `QUALIFIER`'s value is the table (or alias) name exactly as written;
the parser does not check that it names anything. That is the binder's job.
Every `COLUMN: x.y` tree in earlier versions of this document is now
`COLUMN: y` with a `QUALIFIER: x` child, including the `ON`, `ORDER BY`,
`GROUP BY` and function-argument trees below.

**Three bugs caught while wiring this up**, all variations of "who
actually advances the real parser cursor":
1. An earlier attempt had the star-branch delegate to `parseStar` on a
   throwaway one-element decoy `std::vector<Token>`, so the *real*
   `pos` never moved — `parseStar` dutifully consumed the fake token,
   leaving the real stream's cursor sitting exactly where it started.
2. The plain-qualified-column case (dot present, suffix not `*`) had no
   fallback branch at all in `parseProjectionList` for a while — when
   `parseQualifier` correctly returned `nullptr`, nothing called
   `parseColumn` to actually consume and build the column, so
   `emp.dept` next to an unqualified column silently produced nothing.
3. In the comma loop, the bare-`*` check and the "must be an
   `IDENTIFIER`" trailing-comma guard were briefly two independent
   `if`s instead of one `if`/`else if` chain, so successfully parsing
   a bare `*` mid-list fell straight through into the guard, which then
   threw a spurious "expected a column or function after ','" on
   whatever token came *after* the star.

```sql
SELECT emp.department, emp.salary, mang.* FROM employees AS emp
  LEFT JOIN manager AS mang ON emp.id = mang.id;
```
```
├─> COLUMN: department
│   └─> QUALIFIER: emp
├─> COLUMN: salary
│   └─> QUALIFIER: emp
└─> STAR
    └─> QUALIFIER: mang
```

### `UPDATE`

```
UpdateStat := UPDATE relation SET assignment (COMMA assignment)* where_clause? SEMICOLON
assignment := column EQUALS operand
```
```
UPDATE employees SET salary = 6000, dept = 'IT' WHERE id = 5;
```
```
Update
├─> TABLE: employees
├─> SET
│   ├─> EQUALS
│   │   ├─> COLUMN: salary
│   │   └─> LITERAL: 6000
│   └─> EQUALS
│       ├─> COLUMN: dept
│       └─> LITERAL: IT
└─> WHERE
    └─> EQUALS
        ├─> COLUMN: id
        └─> LITERAL: 5
```
- **Target is a `relation`**, like `DELETE`: `WHERE` and `SET` may name it
  by alias, so the alias must be consumable.
- **`Clauses::SET` wrapper.** The assignments live under a `SET` node,
  matching the `FROM`/`WHERE` clause nodes. `SET` is a reserved word: it
  went into `TokenType` *and* `keywordTable`, and forgetting the table
  entry made `SET` lex as an identifier.
- **No new `ASSIGNMENT` tag.** An assignment reuses `ExpressionType::EQUALS`.
  The cost is that the same node means "compare" under `WHERE` and "assign"
  under `SET`; **the binder must check the parent** (`SET` vs `WHERE`) to
  tell them apart. Chosen over a new tag because the tree shape is
  identical (column on the left, operand on the right) and a second tag
  would have forced every consumer to handle two equal-shaped nodes.
- **Left side is a `column`, qualifier allowed** (`SET e.salary = 1`,
  MySQL-style). The parser does not check that the qualifier names the
  target; the binder must verify it matches the target relation or alias.
  Aliases on the left are not allowed.
- **Right side is an `operand`**: a literal, a column or a function call.
  Arithmetic (`SET salary = salary + 1000`) is not supported yet — it needs
  the expression grammar to grow first.
- Like the other statements, it ends with `expect(SEMICOLON)`.

### `DISTINCT`

```
SelectStat := SELECT DISTINCT? projection_list ...
```
`DISTINCT` is a childless **marker node**, `Clauses::DISTINCT`, directly
under `Select`, by the same test as `IF_EXISTS`: whether it is present
changes what the executor must do, so its identity matters downstream,
unlike `AS` or `;`. `DISTINCT` is a reserved word (enum *and*
`keywordTable`).

### Literals in the select list

`projection_item` accepts a literal: `SELECT 3.14`, `SELECT name, 1`,
`SELECT 'a.b' FROM t`. They share `parseLiteral` with `INSERT VALUES`
(`NUMBER | STRING` only; `parseLiteral` wraps `expectLiteral` and builds
the `LITERAL` node). The dispatch is on token *type*: the string `'a.b'`
contains a dot but is a `STRING`, so it is a literal, not a qualified
column.

**Decision pending:** an alias on a literal (`SELECT 1 AS one`) is
currently rejected. It is valid SQL; accepting it means giving the
literal item an alias slot the way `column` and `function_call` have one.

### Alias rules and the node-builder rule

**Rule: a function that builds a node must not consume tokens that belong
to the enclosing rule.** The alias leak happened three times before it
became a rule:

1. `parseArg` called `parseColumn`, which swallowed an alias, so
   `SUM(emp.salary AS s)` parsed.
2. `parseOperand` called `parseFunctionCall` with aliasing on, so an alias
   after a function call in `HAVING`/`WHERE` was swallowed.
3. `parseQualifier` had its own alias block (with an `else` that threw
   "alias is not allowed" on perfectly ordinary input).

The cure in each case was the same: the builder builds, and the **caller
that owns the grammar slot decides whether an alias is legal there**.
`parseColumn(allow_alias)` and `parseFunctionCall(allow_alias)` take a flag;
`parseQualifier` has no alias logic at all.

| Slot | Alias? |
|---|---|
| select-list column, function call | yes |
| `FROM` / `JOIN` relation | yes |
| function arguments, operands | no |
| `INSERT` column list, `GROUP BY` | no |
| `UPDATE` / `DELETE` / `DROP` targets: `UPDATE` and `DELETE` take a relation (alias allowed); `DROP` and `INSERT` targets do not | see statement |
| literal in the select list | no (pending) |

**Dotted aliases are rejected.** The lexer folds `.` into identifier
tokens, so `FROM employees e.salary` would otherwise lex `e.salary` as a
valid alias:
```
SELECT a FROM employees e.salary;
Syntax error: at or near "e.salary", alias cannot contain '.'
```

### Boolean expression chains (`AND` / `OR`)

`or_expr` and `and_expr` are **accumulator loops**, which is what makes
`a AND b AND c AND d` left-associative and unlimited in length:
```cpp
auto left = parseAndExpr(tokens, pos);
while (check(tokens, pos, TokenType::OR)) {
    expect(tokens, pos, TokenType::OR);
    auto right = parseAndExpr(tokens, pos);
    auto node = std::make_unique<ASTNode>();
    node->type = ExpressionType::OR;
    node->children.push_back(std::move(left));
    node->children.push_back(std::move(right));
    left = std::move(node);
}
return left;
```
An earlier version used `if` instead of `while` and read only two
operands, so chains of three or more conditions broke (and a later patch
that returned from inside the loop, or moved from an already-moved
child, broke differently). The fix was the shape above: parse one operand,
then fold each `OP operand` pair into the running result.

### Relation names

A relation name is **one `IDENTIFIER` token, stored verbatim**. It may
contain a `.` (so `schema.table` is accepted and kept as the string
`schema.table`) but never a `*`:
```
SELECT a FROM emp.*;
Syntax error: at or near "emp.*", relation name cannot contain '*'
```
Whether `emp.name` names a real schema and table is **not a parser
question**. The binder will split the name on `.` and resolve both parts
against the catalog, the same convention it uses for qualified columns.

**Open decision:** introduce a schema qualifier in the tree
(`TABLE: employees` with a `QUALIFIER: hr` child, mirroring qualified
columns) or keep the verbatim string until the catalog has schemas. The
case for doing it before the binder exists is that no code reads relation
values yet, so changing the tree shape later costs more. If done: at most
one dot (`db.schema.table` is a separate decision), and reuse the dot
validation in `parseQualifier` rather than copying it.

### Design lessons from this stage

- **Single function, single job.** The three alias leaks, the doubled
  error prefix and the dotted index names were all one function doing a
  neighbour's job.
- **Test ordinary inputs, not only error cases.** Bugs in
  `parseQualifier` (`else { throw }`), `3.14` as a qualified column and
  the two-operand `AND` limit were all found by trivially normal queries.
- **Rebuild before believing a stale error message.** Several "bugs"
  were an old binary. If a message doesn't match the source, grep the
  source for the string first.

---

## Lexer

### Token design

```cpp
enum class TokenType {
    // keywords
    SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP,
    FROM, WHERE, GROUP, BY, ORDER, LIMIT, AND, OR, TABLE, JOIN,
    HAVING, AS, ON, LEFT, RIGHT, INTO, VALUES,
    // identifiers & literals
    IDENTIFIER, NUMBER, STRING,
    // operators
    EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL,
    // punctuation
    COMMA, SEMICOLON, LPAREN, RPAREN, STAR,
    // control
    END_OF_INPUT, UNKNOWN,
    // ordering
    ASC, DESC,
};

struct Token {
    TokenType type;
    std::string lexeme;
};
```

**Design decision — flat enum, not categorized.** We considered nesting
`TokenType` under categories (`KEYWORD`, `OPERATOR`, `PUNCTUATION`) via
a `variant` or a category field, but rejected it: the parser never needs
to ask "is this *any* keyword" — every parse decision branches on the
exact token (`SELECT` vs `FROM` vs `WHERE`), never the category. A
stored category with no behavioral use is pure overhead. This matches
how production lexers (PostgreSQL, SQLite, Python's `tokenize`, Rust's
`rustc_lexer`) are built — one flat enum, category groupings expressed
only as comments or on-demand helper functions if ever needed.

**`GROUP` and `BY` (and `ORDER`/`BY`) are separate tokens.** The lexer
scans one word at a time and has no multi-word lookahead, so `GROUP BY`
arrives as two tokens. The *parser* is responsible for expecting them
back-to-back as one clause.

**Operators are not in the keyword table.** `>`, `<`, `>=`, `<=`, `=`
are matched by character in the tokenizer's own branches, not via
string lookup — `keywordTable` only holds actual keyword words. A
previous typo (`"RiGHT"` instead of `"RIGHT"` in the keyword table)
caused `RIGHT` to silently fall through to `IDENTIFIER`, since
`tokenize()` uppercases the scanned word before lookup — a case worth
remembering when adding new keywords: a mismatched-case map key fails
silently, not loudly.

**`INTO` and `VALUES` were briefly missing from `keywordTable`
entirely** — a full omission this time, not a case typo. `TokenType`
had both, and `parser.cpp` referenced `TokenType::INTO`/`TokenType::VALUES`
throughout `parseInsertStatement`, but since neither word was actually a
key in `keywordTable`, `tokenize()` fell through to `IDENTIFIER` for
both, identical to any ordinary table or column name. Caught by
dedicated lexer output (`lexer_test`, printing each token's type number
and lexeme) showing `INTO`/`VALUES`/`EMPLOYEE` all tokenizing to the same
type. The general lesson from both this and the `RiGHT` typo: adding a
token to `TokenType` and referencing it in the parser proves nothing
about whether the lexer actually produces it — that only happens if the
exact keyword string is also registered in `keywordTable`.

**`(` and `)` were never given a branch in `tokenize()`,** despite
`LPAREN`/`RPAREN` existing in `TokenType` and being used throughout the
parser (`INSERT`'s value tuples, function-call argument lists). Adding a
token to the enum doesn't make the lexer emit it — `tokenize()`'s
character-classification chain has to have a matching branch. Caught by
the lexer throwing "Unexpected symbol '('" — its final `else` branch,
which only fires when a character matches none of the known branches.
Fixed with the same shape as the existing `,`/`;` branches:
```cpp
else if (sql[pos] == '(') { tokens.push_back({TokenType::LPAREN, "("}); pos++; }
else if (sql[pos] == ')') { tokens.push_back({TokenType::RPAREN, ")"}); pos++; }
```

### `tokenize()` behavior

- Skips whitespace
- **Digits** → accumulate into a `NUMBER` token. After consuming all
  leading digits, if a `.` is followed by another digit (checked with
  lookahead), the `.` and the following digits are consumed too,
  producing a single decimal `NUMBER` token (`3000.50`). Without the
  lookahead, a trailing or dangling `.` (`5000.`, `5000.AND`) is left
  alone rather than guessed at.
- **Letter/underscore** → accumulate into a word, including `.` *and*
  `*` as continuation characters (not just letters/digits/underscore),
  so qualified identifiers (`employees.dept_id`) and qualified stars
  (`emp.*`) both tokenize as a single `IDENTIFIER`-shaped token rather
  than several. Splitting that token into its qualifier/name (or
  qualifier/star) parts is a parse-time decision (`parseQualifier`),
  not the lexer's concern — see **`SELECT *` and qualified star**
  above. The word is uppercased and looked up in `keywordTable`;
  matches become that keyword's `TokenType`, otherwise `IDENTIFIER`.
- `'...'` → `STRING` token, quotes stripped from the stored lexeme
- `>`/`<` → peek the next character; if `=` follows, consume both and
  emit `GREATER_EQUAL`/`LESSER_EQUAL`, otherwise emit `GREATER`/`SMALLER`
- `=`, `,`, `;`, `(`, `)`, `*` → single-character punctuation/operator
  tokens (a bare `*` not preceded by a letter/underscore hits this
  branch directly and becomes a `STAR` token immediately — only a
  `*` that follows an identifier-starting word gets swallowed into that
  word's lexeme instead, per the point above)
- Anything unrecognized → throws `std::runtime_error`
- Always appends a trailing `END_OF_INPUT` token

**A digit-branch parenthesization bug was caught and fixed.** An early
attempt at decimal support wrote
`isdigit(sql[pos] && sql[pos] == '.')` — the `&&` landed *inside*
`isdigit()`'s argument rather than joining two loop conditions, so the
expression always evaluated to `isdigit(0)` or `isdigit(1)`, both
false. The `while` loop body never ran, `pos` never advanced past a
digit, and the outer loop re-hit the same character forever — an
infinite hang on any input containing a digit. Fixed by using two
separate loops (consume digits, then optionally consume `.` + more
digits with lookahead) rather than trying to fold both cases into one
condition.

### File layout (lexer)

| File | Contents |
|---|---|
| `token.hpp` | `TokenType`, `Token`, `keywordTable` — pure data, no behavior |
| `lexer.hpp` / `lexer.cpp` | Declares/defines `tokenize()` |

(Naming note: originally called `token.cpp`, renamed since `tokenize()`
is lexer *behavior*, not token *data* — the two belong in separate
files.)

---

## Parser

### Grammar

```
Statement        := SelectStat | InsertStat | UpdateStat | DeleteStat | DropStat

InsertStat       := INSERT INTO IDENTIFIER (LPAREN plain_column_list RPAREN)? VALUES value_tuple (COMMA value_tuple)* SEMICOLON
value_tuple      := LPAREN value_list RPAREN
value_list       := literal (COMMA literal)*
literal          := NUMBER | STRING
UpdateStat       := UPDATE relation SET assignment (COMMA assignment)* where_clause? SEMICOLON
assignment       := column EQUALS operand        # column: no alias; qualifier allowed
DeleteStat       := DELETE FROM relation where_clause? SEMICOLON
DropStat         := DROP TABLE (IF EXISTS)? IDENTIFIER SEMICOLON
                  | DROP INDEX (IF EXISTS)? index_list ON IDENTIFIER SEMICOLON
index_list       := index (COMMA index)*
index            := IDENTIFIER                   # no '.' or '*'

SelectStat       := SELECT DISTINCT? projection_list from_clause where_clause? group_by_clause?
                      having_clause? order_by_clause? limit_clause? SEMICOLON

projection_list  := projection_item (COMMA projection_item)*
projection_item  := STAR | qualified_star | function_call | literal | column
qualified_star   := IDENTIFIER_DOT_STAR      # lexed as one token, e.g. emp.*
function_call    := IDENTIFIER LPAREN arg_list RPAREN alias?
arg_list         := STAR | arg_item (COMMA arg_item)*
arg_item         := function_call | column | literal     # no alias inside arguments

plain_column_list := plain_column (COMMA plain_column)*  # INSERT, GROUP BY: no alias
column            := qualified_name alias?               # select list only for alias
qualified_name    := IDENTIFIER                          # optionally table.column

from_clause       := FROM relation (COMMA relation)* joins*
joins             := (LEFT | RIGHT)? JOIN relation ON condition
relation          := IDENTIFIER alias?                   # may contain '.', never '*'
alias             := AS? IDENTIFIER                      # no '.'

where_clause      := WHERE condition
having_clause     := HAVING condition   # requires a preceding group_by_clause

condition         := or_expr
or_expr           := and_expr (OR and_expr)*
and_expr          := primary (AND primary)*
primary           := LPAREN or_expr RPAREN | comparison

comparison        := operand comparator operand
comparator        := EQUALS | GREATER | SMALLER | GREATER_EQUAL | LESSER_EQUAL
operand           := function_call | IDENTIFIER | NUMBER | STRING

group_by_clause   := GROUP BY plain_column_list
order_by_clause   := ORDER BY order_item (COMMA order_item)*
order_item        := (column | position) (ASC | DESC)?
position          := NUMBER

limit_clause      := LIMIT NUMBER
```

**Why `or_expr`/`and_expr` are layered instead of one self-referential
`condition` rule.** A naive grammar like
`condition := condition AND condition | condition OR condition | comparison`
is left-recursive, which recursive-descent parsing cannot handle (it
would call itself forever without consuming a token). Layering
`or_expr` on top of `and_expr` avoids left recursion *and* encodes
precedence: `AND` always binds tighter than `OR`, since `and_expr` sits
below `or_expr` in the call chain — matching standard SQL semantics for
`A OR B AND C` meaning `A OR (B AND C)`. This same `or_expr` chain is
reused, unmodified, by `HAVING` and — as of the compound-`ON` fix above
— fully by `ON` as well; all three clauses share one precedence
implementation rather than three parallel ones.

**Why the plain column list (used by `GROUP BY` and the `INSERT` column
list) stayed separate from `projection_list` (used by `SELECT`).** `GROUP BY SUM(x)` isn't valid
SQL — you can't group rows by an aggregate result computed from those
same rows. So only the `SELECT` list needed the ability to hold a
function call; `column_list` deliberately kept its original, narrower
shape (`column` only) rather than being widened to match.

### Grammar → code translation pattern

| Grammar symbol | Code |
|---|---|
| Literal keyword (`SELECT`, `FROM`, `;`) | `expect(tokens, pos, TokenType::X)` |
| Non-terminal (another rule) | Call that rule's parse function |
| `X?` (optional) | `if (check(tokens, pos, ...)) { ... }` |
| `X*` (zero or more) | `while (check(tokens, pos, ...)) { ... }` |
| `A \| B \| C` (alternatives) | `if/else if` chain, each guarded by `check()` |

Each grammar rule maps to one parse function; the function body is a
direct translation of the rule's right-hand side using the table above.

### Cursor-passing convention

Every parse function shares one token stream and one cursor:
```cpp
std::unique_ptr<ASTNode> parseX(const std::vector<Token>& tokens, size_t& pos);
```
`pos` is passed by reference so each function's consumption is visible
to its caller — standard recursive-descent threading. `tokens` stays
`const&` since parsing only ever reads the stream, never mutates it —
including `parseQualifier` (see **`SELECT *` and qualified star**
above), which decides what the stream *means* without ever writing to
it.

### Private helpers — `check` / `expect`

Declared only in `parser.cpp` (anonymous namespace), **not** in
`parser.hpp` — they're internal implementation details, not part of the
public API.

```cpp
bool check(const std::vector<Token>& tokens, size_t pos, TokenType type);
// Bounds-safe peek. No side effects, no consumption. Returns false (not
// an error) on mismatch or out-of-bounds. Used whenever the parser
// doesn't yet know what comes next and needs to decide which branch to
// take — e.g. "is there a WHERE clause, or not?" Because pos is taken
// by value here (unlike expect()'s pos), passing pos + 1 (or any other
// offset) peeks further ahead safely, with no change to check() itself
// — used by parseProjectionItem/parseOperand to distinguish a plain
// column from a function call before consuming anything.

Token expect(const std::vector<Token>& tokens, size_t& pos, TokenType type);
// Consumes and returns the current token if it matches; throws
// std::runtime_error otherwise. Used whenever the next token is
// mandatory and known in advance — e.g. once parseSelectStatement has
// been entered, SELECT *must* be there.
```

Rule of thumb: **mandatory next token → `expect()` directly. Optional
or undetermined next token → `check()` first, then act only if it
matches.**

### Statement dispatcher

```cpp
std::unique_ptr<ASTNode> parseStatement(const std::vector<Token>& tokens, size_t& pos) {
    if (check(tokens, pos, TokenType::SELECT))       return parseSelectStatement(tokens, pos);
    else if (check(tokens, pos, TokenType::INSERT))  return parseInsertStatement(tokens, pos);
    else if (check(tokens, pos, TokenType::UPDATE))  return parseUpdateStatement(tokens, pos);
    else if (check(tokens, pos, TokenType::DELETE))  return parseDeleteStatement(tokens, pos);
    else if (check(tokens, pos, TokenType::DROP))    return parseDropStatement(tokens, pos);
    else if (check(tokens, pos, TokenType::CREATE))  parseError(tokens, pos, "CREATE is not supported yet");
    else if (check(tokens, pos, TokenType::ALTER))   parseError(tokens, pos, "ALTER is not supported yet");
    else parseError(tokens, pos, "expected SELECT, INSERT, UPDATE, DELETE or DROP");
}
```

**What this function is and isn't responsible for:** it only answers
*which kind* of statement this is, based on the leading token, and
routes to the matching specialist parser. It does **not** validate that
everything after the leading keyword is well-formed — that's the job of
whichever `parseXStatement` it dispatches to. E.g. `parseStatement`
won't catch `SELECT FROM;` (missing columns) — `parseSelectStatement` /
`parseProjectionList` will, when they expect an identifier and find
`FROM` instead.

`CREATE` and `ALTER` are recognised but not implemented; they fail with a
proper syntax error at the keyword rather than falling through to
"unknown statement".

`parseGroupByClause` calls `parseColumnList()` directly rather than
duplicating comma-separated-identifier logic, since `GROUP BY
department, salary` is the same grammar shape as `GROUP BY`'s own
column list.

**Optional-clause lookahead pattern**, consistent across `WHERE`,
`GROUP BY`, `HAVING`, `ORDER BY`, and `LIMIT`: `parseSelectStatement`
only peeks at the clause's *opening* keyword (`check(WHERE)`,
`check(GROUP)`, `check(HAVING)`, ...) — not the full clause shape —
then unconditionally calls the specialist parser once it commits. This
means a malformed clause (e.g. `GROUP` with no `BY`) throws a precise
error from inside the specialist function itself (`expect(BY)` failing
with "expected BY"), rather than a confusing downstream error from
whatever token comes next. `HAVING` is the one exception that needs an
*additional* check beyond the keyword lookahead — see the `HAVING`
section above — and its check must run *before* `ORDER BY`'s, matching
the grammar's clause order.

### REPL

An interactive loop for testing arbitrary queries without recompiling
per test case. It is **multi-line**: input accumulates in a buffer until
a line contains `;`, so long queries can be typed or pasted naturally.
The prompt is `SQL> ` for a fresh statement and `...> ` while buffering.

```cpp
std::string buffer;
while (true) {
    std::cout << (buffer.empty() ? "SQL> " : "...> ");
    std::string line;
    std::getline(std::cin, line);

    if (buffer.empty() && (line == "exit" || line == "quit")) break;

    buffer += line + " ";
    if (line.find(';') == std::string::npos) continue;   // keep buffering

    try {
        auto tokens = tokenize(buffer);
        size_t position = 0;
        auto root = parseStatement(tokens, position);
        printAST(*root);
    } catch (const std::runtime_error& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
    buffer.clear();
}
```

- `exit`/`quit` are honored only on an empty buffer, so they can't
  swallow a half-typed query.
- The `try`/`catch` is scoped around the per-query work, so a malformed
  query prints its error, clears the buffer and returns to the prompt.
- **Known limitation:** a *missing* semicolon cannot be tested here, since
  the REPL waits for `;` by design. That case needs a direct unit test.

---

### Status

**Done:**
- [x] `TokenType`, `Token`, `keywordTable`
- [x] `tokenize()` — whitespace, keywords, identifiers (incl. qualified
      `table.column` and qualified-star `table.*` forms), numbers (incl.
      decimals), string literals, comparison operators, punctuation
      (incl. `(`, `)`, `*`)
- [x] `ASTTag`, `ASTNode`, `printAST()`
- [x] `check()` / `expect()` / `parseError()` helpers
- [x] `parseStatement()` dispatcher (SELECT, INSERT, UPDATE, DELETE, DROP;
      CREATE/ALTER fail cleanly)
- [x] `parseSelectStatement()` — full clause set: `DISTINCT`, projection
      list, `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`
- [x] `parseColumnList()` / `parseColumn(allow_alias)`
- [x] `parseFromClause()` / `parseRelation()` — comma-separated
      relations, table aliases
- [x] `parseJoinClause()` — `LEFT JOIN` / `RIGHT JOIN` / plain `JOIN`,
      multi-join chains, compound `AND`/`OR` conditions in `ON`
- [x] `parseWhereClause()` / `parseOrExpr()` / `parseAndExpr()` /
      `parseComparison()` / `parseOperand()` — accumulator loops, correct
      `AND`/`OR` precedence and unlimited chains, shared by `HAVING` and `ON`
- [x] `parseGroupByClause()`
- [x] `parseHavingClause()` — with `GROUP BY`-required validation
- [x] `parseOrderByClause()` / `parseOrderItem()`
- [x] `parseLimitClause()`
- [x] Aliases (`AS` and no-`AS`), attached by callers; dotted aliases rejected
- [x] Interactive multi-line REPL
- [x] `parseInsertStatement()` / `parseValueTuple()` / `parseLiteral()`
- [x] `parseProjectionList()` / `parseFunctionCall()` / `parseArgsList()`
      / `parseArg()`
- [x] `SELECT *` and `table.*` as `STAR`; literals in the select list
- [x] Qualified columns as `COLUMN` + `QUALIFIER` child
      (`parseQualifier(allow_star)`)
- [x] Parenthesized expressions
- [x] `parseDeleteStatement()`, `parseUpdateStatement()` /
      `parseAssignment()`
- [x] `parseDropStatement()` / `parseIndex()` — `DROP TABLE`, `DROP INDEX
      a, b ON t`, `IF EXISTS`
- [x] All syntax errors through `parseError` (one `runtime_error` in the
      parser)

**Not yet done:**
- [ ] `CREATE TABLE` (largest: type keywords, constraints; unblocks the
      catalog), `CREATE INDEX`, `ALTER`, `DROP` of other objects
- [ ] Expression gaps: zero-argument calls (`NOW()`), comparators beyond
      the five (`!=`, `<>`), `NOT`, `NULL`/`IS NULL`, `IN`, `BETWEEN`,
      `LIKE`, arithmetic (`SET salary = salary + 1000`)
- [ ] `OFFSET`
- [ ] Alias on a literal in the select list (`SELECT 1 AS one`)
- [ ] Schema qualifier on relation names (open decision above)
- [ ] Duplicated projection dispatch (before and inside the comma loop) —
      extract `parseProjectionItem`
- [ ] Restrict `DELETE` to a single relation (currently a `relation`
      via `parseFromClause`)
- [ ] `DROP IF` check on `IF` alone, then `expect(EXISTS)`, for a better message
- [ ] Trailing-token check after the terminating `;`
- [ ] Automated regression tests (currently REPL-verified by hand)
- [ ] Top-level `parseSQL(sql)` wrapper + error-handling contract
      (currently: caller runs `tokenize` then `parseStatement`
      separately, and exceptions propagate raw)
- [ ] Semantic analysis (binder/catalog) — separate stage; its rules are
      collected in the sections above (qualifier resolution, `EQUALS`
      under `SET`, `TABLE` meaning under `DROP INDEX`, relation names)

---

### Reference test queries

**Verified end-to-end (Query 1 below):**
```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: name
└─> COLUMN: salary
```
**Verified end-to-end (Query 2 below):**
```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: name
├─> COLUMN: salary
└─> WHERE
    └─> GREATER
        ├─> COLUMN: salary
        └─> LITERAL: 5000
```

**Verified end-to-end (Query 3 below):**
```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: name
├─> COLUMN: salary
└─> WHERE
    └─> AND
        ├─> GREATER EQUAL
        │   ├─> COLUMN: age
        │   └─> LITERAL: 30
        └─> EQUALS
            ├─> COLUMN: department
            └─> LITERAL: IT
```

**Verified end-to-end (Query 4 below):**
```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: name
├─> COLUMN: salary
└─> WHERE
    └─> OR
        ├─> SMALLER
        │   ├─> COLUMN: salary
        │   └─> LITERAL: 3000
        └─> GREATER EQUAL
            ├─> COLUMN: salary
            └─> LITERAL: 10000
```
**Verified end-to-end (Query 5 below):**
```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> GREATER EQUAL
│       ├─> COLUMN: salary
│       └─> LITERAL: 5000
└─> GROUP BY
    └─> COLUMN: department
```
**Verified end-to-end (Query 6 below):**
```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> GREATER EQUAL
│       ├─> COLUMN: salary
│       └─> LITERAL: 5000
└─> GROUP BY
    ├─> COLUMN: department
    └─> COLUMN: salary
```

**Verified end-to-end (Query 7 below):**

```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> GREATER EQUAL
│       ├─> COLUMN: salary
│       └─> LITERAL: 5000
├─> GROUP BY
│   ├─> COLUMN: department
│   └─> COLUMN: salary
└─> LIMIT
    └─> LITERAL: 10
```

**Verified end-to-end (Query 8 below):**
```
Select
  ├─> FROM
  │   └─> TABLE: employees
  ├─> COLUMN: department
  ├─> COLUMN: salary
  ├─> WHERE
  │   └─> GREATER_EQUAL
  │       ├─> COLUMN: salary
  │       └─> LITERAL: 5000
  ├─> GROUP BY
  │   ├─> COLUMN: department
  │   └─> COLUMN: salary
  ├─> ORDER BY
  │   ├─> ORDER_ITEM
  │   │   ├─> COLUMN: salary
  │   │   └─> DESC
  │   └─> ORDER_ITEM
  │       ├─> COLUMN: department
  │       └─> DESC
  └─> LIMIT
      └─> LITERAL: 10
```
**Verified end-to-end (Query 9 below):**
```  
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> GREATER EQUAL
│       ├─> COLUMN: salary
│       └─> LITERAL: 5000
├─> GROUP BY
│   ├─> COLUMN: department
│   └─> COLUMN: salary
├─> ORDER BY
│   ├─> ORDER_ITEM
│   │   ├─> COLUMN: salary
│   │   └─> ASC
│   └─> ORDER_ITEM
│       ├─> COLUMN: department
│       └─> ASC
└─> LIMIT
    └─> LITERAL: 10
```

**Verified end-to-end (Query 10 below):**

```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> GREATER EQUAL
│       ├─> COLUMN: salary
│       └─> LITERAL: 5000
├─> GROUP BY
│   ├─> COLUMN: department
│   └─> COLUMN: salary
├─> ORDER BY
│   ├─> ORDER_ITEM
│   │   ├─> COLUMN: salary
│   │   └─> ASC
│   └─> ORDER_ITEM
│       ├─> COLUMN: department
│       └─> DESC
└─> LIMIT
    └─> LITERAL: 100
```

**Verified end-to-end (Query 11 below):**
```
Select
├─> FROM
│   ├─> TABLE: employees
│   └─> LEFT_JOIN
│       ├─> TABLE: manager
│       └─> EQUALS
│           ├─> COLUMN: id
│           │   └─> QUALIFIER: employee
│           └─> COLUMN: id
│               └─> QUALIFIER: manager
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> GREATER EQUAL
│       ├─> COLUMN: salary
│       └─> LITERAL: 5000
├─> GROUP BY
│   ├─> COLUMN: department
│   └─> COLUMN: salary
├─> ORDER BY
│   ├─> ORDER_ITEM
│   │   ├─> COLUMN: salary
│   │   └─> ASC
│   └─> ORDER_ITEM
│       ├─> COLUMN: department
│       └─> DESC
└─> LIMIT
    └─> LITERAL: 100
```
**Verified end-to-end (Query 12 below):**
```
Select
├─> FROM
│   ├─> TABLE: employees
│   │   └─> ALIAS: emp
│   └─> LEFT_JOIN
│       ├─> TABLE: manager
│       │   └─> ALIAS: mang
│       └─> EQUALS
│           ├─> COLUMN: id
│           │   └─> QUALIFIER: emp
│           └─> COLUMN: id
│               └─> QUALIFIER: mang
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> GREATER EQUAL
│       ├─> COLUMN: salary
│       └─> LITERAL: 5000
├─> GROUP BY
│   ├─> COLUMN: department
│   └─> COLUMN: salary
├─> ORDER BY
│   ├─> ORDER_ITEM
│   │   ├─> COLUMN: salary
│   │   └─> ASC
│   └─> ORDER_ITEM
│       ├─> COLUMN: department
│       └─> DESC
└─> LIMIT
    └─> LITERAL: 100
```
**Verified end-to-end (Query 13 below):**
```
Select
├─> FROM
│   ├─> TABLE: employees
│   └─> LEFT_JOIN
│       ├─> TABLE: departments
│       └─> EQUALS
│           ├─> COLUMN: dept_id
│           │   └─> QUALIFIER: employees
│           └─> COLUMN: id
│               └─> QUALIFIER: departments
├─> COLUMN: department
├─> COLUMN: salary
├─> WHERE
│   └─> OR
│       ├─> SMALLER
│       │   ├─> COLUMN: salary
│       │   └─> LITERAL: 3000.50
│       └─> GREATER EQUAL
│           ├─> COLUMN: salary
│           └─> LITERAL: 10000.40
└─> LIMIT
    └─> LITERAL: 10
```
**Verified end-to-end (Query 14 below):**

```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> COLUMN: salary
├─> GROUP BY
│   └─> COLUMN: department
└─> HAVING
    └─> GREATER
        ├─> COLUMN: salary
        └─> LITERAL: 5000
```

**Verified end-to-end (Query 15 below):**
```
SQL> SELECT department, salary FROM employees HAVING salary > 5000;
Error: HAVING clause cannot exist without GROUP BY
```

**Verified end-to-end (Query 16 below):**
```
Insert
├─> TABLE: EMPLOYEE
├─> COLUMN: NAME
├─> COLUMN: AGE
├─> COLUMN: DEPARTMENT
├─> COLUMN: DESIGNATION
├─> COLUMN: SALARY
└─> VALUE_TUPLE
    ├─> LITERAL: SOMYA
    ├─> LITERAL: 29
    ├─> LITERAL: CSE
    ├─> LITERAL: DB ENGINEER
    └─> LITERAL: 100000
```

**Verified end-to-end (Query 17 below):**
```
Insert
├─> TABLE: EMPLOYEE
├─> COLUMN: NAME
├─> COLUMN: AGE
├─> VALUE_TUPLE
│   ├─> LITERAL: SOMYA
│   └─> LITERAL: 29
├─> VALUE_TUPLE
│   ├─> LITERAL: RAHUL
│   └─> LITERAL: 31
└─> VALUE_TUPLE
    ├─> LITERAL: PRIYA
    └─> LITERAL: 27
```

**Verified end-to-end (Query 18 below):**
```
Insert
├─> TABLE: EMPLOYEE
├─> VALUE_TUPLE
│   ├─> LITERAL: SOMYA
│   └─> LITERAL: 29
├─> VALUE_TUPLE
│   ├─> LITERAL: RAHUL
│   └─> LITERAL: 31
└─> VALUE_TUPLE
    ├─> LITERAL: PRIYA
    └─> LITERAL: 27
```

**Verified end-to-end (Query 19 below):**
```
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> GROUP BY
│   └─> COLUMN: department
└─> HAVING
    └─> GREATER
        ├─> FUNCTION: SUM
        │   └─> ARG_LIST
        │       └─> COLUMN: salary
        └─> LITERAL: 5000
```

**Verified end-to-end (Query 20 below) — alias-on-operand rejection:**
```
SQL> SELECT department FROM employees GROUP BY department HAVING SUM(salary) AS s > 5000;
Error: Syntax error: at or near "AS", <comparator expected>
```
(Originally printed `Expected a comparator at position 12`. The error now
goes through `parseError` and is reported at `"AS"`; re-run Query 20 to
refresh the exact wording.)

**Verified end-to-end (Query 21 below) — function calls in the projection list:**
```
SQL> SELECT department, COUNT(*), SUM(salary) FROM employees
     GROUP BY department HAVING department = 'CSE' ORDER BY department DESC;
Select
├─> FROM
│   └─> TABLE: employees
├─> COLUMN: department
├─> FUNCTION: COUNT
├─> FUNCTION: SUM
│   └─> ARG_LIST
│       └─> COLUMN: salary
├─> GROUP BY
│   └─> COLUMN: department
├─> HAVING
│   └─> EQUALS
│       ├─> COLUMN: department
│       └─> LITERAL: CSE
└─> ORDER BY
    └─> ORDER_ITEM
        ├─> COLUMN: department
        └─> DESC
```

**Verified end-to-end (Query 22 below) — qualified star:**
```
SQL> select emp.* from employee emp;
Select
├─> FROM
│   └─> TABLE: employee
│       └─> ALIAS: emp
└─> STAR
    └─> QUALIFIER: emp
```

**Verified end-to-end (Query 23 below) — qualified star mixed with qualified columns, across a join:**
```
SQL> SELECT emp.department, emp.salary, mang.* FROM employees AS emp LEFT JOIN manager AS mang ON emp.id = mang.id;
Select
├─> FROM
│   ├─> TABLE: employees
│   │   └─> ALIAS: emp
│   └─> LEFT_JOIN
│       ├─> TABLE: manager
│       │   └─> ALIAS: mang
│       └─> EQUALS
│           ├─> COLUMN: id
│           │   └─> QUALIFIER: emp
│           └─> COLUMN: id
│               └─> QUALIFIER: mang
├─> COLUMN: department
│   └─> QUALIFIER: emp
├─> COLUMN: salary
│   └─> QUALIFIER: emp
└─> STAR
    └─> QUALIFIER: mang
```

**Verified end-to-end (Query 24 below) — bare star mid-list, after a comma:**
```
SQL> select id, * from employee;
Select
├─> FROM
│   └─> TABLE: employee
├─> COLUMN: id
└─> STAR
```

**Verified end-to-end (Query 25 below) — unqualified column next to a qualified one:**
```
SQL> select name, emp.dept from employees emp;
Select
├─> FROM
│   └─> TABLE: employees
│       └─> ALIAS: emp
├─> COLUMN: name
└─> COLUMN: dept
    └─> QUALIFIER: emp
```

**Verified end-to-end (Query 26 below) — compound `AND` in `ON`:**
```
SQL> SELECT emp.name, mang.name FROM employees emp LEFT JOIN manager mang ON emp.dept_id = mang.dept_id AND emp.location = mang.location;
Select
├─> FROM
│   ├─> TABLE: employees
│   │   └─> ALIAS: emp
│   └─> LEFT_JOIN
│       ├─> TABLE: manager
│       │   └─> ALIAS: mang
│       └─> AND
│           ├─> EQUALS
│           │   ├─> COLUMN: dept_id
│           │   │   └─> QUALIFIER: emp
│           │   └─> COLUMN: dept_id
│           │       └─> QUALIFIER: mang
│           └─> EQUALS
│               ├─> COLUMN: location
│               │   └─> QUALIFIER: emp
│               └─> COLUMN: location
│                   └─> QUALIFIER: mang
├─> COLUMN: name
│   └─> QUALIFIER: emp
└─> COLUMN: name
    └─> QUALIFIER: mang
```

**Verified end-to-end (Query 27 below) — mixed `OR`/`AND` in `ON`, same precedence as `WHERE`:**
```
SQL> SELECT emp.name, mang.name FROM employees emp LEFT JOIN manager mang ON emp.id = mang.id OR emp.backup_manager_id = mang.id AND emp.active = 1;
Select
├─> FROM
│   ├─> TABLE: employees
│   │   └─> ALIAS: emp
│   └─> LEFT_JOIN
│       ├─> TABLE: manager
│       │   └─> ALIAS: mang
│       └─> OR
│           ├─> EQUALS
│           │   ├─> COLUMN: id
│           │   │   └─> QUALIFIER: emp
│           │   └─> COLUMN: id
│           │       └─> QUALIFIER: mang
│           └─> AND
│               ├─> EQUALS
│               │   ├─> COLUMN: backup_manager_id
│               │   │   └─> QUALIFIER: emp
│               │   └─> COLUMN: id
│               │       └─> QUALIFIER: mang
│               └─> EQUALS
│                   ├─> COLUMN: active
│                   │   └─> QUALIFIER: emp
│                   └─> LITERAL: 1
├─> COLUMN: name
│   └─> QUALIFIER: emp
└─> COLUMN: name
    └─> QUALIFIER: mang
```

**Verified end-to-end (Query 38 below) — ORDER BY, identifier then position:**
```
SQL> SELECT a FROM t ORDER BY name, 2;
Select
├─> FROM
│   └─> TABLE: t
├─> COLUMN: a
└─> ORDER BY
    ├─> ORDER_ITEM
    │   ├─> COLUMN: name
    │   └─> ASC
    └─> ORDER_ITEM
        ├─> POSITION: 2
        └─> ASC
```

**Verified end-to-end (Query 39 below) — INSERT, multi-row with explicit columns:**
```
SQL> INSERT INTO t (a, b) VALUES (1, 'x'), (2, 'y');
Insert
├─> TABLE: t
├─> COLUMN: a
├─> COLUMN: b
├─> VALUE_TUPLE
│   ├─> LITERAL: 1
│   └─> LITERAL: x
└─> VALUE_TUPLE
    ├─> LITERAL: 2
    └─> LITERAL: y
```

**Verified end-to-end (Query 40 below) — alias in the select list, aggregate with GROUP BY:**
```
SQL> SELECT a x FROM t;
Select
├─> FROM
│   └─> TABLE: t
└─> COLUMN: a
    └─> ALIAS: x

SQL> SELECT SUM(a) FROM t GROUP BY a;
Select
├─> FROM
│   └─> TABLE: t
├─> FUNCTION: SUM
│   └─> ARG_LIST
│       └─> COLUMN: a
└─> GROUP BY
    └─> COLUMN: a
```

**Verified end-to-end (Query 41 below) — error routing and alias/name rules:**
```
SQL> INSERT INTO t (a x) VALUES (1);
Error: Syntax error: at or near "x", expected ')'
SQL> INSERT INTO t (a AS x) VALUES (1);
Error: Syntax error: at or near "AS", expected ')'
SQL> SELECT a FROM t GROUP BY a x;
Error: Syntax error: at or near "x", expected ';'
SQL> SELECT a FROM t ORDER BY name, ;
Error: Syntax error: at or near ";", expected column or position after ','
SQL> DROP INDEX idx.name ON t;
Error: Syntax error: at or near "idx.name", index name cannot contain '.'
SQL> DROP INDEX emp.* ON t;
Error: Syntax error: at or near "emp.*", index name cannot contain '.'
SQL> DROP INDEX idx;
Error: Syntax error: at or near ";", expected ON
SQL> DROP INDEX ;
Error: Syntax error: at or near ";", expected an identifier
```
(`DROP INDEX idx.name ON t` and `DROP INDEX emp.* ON t` above are the
`parseIndex` checks; the `idx;` and `;` cases confirm the doubled prefix
is gone.)

Each query below is used as the target for one stage of the parser
build-out, in increasing order of grammar coverage — from a bare
`SELECT`/`FROM` up through joins, aliases, `INSERT`, function calls,
`SELECT *`, parenthesized conditions, `DELETE` and `DROP`.

```sql
-- Query 1
SELECT name, salary FROM employees;

-- Query 2 (Where)
SELECT name, salary FROM employees WHERE salary > 5000;

-- Query 3 (Multi condition Where AND)
SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';

-- Query 4 (Multi condition Where OR)
SELECT name, salary FROM employees WHERE salary < 3000 OR salary >= 10000;

-- Query 5 (GROUP BY)
SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department;

-- Query 6 (GROUP BY Multi Column)
SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary;

-- Query 7 (LIMIT)
SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary LIMIT 10;

-- Query 8 (ORDER BY DESC)
SELECT department, salary FROM employees WHERE salary >= 5000
  GROUP BY department, salary ORDER BY salary DESC, department DESC LIMIT 10;

-- Query 9 (ORDER BY Bydefault ASC)
SELECT department, salary FROM employees WHERE salary >= 5000
  GROUP BY department, salary ORDER BY salary, department LIMIT 10;

-- Query 10 (ORDER BY Mixed Order By deirection)
SELECT department, salary FROM employees WHERE salary >= 5000
  GROUP BY department, salary ORDER BY salary ASC, department DESC LIMIT 100;

-- Query 11 (JOIN)
SELECT department, salary FROM employees LEFT JOIN manager ON employee.id = manager.id
  WHERE salary >= 5000 GROUP BY department, salary ORDER BY salary ASC, department DESC LIMIT 100;

-- Query 12 (JOIN + alias)
SELECT department, salary FROM employees AS emp LEFT JOIN manager AS mang ON emp.id = mang.id
  WHERE salary >= 5000 GROUP BY department, salary ORDER BY salary ASC, department DESC LIMIT 100;

-- Query 13 (decimal literals + JOIN, qualified identifiers)
SELECT department, salary FROM employees
  LEFT JOIN departments ON employees.dept_id = departments.id
  WHERE salary < 3000.50 OR salary >= 10000.40
  LIMIT 10;

-- Query 14 (HAVING)
SELECT department, salary FROM employees GROUP BY department HAVING salary > 5000;

-- Query 15 (HAVING error case — must throw)
SELECT department, salary FROM employees HAVING salary > 5000;
-- expected: "HAVING clause cannot exist without GROUP BY"

-- Query 16 (INSERT, explicit column list)
INSERT INTO EMPLOYEE (NAME, AGE, DEPARTMENT, DESIGNATION, SALARY) VALUES('SOMYA', 29, 'CSE', 'DB ENGINEER', 100000);

-- Query 17 (INSERT, multi-row VALUES)
INSERT INTO EMPLOYEE (NAME, AGE) VALUES ('SOMYA', 29), ('RAHUL', 31), ('PRIYA', 27);

-- Query 18 (INSERT, no column list — schema-implied form)
INSERT INTO EMPLOYEE VALUES ('SOMYA', 29), ('RAHUL', 31), ('PRIYA', 27);

-- Query 19 (aggregate as a HAVING operand)
SELECT department FROM employees GROUP BY department HAVING SUM(salary) > 5000;

-- Query 20 (alias-on-operand error case — must throw)
SELECT department FROM employees GROUP BY department HAVING SUM(salary) AS s > 5000;
-- expected: error at "AS" (comparator expected)

-- Query 21 (function calls in the projection list, combined with GROUP BY/HAVING/ORDER BY)
SELECT department, COUNT(*), SUM(salary) FROM employees
  GROUP BY department HAVING department = 'CSE' ORDER BY department DESC;

-- Query 22 (qualified star)
select emp.* from employee emp;

-- Query 23 (qualified star mixed with qualified columns, across a join)
SELECT emp.department, emp.salary, mang.* FROM employees AS emp LEFT JOIN manager AS mang ON emp.id = mang.id;

-- Query 24 (bare star mid-list, after a comma)
select id, * from employee;

-- Query 25 (unqualified column next to a qualified one)
select name, emp.dept from employees emp;

-- Query 26 (compound AND in ON)
SELECT emp.name, mang.name FROM employees emp LEFT JOIN manager mang
  ON emp.dept_id = mang.dept_id AND emp.location = mang.location;

-- Query 27 (mixed OR/AND in ON, same precedence as WHERE)
SELECT emp.name, mang.name FROM employees emp LEFT JOIN manager mang
  ON emp.id = mang.id OR emp.backup_manager_id = mang.id AND emp.active = 1;

-- Query 28 (parenthesized ON — grouping changes the tree)
SELECT emp.name, mang.name FROM employees emp LEFT JOIN manager mang
  ON (emp.id = mang.id OR emp.backup_manager_id = mang.id) AND emp.active = 1;

-- Query 29 (parentheses in WHERE, nested and redundant)
SELECT name FROM employees WHERE ((salary > 5000) AND (department = 'CSE'));

-- Query 30 (DELETE with WHERE)
DELETE FROM employees WHERE id = 5;

-- Query 31 (DELETE with alias, no WHERE)
DELETE FROM employees e;

-- Query 32 (DROP TABLE)
DROP TABLE Employee;

-- Query 33 (error cases — must throw)
DROP TABLE Employee e;        -- at or near "e", expected ';'
DROP Employee;                -- at or near "Employee", expected TABLE or INDEX
DELETE employees WHERE id = 5; -- at or near "employees", expected FROM

-- Query 34 (DROP INDEX, multiple indexes)
DROP INDEX a, b ON t;

-- Query 35 (DROP INDEX, single index)
DROP INDEX EMP_ID ON EMPLOYEE;

-- Query 36 (DROP ... IF EXISTS)
DROP TABLE IF EXISTS t;
DROP INDEX IF EXISTS a, b ON t;

-- Query 37 (DROP error cases — must throw)
DROP INDEX EMP_ID ON;            -- at or near ";", expected an identifier
DROP INDEX ON EMPLOYEE;          -- at or near "ON", expected an identifier
DROP INDEX EMP_ID ON EMPLOYEE e; -- at or near "e", expected ';'
DROP TABLE IF t;                 -- at or near "IF", expected an identifier
DROP TABLE IF EXISTS;            -- at or near ";", expected an identifier
DROP;                            -- at or near ";", expected TABLE or INDEX
-- Query 38 (ORDER BY, identifier then position)
SELECT a FROM t ORDER BY name, 2;

-- Query 39 (INSERT, multi-row with explicit columns)
INSERT INTO t (a, b) VALUES (1, 'x'), (2, 'y');

-- Query 40 (select-list alias, aggregate + GROUP BY)
SELECT a x FROM t;
SELECT SUM(a) FROM t GROUP BY a;

-- Query 41 (error routing — must throw, all "Syntax error: ...")
INSERT INTO t (a x) VALUES (1);       -- at or near "x", expected ')'
INSERT INTO t (a AS x) VALUES (1);    -- at or near "AS", expected ')'
SELECT a FROM t GROUP BY a x;         -- at or near "x", expected ';'
SELECT a FROM t ORDER BY name, ;      -- at or near ";", expected column or position after ','
DROP INDEX idx.name ON t;             -- at or near "idx.name", index name cannot contain '.'
DROP INDEX emp.* ON t;                -- at or near "emp.*", index name cannot contain '.'
DROP INDEX idx;                       -- at or near ";", expected ON
DROP INDEX ;                          -- at or near ";", expected an identifier
INSERT INTO t VALUES (a);             -- at or near "a", expected a number or string
INSERT INTO t VALUES (1, );           -- at or near ")", expected a number or string
CREATE TABLE t (a);                   -- at or near "CREATE", CREATE is not supported yet
SELECT a FROM employees e.salary;     -- at or near "e.salary", alias cannot contain '.'
SELECT a FROM emp.*;                  -- at or near "emp.*", relation name cannot contain '*'
DELETE FROM t WHERE id = 1 foo;       -- at or near "foo", expected ';'

-- Query 42 (UPDATE — tree in the UPDATE section)
UPDATE employees SET salary = 6000, dept = 'IT' WHERE id = 5;

-- Query 43 (qualified column — tree in the qualifier section)
SELECT emp.name FROM employees emp WHERE emp.id = 5;

-- Query 44 (DISTINCT, literal in the select list)
SELECT DISTINCT name FROM employees;
SELECT name, 1 FROM employees;
```

Full pipeline (`tokenize` → `parseStatement` → `printAST`) confirmed for
every query above via `tests/parse_test.cpp` and the REPL. The
hand-built-tree test used to verify `printAST` in isolation, before the
parser existed, lives separately in `tests/ast_test.cpp`.