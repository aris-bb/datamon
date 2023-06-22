# Datamon

Datamon is a Windows-only library for monitoring access to data in a process. It does this by setting PAGE_GUARD on the memory pages that contain the data. This causes an exception to be raised when the data is accessed, which datamon catches and reports to the user.

## Vectored Exception Handling

Datamon catches the exception by using the Vectored Exception Handling (VEH) mechanism. In the exception handler, datamon checks if the exception was caused by an access to data that is being monitored. If it is, datamon calls the user's callback function.

## AVL Augmented Interval Tree

Datamon uses an augmented interval tree on top of an AVL tree in order to store the intervals of which addresses are being monitored. This allows datamon to quickly and efficiently find which callbacks to call when an exception is caught.

## Usage

```cpp
// define a callback function
void callback(uintptr_t accessing_address, bool read, void *data) {
    std::cout << "callback!\n";
}

// initialize datamon to monitor some data
datamon::Datamon dm{ reinterpret_cast<uintptr_t>(my_data), sizeof(*my_data), callback };

// access the data and datamon will catch the access for you
my_data->value = 123; // prints "callback!" to stdout
```

Check out [src/example](src/example) in order to see the example usage and demonstration of datamon.
