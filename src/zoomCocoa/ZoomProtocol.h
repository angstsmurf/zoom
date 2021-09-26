/*
 *  A Z-Machine
 *  Copyright (C) 2000 Andrew Hunter
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * Protocol for an application to talk to/from Zoom
 */

#import <Cocoa/Cocoa.h>

extern NSNotificationName const ZBufferNeedsFlushingNotification;

@protocol ZMachine;
@protocol ZDisplay;
@protocol ZFile;
@class ZStyle;
@class ZBuffer;

typedef NS_ENUM(NSInteger, ZFileType) {
    ZFileQuetzal,
    ZFileTranscript,
    ZFileRecording,
    ZFileData
};

typedef NS_OPTIONS(unsigned int, ZValueTypeMasks) {
	ZValueRoutine = 1,
	ZValueObject  = 2,
	ZValueClass   = 4,
	ZValueString  = 8,
	ZValueArray   = 16,
	ZValueAction  = 32,
};

// == Server-side objects ==
@protocol ZMachine <NSObject>

// Setup
- (void) loadStoryFile: (in bycopy NSData*) storyFile;

// Restoring game state (returns nil if successful)
- (bycopy NSString*) restoreSaveState: (in bycopy NSData*) gameSave;

// Running
- (oneway void) startRunningInDisplay: (in byref NSObject<ZDisplay>*) display;

// Recieving text/characters
- (oneway void) inputText: (in bycopy NSString*) text;
- (oneway void) inputTerminatedWithCharacter: (unsigned int) termChar;
- (oneway void) inputMouseAtPositionX: (int) x
                                    Y: (int) y;

- (void) displaySizeHasChanged;

// Recieving files
- (oneway void) filePromptCancelled;
- (oneway void) promptedFileIs: (in byref NSObject<ZFile>*) file
                          size: (NSInteger) size;

// Obtaining game state
- (bycopy NSData*) createGameSave;
- (bycopy NSData*) storyFile;

// Debugging
- (void) loadDebugSymbolsFrom: (NSString*) symbolFile
			   withSourcePath: (NSString*) sourcePath;

- (void) continueFromBreakpoint;
- (void) stepFromBreakpoint;
- (void) stepIntoFromBreakpoint;
- (void) finishFromBreakpoint;

- (bycopy NSData*) staticMemory;
- (int)    evaluateExpression: (NSString*) expression;
- (void)   setBreakpointAt: (int) address;
- (BOOL)   setBreakpointAtName: (NSString*) name;
- (void)   removeBreakpointAt: (int) address;
- (void)   removeBreakpointAtName: (NSString*) name;
- (void)   removeAllBreakpoints;

- (int)        addressForName: (NSString*) name;
- (NSString*)  nameForAddress: (int) address;

- (NSString*)   sourceFileForAddress: (int) address;
- (NSString*)   routineForAddress: (int) address;
- (int)         lineForAddress: (int) address;
- (int)			characterForAddress: (int) address;

- (unsigned)			 typeMasksForValue: (unsigned) value;
- (int)					 zRegion: (int) addr;
- (bycopy NSString*) descriptionForValue: (unsigned) value;

@end

// == Client-side objects ==
@protocol ZFile <NSObject>
- (int)				   readByte;
- (unsigned int)	   readWord;
- (unsigned int)	   readDWord;
- (bycopy NSData*) readBlock: (NSInteger) length;

- (oneway void)		   seekTo: (off_t) pos;

- (oneway void) writeByte:  (int) byte;
- (oneway void) writeWord:  (int) word;
- (oneway void) writeDWord: (unsigned int) dword;
- (oneway void) writeBlock: (in bycopy NSData*) block;

@property (readonly) BOOL sufferedError;
- (bycopy NSString*)    errorMessage;

@property (readonly) off_t fileSize;
@property (readonly) BOOL endOfFile;
- (BOOL)		endOfFile;

- (oneway void) close;
@end

//! General Z-Machine window protocol (all windows should have this and another
//! protocol)
@protocol ZWindow <NSObject>
//! Clears the window
- (oneway void) clearWithStyle: (in bycopy ZStyle*) style;

//! Sets the input focus to this window
- (oneway void) setFocus;

//! Sending data to a window
- (oneway void) writeString: (in bycopy NSString*) string
                  withStyle: (in bycopy ZStyle*) style;

