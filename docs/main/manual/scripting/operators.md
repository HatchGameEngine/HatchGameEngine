@page operators Operators

# Operators

TODO

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
