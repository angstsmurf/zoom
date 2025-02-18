//
//  ZoomSkeinXML.m
//  ZoomCocoa
//
//  Created by Andrew Hunter on Thu Jul 01 2004.
//  Copyright (c) 2004 Andrew Hunter. All rights reserved.
//

#import "ZoomSkein.h"
#import "ZoomSkeinInternal.h"

NSErrorDomain const ZoomSkeinXMLParserErrorDomain = @"uk.org.logicalshift.zoomview.skein.xmlerrors";

#pragma mark XML input class

static NSString* const xmlAttributes = @"xmlAttributes";
static NSString* const xmlName	     = @"xmlName";
static NSString* const xmlChildren   = @"xmlChildren";
static NSString* const xmlType	     = @"xmlType";
static NSString* const xmlChars      = @"xmlChars";

static NSString* const xmlElement    = @"xmlElement";
static NSString* const xmlCharData   = @"xmlCharData";

typedef NSDictionary<NSString*,id> SkeinXMLElement;

@interface ZoomSkeinXMLInput : NSObject <NSXMLParserDelegate> {
	NSMutableDictionary* result;
	NSMutableArray<NSMutableDictionary*>*      xmlStack;
}

- (BOOL) processXML: (NSData*) xml error: (NSError**) outError;
- (BOOL) processXMLAtURL: (NSURL*) url error: (NSError**) outError;
- (NSDictionary*) processedXML;

- (SkeinXMLElement*) childForElement: (SkeinXMLElement*) element
							withName: (NSString*) elementName;
- (NSArray<SkeinXMLElement*>*) childrenForElement: (SkeinXMLElement*) element
										 withName: (NSString*) elementName;
- (NSString*) innerTextForElement: (SkeinXMLElement*) element;
- (NSString*) attributeValueForElement: (SkeinXMLElement*) element
							  withName: (NSString*) elementName;

@end

@implementation ZoomSkein(ZoomSkeinXML)

#pragma mark - XML data

static NSXMLNode *addAttributeToElement(NSXMLElement *element, NSString *attributeName, NSString *value) {
	NSXMLNode *attributeNode = [[NSXMLNode alloc] initWithKind: NSXMLAttributeKind options: NSXMLNodePrettyPrint];
	attributeNode.name = attributeName;
	attributeNode.stringValue = value;
	[element addAttribute: attributeNode];
	
	return attributeNode;
}

static NSXMLElement *elementWithNameAndAttribute(NSString *elementName, NSString *attributeName, NSString *attributeValue) {
	NSXMLElement *root = [[NSXMLElement alloc] initWithKind: NSXMLElementKind options: NSXMLNodePrettyPrint];
	root.name = elementName;
	addAttributeToElement(root, attributeName, attributeValue);
	
	return root;
}

static NSXMLElement *elementWithNameAndValue(NSString *elementName, NSString *value, BOOL preserveWhitespace) {
	NSXMLNodeOptions options = NSXMLNodePrettyPrint;
	if (preserveWhitespace) {
		options |= NSXMLNodePreserveWhitespace;
	}
	NSXMLElement *root = [[NSXMLElement alloc] initWithKind: NSXMLElementKind options: options];
	if (preserveWhitespace) {
		addAttributeToElement(root, @"xml:space", @"preserve");
	}
	root.name = elementName;
	root.stringValue = value;
	
	return root;
}

#pragma mark Creating XML

