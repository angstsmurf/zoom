//
//  ZoomStoryOrganiser.h
//  ZoomCocoa
//
//  Created by Andrew Hunter on Thu Jan 22 2004.
//  Copyright (c) 2004 Andrew Hunter. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <ZoomPlugIns/ZoomStory.h>
#import <ZoomPlugIns/ZoomStoryID.h>
#import <ZoomView/ZoomBlorbFile.h>


@protocol ZoomStoryIDFetcherProtocol <NSObject>

- (out bycopy ZoomStoryID*) idForFile: (in bycopy NSString*) filename;
- (void) renamedIdent: (in bycopy ZoomStoryID*) ident
		   toFilename: (in bycopy NSString*) filename;

@end

// The story organiser is used to store story locations and identifications
// (Mainly to build up the iFiction window)

// Notifications
extern NSNotificationName const ZoomStoryOrganiserChangedNotification;
extern NSNotificationName const ZoomStoryOrganiserProgressNotification;

@interface ZoomStoryOrganiser : NSObject<ZoomStoryIDFetcherProtocol> {
	// Arrays of the stories and their idents
	NSMutableArray<NSString*>* storyFilenames;
	NSMutableArray<ZoomStoryID*>* storyIdents;
	
	// Dictionaries associating them
	NSMutableDictionary<NSString*,ZoomStoryID*>* filenamesToIdents;
	NSMutableDictionary<ZoomStoryID*,NSString*>* identsToFilenames;
	
	NSLock* storyLock;
	
	// Story organising thread
	BOOL alreadyOrganising;
}

//! The shared organiser
@property (class, readonly, retain) ZoomStoryOrganiser *sharedStoryOrganiser;

// Image management
+ (NSImage*) frontispieceForBlorb: (ZoomBlorbFile*) decodedFile;
+ (NSImage*) frontispieceForFile: (NSString*) filename;

// Storing stories
- (void) addStory: (NSString*) filename
		withIdent: (ZoomStoryID*) ident;
- (void) addStory: (NSString*) filename
		withIdent: (ZoomStoryID*) ident
		 organise: (BOOL) organise;

- (void) removeStoryWithIdent: (ZoomStoryID*) ident
		   deleteFromMetadata: (BOOL) delete;

// Sending notifications
- (void) organiserChanged;

// Retrieving story information
- (NSString*) filenameForIdent: (ZoomStoryID*) ident;
- (ZoomStoryID*) identForFilename: (NSString*) filename;

@property (nonatomic, readonly, copy) NSArray<NSString*> *storyFilenames;
@property (nonatomic, readonly, copy) NSArray<ZoomStoryID*> *storyIdents;

// Story-specific data
- (NSString*) directoryForIdent: (ZoomStoryID*) ident
						 create: (BOOL) create;

// Progress
- (void) startedActing;
- (void) endedActing;

// Organising stories
- (void)      organiseStory: (ZoomStory*) story;
- (void)      organiseStory: (ZoomStory*) story
				  withIdent: (ZoomStoryID*) ident;
- (void)      organiseAllStories;
- (void)      reorganiseStoriesTo: (NSString*) newStoryDirectory;

@end
