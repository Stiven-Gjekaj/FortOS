#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/QuartzCore.h>
#import "FOGameViewController.h"
#include "fo_game.h"

// The view of the game is a CAEAGLLayer.
@interface FOGLView : UIView
@end

@implementation FOGLView
+ (Class)layerClass {
    return [CAEAGLLayer class];
}
@end

@implementation FOGameViewController {
    EAGLContext *_context;
    GLuint _framebuffer;
    GLuint _colorbuffer;
    GLuint _depthbuffer;
    GLint _width;
    GLint _height;
    CADisplayLink *_link;
    CFTimeInterval _last;
    BOOL _started;
    UILabel *_message;
}

- (void)loadView {
    FOGLView *view = [[FOGLView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    view.multipleTouchEnabled = YES;
    view.contentScaleFactor = [UIScreen mainScreen].scale;
    CAEAGLLayer *layer = (CAEAGLLayer *)view.layer;
    layer.opaque = YES;
    layer.drawableProperties = @{kEAGLDrawablePropertyRetainedBacking: @NO,
                                 kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8};
    self.view = view;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    [EAGLContext setCurrentContext:_context];
    glGenFramebuffers(1, &_framebuffer);
    glGenRenderbuffers(1, &_colorbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorbuffer);
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    // The storage follows the size of the layer, which is landscape after
    // the rotation.
    [EAGLContext setCurrentContext:_context];
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.view.layer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_height);
    // The depth buffer lets the GPU skip the hidden pixels of the map.
    if (!_depthbuffer) glGenRenderbuffers(1, &_depthbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _width, _height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthbuffer);
    if (_width > _height && !_started) [self start];
}

- (void)start {
    _started = YES;
    NSString *apk = @"/var/mobile/Media/FortOS/GANGFORT.apk";
    if (!fo_game_start(apk.fileSystemRepresentation, _width, _height)) {
        [self showMessage:@(fo_game_error())];
        return;
    }
    _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)showMessage:(NSString *)text {
    _message = [[UILabel alloc] initWithFrame:CGRectInset(self.view.bounds, 24, 24)];
    _message.numberOfLines = 0;
    _message.textAlignment = NSTextAlignmentCenter;
    _message.backgroundColor = [UIColor clearColor];
    _message.textColor = [UIColor whiteColor];
    _message.font = [UIFont boldSystemFontOfSize:15];
    _message.text = text;
    self.view.backgroundColor = [UIColor colorWithRed:0x37 / 255.0 green:0x42 / 255.0 blue:0x44 / 255.0 alpha:1];
    [self.view addSubview:_message];
}

- (void)frame:(CADisplayLink *)link {
    CFTimeInterval now = link.timestamp;
    double dt = _last ? now - _last : 1.0 / 60;
    _last = now;
    [EAGLContext setCurrentContext:_context];
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    fo_game_frame(dt > 0.1 ? 0.1 : dt);
    // The depth is not needed after the frame, so the GPU need not store it.
    static const GLenum discard[] = {GL_DEPTH_ATTACHMENT};
    glDiscardFramebufferEXT(GL_FRAMEBUFFER, 1, discard);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorbuffer);
    [_context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)touch:(NSSet *)touches phase:(int)phase {
    CGFloat scale = self.view.contentScaleFactor;
    for (UITouch *touch in touches) {
        CGPoint point = [touch locationInView:self.view];
        // The address of the UITouch stays the same while the finger is down.
        int identifier = (int)((uintptr_t)(__bridge void *)touch >> 4);
        fo_game_touch(identifier, phase, (float)(point.x * scale), (float)(point.y * scale));
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    [self touch:touches phase:FO_TOUCH_BEGAN];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
    [self touch:touches phase:FO_TOUCH_MOVED];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    [self touch:touches phase:FO_TOUCH_ENDED];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
    [self touch:touches phase:FO_TOUCH_ENDED];
}

- (void)setPaused:(BOOL)paused {
    _link.paused = paused;
    _last = 0;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (BOOL)shouldAutorotate {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    return UIInterfaceOrientationLandscapeRight;
}

@end
