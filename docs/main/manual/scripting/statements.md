@page statements Statements

A statement is something that performs an action. A semicolon character (`;`) is used to separate statements.

## Block statements

The most commonly used kind of statement is a **block statement**. It begins with a left curly brace and ends with a right curly brace.

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

An `if` executes the statement that follows it if the expression between the parentheses is a truthy value. Otherwise, if there is an `else` statement following it, it executes that instead.

```java
if (expression)
    statement
else if (expression)
    statement
else
    statement
```

`else if` is actually two statements together: `else` and `if`. An `if` condition may be followed by multiple `else if` conditions.

```java
if (expression)
    statement
else if (expression)
    statement
else if (expression)
    statement
else if (expression)
    statement
else
    statement;
```

To execute multiple statements after an `if` or `else` statement, a block statement is required.

```java
if (expression) {
    statement1
    statement2
    statement3
}
else if (expression) {
    statement1
    statement2
}
else {
    statement
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

## Loop and iteration statements

A **loop** is a way to execute a specific piece of code many times. The simplest kind of loop is a `repeat`, but there are conventional `while`, `do`-`while`, and `for` loops as well. In any loop, `continue` can be used to finish the current iteration and begin the next one, and `break` can be used to exit the entire loop immediately.

### `repeat` loops

A `repeat` loop executes code a specific amount of times. In its basic form, it looks like the following:

```java
repeat (expression)
    statement
```

For example, the following code prints `"Hello world!"` three times:

```java
repeat (3) {
    print "Hello world!";
}
```

```
     INFO: Hello world!
     INFO: Hello world!
     INFO: Hello world!
```

A `repeat` loop can also be written as the following:

```java
repeat (expression, counter, remaining)
    statement
```

`counter` and `remaining` are optional. They are variables that can be provided in a `repeat` loop. `counter` is an integer that counts *up* from zero, and `remaining` is an integer that counts *down* from the initial value.

```java
print "First loop:";
repeat (3, counter) {
    print "Counter: " + counter;
}

print "Second loop:";
repeat (3, counter, remaining) {
    print "Counter: " + counter;
    print "Remaining: " + remaining;
}
```
```
     INFO: First loop:
     INFO: Counter: 0
     INFO: Counter: 1
     INFO: Counter: 2
     INFO: Second loop:
     INFO: Counter: 0
     INFO: Remaining: 3
     INFO: Counter: 1
     INFO: Remaining: 2
     INFO: Counter: 2
     INFO: Remaining: 1
```

Be careful when using a decimal as the repeat amount, as `repeat` checks if `remaining` is *equal to* zero, not if it's *lower than or equal to* zero:

```java
repeat (3.5, counter) {
    print "Counter: " + counter;
}
```
```
     INFO: Counter: 0
     INFO: Counter: 1
     INFO: Counter: 2
     INFO: Counter: 3
     INFO: Counter: 4
     INFO: Counter: 5
     INFO: Counter: 6
     INFO: Counter: 7
     INFO: Counter: 8
     INFO: Counter: 9
     INFO: Counter: 10
     ...
```

`remaining` can be assigned to during an iteration.

```java
repeat (10, counter, remaining) {
    print "Counter: " + counter;

    if (counter == 3) {
        print "Skipping iterations...";

        remaining = 3;
    }
}
```
```
     INFO: Counter: 0
     INFO: Counter: 1
     INFO: Counter: 2
     INFO: Counter: 3
     INFO: Skipping iterations...
     INFO: Counter: 4
     INFO: Counter: 5
```

Notice how `counter` still counted up to five. Even if `remaining` is modified, it will still increment by one on each iteration. `counter`, however, cannot be assigned to.

```java
repeat (10, counter) {
    print "Counter: " + counter;

    if (counter == 3) {
        print "Skipping iterations...";

        counter = 8; // Compile error: "Attempted to assign to constant!"
    }
}
```

`continue` can be used to finish the current iteration of the loop.

```java
repeat (10, counter, remaining) {
    print "Counter: " + counter;

    if (counter < 7) {
        continue;
    }

    print "Loop will end in " + remaining;
}
```
```
     INFO: Counter: 0
     INFO: Counter: 1
     INFO: Counter: 2
     INFO: Counter: 3
     INFO: Counter: 4
     INFO: Counter: 5
     INFO: Counter: 6
     INFO: Counter: 7
     INFO: Loop will end in 3
     INFO: Counter: 8
     INFO: Loop will end in 2
     INFO: Counter: 9
     INFO: Loop will end in 1
