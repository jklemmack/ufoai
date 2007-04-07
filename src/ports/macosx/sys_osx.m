/**
 * @file sys_osx.m
 * Written by:	awe				[mailto:awe@fruitz-of-dojo.de]
 * 2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de]
 */

/*

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma mark =Includes=

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>

#import <IOKit/hidsystem/event_status_driver.h>

#include "../../client/client.h"

#pragma mark -


#pragma mark =Macros=

#define SYS_CHECK_MALLOC(MEM_PTR)	if ((MEM_PTR) == NULL) {	\
			Sys_Error("Out of memory!");	\
		}

#define SYS_CHECK_MOUSE_ENABLED()	if ((vid_fullscreen != NULL && vid_fullscreen->value == 0.0f &&		 \
			(_windowed_mouse == NULL || (_windowed_mouse != NULL && _windowed_mouse->value == 0.0f))) ||	 \
			gSysIsDeactivated == YES || gSysIsMinimized->value != 0.0f ||	 \
			(in_mouse == NULL || (in_mouse != NULL && in_mouse->value == 0.0f))) {	\
			return;	\
		}

#define SYS_UFO_DURING			NS_DURING
#define	SYS_UFO_HANDLER			NS_HANDLER		\
		{	\
			NSString	*myException = [localException reason];	\
			if (myException == NULL) {	\
				myException = @"Unknown exception!";	\
			}	\
			NSLog (@"An exception has occured: %@\n", myException);	\
			NSRunCriticalAlertPanel (@"An exception has occured:", myException, NULL, NULL, NULL);	\
			exit (1);								 \
		}									 \
		NS_ENDHANDLER;

#pragma mark -


#pragma mark =Defines=

#define	SYS_STRING_SIZE			1024			// string size for vsnprintf ().
#define	SYS_MOUSE_BUTTONS		5			// number of supported mouse buttons [max. 32].
#define	SYS_DEFAULT_BASE_PATH		@"/Users/bomhoff/temp/ufoai/ufoai/trunk/base/"
#define	SYS_DEFAULT_USE_MP3		@"UFO:AI Use MP3"
#define	SYS_INITIAL_USE_MP3		@"NO"
#define	SYS_DEFAULT_MP3_PATH		@"UFO:AI MP3 Path"
#define	SYS_INITIAL_MP3_PATH		@""
#define	SYS_DEFAULT_OPTION_KEY		@"UFO:AI Dialog Requires Option Key"
#define	SYS_INITIAL_OPTION_KEY		@"NO"
#define SYS_DEFAULT_USE_PARAMETERS	@"UFO:AI Use Command-Line Parameters"
#define SYS_INITIAL_USE_PARAMETERS	@"NO"
#define SYS_DEFAULT_PARAMETERS		@"UFO:AI Command-Line Parameters"
#define SYS_INITIAL_PARAMETERS		@""
#define SYS_BASEUFO_PATH			@"base"
#define	SYS_VALIDATION_FILE1		@"GamePPC.ufoplug/Contents/MacOS/GamePPC"
#define	SYS_VALIDATION_FILE2		@"GamePPC.bundle/Contents/MacOS/GamePPC"
#define SYS_UFOAI_HP		@"http://ufoai.sf.net/"
#define	SYS_SET_COMMAND			"+set"
#define	SYS_GAME_COMMAND		"fs_gamedir"
#define	SYS_CDDIR_COMMAND		"cddir"
#define SYS_COMMAND_BUFFER_SIZE		1024

#pragma mark -

/* Variables */

#pragma mark =Variables=

unsigned int			sys_frame_time;
qboolean 			stdin_active = YES;

static cvar_t 			*gSysNoStdOut = NULL;
static void 			*gSysGameLibrary = NULL;
static int			gSysArgCount;
static char 			**gSysArgValues = NULL;

#ifndef DEDICATED_ONLY

NSString			*gSysMP3Folder;
cvar_t				*gSysIsMinimized = NULL;
BOOL				gSysIsDeactivated = NO,
                                gSysAbortMediaScan = NO;

static int			gSysMsgTime,
                                gSysLastFrameTime;
static BOOL			gSysDenyDrag = NO,
                                gSysWasDragged = NO,
                                gSysHidden = NO,
                                gSysHostInitialized = NO,
                                gSysAllowRunCommand = YES;
static char 			*gSysModDir = NULL,
                                gSysRequestedCommand[SYS_COMMAND_BUFFER_SIZE];
static NSDate			*gSysDistantPast = NULL;
static NSTimer			*gFrameTimer = NULL;

static char			*gSysCDPath[] = {
                                                    "/Volumes/QUAKE2/install/data",
                                                    "/Volumes/Quake2/install/data",
                                                    "/Volumes/QUAKE2/Quake2InstallData",
                                                    NULL
                                                };

UInt8				gSpecialKey[] = {
                                                    K_UPARROW, K_DOWNARROW, K_LEFTARROW, K_RIGHTARROW,
                                                         K_F1,        K_F2,        K_F3,         K_F4,
                                                         K_F5,        K_F6,        K_F7,         K_F8,
                                                         K_F9,       K_F10,       K_F11,        K_F12,
                                                        K_F13,       K_F14,       K_F15,            0,
                                                            0,   	 0, 	      0, 	    0,
                                                            0, 		 0, 	      0, 	    0,
                                                            0, 		 0, 	      0, 	    0,
                                                            0, 		 0, 	      0, 	    0,
                                                            0, 	 	 0, 	      0,        K_INS,
                                                        K_DEL, 	    K_HOME, 	      0, 	K_END,
                                                       K_PGUP,      K_PGDN,	      0,	    0,
                                                      K_PAUSE,		 0,	      0,	    0,
                                                            0,		 0,	      0,	    0,
                                                            0, 	 K_NUMLOCK, 	      0, 	    0,
                                                            0, 		 0, 	      0, 	    0,
                                                            0, 		 0, 	      0, 	    0,
                                                            0, 		 0, 	  K_INS, 	    0
                                                };

static 	UInt8	gNumPadKey[] = 	{
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,	          0,	       0, 	       0,
                                            0,     K_KP_DEL,	       0,      K_KP_STAR,
                                            0,    K_KP_PLUS,	       0,	       0,
                                            0,		  0,	       0,	       0,
                                   K_KP_ENTER,   K_KP_SLASH,  K_KP_MINUS,	       0,
                                            0,   K_KP_EQUALS,    K_KP_INS, 	K_KP_END,
                               K_KP_DOWNARROW,    K_KP_PGDN, K_KP_LEFTARROW, 	  K_KP_5,
                              K_KP_RIGHTARROW,    K_KP_HOME,           0,   K_KP_UPARROW,
                                    K_KP_PGUP,	   	  0,	       0,              0
                                };

#pragma mark -

//_______________________________________________________________________________________________________iNTERFACES

#pragma mark =ObjC Interfaces=

//_________________________________________________________________________________________________iNTERFACE_Quake2

@interface UFO : NSObject
{
	IBOutlet NSWindow			*mediascanWindow;

	IBOutlet NSTextField		*mediascanText;
	IBOutlet NSProgressIndicator	*mediascanProgressIndicator;

	IBOutlet NSWindow			*startupWindow;

	IBOutlet NSButton			*mp3CheckBox;
	IBOutlet NSButton			*mp3Button;
	IBOutlet NSTextField		*mp3TextField;

