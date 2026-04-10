@page statements_and_expressions Statements and expressions

# Introduction

TODO

## Constant expressions

A constant expression is one that can be evaluated at compile time. They can be assigned to a `var` or a `const` variable, but `const` variables require a constant expression.

```java
const value = 2 + 2;
const text = "Hello" + " world";
// const err; // Does not compile!
// const err = someGlobal * 2; // Does not compile!

print(value); // Prints "4"
print(text); // Prints "Hello world"
```

The compiler is able to determine whether a variable can be `const` if it's a constant expression that does not assigned to a global. For example:

```java
var item = "candy bar";
if (item == "candy bar") {
    const quantity = 10;
    const cost = 1.50;

    const text = "Total: ";
    print(text);

    var total = quantity * cost;
    print(total);
}
```

This compiles correctly, but there will be compiler warnings:

```
  WARNING: In top level code of file 'init.hsl':
    Variable 'quantity' can be const. (Declared on line 3)
    Variable 'cost' can be const. (Declared on line 4)
    Variable 'text' can be const. (Declared on line 6)
```

Changing the above variables to `const` makes the warning disappear, but now a different warning appears:

```
  WARNING: In top level code of file 'init.hsl':
    Variable 'total' can be const. (Declared on line 9)
```

Since the compiler can now determine `quantity * cost` is a constant expression, it started to warn about it. Changing `total` to be `const` makes all warnings disappear.

```java
var item = "candy bar";
if (item == "candy bar") {
    const quantity = 10;
    const cost = 1.50;

    const text = "Total: ";
    print(text);

    const total = quantity * cost;
    print(total); // Prints "15.000000"
}
```

Constant expressions can be *folded*, which means that the compiler can generate the result of the expression, rather than generate code that causes the expression to be computed at runtime. This makes your code take less time to execute. See the following script, and then the bytecode that the compiler generated for both functions:

```java
event returnFour() {
    const two = 2;
    const value = two + two;

    return value;
}

event returnHelloWorld() {
    const hello = "Hello";
    const world = " world";
    const text = hello + world;

    return text;
}
```

```
== returnFour (argCount: 0) ==
byte   ln
0000    7 OP_INTEGER                 '4'
0005    | OP_RETURN
0006    8 OP_NULL
0007    | OP_RETURN
```

```
== returnHelloWorld (argCount: 0) ==
byte   ln
0000   15 OP_CONSTANT              2 'Hello world'
0005    | OP_RETURN
0006   16 OP_NULL
0007    | OP_RETURN
```

Instead of generating code that executes `2 + 2` and `"Hello" + " world"`, the compiler evaluated those expressions and generated the result. Compare with bytecode generated for the same script, but the `const` variables are declared with `var` instead:

```
== returnFour (argCount: 0) ==
byte   ln
0000    2 OP_INTEGER                 '2'
0005    3 OP_GET_LOCAL             1
0007    | OP_GET_LOCAL             1
0009    | OP_ADD
0010    5 OP_GET_LOCAL             2
0012    | OP_RETURN
0013    6 OP_NULL
0014    | OP_RETURN
```

```
== returnHelloWorld (argCount: 0) ==
byte   ln
0000    9 OP_CONSTANT              0 'Hello'
0005   10 OP_CONSTANT              1 ' world'
0010   11 OP_GET_LOCAL             1
0012    | OP_GET_LOCAL             2
0014    | OP_ADD
0015   13 OP_GET_LOCAL             3
0017    | OP_RETURN
0018   14 OP_NULL
0019    | OP_RETURN
```

## Operations with numbers

If a binary operation consists of numbers, and either value is a decimal, the result is always a decimal.

```java
var value = 1 / 3;
print(value); // Prints "0"

var value = 1 / 3.0;
print(value); // Prints "0.333333"

var value = 1.0 / 3;
print(value); // Also prints "0.333333"

var value = 10 * 0.125;
print(value); // Prints "1.250000"
```

```java
var quantity = 10;
var cost = 1.50;
var total = quantity * cost;

print(total); // Prints "15.000000"
```

Numbers can be casted to integers with @ref Number.AsInteger, and to decimals with @ref Number.AsDecimal.

```java
var value = 15.235;
var valueAsInteger = Number.AsInteger(value);
print(valueAsInteger); // Prints "15"

var valueAsDecimal = Number.AsDecimal(valueAsInteger);
print(valueAsDecimal); // Prints "15.000000"
```

Strings can be concatenated with numbers.

```java
var item = "candy bar";
var quantity = 10;
var cost = 1.50;

// Prints "If you want 10 of a candy bar, that is going to cost $15.000000."
print("If you want " + quantity + " of a " + item + ", that is going to cost $" + (quantity * cost) + ".");
```
