@page functions Functions

**Functions** or are blocks of code that execute pre-defined tasks.

## Declaring a function

A function can be declared by using the `event` statement followed by an identifier and its arguments. Similar to variables, arguments can store any value, including other functions.

```java
// This function has 2 arguments that get multiplied, the result is then printed to the screen.
event multiplyAndPrint(a, b) {
	print(a * b);
}
```

Functions can then be called from anywhere else in the same scope by typing its identifier and provide its required arguments, calling a function will execute any code stored inside of it.

```java
multiplyAndPrint(4, 2);
```

```
INFO: 8
```

It's also possible to store functions in variables the same as you do any other type by setting the varible to the function's identifier, omitting the arguments.

```java
var functionVariable = multiplyAndPrint;
```

To call the function stored in the variable the same way as any other function with the only difference being that to call the function you use the variable's identifier instead.

```java
functionVariable(4, 8);
```

```
INFO: 32
```
### Optional arguments

A **Optional argument** is a function argument that has a default value that gets used if you don't specify one in the function call. You can declare an optional argument by using the `=` sign after the argument name, one can only be declared after normal arguments.

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
INFO: 1
INFO: 2
INFO: 3
INFO: 4
INFO: 5
INFO: 6
INFO: When specifying a step.
INFO: 0
INFO: 2
INFO: 4
INFO: 6
```

### Return values

Functions can optionally return any value at any point in its execution, you can specify a return value by using the `return` statement.

```java
event multiply(a, b) {
	return a * b;
}

var result = multiply(2, 5);
print(result);

print(multply(result, 2))
```

```
INFO: 10
INFO: 20
```

`return` statements will completely stop the execution of a function, which means that any code after the `return` statement won't get executed.

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