	IBOutlet NSButton			*optionCheckBox;
	IBOutlet NSButton			*parameterCheckBox;
	IBOutlet NSTextField		*parameterTextField;
	IBOutlet NSMenuItem			*pasteMenuItem;

	IBOutlet NSView			*mp3HelpView;

	NSOpenPanel				*gMP3Panel;

	BOOL				gOptionPressed;
}

+ (void) initialize;
- (void) dealloc;

- (BOOL) application: (NSApplication *) theSender openFile: (NSString *) theFilePath;
- (void) applicationDidResignActive: (NSNotification *) theNote;
- (void) applicationDidBecomeActive: (NSNotification *) theNote;
- (void) applicationWillHide: (NSNotification *) theNote;
- (void) applicationDidFinishLaunching: (NSNotification *) theNote;
- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) theSender;

- (void) checkForOptionKey;
- (void) setupDialog: (NSTimer *) theTimer;
- (void) saveCheckBox: (NSButton *) theButton initial: (NSString *) theInitial
              default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults;
- (void) saveString: (NSString *) theString initial: (NSString *) theInitial
            default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults;
- (void) stringToParameters: (NSString *) theString;
- (BOOL) isEqualTo: (NSString *) theString;
- (void) installFrameTimer;
- (void) renderFrame: (NSTimer *) theTimer;
- (void) scanMediaThread: (id) theSender;
- (void) fireFrameTimer: (NSNotification *) theNotification;

- (IBAction) pasteString: (id) theSender;
- (IBAction) startUFO: (id) theSender;
- (IBAction) visitFOD: (id) theSender;
- (IBAction) toggleParameterTextField: (id) theSender;
- (IBAction) toggleMP3Playback: (id) theSender;
- (IBAction) selectMP3Folder: (id) theSender;
- (IBAction) stopMediaScan: (id) theSender;

- (void) closeMP3Sheet: (NSOpenPanel *) theSheet returnCode: (int) theCode contextInfo: (void *) theInfo;
- (void) connectToServer: (NSPasteboard *) thePasteboard userData:(NSString *) theData error:(NSString **)theError;

@end

/**
 * @brief
 */
@interface UFOApplication : NSApplication

- (void) sendEvent: (NSEvent *) theEvent;
- (void) sendSuperEvent: (NSEvent *) theEvent;

- (void) handleRunCommand: (NSScriptCommand *) theCommand;
- (void) handleConsoleCommand: (NSScriptCommand *) theCommand;

@end

#pragma mark -

//______________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

//extern	void	M_Menu_Quit_f (void);
extern	void	Key_Event (int key, qboolean down, unsigned time);
extern	void	IN_SetKeyboardRepeatEnabled (BOOL theState);
extern	void	IN_SetF12EjectEnabled (qboolean theState);
extern	void	IN_ShowCursor (BOOL theState);
extern	void	IN_ReceiveMouseMove (CGMouseDelta theDeltaX, CGMouseDelta theDeltaY);
extern  BOOL	CDAudio_GetTrackList (void);
extern	void	CDAudio_Enable (BOOL theState);
extern qboolean	SNDDMA_ReserveBufferSize (void);
// extern	void	VID_SetPaused (BOOL theState); // FIXME Re-enable after finding the function :)

int		Sys_CheckSpecialKeys (int theKey);

static	void	Sys_HideApplication_f (void);
static	void	Sys_CheckForIDDirectory (void);
static	void	Sys_DoEvents (NSEvent *myEvent, NSEventType myType);

#endif /* !DEDICATED_ONLY */

static	BOOL	Sys_OpenGameAPI (const char *theGameName, char *thePath, char *theCurPath);

#pragma mark -

/**
 * @brief
 */
void Sys_Error (const char *error, ...)
{
	va_list myArgPtr;
	char myString[SYS_STRING_SIZE];

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	va_start(myArgPtr, error);
	Q_vsnprintf(myString, SYS_STRING_SIZE, error, myArgPtr);
	va_end(myArgPtr);

#ifdef DEDICATED_ONLY
	fprintf(stderr, "Error: %s\n", myString);
	exit(1);
#else
	NSLog(@"An error has occured: %@\n", [NSString stringWithCString: myString]);
	CL_Shutdown();
	Qcommon_Shutdown();
	gSysHostInitialized = NO;

	IN_SetKeyboardRepeatEnabled(YES);
	IN_SetF12EjectEnabled(YES);

	NSRunCriticalAlertPanel(@"An error has occured:", [NSString stringWithCString: myString],
							NULL, NULL, NULL);

	exit(1);
#endif /* DEDICATED_ONLY */
}

/**
 * @brief
 */
void Sys_Quit (void)
{
	CL_Shutdown();
	Qcommon_Shutdown();

#ifndef DEDICATED_ONLY
	gSysHostInitialized = NO;

	IN_SetKeyboardRepeatEnabled(YES);
	IN_SetF12EjectEnabled(YES);
#endif /* DEDICATED_ONLY */
	exit(0);
}

/**
 * @brief
 */
void Sys_UnloadGame (void)
{
	if (gSysGameLibrary != NULL) {
		dlclose(gSysGameLibrary);
	}
	gSysGameLibrary = NULL;
}

/**
 * @brief
 */
static BOOL Sys_OpenGameAPI (const char *theGameName, char *thePath, char *theCurPath)
{
	char myName[MAXPATHLEN];

	snprintf(myName, MAXPATHLEN, "%s/%s/%s", theCurPath, thePath, theGameName);
	Com_Printf("Trying to load library (%s)\n", myName);

	gSysGameLibrary = dlopen(myName, RTLD_NOW );
	if (gSysGameLibrary != NULL) {
		Com_DPrintf("LoadLibrary (%s)\n", myName);
		return (YES);
	}
	return (NO);
}

/**
 * @brief
 */
game_export_t *Sys_GetGameAPI (game_import_t *theParameters)
{
	void *	(*myGameAPI) (void *);
	char	myCurPath[MAXPATHLEN];
	char	*myPath;

	if (gSysGameLibrary != NULL) {
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");
	}

	getcwd(myCurPath, sizeof (myCurPath));

	Com_Printf("------ Loading GamePPC.ufoplug ------\n");

	// now run through the search paths
	myPath = NULL;
	while (1) {
		myPath = FS_NextPath(myPath);
		if (myPath == NULL)
			return (NULL);

		if (Sys_OpenGameAPI("../debugppc/game.dylib", "", myCurPath) == YES)
			break;
		// we try both extensions since we need to be compatible with the pre v1.0.6 ".bundle" extension:
		if (Sys_OpenGameAPI("GamePPC.ufoplug/Contents/MacOS/GamePPC", myPath, myCurPath) == YES)
			break;
		if (Sys_OpenGameAPI("GamePPC.bundle/Contents/MacOS/GamePPC", myPath, myCurPath) == YES)
			break;
	}

	myGameAPI = (void *) dlsym(gSysGameLibrary, "GetGameAPI");
	if (myGameAPI == NULL) {
		Sys_UnloadGame();
		return (NULL);
	}

	return (myGameAPI(theParameters));
}

/**
 * @brief
 */
