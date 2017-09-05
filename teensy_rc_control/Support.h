#if !defined(Support_h)
#define Support_h

#include <math.h>
#include <string.h>

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

static inline int16_t intFloat(float f) {
  if (f < -2.0f) return -32766;
  if (f > 2.0f) return 32766;
  return (int16_t)floorf(f * 16383 + 0.5f);
}

/* Use DebugVal to store key/value data for debugging */
struct DebugVal {
  //  name should be a string literal
  char const *name;
  //  Don't poke at the _ptr field
  DebugVal *_ptr;
  //  float and int values fit in 12 bytes
  char value[16];
};
template<size_t Mx> struct DebugValStr : public DebugVal {
  char extra[Mx > sizeof(value) ? (Mx - sizeof(value)) : 0];
  void set(char const *str) {
    strncpy(value, str, Mx);
    value[Mx-1] = 0;
  }
};
/* Update the value of a DebugVal by calling DebugVal on it. */
void debugVal(DebugVal *dv);
/* convenience to update a DebugVal with int value */
void debugVal(DebugVal *dv, char const *name, int val);
/* convenience to update a DebugVal with float value */
void debugVal(DebugVal *dv, char const *name, float val);

DebugVal const *firstDebugVal();
DebugVal const *nextDebugVal(DebugVal const *prev);
DebugVal const *findDebugVal(char const *name);

void software_reset();

#endif  //  Support_h

