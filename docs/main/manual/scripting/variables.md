@page variables Variables

# Introduction

A variable is used to store and retrieve data. There are five types of values in HSL: integers, numbers, objects, hitboxes, and `null`. The type of a variable is inferred from the value it's assigned to, and a variable can be reassigned to a different type from what it was initialized with.

## Declaring a variable

Variables can be declared using `var` or `const`. Variables declared using `var` are mutable and may be initialized with an expression. If a `var` is not assigned to any value, it's initialized to `null`.

```java
var x = 5;
var y; // Implied to be null
```

Variables declared with `const` are immutable and must be initialized with a constant expression.

```java
const MAX_PLAYERS_IN_GAME = 4;
const MAX_LIVES; // Compile error: ""const" variables must have an explicit constant declaration."
```

The name of a variable is called an *identifier*. An identifier can start with an alphabetic letter, `_`, or `$`, and the remaining characters can be digits. Variable names are case sensitive: `playerName`, `PlayerName`, and `PLAYERNAME` are different variables. Examples of valid identifiers are: `onMainMenu`, `top10`, `MAX_HIGH_SCORES`, `$total`, and `_average`.

```java
const MAX_PLAYERS_IN_GAME = 4;

var playersInGame = 2;
var _allReady;

if (playersInGame > 0) {
	_allReady = playersInGame == MAX_PLAYERS_IN_GAME;
}
```

## Scope of a variable

There are three types of variable scope:
- Global scope: The variable can be accessed from any script.
- Script scope: The variable can only be accessed the script it was declared in.
- Block scope: The variable can only be accessed in the block statement (a pair of curly braces) it was declared in, and all blocks found inside of that block.

When a variable is declared inside of a block, it's called a *local variable*. When a variable is declared outside of a block, it can be either a *global variable* a *script variable*. A global variable is any `var` or `const` declaration in top-level code, and a script variable is any `local var` or `local const` declaration in top-level code; since it's *local* to the script it's declared, it cannot be accessed from other scripts.

In script scope or block scope, a local variable cannot reuse the identifier of another local variable in the same scope.

```java
event printHello() {
	var hello = "hello";
	var hello = "hi"; // Compile error: "Variable with this name already declared in this scope."
}
```

```java
local var hello = "hello";
local var hello = "hi"; // Compile error: "Local with this name already declared in this module."
```

A variable in block scope can be declared with the same identifier as another previously declared variable if the latter declaration is in a deeper block scope.

```java
event printFruit() {
	const preferBananas = true;
	const fruit = "apple";

	if (preferBananas) {
		const fruit = "banana";

		print fruit; // Prints "banana"
	}

	print fruit; // Prints "apple"
}

printFruit();
```

In global scope, a global variable can reuse the identifier of another global variable.

```java
var hello = "hello";
var hello = "hi"; // This declaration overrides the earlier one.

print hello; // Prints "hi"
```

A variable in script or block scope can be declared with the same identifier as a variable in global scope, temporarily overriding the global variable, until it goes out of scope.

```java
var hello = "hello";

if (hello) {
	var hello = "hi";

	print hello; // Prints "hi"
}

print hello; // Prints "hello"
```

Finally, a variable declared in block scope can no longer be accessed once the block ends (that is, after the `}`.)

```java
var text;

if (PlayersInGame != MAX_PLAYERS_IN_GAME) {
	var remainingPlayers = MAX_PLAYERS_IN_GAME - PlayersInGame;
	if (remainingPlayers == 1) {
		text = "1 player";
	}
	else {
		text = remainingPlayers + " players";
	}

	// remainingPlayers can no longer be accessed after this line
}
```

# Types

In HSL, all types are passed by value, except objects, which are passed by reference. The following are considered basic types in HSL:

## Basic types

| Type             | Description      | Examples         |
| ---------------- | ---------------- | ---------------- |
| Integer | A whole number. Can be in hexadecimal. | `123`, `-456`, `0x7F` |
| Decimal | A floating point number. The integer or fractional part can be omitted. | `1.0`, `-0.25`, `.75`, `3.` |
| Object | A reference to an object. | `new Car()`, `"Hello world"`, `[ 10, 2.0, "omelette" ]` |
| Hitbox | A rectangular shape, used for collision. | `hitbox{-16, -16, 16, 16}` |
| `null` | Absence of a value. | `null` |

### Hitboxes

A hitbox type can be constructed with integer literals, or integer variables. The values *must* be integers; they cannot be decimals. The order of the values are: left, top, right, and bottom.

```java
var playerBox = hitbox{-16, -16, 16, 16};
print playerBox; // Prints "[-16, -16, 16, 16]"

var enemySize = 32;
var enemyBox = hitbox{-enemySize, -enemySize, enemySize, enemySize};
print enemyBox; // Prints "[-32, -32, 32, 32]"
```

The individual sides of a hitbox can be retrieved using the element-access syntax.

```java
var playerBox = hitbox{-16, -24, 16, 24};
print playerBox[HITBOX_LEFT]; // Prints "-16"
print playerBox[HITBOX_TOP]; // Prints "-24"
print playerBox[HITBOX_RIGHT]; // Prints "16"
print playerBox[HITBOX_BOTTOM]; // Prints "24"
```

Despite being a keyword, `hitbox` can still be used as an identifier.

```java
var hitbox = hitbox{-16, -24, 16, 24};
print hitbox; // Prints "[-16, -16, 16, 16]"
```

A hitbox is immutable.

```java
var playerBox = hitbox{-16, -24, 16, 24};
playerBox[HITBOX_LEFT] = 5; // Throws a runtime error: "Cannot set element of hitbox."
```

### Booleans

While HSL doesn't natively have a boolean type, `true` and `false` are accepted and considered the same as `1` and `0` respectively.

```java
var isRed = false;
if (color == "red") {
	isRed = true;
}

if (isVehicle == true && isRed == true) {
	print "Is a red vehicle";
}
```

## Object types

The following are considered object types in HSL:

| Type             | Description      | Example          |
| ---------------- | ---------------- | ---------------- |
| String | Text. | `"Hello world"` |
| Array | A resizable list of values. | `[ 10, 2.0, "omelette" ]` |
| Map | A collection of key-value pairs. Other languages also call this type a dictionary. Keys must be strings and are stored in order of insertion. | `{"tankCapacity": 60.0, "color": "red"}` |
| Class | Contains methods and (optionally) a constructor. Typically used to define entities. | `class Car { ... }` |
| Function | A callable that accepts arguments and optionally returns a value. When defined inside of a class, a function is called a "method". Some functions are defined natively and execute C++ code. | `event IsVehicle() { return true; }` |
| Bound method | A method with arguments bound to it. | `IsVehicle.bind(car);` |
| Instance | An instance of a class. | `new Car()` |
| Entity | A game object in a scene. | `Instance.Create("Car", 200.0, 192.0);` |