char *Sys_ConsoleInput (void)
{
	static char 	myText[256];
	int     		myLength;
	fd_set		myFDSet;
	struct timeval	myTimeOut;

	if (!dedicated || !dedicated->value) {
		return NULL;
	}

	if (!stdin_active) {
		return NULL;
	}

	FD_ZERO (&myFDSet);
	FD_SET (0, &myFDSet);
	myTimeOut.tv_sec = 0;
	myTimeOut.tv_usec = 0;
	if (select(1, &myFDSet, NULL, NULL, &myTimeOut) == -1 || !FD_ISSET (0, &myFDSet)) {
		return (NULL);
	}

	myLength = read(0, myText, sizeof (myText));
	if (myLength == 0) {
		stdin_active = false;
		return (NULL);
	}

	if (myLength < 1) {
		return (NULL);
	}
	myText[myLength - 1] = 0x00;

	return (myText);
}

/**
 * @brief
 */
void Sys_ConsoleOutput (const char *theString)
{
#ifdef DEDICATED_ONLY
	unsigned char	*myChar;

	if (theString == NULL) {
		return;
	}

	if (gSysNoStdOut != NULL && gSysNoStdOut->value != 0.0) {
		return;
	}

	for (myChar = (unsigned char *) theString; *myChar != 0x00; myChar++) {
		*myChar &= SCHAR_MAX;
		if ((*myChar > 128 || *myChar < 32) && *myChar != 10 && *myChar != 13 && *myChar != 9) {
			fprintf(stdout, "[%02x]", *myChar);
		} else {
			putc(*myChar, stdout);
		}
	}
	fflush(stdout);
#else
	return;
#endif /* DEDICATED_ONLY */
}

/**
 * @brief
 */
void Sys_SendKeyEvents (void)
{
	sys_frame_time = Sys_Milliseconds();
}

/**
 * @brief
 */
void Sys_AppActivate (void)
{
	/* not used! */
}

/**
 * @brief
 */
void Sys_Init (void)
{
	Cvar_Get("sys_os", "maxosx", CVAR_SERVERINFO, NULL);
#ifndef DEDICATED_ONLY
	gSysIsMinimized = Cvar_Get("_miniwindow", "0", 0, NULL);
	Cmd_AddCommand("sys_hide", Sys_HideApplication_f, NULL);
#endif /* !DEDICATED_ONLY */
}

#ifndef DEDICATED_ONLY

/**
 * @brief
 */
char *Sys_GetClipboardData (void)
{
	NSPasteboard *myPasteboard = NULL;
	NSArray *myPasteboardTypes = NULL;

	myPasteboard = [NSPasteboard generalPasteboard];
	myPasteboardTypes = [myPasteboard types];
	if ([myPasteboardTypes containsObject: NSStringPboardType]) {
		NSString *myClipboardString;

		myClipboardString = [myPasteboard stringForType: NSStringPboardType];
		if (myClipboardString != NULL && [myClipboardString length] > 0) {
			return (strdup([myClipboardString cString]));
		}
	}
	return (NULL);
}

/**
 * @brief
 */
void Sys_HideApplication_f (void)
{
	extern qboolean	keydown[];

	keydown[K_COMMAND] = NO;
	keydown[K_TAB] = NO;
	keydown['H'] = NO;

	[NSApp hide: NULL];
}

/**
 * @brief
 */
int	Sys_CheckSpecialKeys (int theKey)
{
	extern cvar_t *vid_fullscreen;
	extern qboolean	keydown[];
	int myKey;

	/* do a fast evaluation: */
	if (keydown[K_COMMAND] == false) {
		return (0);
	}

	myKey = toupper (theKey);

	// check the keys:
	switch (myKey) {
	case K_TAB:
	case 'H':
		/* CMD-TAB is handled by the system if windowed: */
		if (myKey == 'H' || (vid_fullscreen != NULL && vid_fullscreen->value != 0.0f)) {
			Sys_HideApplication_f();
			return (1);
		}
		break;
	case 'M':
		/* minimize window [CMD-M]: */
		if (vid_fullscreen != NULL && vid_fullscreen->value == 0.0f && gSysIsMinimized->value == 0.0f) {
			NSWindow	*myWindow = NULL;

			myWindow = [NSApp keyWindow];
			if (myWindow != NULL) {
				[myWindow miniaturize: NULL];
			}

			return (1);
		}
		break;
	case 'Q':
		/* application quit [CMD-Q]: */
		return (1);
	case '?':
		if (vid_fullscreen != NULL && vid_fullscreen->value == 0.0f) {
			[NSApp showHelp: NULL];
			return (1);
		}
		break;
	}

	/* paste [CMD-V] already checked inside "keys.c"! */
	return (0);
}

/**
 * @brief
 */
void Sys_CheckForIDDirectory (void)
{
	char		*myBaseDir = NULL;
	SInt		myResult, myPathLength;
	BOOL		myFirstRun = YES, myDefaultPath = YES, myPathChanged = NO, myFileExists = NO;
	NSOpenPanel		*myOpenPanel;
	NSUserDefaults 	*myDefaults;
	NSArray		*myFolder;
	NSString	*myValidatePath = nil, *myBasePath = nil;

	/* get the user defaults: */
	myDefaults = [NSUserDefaults standardUserDefaults];

	/* prepare the open panel for requesting the "baseq2" folder: */
	myOpenPanel = [NSOpenPanel openPanel];
	[myOpenPanel setAllowsMultipleSelection: NO];
	[myOpenPanel setCanChooseFiles: NO];
	[myOpenPanel setCanChooseDirectories: YES];
	[myOpenPanel setTitle: @"Please locate the \"base\" folder:"];

	/* get the "base" path from the prefs: */
	myBasePath = [myDefaults stringForKey: SYS_DEFAULT_BASE_PATH];

	while (1) {
		if (myBasePath) {
			/* check if the path exists: */
			myValidatePath = [myBasePath stringByAppendingPathComponent: SYS_VALIDATION_FILE1];
			myFileExists = [[NSFileManager defaultManager] fileExistsAtPath: myValidatePath];
			if (true || myFileExists == NO) { /* Matthijs FIXME */
				myValidatePath = [myBasePath stringByAppendingPathComponent: SYS_VALIDATION_FILE2];
				myFileExists = [[NSFileManager defaultManager] fileExistsAtPath: myValidatePath];
			}
			if (true || myFileExists == YES) { /* Matthijs FIXME */
				/* get a POSIX version of the path: */
				myBaseDir = (char *) [myBasePath fileSystemRepresentation];
				myPathLength = strlen (myBaseDir);

				/* check if the last component was "base": */
				if (myPathLength >= 6) {
					/* FIXME Matthijs really needs to fix this :) */
					if ((myBaseDir[myPathLength - 4] == 'b' || myBaseDir[myPathLength - 4] == 'B') &&
						(myBaseDir[myPathLength - 3] == 'a' || myBaseDir[myPathLength - 3] == 'A') &&
						(myBaseDir[myPathLength - 2] == 's' || myBaseDir[myPathLength - 2] == 'S') &&
						(myBaseDir[myPathLength - 1] == 'e' || myBaseDir[myPathLength - 1] == 'E')) {
						/* remove "base": */
						myBaseDir[myPathLength - 4] = 0x00;

						/* change working directory to the selected path: */
						if (!chdir (myBaseDir)) {
							if (myPathChanged) {
								[myDefaults setObject: myBasePath forKey: SYS_DEFAULT_BASE_PATH];
								[myDefaults synchronize];
							}
							break;
						} else {
							NSRunCriticalAlertPanel(@"Can\'t change to the selected path!",
													@"The selection was: \"%@\"", NULL, NULL, NULL,
													myBasePath);
						}
					}
				}
			}
		}

		/* if the path from the user defaults is bad, look if the baseq2 folder is located at the same folder */
		/* as our UFO application: */
		if (myDefaultPath == YES) {
			myBasePath = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent]
				stringByAppendingPathComponent: SYS_BASEUFO_PATH];
			myPathChanged = YES;
			myDefaultPath = NO;
			continue;
		} else {
			/* if we run for the first time or the location of the "baseq2" folder changed, show an info dialog: */
			if (myFirstRun == YES) {
				NSRunInformationalAlertPanel (@"You will now be asked to locate the \"baseq2\" folder.",
											@"This folder is part of the standard installation of "
											@"Quake II. You will only be asked for it again, if you "
											@"change the location of this folder.",
											NULL, NULL, NULL);
				myFirstRun = NO;
			}
		}

		/* request the "base" folder: */
		myResult = [myOpenPanel runModalForDirectory: nil file: nil types: nil];

		/* if the user selected "Cancel", quit the game: */
		if (myResult == NSCancelButton)
			[NSApp terminate: nil];

		/* get the selected path: */
		myFolder = [myOpenPanel filenames];
		if (![myFolder count])
			continue;
		myBasePath = [myFolder objectAtIndex: 0];
		myPathChanged = YES;
	}

	/* just check if the mod is located at the same folder as the id1 folder: */
	if (gSysWasDragged == YES && strcmp (gSysModDir, myBaseDir) != 0) {
		NSRunInformationalAlertPanel (@"An error has occured:", @"The modification has to be located within "
									@"the same folder as the \"baseq2\" folder.", @"", NULL, NULL, NULL);
		[NSApp terminate: nil];
	}
}

