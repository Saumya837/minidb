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

A future `parseSQL(sql)` function will wrap both stages into one public
entry point, so callers never need to know lexing and parsing are separate.

---

## AST design

### Tag types

The AST node "kind" is represented as one of five closed enums, combined
into a `std::variant`:

```cpp
enum class StatementType  { SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP };
enum class Clauses        { FROM, WHERE, GROUP_BY, ORDER_BY, LIMIT };
enum class Relations      { TABLE, JOIN };
enum class ExpressionType { EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL, AND, OR };
enum class ValueType      { COLUMN, LITERAL };

using ASTTag = std::variant<StatementType, Clauses, Relations, ExpressionType, ValueType>;
```

We deliberately use a **flat variant of closed enums**, not a generic
`template<EnumType T>` — the AST only ever produces nodes of these five
kinds, so the type should reflect that closed set rather than accepting
any enum.

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

### Tree shape (reference examples)

For `SELECT name, salary FROM employees WHERE salary > 5000;`:

```
Select
├─> From
│   └─> Table: employees
├─> Column: name
├─> Column: salary
└─> Where
    └─> Greater
        ├─> Column: salary
        └─> Literal: 5000
```

`AND`/`OR` nest the same way — the operator is the parent, its two operands
(which may themselves be comparisons or nested `AND`/`OR`) are children.

### Tree printer

`printAST(root)` walks the tree recursively and renders it with
`├─>` / `└─>` branch characters, matching the shape above. Built and
verified against a hand-constructed tree before the parser existed, so
printer bugs and parser bugs could be isolated from each other.

---

## Lexer

### Token design

```cpp
enum class TokenType {
    // keywords
    SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP,
    FROM, WHERE, GROUP, BY, ORDER, LIMIT, AND, OR, TABLE, JOIN,
    // identifiers & literals
    IDENTIFIER, NUMBER, STRING,
    // operators
    EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL,
    // punctuation
    COMMA, SEMICOLON,
    // control
    END_OF_INPUT, UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
};
```

**Design decision — flat enum, not categorized.** We considered nesting
`TokenType` under categories (`KEYWORD`, `OPERATOR`, `PUNCTUATION`) via a
`variant` or a category field, but rejected it: the parser never needs to
ask "is this *any* keyword" — every parse decision branches on the exact
token (`SELECT` vs `FROM` vs `WHERE`), never the category. A stored
category with no behavioral use is pure overhead. This matches how
production lexers (PostgreSQL, SQLite, Python's `tokenize`, Rust's
`rustc_lexer`) are built — one flat enum, category groupings expressed
only as comments or on-demand helper functions if ever needed.

**`GROUP` and `BY` (and `ORDER`/`BY`) are separate tokens.** The lexer
scans one word at a time and has no multi-word lookahead, so `GROUP BY`
arrives as two tokens. The *parser* is responsible for expecting them
back-to-back as one clause.

**Operators are not in the keyword table.** `>`, `<`, `>=`, `<=`, `=` are
matched by character in the tokenizer's own branches, not via string
lookup — `keywordTable` only holds actual keyword words.

### `tokenize()` behavior

- Skips whitespace
- Digits → accumulate into a `NUMBER` token
- Letter/underscore → accumulate into a word, uppercase it, look it up in
  `keywordTable`; matches become that keyword's `TokenType`, otherwise
  `IDENTIFIER`
- `'...'` → `STRING` token, quotes stripped from the stored lexeme
- `>`/`<` → peek the next character; if `=` follows, consume both and emit
  `GREATER_EQUAL`/`LESSER_EQUAL`, otherwise emit `GREATER`/`SMALLER`
- `=`, `,`, `;` → single-character punctuation/operator tokens
- Anything unrecognized → throws `std::runtime_error`
- Always appends a trailing `END_OF_INPUT` token

**Verified against:**
```sql
SELECT name, salary FROM employees WHERE salary > 5000;
SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';
```
Confirmed: multi-char operators collapse correctly, string literals have
quotes stripped, keywords resolve to their specific `TokenType` (not
`IDENTIFIER`).

### File layout (lexer)

| File | Contents |
|---|---|
| `token.hpp` | `TokenType`, `Token`, `keywordTable` — pure data, no behavior |
| `lexer.hpp` / `lexer.cpp` | Declares/defines `tokenize()` |

(Naming note: originally called `token.cpp`, renamed since `tokenize()` is
lexer *behavior*, not token *data* — the two belong in separate files.)

---

## Parser

### Grammar

