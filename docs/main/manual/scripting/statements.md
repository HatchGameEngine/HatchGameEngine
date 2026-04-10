@page statements Statements

# Introduction

A statement is something that performs an action. A semicolon character (`;`) is used to separate statements.

## Block statements

The most commonly used kind of statement is a block statement. It begins with a left curly brace and ends with a right curly brace.

```java
{
    const a = 7 + 5;
    const b = a * 4;

    {
        const c = (b - a) / 2;

        print c;
    }
}
```

## `print` statements

`print` is also a statement. It requires an expression.

```java
print "Hello world!";

print("Good evening.");

print 2 + 2;

print Application;
```

```
     INFO: Hello world!
     INFO: Good evening.
     INFO: 4
     INFO: <class Application>
```

## Conditional statements

Statements are used to introduce control flow. In HSL, conditional statements are `if`, `if`-`else`, and `switch`.

### `if` and `else` statements

An `if` statements executes the statement that follows it if the expression between the parentheses is a truthy value. Otherwise, if there is an `else` statement following it, it executes that instead.

```java
if (expression)
    statement;
else if (expression)
    statement;
else
    statement;
```

`else if` is actually two statements together: `else` and `if`. An `if` condition may be followed by multiple `else if` conditions.

```java
if (expression)
    statement;
else if (expression)
    statement;
else if (expression)
    statement;
else if (expression)
    statement;
else
    statement;
```

To execute multiple statements after an `if` or `else` statement, a block statement is required.

```java
if (expression) {
    statement1;
    statement2;
    statement3;
}
else if (expression) {
    statement1;
    statement2;
}
else {
    statement;
}
```

### `switch` statement

A `switch` statement matches the expression with `case` labels:

```java
switch (expression) {
    case expression1:
        statement;
        break;
    case expression2:
        statement;
        break;
    case expression3:
        statement;
        break;
    default:
        statement;
        break;
}
```

When a `case` is matched, the statements inside it are executed until a `break` statement is found.

```java
var drink = "juice";
switch (drink) {
    case "water":
        print "Drinking water.";
        break;
    case "juice":
        print "Drinking juice.";
        break;
    case "soda":
        print "Drinking soda.";
        break;
}
```

```
     INFO: Drinking juice.
```

If there is no `break` statement, the execution *falls through* to the next `case` label, even if the expression in the `case` does not match the expression in `switch`.

```java
var drink = "juice";
switch (drink) {
    case "water":
        print "Drinking water.";
    case "juice":
        print "Drinking juice.";
    case "soda":
        print "Drinking soda.";
}
```

```
     INFO: Drinking juice.
     INFO: Drinking soda.
```

If none of the `case`s are matched, the `default` label is executed, if there is any.

```java
var drink = null;
switch (drink) {
    case "water":
        print "Drinking water.";
        break;
    case "juice":
        print "Drinking juice.";
        break;
    case "soda":
        print "Drinking soda.";
        break;
    default:
        print "Not drinking anything.";
        break;
}
```

```
     INFO: Not drinking anything.
```

There cannot be multiple `default` labels in a `switch`.

```java
var drink = null;
switch (drink) {
    case "water":
        print "Drinking water.";
        break;
    case "juice":
        print "Drinking juice.";
        break;
    case "soda":
        print "Drinking soda.";
        break;
    default:
        print "Not drinking anything.";
        break;
    default: // Compile error: "Cannot have multiple default clauses."
        print "No drink?";
        break;
}
```

A `case` or `default` label can `return` instead of `break`. This exits the entire function the `switch` is in.

```java
event calculate(a, b, operation) {
    switch (operation) {
        case "plus":
            return a + b;
        case "minus":
            return a - b;
        case "multiply":
            return a * b;
        case "divide":
            return a / b;
    }
}

print calculate(5, 2, "plus"); // Prints "7"
```

A variable cannot be declared inside a `switch` statement without a block.

```java
event calculate(a, b, operation) {
    switch (operation) {
        case "plus":
            var result = a + b; // Compile error: "Cannot initialize variable inside switch statement.""
            return result;
        case "minus":
            var result = a - b;
            return result;
        case "multiply":
            var result = a * b;
            return result;
        case "divide":
            var result = a / b;
            return result;
    }
}
```

Using a block after a label lets you declare variables:

```java
event calculate(a, b, operation) {
    switch (operation) {
        case "plus": {
            var result = a + b;
            return result;
        }
        case "minus": {
            var result = a - b;
            return result;
        }
        case "multiply": {
            var result = a * b;
            return result;
        }
        case "divide": {
            var result = a / b;
            return result;
        }
    }
}

print calculate(5, 2, "plus"); // Prints "7"
```
