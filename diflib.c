/*

  The two main routines implemented in this file are

  ComputeEditScript() and ApplyEditScript()

  ComputeEditScript takes two strings and returns the edit script needed to transform the first string into the second string

  ApplyEditScript takes the OldString that was supplied to ComputeEditScript and the Edit Script and returns the second string

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diflib.h"

//
//  Our work space is a fan out of entries.  Laid out starting from left to right like the following
//
//    V D  k SavedX SavedY Delete Index Back Token
//      
//    0 0  0 
//    1 1 -1
//    2 1 +1
//    3 2 -2
//    4 2  0
//    5 2 +2
//    6 3 -3
//    7 3 -1
//    8 3 +1
//    9 3 +3
//
//  SavedX and SavedY contain the location where the two strings last matched
//  Delete indicates of the we are going to do a delete or an insert of get over this difference
//  Index is the location within the OldString that we are either going to delete or insert right after
//  Back is the index into V that this location is based on.  It will be either D-1,k-1 or D-1,k+1
//  Token is used when we are doing an insert and is the value being inserted in the string
//

typedef struct _WORK_SPACE_ENTRY_ {
  int D,k;           // the computed D,k pair for this index, based on Myers' algorithm
  int SavedX,SavedY; // the indices where X and Y last matched
  char IsDelete;     // 1 for delete and 0 for Insert
  int Index, Back;   // the index within the OldString (i.e., X) where we are either going to delete or insert a character
  char *Token;        // for an insert this is the byte value to be inserted
} WORK_SPACE_ENTRY, *PWORK_SPACE_ENTRY;

//
//  This macro takes D and k from Myers' algoritm and computes its unique index in the V array
//

#define DkIndex(D,K) ((((D)*(D))+(2*(D))+(K))/2)

void DebugPrintArray(PWORK_SPACE_ENTRY V, char PrintHeading, int StartIndex, int StopIndex)
{
  int i;
  if (PrintHeading) { printf("  V   D   k SavedX SavedY Del Ind Back Token\n"); }
  for (i=StartIndex; i<=StopIndex; i++) {
    printf("%3d %3d %3d    %3d    %3d %3d %3d %4d   %3d ", i,V[i].D,V[i].k,V[i].SavedX,V[i].SavedY,V[i].IsDelete,V[i].Index,V[i].Back,*V[i].Token);
    if (V[i].IsDelete) { printf("%3dD\n",V[i].Index); } else { printf("%3dI%c\n", V[i].Index, *V[i].Token); }
  }
}

//
//  The edit script is a list of edit script entries.  Each entry contains a count and an opcode
//  indicating if it is an insert, delete, or copy operation.
//
//  Count is stored as 0..63 but will be interpreted as 1 to 64
//
//  Reconstruction of the NewString takes as input the OldString and this edit Script
//
//  We build the new string as follows
//
//    For an insert, the entry is followed by the Count number of bytes to add to the current location in the new string
//    For a delete, count indictes the number of bytes to skip over in the Old string that is to skipped over (i.e., not
//        added to the new string
//    For a keep, count is the number of bytes to keep from the old string
//

typedef struct _EDIT_SCRIPT_ENTRY_ {
  unsigned char Count:6;     // number of bytes to operation upon range is 1..64 (so we add 1 to get the real count)
  unsigned char Opcode:2; // Type of operation 0 = unused, 1 = Insert, 2 = Delete, 3 = Keep
} EDIT_SCRIPT_ENTRY, *PEDIT_SCRIPT_ENTRY;

#define NoopOpcode   (0)
#define InsertOpcode (1)
#define DeleteOpcode (2)
#define KeepOpcode   (3)

//
//  Here are support routines to help build the edit script.
//
//  Each routine takes as input three values that describe the edit script being added to,
//  first a pointer to the start of the edit script; second, its total length, and third the
//  current index in the edit script that we being added to.
//
//  Each routine also takes as input an opcode
//
//  Each routine also takes as input the count for the opcode. This value can be greater than
//  64 which means that the routine needs to break it down into multiple operations. For example,
//  a delete of 67 will be a delete of 64 followed by a delete of 3.
//
//  The Insert opcode also takes a pointer to the string that is to be Inserted into the edit script.
//
//  Each routine returns the next free index in the edit script, or -1 if the edit script buffer
//  overflows. or -3 for unexpected errors, such as bad opcodes.
//

int AddEditScript( PEDIT_SCRIPT_ENTRY P, // the start of the script buffer
		   int Length,           // the total length of the script buffer (needed to check for overflow)
		   int Index,            // current index into the script buffer to add the next entry
		   unsigned int Opcode,
		   int Count,            // the number of characters to insert, delete or copy
		   char *NewString       // the start in the new string where we will get the bytes to insert
		   )
{
  int i,j,k;

  //  printf("AddEditScript(%08lx, %d, %d, %d, %d, %08lx)\n", P, Length, Index, Opcode, Count, NewString); 

  //
  //  For the keep and delete opcode if the count is great than 64 we will keep on emiting
  //  an entry until the count is less than 64 or until we run out of space
  //
  
  if ((Opcode == KeepOpcode) || (Opcode == DeleteOpcode)) {

    //
    //  First do the chunks of 64
    //
    
    while ((Index < Length) && (Count > 64)) {
      P[Index].Opcode = Opcode;
      P[Index].Count = 64 - 1; // bias by 1
      Index++;
      Count -= 64;
    }

    //
    //  Then do the remaining chunk of 64 or less
    //
    
    if ((Index < Length) && (Count != 0)) {
      P[Index].Opcode = Opcode;
      P[Index].Count = Count - 1; // bias by 1
      Index++;
      Count = 0;
    }

    //
    //  If the count is zero then everythingt fits, other we return an error to our caller of buffer overflow
    //
    
    if (Count == 0) {
      return Index;
    }
    return -1;

  } else if (Opcode == InsertOpcode) {

    //
    //  This is the copy opcode which is similar to the insert and delete opcodes but has the additional
    //  requirement of added bytes after the opcode to the copy operation
    //

    k = 0;
    while ((Index < Length) && (Count > 64)) {
      P[Index].Opcode = Opcode;
      P[Index].Count = 64 - 1; // bias by 1
      Index++;
      for (i=0; (i<64) && (Index < Length); i++, Index++, Count--) {
	((char *)P)[Index] = NewString[k*64+i];
      }
      k++;
    }
    if ((Index < Length) && (Count != 0)) {
      P[Index].Opcode = Opcode;
      P[Index].Count = Count - 1; // bias by 1
      Index++;
      for (i=0, j=Count; (i<j) && (Index < Length); i++, Index++, Count--) {
	((char *)P)[Index] = NewString[k*64+i];
      }
    }
    if (Count == 0) {
      return Index;
    }
    return -1;
    
  } else {

    //
    //  Otherwise we were given an invalid opcode
    //
    
    return -3;
  }
}

void DebugPrintEditScript(PEDIT_SCRIPT_ENTRY p, int len)
{
  int i,j;
  unsigned char *c;
  printf("Edit Script, length = %d, >>>", len);
  for (i = 0; i < len; i++) {
    if (p[i].Opcode == InsertOpcode) { // Insert
      printf(" I%d\"", p[i].Count+1);
      for (j = 0; j < p[i].Count+1; j++) {
	printf("%c", ((char *)p)[i+j+1]);
      }
      printf("\"");
      i += p[i].Count+1;
    } else if (p[i].Opcode == DeleteOpcode) { // Delete
      printf(" D%d" , p[i].Count+1);
    } else if (p[i].Opcode == KeepOpcode) { // Skip
      printf(" K%d", p[i].Count+1);
    }
  }
  printf(" <<<Edit Script\n");
}

int ConstructEditScript(PEDIT_SCRIPT_ENTRY EditScript,     //PEDIT_SCRIPT_ENTRY EditScript,  // output for the edit script
			int EditScriptLength, // length of edit script
			PWORK_SPACE_ENTRY V,  // the workspace array holding our solution
			int EndIndex,            // the index in the workspace array where our solution ends
			char *OldString, int OldStringLength,
			char *NewString, int NewStringLength
			)
{
  int CurrentVIndex; // the index of the current entry in the V array that we are processing
  int CurrentEditScriptIndex;
  int i,j,k;
  int DeleteCount, StartDeleteIndex, InsertCount, StartInsertIndex;
  int InsertY;
  int CurrentOutputIndex; // the index within the string that we are constructing
  int y;
  int LastOpcode, OpcodeCount;
  char *StartInsertToken;
  
  //printf("ConstructEditScript(...)\n");
  //DebugPrintArray(V,1,0,Index);

  //
  //  Now we need to build the edit script.  Is is a backwards process.  First we'll
  //  reverse the back pointers to make them forward pointers.
  //

  //printf("\nReverse back pointers\n");
  for (i = EndIndex, k = -1; i != 0;) {
    j = V[i].Back;
    V[i].Back = k;
    k = i;
    i = j;
  }
  V[i].Back = k;
  //DebugPrintArray(V,1,0,EndIndex);
  //  for (i = V[0].Back; i != -1; i = V[i].Back) { DebugPrintArray(V,(i == V[0].Back),i,i); }

  LastOpcode = NoopOpcode;
  OpcodeCount = 0;
  StartInsertToken = NULL; 
  
  CurrentVIndex = V[0].Back;
  CurrentEditScriptIndex = 0;

  //
  //  In this loop we essentially walk through what would be the new string. Using i as our index into the
  //  string that we are constructing and comparing i with index of the operations that are stored in the
  //  work array. If i is less than the index of the insert or delete then we want to keep (or retain) the
  //  byte in the old string. so we issue a keep opcode and move one.  If i lines up with the index of the
  //  current command then we want to process that command, and if necsssary advance i.
  //
  //  We actually postpone adding the command to the edit script until opcode changes so do don't do things
  //  like issue delete 1 char, delete 1 char, delete 1 char.  But instead issue a delete 3 char.
  //
  
  for (i = 0; CurrentVIndex != -1;) {
    if (V[CurrentVIndex].IsDelete) {

      if (i < V[CurrentVIndex].Index) {
	if (i != 0) {
	  if (LastOpcode == KeepOpcode) {
	    OpcodeCount++;
	  } else {
	    if (OpcodeCount > 0) {
	      if ((CurrentEditScriptIndex = j = AddEditScript(EditScript, EditScriptLength, CurrentEditScriptIndex,
							      LastOpcode, OpcodeCount, StartInsertToken)) < 0) return j;
	    }
	    OpcodeCount = 1;
	  }
	  LastOpcode = KeepOpcode;
	  //printf("%3d Keep   %d\n", CurrentVIndex, i);
	}
        i++;

      } else {
	
	if (LastOpcode == DeleteOpcode) {
	  OpcodeCount++;
	} else {
	  if (OpcodeCount > 0) {
	    if ((CurrentEditScriptIndex = j = AddEditScript(EditScript, EditScriptLength, CurrentEditScriptIndex,
							    LastOpcode, OpcodeCount, StartInsertToken)) < 0) return j;
	  }
	  OpcodeCount = 1;
	}

	LastOpcode = DeleteOpcode;
	//printf("%3d Delete %d\n", CurrentVIndex, V[CurrentVIndex].Index);
	CurrentVIndex = V[CurrentVIndex].Back;
	i++;
      }

    } else {

      if (i <= V[CurrentVIndex].Index) {
	if (i != 0) {
	  if (LastOpcode == KeepOpcode) {
	    OpcodeCount++;
	  } else {
	    if (OpcodeCount > 0) {
	      if ((CurrentEditScriptIndex = j = AddEditScript(EditScript, EditScriptLength, CurrentEditScriptIndex,
							      LastOpcode, OpcodeCount, StartInsertToken)) < 0) return j;
	    }
	    OpcodeCount = 1;
	  }
	  LastOpcode = KeepOpcode;
	  //printf("%3d Keep   %d\n", CurrentVIndex, i);
	}
        i++;

      } else {

	if (LastOpcode == InsertOpcode) {
	  OpcodeCount++;
	} else {
	  if (OpcodeCount > 0) {
	    if ((CurrentEditScriptIndex = j = AddEditScript(EditScript, EditScriptLength, CurrentEditScriptIndex,
							    LastOpcode, OpcodeCount, StartInsertToken)) < 0) return j;
	  }
	  LastOpcode = InsertOpcode;
	  OpcodeCount = 1;
	  StartInsertToken = V[CurrentVIndex].Token;
	}

	LastOpcode = InsertOpcode;
	//printf("%3d Insert %d %c\n", CurrentVIndex, V[CurrentVIndex].Index, *V[CurrentVIndex].Token);
	CurrentVIndex = V[CurrentVIndex].Back;
      }
    }
  }
  
  if (OpcodeCount > 0) {
    if ((CurrentEditScriptIndex = j = AddEditScript(EditScript, EditScriptLength, CurrentEditScriptIndex,
						    LastOpcode, OpcodeCount, StartInsertToken)) < 0) return j;
  }
  
  return CurrentEditScriptIndex;
}  

int ComputeEditScript (char *OldString,
		       int OldStringLength,
		       char *NewString,
		       int NewStringLength,
		       char *EditScript,
		       int EditScriptLength)
/*++

  Description:

    This routine implements Eugene W. Myers' difference algorithm for computing the difference 
    between two input strings.

  Input:

    OldString, OldStringLength: describe the first string that we are converting from, also known
      as X in this routine and Myers' paper.

    NewString, NewStringLength: describe the second string that we are converting to, also know as 
      Y in this rouitne and Myers' paper.

    EditScript, EditScriptLength: is the destination for the edit script that we will generate
      for converting the OldString into the NewString.

  Output:

    We return the number of bytes that we used in the EditScript.  Or -1 if the EditScriptLength is 
    too short to contain the needed script. Or -2 if the malloc for our workspace failed, and -3 if
    fell out of the bottom of the algorithm without successfully computed the edit script (i.e., an 
    internal error).

--*/
{
  int MaxVSize;
  PWORK_SPACE_ENTRY V;
  int D, k;
  int X, Y;
  int i;

  //printf("ComputeEditScript( %08lx, %d, %08lx, %d, %08lx, %08lx )\n", OldString, OldStringLength, NewString, NewStringLength, EditScript, EditScriptLength); 

  //
  //  Calculate how much work space is needed, then allocate and initialize it by preloading
  //  D and k values in our work array.  Also initialize the odd first step that allows us to
  //  start properly
  //

  MaxVSize = (OldStringLength+1) * (NewStringLength+1);
  if ((V = malloc(sizeof(WORK_SPACE_ENTRY) * MaxVSize * (MaxVSize+1))) == NULL) return -2;
  for (D = 0; D < MaxVSize; D++){
    for (k = -D; k <= D; k += 2){
      i = DkIndex(D,k);
      V[i].D = D;
      V[i].k = k;
    }
  }
  V[0].D = 0; V[0].k = 0; V[0].SavedX = 0; V[0].SavedY = -1;    
  //DebugPrintArray(V,0,20);

  //
  //  Now do the real work of discovering the various insert and delete paths
  //

  for (D = 0; D < MaxVSize; D++){
    for (k = -D; k <= D; k += 2){

      //
      //  Compute the index for the current D,k pairing and the indices of where we could start from
      //

      int Index,TopIndex, BotIndex;
      Index = DkIndex(D,k);
      TopIndex = DkIndex(D-1,k+1);
      BotIndex = DkIndex(D-1,k-1);
	
      if ((k == -D) || ((k != D) && V[BotIndex].SavedX < V[TopIndex].SavedX)) {

        //
        //  This is an insert command if we do not have a bottom start point (k == -D) or
	//  if the bottom start point has a saved x value that is less than tbe top saved x value.
	//
	//  X and Y  is then the where we left off in the old and new strings respecttively.
	//  These counters are based and important to remember because 'C' is zero based.  So whenever
	//  we acces the old and new strings we need to subtract 1 from our saved index.
        //
        //  For an insert we do not bump up the X but need to move the Y because that is
        //  now the index of the token we are inserting.
        //

        X = V[TopIndex].SavedX;
        Y = V[TopIndex].SavedY+1;

        //
        //  Store in the current index that fact that is an insert, the token that is being
        //  inserted and the back trace index so that we can reconstruct the edit script
        //
	  
        V[Index].IsDelete = 0;
        V[Index].Index = X;
        V[Index].Token = &NewString[Y-1];
        V[Index].Back = TopIndex;
        //printf("Insert NewString[%d] at OldString[%d-1]=%c\n", x,y,NewString[y-1]);
	  
      } else {
	  
        //
        //  Delete comand is lihe the insert command but we bump the X to skip over the
        //  token that we are deleting
        //
	  
        X = V[BotIndex].SavedX+1;
        Y = V[BotIndex].SavedY;

        //
        //  And store the necessary information at the current index
        //
	  
        V[Index].IsDelete = 1;
        V[Index].Index = X;
        V[Index].Back = BotIndex;
        //printf("Delte OldString[%d]=%c\n", x, OldString[x-1]);
      }

      //
      //  Skip over common tokens, by comparing tokens from both the Old and New Strings
      //  and skipping over common tokens, until we've exhausted either string or we do
      //  not get a match
      //
	
      while ((X<OldStringLength) && (Y<NewStringLength) && (OldString[X]==NewString[Y])) {
        X++;
        Y++;
        //printf("x = %d y = %d skip\n", x, y);
      }
	
      //
      //  Save in the currnet index where we stopped our search
      //
	
      V[Index].SavedX = X;
      V[Index].SavedY = Y;
      //DebugPrintArray(V,1,Index,Index);

      //
      //  Check if done when both X and Y have reached the ends of their perspective strings
      //
	
      if ((X >= OldStringLength) && (Y >= NewStringLength)) {
        i = ConstructEditScript((PEDIT_SCRIPT_ENTRY)EditScript, EditScriptLength, V, Index,
				OldString, OldStringLength,
				NewString, NewStringLength);
        free(V);
        return i;
      }
    }
  }
  //printf("fell through to the bottom\n");
  free(V);
  return -3;
}

