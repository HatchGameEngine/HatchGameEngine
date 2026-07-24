@page arrays Arrays

As shown in [Variables](@ref variables), **array**s are objects that hold a resizable ordered list of values.

## Declaring an array

Arrays can be declared in 2 ways. One of the ways is by assigning the values of the array separated by commas between the `[]` operator.


```java
var myArray = ["H", "a", "t", "c", "h", " ", "E", "n", "g", "i", "n", "e"];
print(myArray);
```
```
INFO: ["H", "a", "t", "c", "h", " ", "E", "n", "g", "i", "n", "e"]
```

Due to the object nature of arrays, you can also create an array by calling the static `Create` method defined in the `Array` class. The `Create` method takes a size and a initial value as parameters, the array will then be filled with the initial value.

```java
print(Array.Create(4, 9 + 10));
```
```
INFO: [19, 19, 19, 19]
```

## Reading & writing to individual values

It's possible to read the values of individual items present in an array by typing the array's name followed by the index of the item between `[]`. The first value of an array in HSL will always be 0.

```java
print(myArray[2] + myArray[6] + myArray[1] + myArray[3] + myArray[4] + myArray[9] + myArray[7] + myArray[8] + myArray[5] + myArray[0] + myArray[11] + myArray[10]);
```
```
INFO: tEaching Hen
```

Similarly, you can can set the value by appending the `=` operator to the end of the above syntax followed by the value you want.

```java
// Shifts the values by 1.
// Length() is a method found in the Array object class that returns the number of items present in the array, in this case, 11. Methods will be further explained in namespaces & classes.
repeat (myArray.Length(), number) {
    myArray[number] = myArray[(number + 1) % 11];
}
print(myArray);
```
```
INFO: ["a", "t", "c", "h", " ", "E", "n", "g", "i", "n", "e", "H"]
```

In both cases, trying to access a index greater than the array's size or below 0 results in your project crashing.

```java
print(myArray[4096]);
```
```
ERROR: [...]

    Index 4096 is out of bounds of array of size 11.
```

```java
print(myArray[-32]);
```
```
ERROR: [...]

    Index -32 is out of bounds of array of size 11.
```

## Inserting & removing items

In this section we'll show a few methods that can be used to insert or remove items inside an array, those being: `Push`, `Pop`, `Insert`, `Erase` and `Clear`. As mentioned previously, methods will be explained in future chapters.

You may find other methods and their explanations in [Array](@ref classArray).

### Push & Pop

The `Push` method can be used to append values to the end of an array.

```java
var myFavoriteNumbers = [0, 8, 16];
myFavoriteNumbers.Push(32);

print(myFavoriteNumbers);
```
```
INFO: [0, 8, 16, 32]
```

`Pop`, on the other hand, can be used to remove the last item inside an array.

```java
myFavoriteNumbers.Pop();
myFavoriteNumbers.Pop();


print(myFavoriteNumbers);
```
```
INFO: [0, 8]
```

### Insert & Erase

`Insert` takes an index and a value as arguments, it uses those to add the specified value to the index defined in the first argument. Using `Insert` shifts all the items to the right to accomodate the new value.

```java
var dayAction = "Game";
var nightAction = "Engine";
var combination = "Hatch";

var encouragement = ["First we ", ", then we ", ". Let\'s ", "!"]; 

encouragement.Insert(1, dayAction);
encouragement.Insert(3, nightAction);
encouragement.Insert(5, combination);

print(encouragement);
```
```
INFO: ["First we ", "Game", ", then we ", "Engine", ". Let's ", "Hatch", "!"]
```

As a counterpart, `Erase` removes any value specified in the index argument.


```java

encouragement.Erase(0);
encouragement.Erase(2);
encouragement.Erase(4);
encouragement.Erase(6);

print(encouragement);
```
```
INFO: ["Game", Engine", "Hatch"]
```

### Clear

Finally, `Clear` erases all the elements inside the array.

```java

var competitors = ["Blender", "Scratch", "Doom Engine"];

competitors.Clear();

print(competitors);
```
```
INFO: []
```



