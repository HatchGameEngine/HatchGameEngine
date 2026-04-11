@page expressions_and_operators Expressions and operators

An **expression** is a statement that results into a value. Most expressions are used with **operators**. For example, `x = 2` is an expression that uses the assignment operator (`=`) to assign the literal `2` to the variable `x`.

Operators have precedence, which changes the order the code is executed.

```java
var a = 4 + 2 * 3;
var b = 11 - 7 / 2;
```

Since `*` and `/` have higher precedence than `+` and `-`, the above expressions result in `10` and `8`. Using parentheses to create a *grouped expression* changes the precedence of the operators.

```java
var a = (4 + 2) * 3;
var b = (11 - 7) / 2;
```

The grouped expressions are evaluated first, so they result in `18` and `2`.

## Unary and binary operators

Operators in HSL can be either *unary*, *binary*, or *ternary*.

Unary operators require a single operand before or after the operator.

```
operator operand // Prefix
operand operator // Postfix
```

When an unary operator is placed before the operand, it's called a *prefix unary operator*. Otherwise, if an unary operator is placed after the operand, it's called a *postfix unary operator*. Examples of expressions with unary operators are `++a`, `!value`, `typeof text`, and `b--`.

Binary operators require two operands: one before the operator, and one after the operator.

```
operand operator operand // Infix
```

The above is called an *infix binary operator* because the operator is between the two operands. Examples of expressions with binary operators are `5 + 3`, `a > b`, and `10 /= 2`.

HSL only has one kind of ternary operator:

```
condition ? expressionIfTruthy : expressionIfFalsey
```

This operator evaluates `condition`, and if it's a truthy value, evaluates the `expressionIfTruthy` expression and skips the `expressionIfFalsey` expression. Otherwise, if `condition` evaluates to a falsey value, the operator skips the `expressionIfTruthy` expression and evaluates the `expressionIfFalsey` expression.

This can be used as an alternative for `if`-`else` statements.

```java
event getPlayerAliveOrDead(health) {
    var playerAliveOrDead = health > 0 ? "alive" : "dead";
    return playerAliveOrDead;
}

print(getPlayerAliveOrDead(10)); // Prints "alive"
print(getPlayerAliveOrDead(0)); // Prints "dead"
```

The code above is equivalent to the code below:

```java
event getPlayerAliveOrDead(health) {
    var playerAliveOrDead;
    if (health > 0) {
        playerAliveOrDead = "alive";
    }
    else {
        playerAliveOrDead = "dead";
    }
    return playerAliveOrDead;
}

print(getPlayerAliveOrDead(10)); // Prints "alive"
print(getPlayerAliveOrDead(0)); // Prints "dead"
```

## Constant expressions

A **constant expression** is one that can be evaluated at compile time. They can be assigned to a `var` or a `const` variable, but `const` variables require a constant expression.

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

Since the compiler can now determine `quantity * cost` is a constant expression, it started to warn about it. Changing `total` to be `const` makes all warnings disappear:

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

## The `typeof` operator

`typeof` can be used to retrieve the *type of* a variable. It returns a string.

```java
var item = "candy bar";
var quantity = 10;
var cost = 1.50;

print typeof item; // Prints "string"
print typeof quantity; // Prints "integer"
print typeof cost; // Prints "decimal"
```
