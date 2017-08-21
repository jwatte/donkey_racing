#if !defined(Support_h)
#define Support_h

/* Call this to freeze up the sketch and blink the LED, 
 * and occasionally print an error message to serial USB.
 */
extern void CRASH_ERROR(char const *msg);

/* The type of a crash clean-up function.
 */
typedef void (*CRASH_CALLBACK)();

/* Install a function that's called when a crash is forced, to do things 
 * like shut off things that are bad when they're on for too long.
 */
extern CRASH_CALLBACK onCrash(CRASH_CALLBACK func);

#endif  //  Support_h
