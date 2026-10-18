// The three parts of the C++ runtime that Box2D and fo_phys use, for the
// phone build. With them, the app links no C++ library on iOS 6. The build
// has no exceptions and no RTTI, so nothing else is needed. The Mac tests
// use the C++ library of the Mac, so this file is empty there.

#ifdef FO_NO_CXX_RUNTIME

#include <stdlib.h>

void *operator new(size_t size) {
    void *p = malloc(size ? size : 1);
    if (!p) abort();
    return p;
}

void *operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void *p) throw() {
    free(p);
}

void operator delete[](void *p) throw() {
    free(p);
}

extern "C" void __cxa_pure_virtual(void) {
    abort();
}

#endif
