// SPDX-License-Identifier: MIT

#import <Carbon/Carbon.h>
#import <InputMethodKit/InputMethodKit.h>

#include "app/NativeEngine.hpp"
#include "core/CompositionSession.hpp"
#include "core/Unicode.hpp"

#include <exception>
#include <memory>
#include <string>

@interface STInputMethodController : IMKInputController {
@private
    std::unique_ptr<st::CompositionSession> session_;
}
@end

namespace {

NSString* toNSString(const std::u32string& text)
{
    const std::u16string utf16 = st::unicode::toUtf16(text);
    return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(utf16.data())
                                    length:utf16.size()];
}

st::KeyEvent mapEvent(NSEvent* event)
{
    st::KeyEvent key;
    const NSEventModifierFlags flags = event.modifierFlags;
    key.ctrl  = (flags & NSEventModifierFlagControl) != 0;
    key.alt   = (flags & NSEventModifierFlagOption) != 0;
    key.meta  = (flags & NSEventModifierFlagCommand) != 0;
    key.shift = (flags & NSEventModifierFlagShift) != 0;
    key.rawCode = event.keyCode;

    switch (event.keyCode) {
    case kVK_Delete: key.kind = st::KeyKind::Backspace; return key;
    case kVK_Return:
    case kVK_ANSI_KeypadEnter: key.kind = st::KeyKind::Enter; return key;
    case kVK_Tab: key.kind = st::KeyKind::Tab; return key;
    case kVK_Escape: key.kind = st::KeyKind::Escape; return key;
    case kVK_LeftArrow:
    case kVK_RightArrow:
    case kVK_UpArrow:
    case kVK_DownArrow:
    case kVK_Home:
    case kVK_End:
    case kVK_PageUp:
    case kVK_PageDown: key.kind = st::KeyKind::Navigation; return key;
    default: break;
    }

    NSString* characters = event.characters;
    NSData* utf8 = [characters dataUsingEncoding:NSUTF8StringEncoding];
    if (utf8.length == 0)
        return key;

    const auto decoded = st::unicode::fromUtf8(
        std::string(static_cast<const char*>(utf8.bytes), utf8.length));
    if (decoded.empty())
        return key;
    key.character = decoded.front();
    if (key.character == U' ')
        key.kind = st::KeyKind::Space;
    else
        key.kind = st::KeyKind::Character;
    return key;
}

void insertCommit(id<IMKTextInput> client, const std::u32string& text)
{
    NSString* value = toNSString(text);
    [client insertText:value replacementRange:NSMakeRange(NSNotFound, 0)];
}

} // namespace

@implementation STInputMethodController

- (NSUInteger)recognizedEvents:(id)sender
{
    (void)sender;
    return NSEventMaskKeyDown;
}

- (void)activateServer:(id)sender
{
    [super activateServer:sender];
    try {
        session_ = std::make_unique<st::CompositionSession>(st::loadNativeEngine());
    } catch (const std::exception& error) {
        NSLog(@"schnellerTyp-e: could not load input method engine: %s", error.what());
        session_.reset();
    } catch (...) {
        NSLog(@"schnellerTyp-e: could not load input method engine");
        session_.reset();
    }
}

- (void)finishCompositionForClient:(id<IMKTextInput>)client
{
    if (!session_)
        return;
    try {
        const auto result = session_->finish();
        if (result.commit && !result.text.empty())
            insertCommit(client, result.text);
    } catch (const std::exception& error) {
        NSLog(@"schnellerTyp-e: could not finish composition: %s", error.what());
        session_->reset();
    } catch (...) {
        NSLog(@"schnellerTyp-e: could not finish composition");
        session_->reset();
    }
}

- (BOOL)handleEvent:(NSEvent*)event client:(id)sender
{
    if (event.type != NSEventTypeKeyDown)
        return NO;

    id<IMKTextInput> client = (id<IMKTextInput>)sender;

    // Carbon exposes only the system-wide secure-input state, not which field
    // enabled it. Bypass transformation while it is active and discard preedit
    // rather than committing it into a newly focused secure field.
    if (IsSecureEventInputEnabled()) {
        if (session_)
            session_->reset();
        return NO;
    }

    const st::KeyEvent key = mapEvent(event);
    if (key.isChord()) {
        [self finishCompositionForClient:client];
        return NO;
    }

    if (!session_)
        return NO;

    try {
        const auto result = session_->processKey(key);
        if (result.commit) {
            if (!result.text.empty())
                insertCommit(client, result.text);
            return result.handled ? YES : NO;
        }
        if (result.handled) {
            NSString* preedit = toNSString(result.text);
            [client setMarkedText:preedit
                    selectedRange:NSMakeRange(preedit.length, 0)
                  replacementRange:NSMakeRange(NSNotFound, 0)];
            return YES;
        }
    } catch (const std::exception& error) {
        NSLog(@"schnellerTyp-e: input engine failed: %s", error.what());
        session_->reset();
        [client setMarkedText:@"" selectedRange:NSMakeRange(0, 0)
              replacementRange:NSMakeRange(NSNotFound, 0)];
    } catch (...) {
        NSLog(@"schnellerTyp-e: input engine failed");
        session_->reset();
        [client setMarkedText:@"" selectedRange:NSMakeRange(0, 0)
              replacementRange:NSMakeRange(NSNotFound, 0)];
    }
    return NO;
}

- (void)deactivateServer:(id)sender
{
    [self finishCompositionForClient:(id<IMKTextInput>)self.client];
    session_.reset();
    [super deactivateServer:sender];
}

- (void)doCommandBySelector:(SEL)selector commandDictionary:(NSDictionary*)info
{
    [self finishCompositionForClient:(id<IMKTextInput>)self.client];
    [super doCommandBySelector:selector commandDictionary:info];
}

- (void)inputControllerWillClose
{
    [self finishCompositionForClient:(id<IMKTextInput>)self.client];
    session_.reset();
    [super inputControllerWillClose];
}

@end