/**
 * @brief
 */
void Sys_DoEvents (NSEvent *myEvent, NSEventType myType)
{
	extern cvar_t *vid_fullscreen, *_windowed_mouse, *in_mouse;

	static NSString *myKeyboardBuffer;
	static unichar myCharacter;
	static CGMouseDelta myMouseDeltaX, myMouseDeltaY, myMouseWheel;
	static UInt8 i;
	static UInt16 myKeyPad;
	static UInt32 myKeyboardBufferSize, myFilteredFlags, myFlags, myLastFlags = 0,
					myFilteredMouseButtons, myMouseButtons, myLastMouseButtons = 0;

	/* we check here for events: */
	switch (myType) {
		case NSSystemDefined:
			SYS_CHECK_MOUSE_ENABLED();

			if ([myEvent subtype] == 7) {
				myMouseButtons = [myEvent data2];
				myFilteredMouseButtons = myLastMouseButtons ^ myMouseButtons;

				for (i = 0; i < SYS_MOUSE_BUTTONS; i++) {
					if(myFilteredMouseButtons & (1 << i)) {
						Key_Event (K_MOUSE1 + i, (myMouseButtons & (1 << i)) ? 1 : 0, gSysMsgTime);
					}
				}

				myLastMouseButtons = myMouseButtons;
			} else {
				[NSApp sendSuperEvent: myEvent];
			}

			break;

		/* scroll wheel: */
		case NSScrollWheel:
			SYS_CHECK_MOUSE_ENABLED();

			myMouseWheel = [myEvent deltaY];

			if(myMouseWheel > 0) {
				Key_Event(K_MWHEELUP, true, gSysMsgTime);
				Key_Event(K_MWHEELUP, false, gSysMsgTime);
			} else {
				Key_Event(K_MWHEELDOWN, true, gSysMsgTime);
				Key_Event(K_MWHEELDOWN, false, gSysMsgTime);
			}
			break;

		/* mouse movement: */
		case NSMouseMoved:
		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:
			SYS_CHECK_MOUSE_ENABLED ();

			CGGetLastMouseDelta (&myMouseDeltaX, &myMouseDeltaY);
			IN_ReceiveMouseMove (myMouseDeltaX, myMouseDeltaY);

			break;

		/* key up and down: */
		case NSKeyDown:
		case NSKeyUp:
			myKeyboardBuffer = [myEvent charactersIgnoringModifiers];
			myKeyboardBufferSize = [myKeyboardBuffer length];

			for (i = 0; i < myKeyboardBufferSize; i++) {
				myCharacter = [myKeyboardBuffer characterAtIndex: i];

				if ((myCharacter & 0xFF00) ==  0xF700) {
					myCharacter -= 0xF700;
					if (myCharacter < 0x47) {
						if (gSpecialKey[myCharacter]) {
							Key_Event (gSpecialKey[myCharacter], (myType == NSKeyDown), gSysMsgTime);
							break;
						}
					}
				}
				/* else */
				{
					myFlags = [myEvent modifierFlags];

					if (myFlags & NSNumericPadKeyMask) {
						myKeyPad = [myEvent keyCode];

						if (myKeyPad < 0x5D && gNumPadKey[myKeyPad] != 0x00) {
							Key_Event (gNumPadKey[myKeyPad], (myType == NSKeyDown), gSysMsgTime);
							break;
						}
					}
					if (myCharacter < 0x80) {
						if (myCharacter >= 'A' && myCharacter <= 'Z')
							myCharacter += 'a' - 'A';
						Key_Event (myCharacter, (myType == NSKeyDown), gSysMsgTime);
					}
				}
			}

			break;

		/* special keys: */
		case NSFlagsChanged:
			myFlags = [myEvent modifierFlags];
			myFilteredFlags = myFlags ^ myLastFlags;

			if (myFilteredFlags & NSAlphaShiftKeyMask)
				Key_Event (K_CAPSLOCK, (myFlags & NSAlphaShiftKeyMask) ? 1 : 0, gSysMsgTime);

			if (myFilteredFlags & NSShiftKeyMask)
				Key_Event (K_SHIFT, (myFlags & NSShiftKeyMask) ? 1 : 0, gSysMsgTime);

			if (myFilteredFlags & NSControlKeyMask)
				Key_Event (K_CTRL, (myFlags & NSControlKeyMask) ? 1 : 0, gSysMsgTime);

			if (myFilteredFlags & NSAlternateKeyMask)
				Key_Event (K_ALT, (myFlags & NSAlternateKeyMask) ? 1 : 0, gSysMsgTime);

			if (myFilteredFlags & NSCommandKeyMask)
				Key_Event (K_COMMAND, (myFlags & NSCommandKeyMask) ? 1 : 0, gSysMsgTime);

			if (myFilteredFlags & NSNumericPadKeyMask)
				Key_Event (K_NUMLOCK, (myFlags & NSNumericPadKeyMask) ? 1 : 0, gSysMsgTime);

			myLastFlags = myFlags;

			break;

		/* process other events: */
		default:
			[NSApp sendSuperEvent: myEvent];
			break;
	}
}

#pragma mark -

@implementation UFO : NSObject

/**
 * @brief
 */
