@page variables Variables

# Introduction

A variable is used to store and retrieve data. They can be declared using `var` or `const`. There are five types of variables in HSL: integers, numbers, objects, hitboxes, and `null`. All types are passed by value, except objects, which are passed by reference.

## Declaring a variable

Variables can be declared using `var`, for variables that are mutable, and `const`, for variables that are immutable and are a constant value. A variable declared with `var` may be assigned to, but `const` variables must be assigned to a constant expression.

A variable can start with an alphabetic letter, `$`, or `_`, and the remaining characters can be digits.

```java
var playersInGame = 2;
var _allReady;

if (playersInGame > 0) {
	var has4Players = playersInGame == 4;

	_allReady = has4Players;

	return _allReady;
}
```

A variable can be declared in top-level code, which makes them global variables. If a variable declared with `var` is not assigned to, it's initialized as `null`.

```java
var PlayersInGame; // Can be accessed from another script

const MAX_PLAYERS_IN_GAME = 4; // Can also be accessed from another script

PlayersInGame = 2;
```

A variable declared in top-level code can be *local*, which makes them inaccessible to other scripts.

```java
local var PlayersInGame; // Cannot be accessed from another script

local const MAX_PLAYERS_IN_GAME = 4; // Also cannot be accessed from another script

PlayersInGame = 2;
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
else {
	text = "All players have joined!";
}

print("Waiting for " + text + " to join...");
```

A variable is permitted to *shadow* another variable if it's in a deeper scope than the variable it's shadowing.

```java
const preferBananas = true;
const fruit = "apple";

if (preferBananas) {
	const fruit = "banana";

	print(fruit); // Prints "banana"
}

print(fruit); // Prints "apple"
```

# Types

The following are considered basic types in HSL:

## Basic types

| Type             | Description      | Examples         |
| ---------------- | ---------------- | ---------------- |
| Integer | A whole number. | `123`, `-456`, `0x7F` |
| Decimal | A floating point number. | `1.0`, `0.25`, `-.75`, `3.` |
| Object | A reference to an object. | `new Car()`, `"Hello world"`, `[ 10, 2.0, "omelette" ]` |
| Hitbox | A rectangle shape used for collision. | `hitbox{-16, -16, 16, 16}` |
| `null` | Absence of a value. | `null` |

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

## Booleans

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

## The `typeof` operator

`typeof` can be used to retrieve the *type of* a variable.

```java
var item = "candy bar";
var quantity = 10;
var cost = 1.50;

print(typeof item); // Prints "string"
print(typeof quantity); // Prints "integer"
print(typeof cost); // Prints "decimal"
```
