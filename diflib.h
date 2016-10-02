#ifndef _DIFLIB_
#define _DIFLIB_

#ifdef __cplusplus
extern "C" {
#endif

int ComputeEditScript (char *OldString,
		       int OldStringLength,
		       char *NewString,
		       int NewStringLength,
		       char *EditScript,
		       int EditScriptLength);

int ApplyEditScript( char *OldString,
		     int OldStringLength,
		     char *EditScript,
		     int EditScriptLength,
		     char *NewString,
		     int NewStringLength);

#ifdef __cplusplus
}
#endif

#endif // _DIFLIB_