/// Creates an XML representation of the Skein.
- (NSString*) xmlData {
	// Structure summary (note to me: write this up properly later)
	
	// <Skein rootNode="<nodeID>" xmlns="http://www.logicalshift.org.uk/IF/Skein">
	//   <generator>Zoom</generator>
	//   <activeNode nodeId="<nodeID>" />
	//   <item nodeId="<nodeID>">
	//     <command/>
	//     <result/>
	//     <annotation/>
	//	   <commentary/>
	//     <played>YES/NO</played>
	//     <changed>YES/NO</changed>
	//     <temporary score="score">YES/NO</temporary>
	//     <children>
	//       <child nodeId="<nodeID>"/>
	//     </children>
	//   </item>
	// </Skein>
	//
	// nodeIDs are string uniquely identifying a node: any format
	// A node must not be a child of more than one item
	// All item fields are optional.
	// Root item usually has the command '- start -'
	
	NSXMLElement *root = elementWithNameAndAttribute(@"Skein", @"rootNode", rootItem.nodeIdentifier.UUIDString);
	[root addNamespace:[NSXMLNode namespaceWithName:@"" stringValue:@"http://www.logicalshift.org.uk/IF/Skein"]];
	
	NSXMLDocument *xmlDoc = [[NSXMLDocument alloc] initWithKind: NSXMLDocumentKind options: NSXMLDocumentTidyXML | NSXMLNodePrettyPrint];
	xmlDoc.version = @"1.0";
	xmlDoc.characterEncoding = @"UTF-8";
	[xmlDoc setRootElement: root];
	
	[root addChild: elementWithNameAndValue(@"generator", @"Zoom", NO)];
	if (self.activeItem) {
		[root addChild: elementWithNameAndAttribute(@"activeNode", @"nodeId", self.activeItem.nodeIdentifier.UUIDString)];
	}
	
	// Write items
	NSMutableArray<ZoomSkeinItem*> *itemStack = [NSMutableArray arrayWithObject: rootItem];
	
	while (itemStack.count > 0) {
		// Pop from the stack
		ZoomSkeinItem* node = [itemStack lastObject];
		[itemStack removeLastObject];
		
		// Push any children of this node
		[itemStack addObjectsFromArray: node.children.allObjects];
		
		// Generate the XML for this node
		NSXMLElement *item = elementWithNameAndAttribute(@"item", @"nodeId", node.nodeIdentifier.UUIDString);
		NSString *testString;
		
		testString = node.command;
		if (testString) {
			[item addChild: elementWithNameAndValue(@"command", testString, YES)];
		}
		
		testString = node.result;
		if (testString) {
			[item addChild: elementWithNameAndValue(@"result", testString, YES)];
		}
		
		testString = node.annotation;
		if (testString) {
			[item addChild: elementWithNameAndValue(@"annotation", testString, YES)];
		}
		
		testString = node.commentary;
		if (testString) {
			[item addChild: elementWithNameAndValue(@"commentary", testString, YES)];
		}
		
		[item addChild: elementWithNameAndValue(@"played", node.played ? @"YES" : @"NO", NO)];
		[item addChild: elementWithNameAndValue(@"changed", node.changed ? @"YES" : @"NO", NO)];
		
		{
			NSXMLElement *score = elementWithNameAndValue(@"temporary", node.temporary ? @"YES" : @"NO", NO);
			addAttributeToElement(score, @"score", [@(node.temporaryScore) stringValue]);
			[item addChild: score];
		}
		
		if (node.children.count > 0) {
			NSMutableArray *children = [NSMutableArray arrayWithCapacity: node.children.count];
			
			for (ZoomSkeinItem *childItem in node.children) {
				[children addObject: elementWithNameAndAttribute(@"child", @"nodeId", childItem.nodeIdentifier.UUIDString)];
			}
			[item addChild: [NSXMLNode elementWithName: @"children" children: children attributes:nil]];
		}
		[root addChild:item];
	}
	
	return [xmlDoc XMLStringWithOptions: NSXMLNodePrettyPrint | NSXMLNodeCompactEmptyElement];
}

#pragma mark - Parsing the XML

