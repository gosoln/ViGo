#import <Foundation/Foundation.h>

#import "../../NAKL/KeyboardHandler.h"

enum {
    LegacyMethodVni = 1,
    LegacyMethodTelex = 2,
};

static void printUsage(void)
{
    fprintf(stderr, "usage: legacy-oracle <telex|vni> <input>\n");
}

static int methodFromArgument(const char *argument)
{
    if (strcmp(argument, "telex") == 0) {
        return LegacyMethodTelex;
    }
    if (strcmp(argument, "vni") == 0) {
        return LegacyMethodVni;
    }
    return 0;
}

static NSString *runOracle(KeyboardHandler *handler, NSString *input)
{
    NSMutableString *output = [NSMutableString string];
    [handler clearBuffer];

    for (NSUInteger index = 0; index < [input length]; index++) {
        UniChar character = [input characterAtIndex:index];
        int result = [handler addKey:character];

        if (result < 0) {
            [output appendFormat:@"%C", character];
            continue;
        }

        NSUInteger deleteCount = (NSUInteger)handler.kbPLength;
        if (deleteCount > [output length]) {
            fprintf(stderr, "oracle produced an invalid delete count\n");
            exit(2);
        }

        [output deleteCharactersInRange:NSMakeRange([output length] - deleteCount, deleteCount)];
        NSString *replacement = [NSString stringWithCharacters:handler.kbBuffer + BACKSPACE_BUFFER
                                                         length:(NSUInteger)handler.kbBLength];
        [output appendString:replacement];
    }

    return output;
}

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        if (argc != 3) {
            printUsage();
            return 64;
        }

        int method = methodFromArgument(argv[1]);
        if (method == 0) {
            printUsage();
            return 64;
        }

        NSString *input = [NSString stringWithUTF8String:argv[2]];
        if (input == nil) {
            fprintf(stderr, "input must be valid UTF-8\n");
            return 65;
        }

        KeyboardHandler *handler = [[KeyboardHandler alloc] init];
        handler.kbMethod = method;
        NSString *output = runOracle(handler, input);
        printf("%s\n", [output UTF8String]);
        [handler release];
    }
    return 0;
}