+ (void) initialize
{
	NSUserDefaults	*myDefaults = [NSUserDefaults standardUserDefaults];
	NSString		*myDefaultPath = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent]
					stringByAppendingPathComponent: SYS_BASEUFO_PATH];

	gSysRequestedCommand[0] = 0x0;

	/* required for event handling: */
	gSysDistantPast = [[NSDate distantPast] retain];

	/* set the default path: */
	[myDefaults registerDefaults: [NSDictionary dictionaryWithObjects:
			[NSArray arrayWithObjects: myDefaultPath,
									SYS_INITIAL_OPTION_KEY,
									SYS_INITIAL_USE_MP3,
									SYS_INITIAL_MP3_PATH,
									SYS_INITIAL_USE_PARAMETERS,
									SYS_INITIAL_PARAMETERS,
									nil]
		forKeys:
			[NSArray arrayWithObjects: SYS_DEFAULT_BASE_PATH,
									SYS_DEFAULT_OPTION_KEY,
									SYS_DEFAULT_USE_MP3,
									SYS_DEFAULT_MP3_PATH,
									SYS_DEFAULT_USE_PARAMETERS,
									SYS_DEFAULT_PARAMETERS,
									nil]
								]
	];
}

/**
 * @brief
 */
- (void) dealloc
{
	[gSysDistantPast release];
	[super dealloc];
}

/**
 * @brief
 */
- (BOOL) application: (NSApplication *) theSender openFile: (NSString *) theFilePath
{
	/* allow only dragging one time as command line parameter: */
	if (gSysDenyDrag == YES) {
		/* insert the dragged item as console command: */
		if (gSysHostInitialized == YES) {
			BOOL		myDirectory;

			if (![[NSFileManager defaultManager] fileExistsAtPath: theFilePath isDirectory: &myDirectory]) {
				Com_Printf("Error: The dragged item is not a valid file!\n");
			} else {
				if (myDirectory == NO) {
					Com_Printf("Error: The dragged item is not a folder!\n");
				} else {
					const char 	*myPath =[theFilePath fileSystemRepresentation];

					if (myPath != NULL) {
						SInt32	myIndex = strlen (myPath) - 1;

						while (myIndex > 1) {
							if (myPath[myIndex - 1] == '/') {
								Cbuf_ExecuteText(EXEC_APPEND, va("set game \"%s\"\n", myPath + myIndex));
								return (YES);
							}
							myIndex--;
						}
						Com_Printf("Error: Can\'t extract path!\n");
					} else {
						Com_Printf("Error: Unable to obtain filesystem representation!\n");
					}
				}
			}
		}
		return (NO);
	}
	gSysDenyDrag = YES;

	if (gSysArgCount > 2) {
		return (NO);
	}

	/* we have received a filepath: */
	if (theFilePath != NULL) {
		char 		*myMod  = (char *) [[theFilePath lastPathComponent] fileSystemRepresentation];
		char 		*myPath = (char *) [theFilePath fileSystemRepresentation];
		char		**myNewArgValues;
		BOOL		myDirectory;
		SInt32		i;

		/* is the filepath a folder? */
		if (![[NSFileManager defaultManager] fileExistsAtPath: theFilePath isDirectory: &myDirectory]) {
			Sys_Error ("The dragged item is not a valid file!");
		}
		if (myDirectory == NO) {
			Sys_Error ("The dragged item is not a folder!");
		}

		/* prepare the new command line options: */
		myNewArgValues = malloc (sizeof(char *) * 4);
		SYS_CHECK_MALLOC (myNewArgValues);
		gSysArgCount = 4;
		myNewArgValues[0] = gSysArgValues[0];
		gSysArgValues = myNewArgValues;
		gSysArgValues[1] = SYS_SET_COMMAND;
		gSysArgValues[2] = SYS_GAME_COMMAND;
		gSysArgValues[3] = malloc (strlen (myPath) + 1);
		SYS_CHECK_MALLOC (gSysArgValues[3]);
		strcpy (gSysArgValues[3], myMod);

		/* get the path of the mod [compare it with the id1 path later]: */
		gSysModDir = malloc (strlen (myPath) + 1);
		SYS_CHECK_MALLOC (gSysModDir);
		strcpy (gSysModDir, myPath);

		/* dispose the foldername of the mod: */
		i = strlen (gSysModDir) - 1;
		while (i > 1) {
			if (gSysModDir[i - 1] == '/') {
				gSysModDir[i] = 0x00;
				break;
			}
			i--;
		}

		return (YES);
	}
	return (NO);
}

/**
 * @brief
 */
- (void) applicationDidResignActive: (NSNotification *) theNote
{
	if (gSysHostInitialized == NO) {
		return;
	}

	IN_ShowCursor(YES);
	IN_SetKeyboardRepeatEnabled(YES);
	IN_SetF12EjectEnabled(YES);
/*	VID_SetPaused(YES); */
	gSysIsDeactivated = YES;
}

/**
 * @brief
 */
- (void) applicationDidBecomeActive: (NSNotification *) theNote
{
	extern qboolean	keydown[];
	extern cvar_t *_windowed_mouse, *in_mouse, *vid_fullscreen;

	if (gSysHostInitialized == NO) {
		return;
	}

	if ((vid_fullscreen != NULL && vid_fullscreen->value != 0.0f) ||
		((in_mouse == NULL || ((in_mouse != NULL && in_mouse->value == 0.0f) &&
		(_windowed_mouse != NULL && _windowed_mouse->value != 0.0f))))) {
		IN_ShowCursor(NO);
	}

	keydown[K_COMMAND] = NO;
	keydown[K_TAB] = NO;
	keydown['H'] = NO;

	IN_SetKeyboardRepeatEnabled (NO);
	IN_SetF12EjectEnabled (NO);
	gSysIsDeactivated = NO;
	/*CDAudio_Enable(YES);*/
	/*VID_SetPaused(NO);*/
	gSysHidden = NO;

	CGPostKeyboardEvent((CGCharCode) 0, (CGKeyCode) 55, NO);	// CMD
	CGPostKeyboardEvent((CGCharCode) 0, (CGKeyCode) 48, NO);	// TAB
	CGPostKeyboardEvent((CGCharCode) 0, (CGKeyCode) 4, NO);	// H

	if (gFrameTimer == NULL) {
		[self installFrameTimer];
	}
}

/**
 * @brief
 */
- (void) applicationWillHide: (NSNotification *) theNote
{
	if (gSysHostInitialized == NO) {
		return;
	}

	IN_ShowCursor (YES);
	IN_SetKeyboardRepeatEnabled (YES);
	IN_SetF12EjectEnabled (YES);
	/*CDAudio_Enable (NO);*/
	gSysHidden = YES;
	gSysIsDeactivated = YES;
	/*VID_SetPaused (YES);*/

	if (gFrameTimer != NULL) {
		[gFrameTimer invalidate];
		gFrameTimer = NULL;
	}
}

/**
 * @brief
 */
- (void) applicationDidFinishLaunching: (NSNotification *) theNote
{
	SYS_UFO_DURING
	{
		NSTimer		*myTimer;

		gSysDenyDrag = YES;

		Sys_CheckForIDDirectory();

		/* check if the user has pressed the Option key on startup: */
		[self checkForOptionKey];

		/* show the settings dialog after 0.5s (required to recognize the "run" AppleScript command): */
		myTimer = [NSTimer scheduledTimerWithTimeInterval: 0.5f
					target: self
					selector: @selector (setupDialog:)
					userInfo: NULL
					repeats: NO];

		if (myTimer == NULL) {
			[self setupDialog: NULL];
		}
	}
	SYS_UFO_HANDLER;
}

