# C++ header shims for iOS 6

The iPhoneOS 9.3 SDK of Theos has no C++ standard headers. Box2D needs
only three of them: `<algorithm>` for `std::sort`, `<new>` for placement
new, and `<memory>`. The files in this directory give these parts, so
that Box2D builds for armv7 with no change to its files. The phone builds
put this directory on the include path. The Mac tests use the headers of
Xcode.
