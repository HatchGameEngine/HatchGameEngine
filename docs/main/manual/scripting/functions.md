@page hsl_functions Functions

**Functions** are blocks of code that execute a set of pre-defined tasks that can be ran at any point in code execution.

## Declaring a function

A function can be declared by using the `event` statement followed by a name and its arguments. Similar to variables, arguments can store any value, including other functions.

```java
// This function has 2 arguments that get multiplied, the result is then printed to the screen.
event multiplyAndPrint(a, b) {
	print(a * b);
}
```

Functions can then be called from anywhere as long as it is in same scope by writing the name and required arguments of the function. Calling a function will execute any code stored inside of it.

```java
multiplyAndPrint(4, 2);
```

```
INFO: 8
```

As mentioned in [Variables](@ref variables), functions are like any other type, meaning you can assign them to a variable. You can do so by setting the variable to the function's name, omitting its arguments.

```java
var functionVariable = multiplyAndPrint;
```

After assigning a function to a variable, it can be called by using the variable's name.

```java
functionVariable(4, 8);
```

```
INFO: 32
```
### Optional arguments

An **optional argument** is a function argument with a default value that gets used in the case a value ins't specified in the function call. You can declare an optional argument by using the `=` sign after the argument name. Optional arguments can only be declared after the required arguments.

```java
event countToNumberInSteps(max, steps = 1) {
	repeat(max, count) {
		if (count % steps != 0) {
			continue;
		}
		print(count);
	}
}
```

Optional arguments can also be defined by using the `[]` syntax.

```java
// If we don't specify a default value, it'll default to NULL.
event countToNumberInSteps(max, [steps = 1]) {
	repeat(max, count) {
		if (count % steps != 0) {
			continue;
		}
		print(count);
	}
}
```

If we call this function without specifying the optional argument, it will default to counting in steps of 1 but we can also specify the step count to any arbitrary number we want.

```java
print("Without specifying a step.");
countToNumberInSteps(6);
print("When specifying a step.");
countToNumberInSteps(6, 2);
```

```
INFO: Without specifying a step.
INFO: 0
INFO: 1
INFO: 2
INFO: 3
INFO: 4
INFO: 5
INFO: When specifying a step.
INFO: 0
INFO: 2
INFO: 4
INFO: 6
```

### Return values

Functions can optionally return any value at any point in its execution. You can specify a return value by using the `return` statement.

```java
event multiply(a, b) {
	return a * b;
}

var result = multiply(2, 5);
print(result);

print(multply(result, 2));
```

```
INFO: 10
INFO: 20
```

`return` statements will completely stop the execution of a function, which means that any code after the `return` statement won't get ran.

```java
event sendMessage(message = "Hello") {
	print("This is my message: " + message);
	// Returns nothing.
	return;
	print("I won't get printed :C");
}

sendMessage("Hello, reader!");
```

```
INFO: This is my message: Hello, reader
```
