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
enum class Clauses        { FROM, WHERE, GROUP_BY, HAVING, ORDER_BY, LIMIT };
enum class Relations      { TABLE, JOIN, LEFT_JOIN, RIGHT_JOIN };
enum class ExpressionType { EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL, AND, OR };
enum class ValueType      { COLUMN, LITERAL, POSITION, ALIAS };
enum class OrderDirection { ASC, DESC };
enum class InternalNode   { ORDER_ITEM };

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
(not because a keyword demands it) belongs in `InternalNode`.

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
        ├─> COLUMN: employees.dept_id
        └─> COLUMN: departments.id
```
Comma-separated relations (`FROM a, b`) need no such wrapper — they
become flat `TABLE` siblings under `FROM`, since there's no per-item
data to bundle, unlike a join's condition.

`ON`'s condition currently goes through `parseComparison` (a single
comparison only) rather than the full `parseOrExpr` chain — compound
`ON` conditions (`ON a.id = b.id AND a.active = true`) are not yet
supported. This is a known follow-up (see **Status**).

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
column   := IDENTIFIER (AS IDENTIFIER)?
relation := IDENTIFIER (AS IDENTIFIER)?
```
Both share the same alias-attachment pattern: parse the base
`IDENTIFIER`, check for `AS`; if present, consume it, parse the alias
`IDENTIFIER`, and attach it as an `ALIAS` child of the node just built.

`AS` is currently **mandatory** when an alias is present — the no-`AS`
form (`salary s`, `employees e`) is not yet supported (see **Status**).

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
        ├─> COLUMN: emp.dept_id
        └─> COLUMN: dept.id
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

