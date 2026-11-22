#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

// typedef float _Complex c32;
// typedef double _Complex c64;

// #define U11_MAX ((2 << 10) - 1)

typedef struct {
  f32 real;
  f32 im;
} c32;

typedef struct {
  f64 real;
  f64 im;
} c64;

#define concat_(a, b) a##b
#define concat(a, b) concat_(a, b)

#define slice(type) concat(type, _Slice)
#define create_slice_type(type)                                                \
  typedef struct {                                                             \
    type *ptr;                                                                 \
    u32 len;                                                                   \
  } slice(type)

#define malloc_slice(type, length)                                             \
  {.ptr = malloc(sizeof(type) * (length)), .len = (length)}

#define calloc_slice(type, length)                                             \
  {.ptr = calloc(sizeof(type), length), .len = (length)}

#ifdef BUILD_DEBUG
#define assert(val)                                                            \
  do {                                                                         \
    if (!(val)) {                                                              \
      __builtin_trap();                                                        \
    }                                                                          \
  } while (0)
#else
#define assert(val) (void)(val)
#endif

#define static_assert(cond, message)                                           \
  typedef char concat(static_assert_, message)[(cond) ? 1 : -1]
