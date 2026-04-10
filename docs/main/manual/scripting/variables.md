@page variables Variables

# Introduction

A variable is used to store and retrieve data. They can be declared using `var` or `const`. Variables are scoped to the block they are declared; those are called *local variables*. However, when declared in top-level code, they are called *global variables*, because they can be accessed from any script. To make a variable declared in top-level code only accessible from the script it lives in, it must be declared using `local var` or `local const`. There are five types of variables in HSL: integers, numbers, objects, hitboxes, and `null`. All types are passed by value, except objects, which are passed by reference.

## Declaring a variable

Variables declared using `var` are mutable and may be initialized with an expression. Variables declared with `const` are immutable and must be initialized with a constant expression. The name of a variable is called an *identifier*. An identifier can start with an alphabetic letter, `_`, or `$`, and the remaining characters can be digits. Variable names are case sensitive: `playerName`, `PlayerName`, and `PLAYERNAME` are different variables. Examples of valid identifiers are: `onMainMenu`, `top10`, `MAX_HIGH_SCORES`, `$total`, and `_average`.

```java
const MAX_PLAYERS_IN_GAME = 4;
var playersInGame = 2;
var _allReady;

if (playersInGame > 0) {
	_allReady = playersInGame == MAX_PLAYERS_IN_GAME;
}
```

A variable cannot reuse the identifier of another variable in the same scope.

```java
event printHello() {
	var hello = "hello";
	var hello = "hi"; // Compile error: "Variable with this name already declared in this scope."
}
```

The exception is top-level code. The latter declaration overrides the earlier one.

```java
var hello = "hello";
var hello = "hi";

print(hello); // Prints "hi"
```

A local variable can be declared with the same identifier as a global variable, and it does not permanently override the global variable. The local variable only exists in its scope.

```java
var hello = "hello";

if (hello) {
	var hello = "hi";

	print(hello); // Prints "hi"
}

print(hello); // Prints "hello"
```

A local variable is permitted to *shadow* another variable if it's in a deeper scope than the variable it's shadowing.

```java
event printFruit() {
	const preferBananas = true;
	const fruit = "apple";

	if (preferBananas) {
		const fruit = "banana";

		print(fruit); // Prints "banana"
	}

	print(fruit); // Prints "apple"
}

printFruit();
```

A variable declared inside of a block can no longer be accessed once that block ends (that is, after the `}`.)

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

The following are considered basic types in HSL:

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
print(playerBox); // Prints "[-16, -16, 16, 16]"

var enemySize = 32;
var enemyBox = hitbox{-enemySize, -enemySize, enemySize, enemySize};
print(enemyBox); // Prints "[-32, -32, 32, 32]"
```

The individual sides of a hitbox can be retrieved using the element-access syntax:

```java
var playerBox = hitbox{-16, -24, 16, 24};
print(playerBox[HITBOX_LEFT]); // Prints "-16"
print(playerBox[HITBOX_TOP]); // Prints "-24"
print(playerBox[HITBOX_RIGHT]); // Prints "16"
print(playerBox[HITBOX_BOTTOM]); // Prints "24"
```

Despite being a keyword, `hitbox` can still be used as an identifier:
```java
var hitbox = hitbox{-16, -24, 16, 24};
print(hitbox); // Prints "[-16, -16, 16, 16]"
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
	print("Is a red vehicle");
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

# The `typeof` operator

`typeof` can be used to retrieve the *type of* a variable. It returns a string.

```java
var item = "candy bar";
var quantity = 10;
var cost = 1.50;

print(typeof item); // Prints "string"
print(typeof quantity); // Prints "integer"
print(typeof cost); // Prints "decimal"
```