**Scoped for now to plain operands** (`IDENTIFIER`/`NUMBER`/`STRING`) —
no aggregate functions (`COUNT(*)`, `SUM(x)`, etc.) yet. Supporting
those needs new lexer tokens (`LPAREN`/`RPAREN`/`STAR`/function names)
and a new `operand` alternative (`aggregate_call`), planned as its own
dedicated feature, the same way `JOIN` and `ALIAS` were each done as
separate passes rather than bundled into one change.

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
        throw std::runtime_error("HAVING clause cannot exist without GROUP BY");
    }
}
```

The naive version of this check — re-peeking the token stream for
`GROUP` at the point `HAVING` is being checked — doesn't work, because
`GROUP BY`'s tokens (if present) were already consumed earlier in the
same function call; `pos` no longer points at them. There is no way to
retroactively ask the token stream "did `GROUP BY` happen," so the flag
has to be captured live, at the moment it's true.

---

## Lexer

### Token design

```cpp
enum class TokenType {
    // keywords
    SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP,
    FROM, WHERE, GROUP, BY, ORDER, LIMIT, AND, OR, TABLE, JOIN,
    HAVING, AS, ON, LEFT, RIGHT,
    // identifiers & literals
    IDENTIFIER, NUMBER, STRING,
    // operators
    EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL,
    // punctuation
    COMMA, SEMICOLON,
    // control
    END_OF_INPUT, UNKNOWN,
    // ordering
    ASC, DESC
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

### `tokenize()` behavior

- Skips whitespace
- **Digits** → accumulate into a `NUMBER` token. After consuming all
  leading digits, if a `.` is followed by another digit (checked with
  lookahead), the `.` and the following digits are consumed too,
  producing a single decimal `NUMBER` token (`3000.50`). Without the
  lookahead, a trailing or dangling `.` (`5000.`, `5000.AND`) is left
  alone rather than guessed at.
- **Letter/underscore** → accumulate into a word, including `.` as a
  continuation character (not just letters/digits/underscore), so
  qualified identifiers (`employees.dept_id`) tokenize as a single
  `IDENTIFIER` rather than three tokens. Splitting a qualified name into
  its table/column parts is deferred to a later stage (semantic
  analysis), not the parser's concern. The word is uppercased and
  looked up in `keywordTable`; matches become that keyword's
  `TokenType`, otherwise `IDENTIFIER`.
- `'...'` → `STRING` token, quotes stripped from the stored lexeme
- `>`/`<` → peek the next character; if `=` follows, consume both and
  emit `GREATER_EQUAL`/`LESSER_EQUAL`, otherwise emit `GREATER`/`SMALLER`
- `=`, `,`, `;` → single-character punctuation/operator tokens
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
statement         := SELECT column_list from_clause where_clause? group_by_clause?
                      having_clause? order_by_clause? limit_clause? SEMICOLON

column_list       := column (COMMA column)*
column            := IDENTIFIER (AS IDENTIFIER)?

from_clause       := FROM relation (COMMA relation)* joins*
joins             := (LEFT | RIGHT)? JOIN relation ON condition
relation          := IDENTIFIER (AS IDENTIFIER)?

where_clause      := WHERE condition
having_clause     := HAVING condition   # requires a preceding group_by_clause

condition         := or_expr
or_expr           := and_expr (OR and_expr)*
and_expr          := comparison (AND comparison)*
comparison        := operand comparator operand
comparator        := EQUALS | GREATER | SMALLER | GREATER_EQUAL | LESSER_EQUAL
operand           := IDENTIFIER | NUMBER | STRING

group_by_clause   := GROUP BY column_list
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
reused, unmodified, by `HAVING` and (partially — see `JOIN` notes above)
by `ON`.

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
`const&` since parsing only ever reads the stream, never mutates it.

### Private helpers — `check` / `expect`

Declared only in `parser.cpp` (anonymous namespace), **not** in
`parser.hpp` — they're internal implementation details, not part of the
public API.

```cpp
bool check(const std::vector<Token>& tokens, size_t pos, TokenType type);
// Bounds-safe peek. No side effects, no consumption. Returns false (not
// an error) on mismatch or out-of-bounds. Used whenever the parser
// doesn't yet know what comes next and needs to decide which branch to
// take — e.g. "is there a WHERE clause, or not?"

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
    else if (check(tokens, pos, TokenType::CREATE))  return parseCreateStatement(tokens, pos);
    // ... UPDATE, DELETE, ALTER, DROP ...
    else throw std::runtime_error("Unknown statement type at position " + std::to_string(pos));
}
```

**What this function is and isn't responsible for:** it only answers
*which kind* of statement this is, based on the leading token, and
routes to the matching specialist parser. It does **not** validate that
everything after the leading keyword is well-formed — that's the job of
whichever `parseXStatement` it dispatches to. E.g. `parseStatement`
won't catch `SELECT FROM;` (missing columns) — `parseSelectStatement` /
`parseColumnList` will, when they expect an identifier and find `FROM`
instead.

Sub-parsers other than `parseSelectStatement` (`parseInsertStatement`,
`parseCreateStatement`, etc.) are still stubs.

`parseGroupByClause` calls `parseColumnList()` directly rather than
duplicating comma-separated-identifier logic, since `GROUP BY
department, salary` is the same grammar shape as `SELECT`'s column
list.

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
section above.

### REPL

An interactive loop for testing arbitrary queries without recompiling
per test case:

```cpp
std::string sql;
while (true) {
    std::cout << "SQL> ";
    std::getline(std::cin, sql);

    if (sql == "exit" || sql == "quit") break;

    try {
        std::vector<Token> tokens = tokenize(sql);
        size_t position = 0;
        auto root = parseStatement(tokens, position);
        printAST(*root);
    } catch (const std::runtime_error& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}
```

The `try`/`catch` is scoped tightly around just the per-query work, so
a malformed query prints its error and returns to the prompt rather
than aborting the whole session — one typo shouldn't force a restart.

---

### Status

**Done:**
- [x] `TokenType`, `Token`, `keywordTable`
- [x] `tokenize()` — whitespace, keywords, identifiers (incl. qualified
      `table.column` form), numbers (incl. decimals), string literals,
      comparison operators, punctuation
- [x] `ASTTag`, `ASTNode`, `printAST()`
- [x] `check()` / `expect()` helpers
- [x] `parseStatement()` dispatcher
- [x] `parseSelectStatement()` — full clause set: column list, `FROM`,
      `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`
- [x] `parseColumnList()` / `parseColumn()` (with alias support)
- [x] `parseFromClause()` / `parseRelation()` — comma-separated
      relations, table aliases
- [x] `parseJoinClause()` — `LEFT JOIN` / `RIGHT JOIN` / plain `JOIN`,
      multi-join chains
- [x] `parseWhereClause()` / `parseOrExpr()` / `parseAndExpr()` /
      `parseComparison()` / `parseOperand()` — correct `AND`/`OR`
      precedence, verified with mixed-precedence queries
- [x] `parseGroupByClause()`
- [x] `parseHavingClause()` — with `GROUP BY`-required validation
- [x] `parseOrderByClause()` / `parseOrderItem()` — `ASC`/`DESC`
      (defaulting to `ASC`), column and positional ordering
- [x] `parseLimitClause()`
- [x] Column and table aliasing (`AS`, mandatory)
- [x] Interactive REPL

**Not yet done:**
- [ ] Aggregate functions (`COUNT(*)`, `SUM(x)`, etc.) as operands —
      needed for `HAVING` to be semantically meaningful, and for
      aggregate expressions generally
- [ ] No-`AS` alias form (`salary s`, `employees e`)
- [ ] Compound `ON` conditions (`ON a.id = b.id AND a.active = true`) —
      currently single-comparison only
- [ ] `SELECT *`
- [ ] Non-`SELECT` statements (`INSERT`, `UPDATE`, `DELETE`, `CREATE`,
      `ALTER`, `DROP`) — dispatcher stubs only
- [ ] Top-level `parseSQL(sql)` wrapper + error-handling contract
      (currently: caller runs `tokenize` then `parseStatement`
      separately, and exceptions propagate raw)

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
│           ├─> COLUMN: employee.id
│           └─> COLUMN: manager.id
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
│           ├─> COLUMN: emp.id
│           └─> COLUMN: mang.id
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
│           ├─> COLUMN: employees.dept_id
│           └─> COLUMN: departments.id
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


Each query below is used as the target for one stage of the parser
build-out, in increasing order of grammar coverage — from a bare
`SELECT`/`FROM` up through joins, aliases, and every optional clause
combined.

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
```

Full pipeline (`tokenize` → `parseStatement` → `printAST`) confirmed for
every query above via `tests/parse_test.cpp` and the REPL. The
hand-built-tree test used to verify `printAST` in isolation, before the
parser existed, lives separately in `tests/ast_test.cpp`.