//! Setting the style that text should be input in
- (oneway void) setInputStyle: (in bycopy ZStyle*) inputStyle;
- (bycopy ZStyle*) inputStyle;
@property (nonatomic, copy) ZStyle *inputStyle;

@end

//! Functions supported by an upper window
@protocol ZUpperWindow <ZWindow>

//! Size (-1 to indicate an unsplit window)
- (oneway void) startAtLine: (int) line;
- (oneway void) endAtLine:   (int) line;

//! Cursor positioning
- (oneway void) setCursorPositionX: (in int) xpos
                                 Y: (in int) ypos;
- (NSPoint) cursorPosition;

//! Line erasure
- (oneway void) eraseLineWithStyle: (in bycopy ZStyle*) style;
@end

@protocol ZLowerWindow <ZWindow>
@end

//! Pixmap windows are used by version 6 Z-Machines
//! You can't combine pixmap and ordinary windows (as yet)
@protocol ZPixmapWindow <ZWindow>

//! Sets the size of this window
- (void) setSize: (in NSSize) windowSize;

//! Plots a rectangle in a given style
- (void) plotRect: (in NSRect) rect
		withStyle: (in bycopy ZStyle*) style;

//! Plots some text of a given size at a given point
- (void) plotText: (in bycopy NSString*) text
		  atPoint: (in NSPoint) point
		withStyle: (in bycopy ZStyle*) style;

//! Gets information about a font
- (void) getInfoForStyle: (in bycopy ZStyle*) style
				   width: (out CGFloat*) width
				  height: (out CGFloat*) height
				  ascent: (out CGFloat*) ascent
				 descent: (out CGFloat*) descent;
- (bycopy NSDictionary*) attributesForStyle: (in bycopy ZStyle*) style;

//! Reading information about the pixmap
- (bycopy NSColor*) colourAtPixel: (NSPoint) point;

//! Scrolls a region of the screen
- (void) scrollRegion: (in NSRect) region
			  toPoint: (in NSPoint) newPoint;

//! Measures a string
- (NSSize) measureString: (in bycopy NSString*) string
			   withStyle: (in bycopy ZStyle*) style;

//! Sets the input position in the window
- (void) setInputPosition: (NSPoint) point
				withStyle: (in bycopy ZStyle*) style;

//! Images
- (void) plotImageWithNumber: (in int) number
					 atPoint: (in NSPoint) point;

@end

//! Overall display functions
@protocol ZDisplay <NSObject>

- (void) zMachineHasRestarted;

// Display information
- (void) dimensionX: (out int*) xSize
                  Y: (out int*) ySize;
- (void) pixmapX: (out int*) xSize
			   Y: (out int*) ySize;
- (void) fontWidth: (out int*) width
			height: (out int*) height;

- (int) foregroundColour;
- (int) backgroundColour;

- (int) interpreterVersion;
- (int) interpreterRevision;

// Functions to create the standard windows
- (byref NSObject<ZLowerWindow>*) createLowerWindow;
- (byref NSObject<ZUpperWindow>*) createUpperWindow;
- (byref NSObject<ZPixmapWindow>*) createPixmapWindow;

//! Requesting user input
- (void)		shouldReceiveCharacters;
- (void)		shouldReceiveText: (in int) maxLength;
- (void)        stopReceiving;
- (bycopy NSString*) receivedTextToDate;

//! Ask the display to backtrack over some input that may already be on the screen
- (bycopy NSString*) backtrackInputOver: (in bycopy NSString*) prefix;

- (oneway void) setTerminatingCharacters: (in bycopy NSSet<NSNumber*>*) characters;

- (void) displayMore: (BOOL) shown;

// !'Exclusive' mode - lock the UI so no updates occur while we're sending
//! large blocks of varied text
- (oneway void) startExclusive;
- (oneway void) stopExclusive;
- (oneway void) flushBuffer: (in bycopy ZBuffer*) toFlush;

//! Prompting for files
- (void) promptForFileToWrite: (in ZFileType) type
				  defaultName: (in bycopy NSString*) name;
- (void) promptForFileToRead: (in ZFileType) type
                 defaultName: (in bycopy NSString*) name;

//! Error messages and warnings
- (void) displayFatalError: (in bycopy NSString*) error;
- (void) displayWarning:    (in bycopy NSString*) warning;

