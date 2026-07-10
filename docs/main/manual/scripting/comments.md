@page comments Comments

**Comments** are lines of code that get ignored by the compiler, meaning they don't affect your project.

## Declaring a comment

There are 2 types of comments, one of them is single-line comments, which you can declare by using the `//` prefix.

```java
// This is a comment!
```

The other is multi-line comments which start with `/*` and end with `*/`. Any text between won't get executed.

```java
/*
This is a multi-line comment!
Multi-line comments can have multiple lines!
*/
```

```java
This isn't a comment /* but this is. */
```

Multi-line comments can also be used to 

## When to use comments

Comments can be used to leave notes for yourself or other programmers that are working on the project.

```java

// Note: This constant variable stores the maximum amount of objects that can be present at the same time.
const MAX_OBJ_C = 5;
```

Comments can be used to "ignore" lines of code, skipping their execution.

```java
event Create() {
	print("Getting executed is my favorite activity.");
	/* 
	print("Hello!");
	print("Please skip these lines!");
	*/
}
```

```
INFO: Getting executed is my favorite activity.
```