/**
 * @brief
 */
- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) theSender
{
	if (gSysHostInitialized == YES) {
		extern cvar_t	*vid_fullscreen;

		if ([NSApp isHidden] == YES || [NSApp isActive] == NO) {
			[NSApp activateIgnoringOtherApps: YES];
		}

		if (vid_fullscreen != NULL && vid_fullscreen->value == 0.0f) {
			NSArray	*myWindowList = [NSApp windows];

			if (myWindowList != NULL) {
				int	myCount = [myWindowList count],
						myIndex;

				for (myIndex = 0; myIndex < myCount; myIndex++) {
					NSWindow	*myWindow = [myWindowList objectAtIndex: myIndex];

					if (myWindow != NULL) {
						if ([myWindow isMiniaturized] == YES) {
							[myWindow deminiaturize: NULL];
						}
					}
				}
			}
		}

		return (NSTerminateCancel);
	}

	return (NSTerminateNow);
}

/**
 * @brief
 */
- (void) checkForOptionKey
{
	NSEvent		*myEvent;
	NSAutoreleasePool	*myPool;

	/* raise shift down/up events [from somewhere deep inside CoreGraphics]: */
	CGPostKeyboardEvent ((CGCharCode) 0, (CGKeyCode) 56, YES);
	CGPostKeyboardEvent ((CGCharCode) 0, (CGKeyCode) 56, NO);

	myPool = [[NSAutoreleasePool alloc] init];
	while (1) {
		myEvent = [NSApp nextEventMatchingMask: NSFlagsChangedMask
					untilDate: gSysDistantPast
					inMode: NSDefaultRunLoopMode
					dequeue: YES];

		/* we are finished when no events are left: */
		if (!myEvent) {
			break;
		}

		/* see if our shift down event has the option/alt key flag set: */
		if ([myEvent modifierFlags] & NSAlternateKeyMask) {
			gOptionPressed = YES;
			break;
		} else {
			[NSApp sendEvent: myEvent];
		}
	}
	[myPool release];
}

/**
 * @brief
 */
- (void) setupParameterUI:  (NSUserDefaults *) theDefaults
{
	/* check if the user passed parameters from the command line or by dragging a mod: */
	if (gSysArgCount > 1) {
		NSString	*myParameters;

		/* someone passed command line parameters: */
		myParameters = [[[NSString alloc] init] autorelease];

		if (myParameters != NULL) {
			SInt	i;

			for (i = 1; i < gSysArgCount; i++) {
				/* surround the string by ", if it contains spaces: */
				if (strchr (gSysArgValues[i], ' ')) {
					myParameters = [myParameters stringByAppendingFormat: @"\"%s\" ", gSysArgValues[i]];
				} else {
					myParameters = [myParameters stringByAppendingFormat: @"%s", gSysArgValues[i]];
				}

				/* add a space if this was not the last parameter: */
				if (i != gSysArgCount - 1) {
					myParameters = [myParameters stringByAppendingString: @" "];
				}
			}

			/* display the current parameters: */
			[parameterTextField setStringValue: myParameters];
		}

		/* don't allow changes: */
		[parameterCheckBox setEnabled: NO];
		[parameterTextField setEnabled: NO];
	} else {
		BOOL	myParametersEnabled;

		/* get the default command line parameters: */
		myParametersEnabled = [theDefaults boolForKey: SYS_DEFAULT_USE_PARAMETERS];
		[parameterTextField setStringValue: [theDefaults stringForKey: SYS_DEFAULT_PARAMETERS]];
		[parameterCheckBox setState: myParametersEnabled];
		[parameterCheckBox setEnabled: YES];
		[self toggleParameterTextField: NULL];
	}
}

/**
 * @brief
 */
- (void) setupDialog: (NSTimer *) theTimer
{
	SYS_UFO_DURING
	{
		NSUserDefaults 	*myDefaults = NULL;

		// don't allow the "run" AppleScript command to be executed anymore:
		gSysAllowRunCommand = NO;

		myDefaults = [NSUserDefaults standardUserDefaults];

		// prepare the "option key" and the "use MP3" checkbox:
		[optionCheckBox setState: [myDefaults boolForKey: SYS_DEFAULT_OPTION_KEY]];
		[mp3CheckBox setState: [myDefaults boolForKey: SYS_DEFAULT_USE_MP3]];
		[self toggleMP3Playback: self];

		// prepare the "MP3 path" textfield:
		[mp3TextField setStringValue: [myDefaults stringForKey: SYS_DEFAULT_MP3_PATH]];

		// prepare the command-line parameter textfield and checkbox:
		[self setupParameterUI: myDefaults];

		if ([optionCheckBox state] == NO || ([optionCheckBox state] == YES && gOptionPressed == YES)) {
			// show the startup dialog:
			[startupWindow center];
			[startupWindow makeKeyAndOrderFront: nil];
		} else {
			// start the game immediately:
			[self startUFO: nil];
		}
	} SYS_UFO_HANDLER;
}

/**
 * @brief
 */
- (void) saveCheckBox: (NSButton *) theButton initial: (NSString *) theInitial
              default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults
{
	// has our checkbox the initial value? if, delete from defaults::
	if ([theButton state] == [self isEqualTo: theInitial]) {
		[theUserDefaults removeObjectForKey: theDefault];
	} else {
		// write to defaults:
		if ([theButton state] == YES) {
			[theUserDefaults setObject: @"YES" forKey: theDefault];
		} else {
			[theUserDefaults setObject: @"NO" forKey: theDefault];
		}
	}
}

/**
 * @brief
 */
- (void) saveString: (NSString *) theString initial: (NSString *) theInitial
            default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults
{
	// has our popup menu the initial value? if, delete from defaults:
	if ([theString isEqualToString: theInitial]) {
		[theUserDefaults removeObjectForKey: theDefault];
	} else {
		// write to defaults:
		[theUserDefaults setObject: theString forKey: theDefault];
	}
}

/**
 * @brief
 */
- (void) stringToParameters: (NSString *) theString
{
	NSArray		*mySeparatedArguments;
	NSMutableArray      *myNewArguments;
	NSCharacterSet	*myQuotationMarks;
	NSString		*myArgument;
	char		**myNewArgValues;
	SInt		i;

	// get all parameters separated by a space:
	mySeparatedArguments = [theString componentsSeparatedByString: @" "];

	// no parameters at all?
	if (mySeparatedArguments == NULL || [mySeparatedArguments count] == 0) {
		return;
	}

	// concatenate parameters that start on " and end on ":
	myNewArguments = [NSMutableArray arrayWithCapacity: 0];
	myQuotationMarks = [NSCharacterSet characterSetWithCharactersInString: @"\""];

	for (i = 0; i < [mySeparatedArguments count]; i++) {
		myArgument = [mySeparatedArguments objectAtIndex: i];
		if (myArgument != NULL && [myArgument length] != 0) {
			if ([myArgument characterAtIndex: 0] == '\"') {
				myArgument = [NSString stringWithString: @""];
				for (; i < [mySeparatedArguments count]; i++) {
					myArgument = [myArgument stringByAppendingString: [mySeparatedArguments objectAtIndex: i]];
					if ([myArgument characterAtIndex: [myArgument length] - 1] == '\"') {
						break;
					} else {
						if (i < [mySeparatedArguments count] - 1) {
							myArgument = [myArgument stringByAppendingString: @" "];
						}
					}
				}
			}
			myArgument = [myArgument stringByTrimmingCharactersInSet: myQuotationMarks];
			if (myArgument != NULL && [myArgument length] != 0) {
				[myNewArguments addObject: myArgument];
			}
		}
	}

	gSysArgCount = [myNewArguments count] + 1;
	myNewArgValues = (char **) malloc (sizeof(char *) * gSysArgCount);
	SYS_CHECK_MALLOC (myNewArgValues);

	myNewArgValues[0] = gSysArgValues[0];
	gSysArgValues = myNewArgValues;

	// insert the new parameters:
	for (i = 0; i < [myNewArguments count]; i++) {
		char *	myCString = (char *) [[myNewArguments objectAtIndex: i] cString];

		gSysArgValues[i+1] = (char *) malloc (strlen (myCString) + 1);
		SYS_CHECK_MALLOC (gSysArgValues[i+1]);
		strcpy (gSysArgValues[i+1], myCString);
	}
}

