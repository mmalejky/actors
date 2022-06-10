## Introduction
Implementation of the [actor model](https://en.wikipedia.org/wiki/Actor_model) for concurrent programming.

The actors send messages to each other and respond to them based on their roles. \
A role is a list of functions assigned to message types. \
Each actor has their own message queue and internal state. \
Each actor supports three predefined message types: ```MSG_SPAWN```, ```MSG_GODIE```, ```MSG_HELLO```


## Build
You can build and test the library this way:
```
mkdir build && cd build
cmake ../src
make
make test
```
## Sample programs using the library
Two sample programs using this library are included:

- ```matrix.c``` \
Program recieves two numbers ```h``` and ```w``` from _stdin_, each on a separate line.
These numbers represent the height and width of the input matrix.
Next, ```h*w``` lines are read of the form ```x t```, where ```x``` is the next value in a matrix, and ```t``` is the time to wait for that value (in milliseconds).
The program creates ```w``` actors, each responsible for a different column.
The next step is to sum the values in each row, with the appropriate waiting time and order from left to right.
Finally, the program prints out the result for each row. Example:
```
./matrix
2
3
1 2
1 5
12 4
23 9
3 11
7 2
```
results in (with appropriate waiting time):
```
14
33
```

- ```factorial.c``` \
Computes the factorial of a given number ```n``` in _stdin_.
Each of the ```n``` actors receives the current result, multiplies it by its value and passes it on. Result is printed to _stdout_.\
Sample usage: ```echo 5 | ./factorial``` results in ```120```.