- (BOOL) parseXmlData: (NSData*) data error: (NSError**) error {
	ZoomSkeinXMLInput* inputParser = [[ZoomSkeinXMLInput alloc] init];
	NSError *tmpError = nil;
	
	// Process the XML associated with this file
	if (![inputParser processXML: data error: &tmpError]) {
		// Failed to parse
		NSLog(@"ZoomSkein: Failed to parse skein XML data");
		if (error) {
			NSMutableDictionary *errDict = [@{
				NSDebugDescriptionErrorKey: @"ZoomSkein: Failed to parse skein XML data",
				NSLocalizedDescriptionKey: @"ZoomSkein: Failed to parse skein XML data"
			} mutableCopy];
			if (tmpError) {
				errDict[NSUnderlyingErrorKey] = tmpError;
			}
			*error = [NSError errorWithDomain: ZoomSkeinXMLParserErrorDomain
										 code: ZoomSkeinXMLErrorParserFailed
									 userInfo: errDict];
		}
		return NO;
	}
	
	return [self parseXMLInput: inputParser error: error];
}

- (BOOL) parseXMLContentsAtURL: (NSURL*) url error: (NSError**) error {
	ZoomSkeinXMLInput* inputParser = [[ZoomSkeinXMLInput alloc] init];
	NSError *tmpError = nil;
	
	// Process the XML associated with this file
	if (![inputParser processXMLAtURL: url error: &tmpError]) {
		// Failed to parse
		NSLog(@"ZoomSkein: Failed to parse skein XML data");
		if (error) {
			NSMutableDictionary *errDict = [@{
				NSDebugDescriptionErrorKey: @"ZoomSkein: Failed to parse skein XML data",
				NSLocalizedDescriptionKey: @"ZoomSkein: Failed to parse skein XML data",
				NSURLErrorKey: url
			} mutableCopy];
			if (tmpError) {
				errDict[NSUnderlyingErrorKey] = tmpError;
			}
			*error = [NSError errorWithDomain: ZoomSkeinXMLParserErrorDomain
										 code: ZoomSkeinXMLErrorParserFailed
									 userInfo: errDict];
		}
		return NO;
	}
	
	return [self parseXMLInput: inputParser error: error];
}