int ApplyEditScript( char *OldString,
		     int OldStringLength,
		     char *EditScript,
		     int EditScriptLength,
		     char *NewString,
		     int NewStringLength)
/*++

  Description:

    This routine applies the edit script to the old string and puts the results into the new string.

    The basic algorithm is to have three active pointers to the old string, edit script and the new string.

    If the current edit script is a delete, delete count then
      skip over the old string by advancing its pointer by the delete count
    If the current edit script is a keep, keep count thne
      copy the count number of bytes from the old string to the new string advancing both pointers
    If the current edit script is an insert, count then
      copy the count number of bytes from the edit script to the new string advancing both pointers

    Lastly, if we reach the end of the edit script and there are still more bytes in the old string then
      copy over the remainder of the old string into the new string

  Input:

    OldString, OldStringLength: describe the first string that we are converting from

    EditScript, EditScriptLength: describes the edit script that is to be applied to the Old String

    NewString, NewStringLength: gets the results of applying the edit script to the old string

  Output:

    We return the number of bytes that we used in the NewString.  Or -1 if the NewStringLength is 
    too short to contain the needed string. Or -3 if the input is corrupt

--*/
{
  int OldStringIndex, EditScriptIndex, NewStringIndex;
  unsigned char Opcode, Count;

  OldStringIndex = EditScriptIndex = NewStringIndex = 0;

  while (EditScriptIndex < EditScriptLength) {

    Opcode = ((PEDIT_SCRIPT_ENTRY)EditScript)[EditScriptIndex].Opcode;
    Count = ((PEDIT_SCRIPT_ENTRY)EditScript)[EditScriptIndex].Count+1; // account for the bias

    if (Opcode == DeleteOpcode) {
      OldStringIndex += Count;
    } else if (Opcode == KeepOpcode) {
      if (NewStringIndex + Count >= NewStringLength) return -1;
      memcpy(&NewString[NewStringIndex], &OldString[OldStringIndex], Count);
      OldStringIndex += Count;
      NewStringIndex += Count;
    } else { // Opcode == InsertOpcoode
      if (NewStringIndex + Count >= NewStringLength) return -1;
      memcpy(&NewString[NewStringIndex], &EditScript[EditScriptIndex+1], Count);
      EditScriptIndex += Count;
      NewStringIndex += Count;
    }
    EditScriptIndex += 1;
  }

  if (OldStringIndex < OldStringLength) {
    if (NewStringIndex + (OldStringLength - OldStringIndex) >= NewStringLength) return -1;
    memcpy(&NewString[NewStringIndex], &OldString[OldStringIndex], OldStringLength - OldStringIndex);
    NewStringIndex += OldStringLength - OldStringIndex;
  }
  return NewStringIndex;
}

