@page arrays Arrays

As shown in [Variables](@ref variables), **arrays** are objects that hold a resizable ordered list of values.

## Declaring an array

Arrays can be declared in 2 ways. The first, and more straightfoward way, is using the `[]` operator. Then you can optionally - between `[]` - fill the array with the desired values, separating them using a comma.


```java
var myArray = ["H", "a", "t", "c", "h", " ", "E", "n", "g", "i", "n", "e"];
print(myArray);
```
```
INFO: ["H", "a", "t", "c", "h", " ", "E", "n", "g", "i", "n", "e"]
```

Due to the object nature of arrays, it's also possible to create one by calling @ref Array.Create, which takes a length and a initial value as parameters. The initial value gets used to fill the array up to the desired length.

```java
print(Array.Create(4, 9 + 10));
```
```
INFO: [19, 19, 19, 19]
```

## Reading & writing to individual values

You can read the value of an item held by an array by typing the array's name then the item's index between `[]`. The first value of an array in HSL will always be 0.

```java
print(myArray[2] + myArray[6] + myArray[1] + myArray[3] + myArray[4] + myArray[9] + myArray[7] + myArray[8] + myArray[5] + myArray[0] + myArray[11] + myArray[10]);
```
```
INFO: tEaching Hen
```

Similarly, you can also set the value by adding the `=` operator to the end of the above syntax followed by the value you want.

```java
// repeat is a statement that repeats a set of instructions by a specified amount of times. Statements will be explained in the next chapter.
repeat (myArray.Length(), number) {
    // Length() is a method found in the Array object class that returns the number of items present in the array, in this case, 11. Methods will be thoroughly explained in further chapters.
    myArray[number] = myArray[(number + 1) % myArray.Length()];
}

print(myArray);
```
```
INFO: ["a", "t", "c", "h", " ", "E", "n", "g", "i", "n", "e", "H"]
```

In both cases, trying to access values greater than or equal to the array's length or below 0 results in an error.

```java
print(myArray[4096]);
```
```
ERROR: [...]

    Index 4096 is out of bounds of array of size 12.
```

```java
print(myArray[-32]);
```
```
ERROR: [...]

    Index -32 is out of bounds of array of size 12.
```

## Inserting & removing items

In this section we'll show a few methods that can be used to insert or remove items inside an array, those being: @ref Array.Push, @ref Array.Pop, @ref Array.Insert, @ref Array.Erase, and @ref Array.Clear. As mentioned previously, methods will be explained in future chapters.

You may find other methods accompanied by explanations in the [Array](@ref classArray) page.

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

`Insert` takes an index and a value as arguments. The value gets appended at the index defined by first argument. Using `Insert` shifts all the items after the specified index to the right to accomodate the new value.

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

As a counterpart, `Erase` removes any value specified in the index argument, shifting all the values after index to the left.


```java
encouragement.Erase(0);
encouragement.Erase(2);
encouragement.Erase(4);
encouragement.Erase(6);

print(encouragement);
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