/**
 * @brief
 */
- (BOOL) isEqualTo: (NSString *) theString
{
	// just some boolean str compare:
	if([theString isEqualToString: @"YES"]) {
		return(YES);
	}

	return(NO);
}

/**
 * @brief
 */
- (void) scanMediaThread: (id) theSender
{
	SYS_UFO_DURING
	{
		// scan for media files:
		//CDAudio_GetTrackList ();

		// post a notification to the main thread:
		[[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"Fire Frame Timer" object: NULL];

		// job done, good bye!
		[NSThread exit];
	} SYS_UFO_HANDLER;
}

/**
 * @brief
 */
- (IBAction) stopMediaScan: (id) theSender
{
	gSysAbortMediaScan = YES;
}

/**
 * @brief
 */
- (void) fireFrameTimer: (NSNotification *) theNotification
{
	SYS_UFO_DURING
	{
		// close the media scan window:
		[mediascanProgressIndicator stopAnimation: self];
		[mediascanWindow close];
		[[NSNotificationCenter defaultCenter] removeObserver: self name: @"Fire Frame Timer" object: NULL];

		// alias the action of the past menu item:
		[pasteMenuItem setTarget: self];
		[pasteMenuItem setAction: @selector (pasteString:)];

		IN_SetKeyboardRepeatEnabled (NO);
		IN_SetF12EjectEnabled (NO);

		[NSApp activateIgnoringOtherApps: YES];

		Qcommon_Init(gSysArgCount, gSysArgValues);

		if (gSysRequestedCommand[0] != 0x00) {
			Cbuf_ExecuteText(EXEC_APPEND, va("%s\n", gSysRequestedCommand));
		}

		gSysHostInitialized = YES;
		gSysAbortMediaScan = NO;

		[NSApp setServicesProvider: self];

		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

		gSysNoStdOut = Cvar_Get("nostdout", "0", 0, NULL);
		if (gSysNoStdOut->value == 0.0) {
				fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
		}

		gSysLastFrameTime = Sys_Milliseconds();

		Qcommon_Frame(0.1);

		// install our frame renderer to the default runloop:
		[self installFrameTimer];
	} SYS_UFO_HANDLER;
}

/**
 * @brief
 */
- (IBAction) startUFO: (id) theSender
{
	SYS_UFO_DURING
	{
		NSUserDefaults	*myDefaults = [NSUserDefaults standardUserDefaults];

		// save the state of the "option key" checkbox:
		[self saveCheckBox: optionCheckBox initial: SYS_INITIAL_OPTION_KEY
									default: SYS_DEFAULT_OPTION_KEY
									userDefaults: myDefaults];

		// save the state of the "use MP3" checkbox:
		[self saveCheckBox: mp3CheckBox initial: SYS_INITIAL_USE_MP3
									default: SYS_DEFAULT_USE_MP3
									userDefaults: myDefaults];

		// save the MP3 path:
		[self saveString: [mp3TextField stringValue] initial: SYS_INITIAL_MP3_PATH
									default: SYS_DEFAULT_MP3_PATH
									userDefaults: myDefaults];

		// save the state of the "use command line parameters" checkbox:
		[self saveCheckBox: parameterCheckBox initial: SYS_INITIAL_USE_PARAMETERS
									default: SYS_DEFAULT_USE_PARAMETERS
									userDefaults: myDefaults];

		// save the command line string from the parameter text field [only if no parameters were passed]:
		if ([parameterCheckBox isEnabled] == YES) {
			[self saveString: [parameterTextField stringValue] initial: SYS_INITIAL_PARAMETERS
									default: SYS_DEFAULT_PARAMETERS
									userDefaults: myDefaults];

			if ([parameterCheckBox state] == YES) {
				[self stringToParameters: [parameterTextField stringValue]];
			}
		}

		if ([mp3CheckBox state] == YES) {
			gSysMP3Folder = [mp3TextField stringValue];
			[mediascanText setStringValue: @"Scanning folder for MP3 and MP4 files..."];
		} else {
			[mediascanText setStringValue: @"Scanning AudioCDs..."];
		}

		[myDefaults synchronize];
		[startupWindow close];

		// scan for medias, show a dialog since this can take a while:
	//SNDDMA_ReserveBufferSize ();
		[mediascanWindow center];
		[mediascanWindow makeKeyAndOrderFront: nil];
		[mediascanProgressIndicator startAnimation: self];
		[[NSDistributedNotificationCenter defaultCenter] addObserver: self
															selector: @selector (fireFrameTimer:)
															name: @"Fire Frame Timer"
															object: NULL];

		[NSThread detachNewThreadSelector: @selector (scanMediaThread:) toTarget: self withObject: nil];
	} SYS_UFO_HANDLER;
}

/**
 * @brief
 */
- (void) installFrameTimer
{
	// we may not set the timer interval too small, otherwise we wouldn't get AppleScript commands. odd eh?
	gFrameTimer = [NSTimer scheduledTimerWithTimeInterval: 0.0003f //0.000001f
					target: self
					selector: @selector (renderFrame:)
					userInfo: NULL
					repeats: YES];

	if (gFrameTimer == NULL) {
		Sys_Error ("Failed to install the renderer loop!");
	}
}

/**
 * @brief
 */
- (void) renderFrame: (NSTimer *) theTimer
{
	static int myNewFrameTime, myFrameTime;

	if (dedicated && dedicated->value) {
		sleep (1);
	}

	do {
		myNewFrameTime = Sys_Milliseconds ();
		myFrameTime = myNewFrameTime - gSysLastFrameTime;
	} while (myFrameTime < 1);
	gSysLastFrameTime = myNewFrameTime;

	Qcommon_Frame (myFrameTime);
}

/**
 * @brief
 */
- (IBAction) toggleParameterTextField: (id) theSender
{
	[parameterTextField setEnabled: [parameterCheckBox state]];
}

/**
 * @brief
 */
- (IBAction) toggleMP3Playback: (id) theSender
{
	BOOL	myState = [mp3CheckBox state];

	[mp3Button setEnabled: myState];
	[mp3TextField setEnabled: myState];
}

/**
 * @brief
 */
- (IBAction) selectMP3Folder: (id) theSender
{
	// prepare the sheet, if not already done:
	if (gMP3Panel == nil) {
		gMP3Panel = [NSOpenPanel openPanel];
	}
	[gMP3Panel setAllowsMultipleSelection: NO];
	[gMP3Panel setCanChooseFiles: NO];
	[gMP3Panel setCanChooseDirectories: YES];
	[gMP3Panel setAccessoryView: mp3HelpView];
	[gMP3Panel setDirectory: [mp3TextField stringValue]];
	[gMP3Panel setTitle: @"Select the folder that holds the MP3s:"];

	// show the sheet:
	[gMP3Panel beginSheetForDirectory: @""
					file: NULL
					types: NULL
					modalForWindow: startupWindow
					modalDelegate: self
					didEndSelector: @selector (closeMP3Sheet:returnCode:contextInfo:)
					contextInfo: NULL];
}

/**
 * @brief
 */
- (void) closeMP3Sheet: (NSOpenPanel *) theSheet returnCode: (int) theCode contextInfo: (void *) theInfo
{
	[theSheet close];

	// do nothing on cancel:
	if (theCode != NSCancelButton) {
		NSArray *		myFolderArray;

		// get the path of the selected folder;
		myFolderArray = [theSheet filenames];
		if ([myFolderArray count] > 0) {
			[mp3TextField setStringValue: [myFolderArray objectAtIndex: 0]];
		}
	}
}

/**
 * @brief
 */
- (IBAction) visitFOD: (id) theSender
{
	NSWorkspace		*myWorkspace;
	NSURL		*myURL;

	myWorkspace = [NSWorkspace sharedWorkspace];
	if (myWorkspace == NULL) {
		NSRunAlertPanel ([NSString stringWithFormat: @"Can\'t launch the URL:\n \"%@\".", SYS_UFOAI_HP],
						@"Reason: Failed to retrieve shared workspace.", NULL, NULL, NULL);
		return;
	}

	myURL = [NSURL URLWithString: SYS_UFOAI_HP];
	if (myURL == NULL) {
		NSRunAlertPanel ([NSString stringWithFormat: @"Can\'t launch the URL:\n \"%@\".", SYS_UFOAI_HP],
						@"Reason: Failed to prepare the URL.", NULL, NULL, NULL);
		return;
	}

	[myWorkspace openURL: myURL];
}

/**
 * @brief
 */
- (IBAction) pasteString: (id) theSender
{
	extern qboolean	keydown[];
	UInt32		myCurTime = Sys_Milliseconds ();
	qboolean		myOldCommand,
						myOldVKey;

	// get the old state of the paste keys:
	myOldCommand = keydown[K_COMMAND];
	myOldVKey = keydown['v'];

	// send the keys required for paste:
	keydown[K_COMMAND] = true;
	Key_Event ('v', true, myCurTime);

	// set the old state of the paste keys:
	Key_Event ('v', false, myCurTime);
	keydown[K_COMMAND] = myOldCommand;
}

/**
 * @brief
 */
- (void) connectToServer: (NSPasteboard *) thePasteboard userData:(NSString *)theData error: (NSString **) theError
{
	NSArray 	*myPasteboardTypes;

	myPasteboardTypes = [thePasteboard types];

	if ([myPasteboardTypes containsObject: NSStringPboardType]) {
		NSString 	*myRequestedServer;

		myRequestedServer = [thePasteboard stringForType: NSStringPboardType];
		if (myRequestedServer != NULL) {
			Cbuf_ExecuteText (EXEC_APPEND, va("connect %s\n", [myRequestedServer cString]));
			return;
		}
	}
	*theError = @"Unable to connect to a server: could not find a string on the pasteboard!";
}

@end

@implementation UFOApplication : NSApplication


/**
 * @brief
 */
- (void) sendEvent: (NSEvent *) theEvent
{
	// we need to intercept NSApps "sendEvent:" action:
	if (gSysHostInitialized == YES) {
		if (gSysHidden == YES) {
			S_StopAllSounds ();
			[super sendEvent: theEvent];
		} else {
			Sys_DoEvents (theEvent, [theEvent type]);
		}
	} else {
		[super sendEvent: theEvent];
	}
}

/**
 * @brief
 */
- (void) sendSuperEvent: (NSEvent *) theEvent
{
	[super sendEvent: theEvent];
}

/**
 * @brief
 */
- (void) handleRunCommand: (NSScriptCommand *) theCommand
{
	if (gSysAllowRunCommand == YES) {
		NSDictionary	*myArguments = [theCommand evaluatedArguments];

		if (myArguments != NULL) {
			NSString 		*myParameters = [myArguments objectForKey: @"UFOParameters"];

			// Check if we got command line parameters:
			if (myParameters != NULL && [myParameters isEqualToString: @""] == NO) {
				[[self delegate] stringToParameters: myParameters];
			} else {
				[[self delegate] stringToParameters: @""];
			}
		}
		gSysAllowRunCommand = NO;
	}
}

/**
 * @brief
 */
- (void) handleConsoleCommand: (NSScriptCommand *) theCommand
{
	NSDictionary	*myArguments = [theCommand evaluatedArguments];

	if (myArguments != NULL) {
		NSString *myCommandList= [myArguments objectForKey: @"UFOCommandlist"];

		// Send the console command only if we got commands:
		if (myCommandList != NULL && [myCommandList isEqualToString: @""] == NO) {
			// required because of the options dialog.
			// we have to wait with the command until host_init() is finished!
			if (gSysHostInitialized == YES) {
				Cbuf_ExecuteText (EXEC_APPEND, va("%s\n", [myCommandList cString]));
			} else {
				snprintf (gSysRequestedCommand, SYS_COMMAND_BUFFER_SIZE, "%s\n", [myCommandList cString]);
			}
		}
		}
}

@end

#pragma mark -

#endif /* !DEDICATED_ONLY */

/**
 * @brief
 */
int main (int theArgCount, const char **theArgValues)
{
#ifdef DEDICATED_ONLY
	int		myTime, myOldTime, myNewTime;

	gSysArgCount = theArgCount;
	gSysArgValues = (char **) theArgValues;

	Qcommon_Init(gSysArgCount, gSysArgValues);

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	gSysNoStdOut = Cvar_Get("nostdout", "0", 0, NULL);
	if (gSysNoStdOut->value == 0.0) {
			fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
	}

	myOldTime = Sys_Milliseconds ();

	Qcommon_Frame (0.1);

	while (1) {
		do {
			myNewTime = Sys_Milliseconds ();
			myTime = myNewTime - myOldTime;
		} while (myTime < 1);
		myOldTime = myNewTime;

		Qcommon_Frame (myTime);
	}

	return (0);

#else

	// the Finder passes "-psn_x_xxxxxxx". remove it.
	if (theArgCount == 2 && theArgValues[1] != NULL && strstr (theArgValues[1], "-psn_") == theArgValues[1]) {
		gSysArgCount = 1;
	} else {
		gSysArgCount = theArgCount;
	}
	gSysArgValues = (char **) theArgValues;

	return (NSApplicationMain (theArgCount, theArgValues));

#endif /* DEDICATED ONLY */
}

/**
 * @brief
 */
char *Sys_GetHomeDirectory (void)
{
	return getenv ( "HOME" );
}

/**
 * @brief
 */
void Sys_NormPath(char* path)
{
}

/* fix for linking */
void Sys_DisableTray(void)
{

}

void Sys_EnableTray(void)
{

}

void Sys_Minimize(void)
{

}