```

`break` can be used to interrupt the execution of the loop.

```java
repeat (10, counter) {
    print "Counter: " + counter;

    if (counter == 3) {
        print "Stopping.";
        break;
    }
}
```
```
     INFO: Counter: 0
     INFO: Counter: 1
     INFO: Counter: 2
     INFO: Counter: 3
     INFO: Stopping.
```

### `for` loops

A `for` loop consists of three expressions. Typically, the first expression initializes a loop counter, the second expression checks the loop counter, and the third expression increments the loop counter.

```java
for (initialization; condition; increment)
    statement
```

Any of the three statements can be omitted. This kind of `for` loop does the following:
1. Run the code in the `initialization` expression, if present. Variables are allowed to be declared with `var` here.
2. The `condition` expression is executed, if present. If it evaluates to a falsey value, the loop finishes here.
3. `statement` is executed. This can be a block statement.
4. `increment` is executed, if present.
5. Go back to step 2.

The following `for` loop is equivalent to the `repeat` example you saw earlier:

```java
for (var counter = 0; counter < 3; counter++) {
    print "Counter: " + counter;
}
```
```
     INFO: Counter: 0
     INFO: Counter: 1
     INFO: Counter: 2
```

The following is also a valid `for` loop:

```java
var counter = 0;
for (; counter < 3;) {
    print "Counter: " + counter;
    counter++;
}
```

So is the following:

```java
var counter = 0;
for (;;) {
    print "Counter: " + counter;
    counter++;

    if (counter == 3) {
        break;
    }
}
```

### `for`-`in` loops

A `for`-`in` loop iterates through an object.

```java
for (value in iteratable)
    statement
```

Examples of objects that can be iterated in this manner are arrays, maps, and instances. When used with an array, this iterates through its contents.

```java
for (string in [ "foo", "bar" ]) {
    print(string);
}
```
```
     INFO: foo
     INFO: bar
```

When used with a map, this iterates through its values.

```java
for (value in { "foo": 123, "bar": 456 }) {
    print(value);
}
```
```
     INFO: 123
     INFO: 456
```

To iterate through the keys of a map, use the `keys()` method.

```java
var map = { "foo": 123, "bar": 456 };
for (key in map.keys()) {
    print(key + ": " + map[key]);
}
```
```
     INFO: foo: 123
     INFO: bar: 456
```

You may make any instance iteratable by defining `iterate` and `iteratorValue` methods in its class. Example:

```java
class Bookcase {
    Bookcase() {
        this.Books = [
            "How To Boil Eggs",
            "The Chicken That Crossed The Road",
            "Coping With Brooding"
        ];
    }

    iterate(state) {
        if (state == null) {
            state = 0;
        }
        else {
            state++;
        }

        if (state >= Array.Length(this.Books)) {
            return null;
        }

        return state;
    }

    iteratorValue(index) {
        return this.Books[index];
    }
}

var bookcase = new Bookcase();
for (book in bookcase) {
    print book;
}
```
```
     INFO: How To Boil Eggs
     INFO: The Chicken That Crossed The Road
     INFO: Coping With Brooding
```

How `for`-`in` works is as following:
1. `iterate` is called. When the iteration begins, `null` is passed to `state`. If the iteration was already ongoing, `state` is the value that `iterate` last returned.
2. If `iterate` returns `null`, the iteration ends.
3. `iteratorValue` is called. The value that `iterate` returned is passed to `index`. It must return the value that will be used in this iteration.
4. If the loop wasn't interrupted using `break` or `return`, go back to step 1.