#ifdef _MAIN_
void main (int argc, char *argv[])
{
  //char OldString[128] = "quickfoxback!"; // "abcabba"; // "DEFJKL"; //"ABCABBA";
  //char NewString[128] = "The quick brown fox jumped over the lazy dog's back!"; // "abcDEFghiJLK"; //JKLMNOPQRSTUVWXYZ"; //"CBABAC";
  char EditScript[32768]; int EditScriptLength = 32768;
  char NewString[65536]; int NewStringLength = 65536;
  int i;

  //printf("sizeof(char) = %d\n", sizeof(char));
  //printf("sizeof(int) = %d\n", sizeof(int));
  //printf("sizeof(short int) = %d\n", sizeof(short int));
  //printf("sizeof(double) = %d\n", sizeof(double));
  //printf("sizeof(long int) = %d\n", sizeof(long int));
  //printf("sizeof(long long int) = %d\n", sizeof(long long int));
  //printf("sizeof(EDIT_SCRIPT_TOKEN) = %d\n", sizeof(EDIT_SCRIPT_TOKEN));

  printf("\n%s \"%s\" \"%s\"\n", argv[0], argv[1], argv[2]);
  EditScriptLength = 128;
  i = ComputeEditScript(argv[1], strlen(argv[1]), argv[2], strlen(argv[2]), EditScript, EditScriptLength);
  if (i < 0) { 
    printf("ComputeEditScript Failure %d\n", i);
  } else {
    DebugPrintEditScript((PEDIT_SCRIPT_ENTRY)EditScript, i);
    i = ApplyEditScript( argv[1], strlen(argv[1]), EditScript, i, NewString, NewStringLength);
    if (i < 0 ) {
      printf("ApplyEditScriptFailure %d\n", i);
    }
    NewString[i] = 0;
    printf("NewString Length=%d, \"%s\"\n", i, NewString);
  }
}
#endif