- (BOOL) parseXMLInput:(ZoomSkeinXMLInput*)inputParser error:(NSError**)outError {
	// OK, actually process the data
	NSDictionary* skein = [inputParser childForElement: [inputParser processedXML]
											  withName: @"Skein"];
	
	if (skein == nil) {
		NSLog(@"ZoomSkein: Failed to find root 'Skein' element");
		if (outError) {
			*outError = [NSError errorWithDomain: ZoomSkeinXMLParserErrorDomain
											code: ZoomSkeinXMLErrorNoRootSkein
										userInfo: @{
				NSDebugDescriptionErrorKey: @"ZoomSkein: Failed to find root 'Skein' element",
				NSLocalizedDescriptionKey: @"ZoomSkein: Failed to find root 'Skein' element"
			}];
		}

		return NO;
	}
	
	// Header fields
	NSString* rootNodeId = [inputParser attributeValueForElement: skein
														withName: @"rootNode"];
	NSString* generator = [inputParser innerTextForElement: [inputParser childForElement: skein
																				withName: @"generator"]];
	NSString* activeNode = [inputParser attributeValueForElement: [inputParser childForElement: skein
																					  withName: @"activeNode"]
														withName: @"nodeId"];
	if (![generator isEqualToString: @"Zoom"]) {
		NSLog(@"ZoomSkein: XML file generated by %@", generator);
	}
	
	if (rootNodeId == nil) {
		NSLog(@"ZoomSkein: No root node ID specified");
		if (outError) {
			*outError = [NSError errorWithDomain: ZoomSkeinXMLParserErrorDomain
											code: ZoomSkeinXMLErrorNoRootNodeID
										userInfo: @{
				NSDebugDescriptionErrorKey: @"ZoomSkein: No root node ID specified",
				NSLocalizedDescriptionKey: @"ZoomSkein: No root node ID specified"
			}];
		}
		return NO;
	}
	
	if (activeNode == nil) {
		NSLog(@"ZoomSkein: Warning: No active node specified");
	}
	
	// Item dictionary: populate with items ready to be linked together
	NSMutableDictionary* itemDictionary = [NSMutableDictionary dictionary];
	
	NSArray<SkeinXMLElement*>* items = [inputParser childrenForElement: skein
															  withName: @"item"];
	
	for (SkeinXMLElement* item in items) {
		NSString* itemNodeId = [inputParser attributeValueForElement: item
															withName: @"nodeId"];
		
		if (itemNodeId == nil) {
			NSLog(@"ZoomSkein: Warning - found item with no ID");
			continue;
		}
		
		NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:itemNodeId];
		
		ZoomSkeinItem* newItem;
		if (uuid) {
			newItem = [[ZoomSkeinItem alloc] initWithCommand: @"- PLACEHOLDER -" identifier: uuid];
		} else {
			// UUID generation failure means old-style, pointer-derived xml.
			newItem = [[ZoomSkeinItem alloc] initWithCommand: @"- PLACEHOLDER -"];
		}
		[itemDictionary setObject: newItem
						   forKey: itemNodeId];
	}
	
	// Item dictionary II: fill in the node data
	for (SkeinXMLElement* item in items) {
		NSString* itemNodeId = [inputParser attributeValueForElement: item
															withName: @"nodeId"];
		
		if (itemNodeId == nil) {
			continue;
		}
		
		ZoomSkeinItem* newItem = itemDictionary[itemNodeId];
		if (newItem == nil) {
			// Should never happen
			// (Hahaha)
			NSString *spoon = [NSString stringWithFormat:@"ZoomSkein: Programmer is a spoon (item ID: %@)", itemNodeId];
			NSLog(@"%@", spoon);
			if (outError) {
				*outError = [NSError errorWithDomain: ZoomSkeinXMLParserErrorDomain
												code: ZoomSkeinXMLErrorProgrammerIsASpoon
											userInfo: @{
					NSDebugDescriptionErrorKey: spoon,
					NSLocalizedDescriptionKey: spoon
				}];
			}
			return NO;
		}
		
		// Item info
		NSString* command = [inputParser innerTextForElement: [inputParser childForElement: item
																				  withName: @"command"]];
		NSString* result = [inputParser innerTextForElement: [inputParser childForElement: item
																				 withName: @"result"]];
		NSString* annotation = [inputParser innerTextForElement: [inputParser childForElement: item
																					 withName: @"annotation"]];
		NSString* commentary = [inputParser innerTextForElement: [inputParser childForElement: item
																					 withName: @"commentary"]];
		BOOL played = [[inputParser innerTextForElement: [inputParser childForElement: item
																			 withName: @"played"]] isEqualToString: @"YES"];
		BOOL changed = [[inputParser innerTextForElement: [inputParser childForElement: item
																			  withName: @"changed"]] isEqualToString: @"YES"];
		BOOL temporary = [[inputParser innerTextForElement: [inputParser childForElement: item
																				withName: @"temporary"]] isEqualToString: @"YES"];
		int  tempVal = [[inputParser attributeValueForElement: [inputParser childForElement: item
																				   withName: @"temporary"]
													 withName: @"score"] intValue];
		
		if (command == nil) {
			NSLog(@"ZoomSkein: Warning: item with no command found");
			command = @"";
		}
		
		[newItem setCommand: command];
		[newItem setResult: result];
		[newItem setAnnotation: annotation];
		[newItem setCommentary: commentary];
		
		[newItem setPlayed: played];
		[newItem setChanged: changed];
		[newItem setTemporary: temporary];
		[newItem setTemporaryScore: tempVal];
	}
	
	// Item dictionary III: fill in the item children
	for (SkeinXMLElement* item in items) {
		NSString* itemNodeId = [inputParser attributeValueForElement: item
															withName: @"nodeId"];
		
		if (itemNodeId == nil) {
			continue;
		}
		
		ZoomSkeinItem* newItem = itemDictionary[itemNodeId];
		if (newItem == nil) {
			// Should never happen
			// (Hahaha)
			NSString *spoon = [NSString stringWithFormat:@"ZoomSkein: Programmer is a spoon (item ID: %@)", itemNodeId];
			NSLog(@"%@", spoon);
			if (outError) {
				*outError = [NSError errorWithDomain: ZoomSkeinXMLParserErrorDomain
												code: ZoomSkeinXMLErrorProgrammerIsASpoon
											userInfo: @{
					NSDebugDescriptionErrorKey: spoon,
					NSLocalizedDescriptionKey: spoon
				}];
			}
			return NO;
		}

		// Item children
		NSArray* itemKids =[inputParser childrenForElement: [inputParser childForElement: item
																				withName: @"children"]
												  withName: @"child"];
		for (SkeinXMLElement* child in itemKids) {
			NSString* kidNodeId = [inputParser attributeValueForElement: child
															   withName: @"nodeId"];
			if (kidNodeId == nil) {
				NSLog(@"ZoomSkein: Warning: Child item with no node id");
				continue;
			}
			
			ZoomSkeinItem* kidItem = itemDictionary[kidNodeId];
			
			if (kidItem == nil) {
				NSLog(@"ZoomSkein: Warning: unable to find node %@", kidNodeId);
				continue;
			}
			
			ZoomSkeinItem* newKid = [newItem addChild: kidItem];
			itemDictionary[kidNodeId] = newKid;
		}
	}
	
	// Root item
	ZoomSkeinItem* newRoot = [itemDictionary objectForKey: rootNodeId];
	if (newRoot == nil) {
		if (outError) {
			*outError = [NSError errorWithDomain: ZoomSkeinXMLParserErrorDomain
											code: ZoomSkeinXMLErrorNoRootNode
										userInfo: @{
				NSDebugDescriptionErrorKey: @"ZoomSkein: No root node",
				NSLocalizedDescriptionKey: @"ZoomSkein: No root node"
			}];
		}
		NSLog(@"ZoomSkein: No root node");
		return NO;
	}
	
	rootItem = newRoot;
	
	if (activeNode != nil)
		self.activeItem = [itemDictionary objectForKey: activeNode];
	else
		self.activeItem = rootItem;
	
	[self zoomSkeinChanged];

	return YES;
}

