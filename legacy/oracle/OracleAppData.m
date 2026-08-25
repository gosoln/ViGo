#import "../macos-hook/NAKL/AppData.h"

@implementation AppData

@synthesize userPrefs;
@synthesize toggleCombo;
@synthesize switchMethodCombo;
@synthesize shortcuts;
@synthesize shortcutDictionary;
@synthesize excludedApps;

+ (AppData *)sharedAppData
{
    static AppData *instance = nil;
    @synchronized(self) {
        if (instance == nil) {
            instance = [[AppData alloc] init];
        }
    }
    return instance;
}

- (id)init
{
    self = [super init];
    if (self != nil) {
        shortcuts = [[NSMutableArray alloc] init];
        shortcutDictionary = [[NSMutableDictionary alloc] init];
        excludedApps = [[NSMutableDictionary alloc] init];
    }
    return self;
}

+ (void)loadUserPrefs {}
+ (void)loadHotKeys {}
+ (void)loadShortcuts {}
+ (void)loadExcludedApps {}

- (void)dealloc
{
    [shortcuts release];
    [shortcutDictionary release];
    [excludedApps release];
    [super dealloc];
}

@end