```
statement        := SELECT column_list from_clause where_clause? group_by_clause? limit_clause? SEMICOLON

column_list       := column (COMMA column)*
column            := IDENTIFIER

from_clause       := FROM relation joins*
joins             := (LEFT | RIGHT)? JOIN relation ON condition
relation          := IDENTIFIER

where_clause      := WHERE condition

condition         := or_expr
or_expr           := and_expr (OR and_expr)*
and_expr          := comparison (AND comparison)*
comparison        := operand comparator operand
comparator        := EQUALS | GREATER | SMALLER | GREATER_EQUAL | LESSER_EQUAL
operand           := IDENTIFIER | NUMBER | STRING

group_by_clause   := GROUP BY column_list
limit_clause      := LIMIT NUMBER
```

**Why `or_expr`/`and_expr` are layered instead of one self-referential
`condition` rule.** A naive grammar like
`condition := condition AND condition | condition OR condition | comparison`
is left-recursive, which recursive-descent parsing cannot handle (it would
call itself forever without consuming a token). Layering `or_expr` on top
of `and_expr` avoids left recursion *and* encodes precedence: `AND` always
binds tighter than `OR`, since `and_expr` sits below `or_expr` in the
call chain — matching standard SQL semantics for `A OR B AND C` meaning
`A OR (B AND C)`.

**`JOIN` support is designed but not yet implemented.** None of the
current sample queries use joins; `LEFT`/`RIGHT` aren't yet in `TokenType`
or `Relations`. Planned as a dedicated future addition once the base
pipeline (no-join queries) is fully working.

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

`pos` is passed by reference so each function's consumption is visible to
its caller — standard recursive-descent threading. `tokens` stays `const&`
since parsing only ever reads the stream, never mutates it.

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

Rule of thumb: **mandatory next token → `expect()` directly. Optional or
undetermined next token → `check()` first, then act only if it matches.**

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
*which kind* of statement this is, based on the leading token, and routes
to the matching specialist parser. It does **not** validate that
everything after the leading keyword is well-formed — that's the job of
whichever `parseXStatement` it dispatches to. E.g. `parseStatement` won't
catch `SELECT FROM;` (missing columns) — `parseSelectStatement` /
`parseColumnList` will, when they expect an identifier and find `FROM`
instead.

Sub-parsers (`parseSelectStatement`, `parseInsertStatement`, etc.) are
currently stubs outside of the `SELECT` path, which is being built out
first.

### Status

- [x] `TokenType`, `Token`, `keywordTable`
- [x] `tokenize()` — verified against 2 sample queries
- [x] `ASTTag`, `ASTNode`, `printAST()` — verified against a hand-built tree
- [x] Full grammar (including `JOIN`, pending implementation)
- [x] `check()`, `expect()` helpers
- [x] `parseStatement()` dispatcher skeleton
- [x] `parseSelectStatement()` —  — SELECT column_list FROM relation SEMICOLON
- [x] `parseColumnList()`
- [x] `parseFromClause()` (no-join case first)
- [x] `parseWhereClause()` / `parseOrExpr()` / `parseAndExpr()` / `parseComparison()` / `parseOperand()`
- [x] `parseGroupByClause()` — GROUP BY column_list, reuses parseColumnList
- [ ] `parseLimitClause()`
- [ ] `JOIN` support in `parseFromClause`
- [ ] Top-level `parseSQL(sql)` wrapper + error-handling contract
- [ ] `parseInsertStatement`, `parseCreateStatement`, etc. (stubs only)

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
`parseGroupByClause` calls `parseColumnList()` directly rather than
duplicating comma-separated-identifier logic, since `GROUP BY department,
salary` is the same grammar shape as `SELECT`'s column list.

**Optional-clause lookahead pattern**, consistent across `WHERE` and
`GROUP BY`: `parseSelectStatement` only peeks at the clause's *opening*
keyword (`check(WHERE)`, `check(GROUP)`) — not the full clause shape —
then unconditionally calls the specialist parser once it commits. This
means a malformed clause (e.g. `GROUP` with no `BY`) throws a precise
error from inside the specialist function itself (`expect(BY)` failing
with "expected BY"), rather than a confusing downstream error from
whatever token comes next.

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

Full pipeline (tokenize → parseStatement → printAST) confirmed via
`tests/parse_test.cpp`. The hand-built-tree test used to verify
`printAST` in isolation now lives separately in `tests/ast_test.cpp`.


```sql
-- Query 1
SELECT name, salary FROM employees;

-- Query 2
SELECT name, salary FROM employees WHERE salary > 5000;

-- Query 3
SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';

-- Query 4
SELECT name, salary FROM employees WHERE salary < 3000 OR salary >= 10000;

-- Query 5
SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department;

-- Query 6
SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary;

-- Query 6
SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary limit 10;

Each is used as the target for one stage of the parser build-out (see
Status above), in increasing order of grammar coverage.