//! Debugging
- (void) hitBreakpointAt: (int) programCounter;

// Resources
- (BOOL)   containsImageWithNumber: (int) number;
- (NSSize) sizeOfImageWithNumber: (int) number;

//! Sound (such as Zoom's support is at the moment)
- (void)  beep;

@end

// Some useful standard classes

//! File from a handle
@interface ZHandleFile : NSObject<ZFile> {
    NSFileHandle* handle;
}

- (id) initWithFileHandle: (NSFileHandle*) handle;
@end

//! File from data stored in memory
@interface ZDataFile : NSObject<ZFile> {
    NSData* data;
    NSInteger pos;
}

- (id) initWithData: (NSData*) data;
@end

//! File(s) from a package
@interface ZPackageFile : NSObject<ZFile> {
	NSFileWrapper* wrapper;
	BOOL forWriting;
	NSString* writePath;
	NSString* defaultFile;
	
	NSFileWrapper* data;
	NSMutableData* writeData;
	
	NSDictionary* attributes;
	
	off_t pos;
}

- (id) initWithPath: (NSString*) path
		defaultFile: (NSString*) filename
		 forWriting: (BOOL) write;

- (void) setAttributes: (NSDictionary*) attributes;

- (void) addData: (NSData*) data
	 forFilename: (NSString*) filename;
- (NSData*) dataForFile: (NSString*) filename;

@end

//! Style attributes
extern NSString* const ZStyleAttributeName;
@interface ZStyle : NSObject<NSCopying, NSSecureCoding> {
    // Colour
    int foregroundColour;
    int backgroundColour;
    NSColor* foregroundTrue;
    NSColor* backgroundTrue;

    // Style
    BOOL isReversed;
    BOOL isFixed;
    BOOL isBold;
    BOOL isUnderline;
    BOOL isSymbolic;
	
	BOOL isForceFixed;
}

@property int foregroundColour;
@property int backgroundColour;
@property (retain) NSColor *foregroundTrue;
@property (retain) NSColor *backgroundTrue;
@property (getter=isReversed) BOOL reversed;
@property (nonatomic, getter=isFixed) BOOL fixed;
@property (getter=isForceFixed) BOOL forceFixed;
@property (getter=isBold) BOOL bold;
@property (getter=isUnderline) BOOL underline;
@property (getter=isSymbolic) BOOL symbolic;

@end

//! Buffering
@interface ZBuffer : NSObject<NSCopying,NSSecureCoding> {
    NSMutableArray* buffer;
	int bufferCount;
}

// Buffering

//! Notifications
- (void) addedToBuffer;

//! General window routines
- (void) writeString: (NSString*) string
           withStyle: (ZStyle*) style
            toWindow: (NSObject<ZWindow>*) window;
- (void) clearWindow: (NSObject<ZWindow>*) window
           withStyle: (ZStyle*) style;

//! Upper window routines
- (void) moveTo: (NSPoint) newCursorPos
       inWindow: (NSObject<ZUpperWindow>*) window;
- (void) eraseLineInWindow: (NSObject<ZUpperWindow>*) window
                 withStyle: (ZStyle*) style;
- (void) setWindow: (NSObject<ZUpperWindow>*) window
         startLine: (int) startLine
           endLine: (int) endLine;

//! Pixmap window routines
- (void) plotRect: (NSRect) rect
		withStyle: (ZStyle*) style
		 inWindow: (NSObject<ZPixmapWindow>*) win;
- (void) plotText: (NSString*) text
		  atPoint: (NSPoint) point
		withStyle: (ZStyle*) style
		 inWindow: (NSObject<ZPixmapWindow>*) win;
- (void) scrollRegion: (NSRect) region
			  toPoint: (NSPoint) newPoint
			 inWindow: (NSObject<ZPixmapWindow>*) win;
- (void) plotImage: (int) number
		   atPoint: (NSPoint) point
		  inWindow: (NSObject<ZPixmapWindow>*) win;

// Unbuffering
- (BOOL) empty; //!< YES if the buffer has no data
- (void) blat; //!< Like blitting, only messier

@end

//! Connecting to the client
@protocol ZClient <NSObject>
- (byref id<ZDisplay>) connectToDisplay: (in byref id<ZMachine>) zMachine;
@end
