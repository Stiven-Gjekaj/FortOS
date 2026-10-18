// FOTest: a small test runner that operates on macOS and on iOS 6.
// Each test is a function that FO_TEST makes and registers at load time.

#import <Foundation/Foundation.h>

typedef void (*FOTestFunction)(void);

void FOTestRegister(const char *name, FOTestFunction function);
void FOTestFail(const char *file, int line, NSString *message);
void FOTestSkip(void);

#define FO_TEST(name)                                                         \
    static void name(void);                                                   \
    __attribute__((constructor)) static void name##_register(void) {          \
        FOTestRegister(#name, name);                                          \
    }                                                                         \
    static void name(void)

#define FO_EXPECT(condition)                                                  \
    do {                                                                      \
        if (!(condition)) FOTestFail(__FILE__, __LINE__, @"expected: " #condition); \
    } while (0)

#define FO_EQUAL(actual, expected)                                            \
    do {                                                                      \
        id fo_a = (actual), fo_e = (expected);                                \
        if (!(fo_a == fo_e || [fo_a isEqual:fo_e]))                           \
            FOTestFail(__FILE__, __LINE__, [NSString stringWithFormat:        \
                @"%s is <%@>, expected <%@>", #actual, fo_a, fo_e]);           \
    } while (0)

#define FO_EQUAL_INT(actual, expected)                                        \
    do {                                                                      \
        long long fo_a = (long long)(actual), fo_e = (long long)(expected);   \
        if (fo_a != fo_e)                                                     \
            FOTestFail(__FILE__, __LINE__, [NSString stringWithFormat:        \
                @"%s is %lld, expected %lld", #actual, fo_a, fo_e]);           \
    } while (0)

// Runs the run loop until done is YES or the time is over.
BOOL FOTestWait(BOOL *done, NSTimeInterval seconds);

// Makes a new empty directory for one test and returns its path.
NSString *FOTestTemporaryDirectory(void);
