# Fermat Reference

## Keywords (14 total)

| Keyword | Description | Example |
|---------|-------------|---------|
| `def` | Define a function | `def add(x y) x + y` |
| `extern` | Declare external function | `extern sin(x)` |
| `let` | Declare immutable variable | `let x = 5` |
| `mut` | Make variable mutable | `let mut x = 5` |
| `if/then/else` | Conditional expression | `if x < 5 then 1 else 0` |
| `for/do/end` | For loop | `for i = 0, 10, 1 do i end` |
| `while/do/end` | While loop | `while x < 10 do x = x + 1 end` |
| `import` | Import a module | `import "lib/math.spy"` |
| `export` | Export a function | `export def square(x) x * x` |

## Operators

| Operator | Description | Precedence |
|----------|-------------|------------|
| `<` | Less than | 10 |
| `>` | Greater than | 10 |
| `+` | Addition | 20 |
| `-` | Subtraction | 20 |
| `*` | Multiplication | 40 |
| `/` | Division | 40 |
| `=` | Assignment | N/A |

## Module System

```spy
# lib/math.spy - Library file
export def square(x) x * x
export def cube(x) x * x * x

# main.spy - Uses the library
import "lib/math.spy"
square(5)   # → 25
cube(3)     # → 27
```

## Syntax Examples

```spy
# Functions
def add(a b) a + b

# Variables
let x = 10
let mut counter = 0
counter = counter + 1

# Conditionals
if x < 10 then x * 2 else x

# Loops
for i = 0, 10, 1 do i * 2 end
while counter < 10 do counter = counter + 1 end
```
