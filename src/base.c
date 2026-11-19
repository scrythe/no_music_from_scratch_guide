#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#define concat_(a, b) a##b
#define concat(a, b) concat_(a, b)

#define slice(type) concat(type, _Slice)
#define create_slice_type(type)                                                \
  typedef struct {                                                             \
    type *ptr;                                                                 \
    u32 len;                                                                   \
  } slice(type)

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
