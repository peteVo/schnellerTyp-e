// SPDX-License-Identifier: MIT

#import <AppKit/AppKit.h>
#import <InputMethodKit/InputMethodKit.h>

#include <QCoreApplication>

int main(int argc, char* argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("schnellerTyp-e"));
    QCoreApplication::setApplicationName(QStringLiteral("schnellerTyp-e"));
    QCoreApplication qtCore(argc, argv);
    (void)qtCore;

    @autoreleasepool {
        NSApplication* application = [NSApplication sharedApplication];
        [application setActivationPolicy:NSApplicationActivationPolicyProhibited];

        NSString* bundleIdentifier = NSBundle.mainBundle.bundleIdentifier;
        IMKServer* server = [[IMKServer alloc]
            initWithName:@"org.schnellertype.inputmethod.Connection"
          bundleIdentifier:bundleIdentifier];
        if (server == nil)
            return 1;
        (void)server;

        [application run];
    }
    return 0;
}