@end

#pragma mark - XML input helper class

// For later, maybe: develop this into a class in it's own right?
// Would really want custom types for the XML tree, then

@implementation ZoomSkeinXMLInput

- (id) init {
	self = [super init];
	
	if (self) {
	}
	
	return self;
}

- (BOOL) processXML: (NSData*) xml error: (NSError**) outError {
	// Setup our state
	result = [[NSMutableDictionary alloc] init];
	xmlStack = [[NSMutableArray alloc] init];
	
	// Initial element on the stack
	[xmlStack addObject: result];
	
	// Initialise the expat parser
	NSXMLParser *theParser = [[NSXMLParser alloc] initWithData: xml];
	theParser.delegate = self;
	
	// Perform the parsing
	BOOL success = [theParser parse];
	if (!success && outError) {
		*outError = theParser.parserError;
	}
	return success;
}

- (BOOL) processXMLAtURL: (NSURL*) url error: (NSError**) outError {
	// Setup our state
	result = [[NSMutableDictionary alloc] init];
	xmlStack = [[NSMutableArray alloc] init];
	
	// Initial element on the stack
	[xmlStack addObject: result];
	
	// Initialise the expat parser
	NSXMLParser *theParser = [[NSXMLParser alloc] initWithContentsOfURL: url];
	if (!theParser) {
		if (outError) {
			*outError = [NSError errorWithDomain: NSCocoaErrorDomain
											code: NSFileReadUnknownError
										userInfo: @{NSURLErrorKey: url}];
		}
		return NO;
	}
	theParser.delegate = self;
	
	// Perform the parsing
	BOOL success = [theParser parse];
	if (!success && outError) {
		*outError = theParser.parserError;
	}
	return success;
}

- (NSDictionary*) processedXML {
	return result;
}

// In the DOM, would iterate. Doesn't here (shouldn't matter)
- (NSString*) innerTextForElement: (SkeinXMLElement*) element {
	NSMutableString* res = nil;
	
	NSEnumerator* children = [element[xmlChildren] objectEnumerator];
	
	for (SkeinXMLElement* child in children) {
		if ([child[xmlType] isEqualToString: xmlCharData]) {
			if (res == nil) {
				res = [[NSMutableString alloc] initWithString: child[xmlChars]];
			} else {
				[res appendString: child[xmlChars]];
			}
		}
	}
	
	return res;
}

- (NSArray<SkeinXMLElement*>*) childrenForElement: (SkeinXMLElement*) element
										 withName: (NSString*) elementName {
	NSMutableArray* res = nil;
	
	NSEnumerator* children = [element[xmlChildren] objectEnumerator];
	
	for (SkeinXMLElement* child in children) {
		if ([child[xmlType] isEqualToString: xmlElement] &&
			[child[xmlName] isEqualToString: elementName]) {
			if (res == nil) {
				res = [[NSMutableArray alloc] init];
			}
			
			[res addObject: child];
		}
	}
	
	return res;
}

- (SkeinXMLElement*) childForElement: (SkeinXMLElement*) element
							withName: (NSString*) elementName {
	NSEnumerator* children = [element[xmlChildren] objectEnumerator];
	
	for (SkeinXMLElement* child in children) {
		if ([child[xmlType] isEqualToString: xmlElement] &&
			[child[xmlName] isEqualToString: elementName]) {
			return child;
		}
	}
	
	return nil;
}

- (NSString*) attributeValueForElement: (SkeinXMLElement*) element
							  withName: (NSString*) elementName {
	return [element[xmlAttributes] objectForKey: elementName];
}

#pragma mark - NSXML callback messages

-   (void)parser: (NSXMLParser *) parser
 didStartElement: (NSString *) elementName
	namespaceURI: (NSString *) namespaceURI
   qualifiedName: (NSString *) qName
	  attributes: (NSDictionary<NSString *,NSString *> *) attributeDict {
	// Create this element
	NSMutableDictionary* lastElement = [xmlStack lastObject];
	NSMutableDictionary* element = [NSMutableDictionary dictionary];

	element[xmlType] = xmlElement;
	element[xmlName] = elementName;
	
	// Attributes
	if ([attributeDict count] != 0) {
		element[xmlAttributes] = [attributeDict mutableCopy];
	}
	
	// Add as a child of the previous element
	NSMutableArray* children = lastElement[xmlChildren];
	if (children == nil) {
		children = [NSMutableArray array];
		lastElement[xmlChildren] = children;
	}
	[children addObject: element];
	
	// Push this element
	[xmlStack addObject: element];
}

- (void) parser: (NSXMLParser *)parser
  didEndElement: (NSString *)elementName
   namespaceURI: (NSString *)namespaceURI
  qualifiedName: (NSString *)qName {
	// Pop the last element
	[xmlStack removeLastObject];
}

-   (void)parser: (NSXMLParser *)parser
 foundCharacters: (NSString *) string {
	if (string.length == 0) {
		return;
	}
	
	// Create this element
	NSMutableDictionary* lastElement = [xmlStack lastObject];
	NSMutableArray<NSMutableDictionary*>* children = lastElement[xmlChildren];
	NSMutableDictionary* element;
	BOOL addAsChild;
	
	if (children && [[children lastObject][xmlType] isEqualToString: xmlCharData]) {
		element = [children lastObject];
		[element[xmlChars] appendString: string];
		
		addAsChild = NO;
	} else {
		element = [NSMutableDictionary dictionary];
		
		element[xmlType] = xmlCharData;
		element[xmlChars] = [string mutableCopy];
		
		addAsChild = YES;
	}
	
	// Add as a child of the previous element, if required
	if (addAsChild) {
		if (children == nil) {
			children = [NSMutableArray array];
			lastElement[xmlChildren] = children;
		}
		[children addObject: element];
	}
}

@end
