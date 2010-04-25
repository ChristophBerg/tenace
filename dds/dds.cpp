
/* DDS 2.0.0   A bridge double dummy solver.				      */
/* Copyright (C) 2006-2010 by Bo Haglund                                      */
/* Cleanups and porting to Linux and MacOSX (C) 2006 by Alex Martelli         */
/*								              */
/* This program is free software; you can redistribute it and/or              */
/* modify it under the terms of the GNU General Public License                */
/* as published by the Free software Foundation; either version 2             */
/* of the License, or (at your option) any later version.                     */ 
/*								              */
/* This program is distributed in the hope that it will be useful,            */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/* GNU General Public License for more details.                               */
/*								              */
/* You should have received a copy of the GNU General Public License          */
/* along with this program; if not, write to the Free Software                */
/* Foundation, Inc, 51 Franklin Street, 5th Floor, Boston MA 02110-1301, USA. */

/*#include "stdafx.h" */		/* Needed by Visual C++ */

#include "dll.h"

struct localVarType localVar[MAXNOOFTHREADS];

int * counttable;
int * highestRank;
int lho[4]; 
int rho[4]; 
int partner[4];
unsigned short int bitMapRank[16];
unsigned char cardRank[15];
unsigned char cardSuit[5];
unsigned char cardHand[4];

struct ttStoreType * ttStore;
int lastTTstore;
int ttCollect;
int suppressTTlog;

#if defined(_WIN32)
int noOfThreads;
#endif

#if defined(_MSC_VER)
CRITICAL_SECTION solv_crit;
#endif

#ifdef _MANAGED
#pragma managed(push, off)
#endif

#if 0
extern "C" BOOL APIENTRY DllMain(HMODULE hModule,
				DWORD ul_reason_for_call,
				LPVOID lpReserved) {
  int k;

  if (ul_reason_for_call==DLL_PROCESS_ATTACH) {
    InitStart();
#if defined(_MSC_VER)
	InitializeCriticalSection(&solv_crit);
#endif
  }
  else if (ul_reason_for_call==DLL_PROCESS_DETACH) {
#if defined(_MSC_VER)
    DeleteCriticalSection(&solv_crit);
#endif
    for (k=0; k<MAXNOOFTHREADS; k++) {	
      Wipe(k); 
      if (localVar[k].pw[0])
        free(localVar[k].pw[0]);
      localVar[k].pw[0]=NULL;
      if (localVar[k].pn[0])
        free(localVar[k].pn[0]);
      localVar[k].pn[0]=NULL;
      if (localVar[k].pl[0])
        free(localVar[k].pl[0]);
      localVar[k].pl[0]=NULL;
      if (ttStore)
        free(ttStore);
      ttStore=NULL;
      if (localVar[k].rel)
        free(localVar[k].rel);
      localVar[k].rel=NULL;
    }
    if (highestRank)
      free(highestRank);
    highestRank=NULL;
    if (counttable)
      free(counttable);
    counttable=NULL;
  }
  return 1;
}
#endif

#ifdef _MANAGED
#pragma managed(pop)
#endif

  int STDCALL SolveBoard(struct deal dl, int target, 
    int solutions, int mode, struct futureTricks *futp, int thrId) {

  int k, l, n, cardCount, found, totalTricks, tricks, last, checkRes;
  int g, upperbound, lowerbound, first, i, j, h, forb, ind, flag, noMoves;
  int mcurr;
  int noStartMoves;
  int handRelFirst;
  int noOfCardsPerHand[4];
  int latestTrickSuit[4];
  int latestTrickRank[4];
  int maxHand=0, maxSuit=0, maxRank;
  unsigned short int aggrRemain; 
  struct movePlyType temp;
  struct moveType mv;
  
  
  /*InitStart();*/   /* Include InitStart() if inside SolveBoard,
			   but preferable InitStart should be called outside
					 SolveBoard like in DllMain for Windows. */

  for (k=0; k<=13; k++) {
    localVar[thrId].forbiddenMoves[k].rank=0;
    localVar[thrId].forbiddenMoves[k].suit=0;
  }

  if ((thrId<0)||(thrId>=MAXNOOFTHREADS)) {
    DumpInput(-15, dl, target, solutions, mode);
	return -15;
  }

  if (target<-1) {
    DumpInput(-5, dl, target, solutions, mode);
    return -5;
  }
  if (target>13) {
    DumpInput(-7, dl, target, solutions, mode);
    return -7;
  }
  if (solutions<1) {
    DumpInput(-8, dl, target, solutions, mode);
    return -8;
  }
  if (solutions>3) {
    DumpInput(-9, dl, target, solutions, mode);
    return -9;
  }

  for (k=0; k<=3; k++)
    noOfCardsPerHand[handId(dl.first, k)]=0;
  
  for (k=0; k<=2; k++) {
	if (dl.currentTrickRank[k]!=0) {
	  noOfCardsPerHand[handId(dl.first, k)]=1;
      aggrRemain=0;
	  for (h=0; h<=3; h++) 
	    aggrRemain|=(dl.remainCards[h][dl.currentTrickSuit[k]]>>2);
	  if ((aggrRemain & bitMapRank[dl.currentTrickRank[k]])!=0) {
		DumpInput(-13, dl, target, solutions, mode);
		return -13;
	  }
	}
  }
  
  if (target==-1)
    localVar[thrId].tricksTarget=99;
  else
    localVar[thrId].tricksTarget=target;

  localVar[thrId].newDeal=FALSE; localVar[thrId].newTrump=FALSE;
  localVar[thrId].diffDeal=0; localVar[thrId].aggDeal=0;
  for (i=0; i<=3; i++) {
    for (j=0; j<=3; j++) {
	  localVar[thrId].diffDeal+=((dl.remainCards[i][j]>>2)^
	    (localVar[thrId].game.suit[i][j]));
      localVar[thrId].aggDeal+=(dl.remainCards[i][j]>>2);
      if (localVar[thrId].game.suit[i][j]!=dl.remainCards[i][j]>>2) {
        localVar[thrId].game.suit[i][j]=dl.remainCards[i][j]>>2;
	    localVar[thrId].newDeal=TRUE;
      }
    }
  }

  if (localVar[thrId].newDeal) {
    if (localVar[thrId].diffDeal==0)
	  localVar[thrId].similarDeal=TRUE;
    else if ((localVar[thrId].aggDeal/localVar[thrId].diffDeal)
       >SIMILARDEALLIMIT)
	  localVar[thrId].similarDeal=TRUE;
	else
	  localVar[thrId].similarDeal=FALSE;
  }

  if (dl.trump!=localVar[thrId].trump)
    localVar[thrId].newTrump=TRUE;

  for (i=0; i<=3; i++)
    for (j=0; j<=3; j++)
      noOfCardsPerHand[i]+=counttable[localVar[thrId].game.suit[i][j]];

  for (i=1; i<=3; i++) {
	if (noOfCardsPerHand[i]!=noOfCardsPerHand[0]) {
	  DumpInput(-14, dl, target, solutions, mode);
	  return -14;
	}
  }

  cardCount=0;
  for (k=0; k<=3; k++)
    for (l=0; l<=3; l++)
      cardCount+=counttable[localVar[thrId].game.suit[k][l]];

  if (dl.currentTrickRank[2]) {
    if ((dl.currentTrickRank[2]<2)||(dl.currentTrickRank[2]>14)
      ||(dl.currentTrickSuit[2]<0)||(dl.currentTrickSuit[2]>3)) {
      DumpInput(-12, dl, target, solutions, mode);
      return -12;
    }
    localVar[thrId].handToPlay=handId(dl.first, 3);
    handRelFirst=3;
    noStartMoves=3;
    if (cardCount<=4) {
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[localVar[thrId].handToPlay][k]!=0) {
          latestTrickSuit[localVar[thrId].handToPlay]=k;
          latestTrickRank[localVar[thrId].handToPlay]=
            InvBitMapRank(localVar[thrId].game.suit[localVar[thrId].handToPlay][k]);
          break;
        }
      }
      latestTrickSuit[handId(dl.first, 2)]=dl.currentTrickSuit[2];
      latestTrickRank[handId(dl.first, 2)]=dl.currentTrickRank[2];
      latestTrickSuit[handId(dl.first, 1)]=dl.currentTrickSuit[1];
      latestTrickRank[handId(dl.first, 1)]=dl.currentTrickRank[1];
      latestTrickSuit[dl.first]=dl.currentTrickSuit[0];
      latestTrickRank[dl.first]=dl.currentTrickRank[0];
    }
  }
  else if (dl.currentTrickRank[1]) {
    if ((dl.currentTrickRank[1]<2)||(dl.currentTrickRank[1]>14)
      ||(dl.currentTrickSuit[1]<0)||(dl.currentTrickSuit[1]>3)) {
      DumpInput(-12, dl, target, solutions, mode);
      return -12;
    }
    localVar[thrId].handToPlay=handId(dl.first, 2);
    handRelFirst=2;
    noStartMoves=2;
    if (cardCount<=4) {
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[localVar[thrId].handToPlay][k]!=0) {
          latestTrickSuit[localVar[thrId].handToPlay]=k;
          latestTrickRank[localVar[thrId].handToPlay]=
            InvBitMapRank(localVar[thrId].game.suit[localVar[thrId].handToPlay][k]);
          break;
        }
      }
      for (k=0; k<=3; k++) {
	if (localVar[thrId].game.suit[handId(dl.first, 3)][k]!=0) {
          latestTrickSuit[handId(dl.first, 3)]=k;
          latestTrickRank[handId(dl.first, 3)]=
            InvBitMapRank(localVar[thrId].game.suit[handId(dl.first, 3)][k]);
          break;
        }
      }
      latestTrickSuit[handId(dl.first, 1)]=dl.currentTrickSuit[1];
      latestTrickRank[handId(dl.first, 1)]=dl.currentTrickRank[1];
      latestTrickSuit[dl.first]=dl.currentTrickSuit[0];
      latestTrickRank[dl.first]=dl.currentTrickRank[0];
    }
  }
  else if (dl.currentTrickRank[0]) {
    if ((dl.currentTrickRank[0]<2)||(dl.currentTrickRank[0]>14)
      ||(dl.currentTrickSuit[0]<0)||(dl.currentTrickSuit[0]>3)) {
      DumpInput(-12, dl, target, solutions, mode);
      return -12;
    }
    localVar[thrId].handToPlay=handId(dl.first,1);
    handRelFirst=1;
    noStartMoves=1;
    if (cardCount<=4) {
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[localVar[thrId].handToPlay][k]!=0) {
          latestTrickSuit[localVar[thrId].handToPlay]=k;
          latestTrickRank[localVar[thrId].handToPlay]=
            InvBitMapRank(localVar[thrId].game.suit[localVar[thrId].handToPlay][k]);
          break;
        }
      }
      for (k=0; k<=3; k++) {
	if (localVar[thrId].game.suit[handId(dl.first, 3)][k]!=0) {
          latestTrickSuit[handId(dl.first, 3)]=k;
          latestTrickRank[handId(dl.first, 3)]=
            InvBitMapRank(localVar[thrId].game.suit[handId(dl.first, 3)][k]);
          break;
        }
      }
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[handId(dl.first, 2)][k]!=0) {
          latestTrickSuit[handId(dl.first, 2)]=k;
          latestTrickRank[handId(dl.first, 2)]=
            InvBitMapRank(localVar[thrId].game.suit[handId(dl.first, 2)][k]);
          break;
        }
      }
      latestTrickSuit[dl.first]=dl.currentTrickSuit[0];
      latestTrickRank[dl.first]=dl.currentTrickRank[0];
    }
  }
  else {
    localVar[thrId].handToPlay=dl.first;
    handRelFirst=0;
    noStartMoves=0;
    if (cardCount<=4) {
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[localVar[thrId].handToPlay][k]!=0) {
          latestTrickSuit[localVar[thrId].handToPlay]=k;
          latestTrickRank[localVar[thrId].handToPlay]=
            InvBitMapRank(localVar[thrId].game.suit[localVar[thrId].handToPlay][k]);
          break;
        }
      }
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[handId(dl.first, 3)][k]!=0) {
          latestTrickSuit[handId(dl.first, 3)]=k;
          latestTrickRank[handId(dl.first, 3)]=
            InvBitMapRank(localVar[thrId].game.suit[handId(dl.first, 3)][k]);
          break;
        }
      }
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[handId(dl.first, 2)][k]!=0) {
          latestTrickSuit[handId(dl.first, 2)]=k;
          latestTrickRank[handId(dl.first, 2)]=
            InvBitMapRank(localVar[thrId].game.suit[handId(dl.first, 2)][k]);
          break;
        }
      }
      for (k=0; k<=3; k++) {
        if (localVar[thrId].game.suit[handId(dl.first, 1)][k]!=0) {
          latestTrickSuit[handId(dl.first, 1)]=k;
          latestTrickRank[handId(dl.first, 1)]=
            InvBitMapRank(localVar[thrId].game.suit[handId(dl.first, 1)][k]);
          break;
        }
      }
    }
  }

  localVar[thrId].trump=dl.trump;
  localVar[thrId].game.first=dl.first;
  first=dl.first;
  localVar[thrId].game.noOfCards=cardCount;
  if (dl.currentTrickRank[0]!=0) {
    localVar[thrId].game.leadHand=dl.first;
    localVar[thrId].game.leadSuit=dl.currentTrickSuit[0];
    localVar[thrId].game.leadRank=dl.currentTrickRank[0];
  }
  else {
    localVar[thrId].game.leadHand=0;
    localVar[thrId].game.leadSuit=0;
    localVar[thrId].game.leadRank=0;
  }

  for (k=0; k<=2; k++) {
    localVar[thrId].initialMoves[k].suit=255;
    localVar[thrId].initialMoves[k].rank=255;
  }

  for (k=0; k<noStartMoves; k++) {
    localVar[thrId].initialMoves[noStartMoves-1-k].suit=dl.currentTrickSuit[k];
    localVar[thrId].initialMoves[noStartMoves-1-k].rank=dl.currentTrickRank[k];
  }

  if (cardCount % 4)
    totalTricks=((cardCount-4)>>2)+2;
  else
    totalTricks=((cardCount-4)>>2)+1;
  checkRes=CheckDeal(&localVar[thrId].cd, thrId);
  if (localVar[thrId].game.noOfCards<=0) {
    DumpInput(-2, dl, target, solutions, mode);
    return -2;
  }
  if (localVar[thrId].game.noOfCards>52) {
    DumpInput(-10, dl, target, solutions, mode);
    return -10;
  }
  if (totalTricks<target) {
    DumpInput(-3, dl, target, solutions, mode);
    return -3;
  }
  if (checkRes) {
    DumpInput(-4, dl, target, solutions, mode);
    return -4;
  }

  if (cardCount<=4) {
    maxRank=0;
    /* Highest trump? */
    if (dl.trump!=4) {
      for (k=0; k<=3; k++) {
        if ((latestTrickSuit[k]==dl.trump)&&
          (latestTrickRank[k]>maxRank)) {
          maxRank=latestTrickRank[k];
          maxSuit=dl.trump;
          maxHand=k;
        }
      }
    }
    /* Highest card in leading suit */
    if (maxRank==0) {
      for (k=0; k<=3; k++) {
        if (k==dl.first) {
          maxSuit=latestTrickSuit[dl.first];
          maxHand=dl.first;
          maxRank=latestTrickRank[dl.first];
          break;
        }
      }
      for (k=0; k<=3; k++) {
        if ((k!=dl.first)&&(latestTrickSuit[k]==maxSuit)&&
          (latestTrickRank[k]>maxRank)) {
          maxHand=k;
          maxRank=latestTrickRank[k];
        }
      }
    }
    futp->nodes=0;
    #ifdef BENCH
    futp->totalNodes=0;
    #endif
    futp->cards=1;
    futp->suit[0]=latestTrickSuit[localVar[thrId].handToPlay];
    futp->rank[0]=latestTrickRank[localVar[thrId].handToPlay];
    futp->equals[0]=0;
    if ((target==0)&&(solutions<3))
      futp->score[0]=0;
    else if ((localVar[thrId].handToPlay==maxHand)||
	(partner[localVar[thrId].handToPlay]==maxHand))
      futp->score[0]=1;
    else
      futp->score[0]=0;
	
    return 1;
  }

  if ((mode!=2)&&
	  (((localVar[thrId].newDeal)&&(!localVar[thrId].similarDeal)) 
	  || localVar[thrId].newTrump)) {
    Wipe(thrId);
	localVar[thrId].winSetSizeLimit=WINIT;
    localVar[thrId].nodeSetSizeLimit=NINIT;
    localVar[thrId].lenSetSizeLimit=LINIT;
    localVar[thrId].allocmem=(WINIT+1)*sizeof(struct winCardType);
	localVar[thrId].allocmem+=(NINIT+1)*sizeof(struct nodeCardsType);
	localVar[thrId].allocmem+=(LINIT+1)*sizeof(struct posSearchType);
    localVar[thrId].winCards=localVar[thrId].pw[0];
	localVar[thrId].nodeCards=localVar[thrId].pn[0];
	localVar[thrId].posSearch=localVar[thrId].pl[0];
	localVar[thrId].wcount=0; localVar[thrId].ncount=0; localVar[thrId].lcount=0;
    InitGame(0, FALSE, first, handRelFirst, thrId);
  }
  else {
    InitGame(0, TRUE, first, handRelFirst, thrId);
	/*localVar[thrId].fp2=fopen("dyn.txt", "a");
	fprintf(localVar[thrId].fp2, "wcount=%d, ncount=%d, lcount=%d\n", 
	  wcount, ncount, lcount);
    fprintf(localVar[thrId].fp2, "winSetSize=%d, nodeSetSize=%d, lenSetSize=%d\n", 
	  winSetSize, nodeSetSize, lenSetSize);
    fclose(localVar[thrId].fp2);*/
  }

  localVar[thrId].nodes=0; localVar[thrId].trickNodes=0;
  localVar[thrId].iniDepth=cardCount-4;
  localVar[thrId].hiwinSetSize=0;
  localVar[thrId].hinodeSetSize=0;

  if (mode==0) {
    MoveGen(&localVar[thrId].lookAheadPos, localVar[thrId].iniDepth, thrId);
    if (localVar[thrId].movePly[localVar[thrId].iniDepth].last==0) {
	futp->nodes=0;
    #ifdef BENCH
        futp->totalNodes=0;
    #endif
	futp->cards=1;
	futp->suit[0]=localVar[thrId].movePly[localVar[thrId].iniDepth].move[0].suit;
	futp->rank[0]=localVar[thrId].movePly[localVar[thrId].iniDepth].move[0].rank;
	futp->equals[0]=
	  localVar[thrId].movePly[localVar[thrId].iniDepth].move[0].sequence<<2;
	futp->score[0]=-2;
	
	return 1;
    }
  }
  if ((target==0)&&(solutions<3)) {
    MoveGen(&localVar[thrId].lookAheadPos, localVar[thrId].iniDepth, thrId);
    futp->nodes=0;
    #ifdef BENCH
    futp->totalNodes=0;
    #endif
    for (k=0; k<=localVar[thrId].movePly[localVar[thrId].iniDepth].last; k++) {
	futp->suit[k]=localVar[thrId].movePly[localVar[thrId].iniDepth].move[k].suit;
	futp->rank[k]=localVar[thrId].movePly[localVar[thrId].iniDepth].move[k].rank;
	futp->equals[k]=
	  localVar[thrId].movePly[localVar[thrId].iniDepth].move[k].sequence<<2;
	futp->score[k]=0;
    }
    if (solutions==1)
	futp->cards=1;
    else
	futp->cards=localVar[thrId].movePly[localVar[thrId].iniDepth].last+1;
	
    return 1;
  }

  if ((target!=-1)&&(solutions!=3)) {
    localVar[thrId].val=ABsearch(&localVar[thrId].lookAheadPos, 
		localVar[thrId].tricksTarget, localVar[thrId].iniDepth, thrId);
    
    temp=localVar[thrId].movePly[localVar[thrId].iniDepth];
    last=localVar[thrId].movePly[localVar[thrId].iniDepth].last;
    noMoves=last+1;
    localVar[thrId].hiwinSetSize=localVar[thrId].winSetSize;
    localVar[thrId].hinodeSetSize=localVar[thrId].nodeSetSize;
    localVar[thrId].hilenSetSize=localVar[thrId].lenSetSize;
    if (localVar[thrId].nodeSetSize>localVar[thrId].MaxnodeSetSize)
      localVar[thrId].MaxnodeSetSize=localVar[thrId].nodeSetSize;
    if (localVar[thrId].winSetSize>localVar[thrId].MaxwinSetSize)
      localVar[thrId].MaxwinSetSize=localVar[thrId].winSetSize;
    if (localVar[thrId].lenSetSize>localVar[thrId].MaxlenSetSize)
      localVar[thrId].MaxlenSetSize=localVar[thrId].lenSetSize;
    if (localVar[thrId].val==1)
      localVar[thrId].payOff=localVar[thrId].tricksTarget;
    else
      localVar[thrId].payOff=0;
    futp->cards=1;
    ind=2;

    if (localVar[thrId].payOff<=0) {
      futp->suit[0]=localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[0].suit;
      futp->rank[0]=localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[0].rank;
	futp->equals[0]=(localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[0].sequence)<<2;
	if (localVar[thrId].tricksTarget>1)
        futp->score[0]=-1;
	else
	  futp->score[0]=0;
    }
    else {
      futp->suit[0]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].suit;
      futp->rank[0]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].rank;
	futp->equals[0]=(localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].sequence)<<2;
      futp->score[0]=localVar[thrId].payOff;
    }
  }
  else {
    g=localVar[thrId].estTricks[localVar[thrId].handToPlay];
    upperbound=13;
    lowerbound=0;
    do {
      if (g==lowerbound)
        tricks=g+1;
      else
        tricks=g;
	  assert((localVar[thrId].lookAheadPos.handRelFirst>=0)&&
		(localVar[thrId].lookAheadPos.handRelFirst<=3));
      localVar[thrId].val=ABsearch(&localVar[thrId].lookAheadPos, tricks, 
		  localVar[thrId].iniDepth, thrId);
      
      if (localVar[thrId].val==TRUE)
        mv=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4];
      localVar[thrId].hiwinSetSize=Max(localVar[thrId].hiwinSetSize, localVar[thrId].winSetSize);
      localVar[thrId].hinodeSetSize=Max(localVar[thrId].hinodeSetSize, localVar[thrId].nodeSetSize);
	localVar[thrId].hilenSetSize=Max(localVar[thrId].hilenSetSize, localVar[thrId].lenSetSize);
      if (localVar[thrId].nodeSetSize>localVar[thrId].MaxnodeSetSize)
        localVar[thrId].MaxnodeSetSize=localVar[thrId].nodeSetSize;
      if (localVar[thrId].winSetSize>localVar[thrId].MaxwinSetSize)
        localVar[thrId].MaxwinSetSize=localVar[thrId].winSetSize;
	if (localVar[thrId].lenSetSize>localVar[thrId].MaxlenSetSize)
        localVar[thrId].MaxlenSetSize=localVar[thrId].lenSetSize;
      if (localVar[thrId].val==FALSE) {
	  upperbound=tricks-1;
	  g=upperbound;
	}	
	else {
	  lowerbound=tricks;
	  g=lowerbound;
	}
      InitSearch(&localVar[thrId].iniPosition, localVar[thrId].game.noOfCards-4,
        localVar[thrId].initialMoves, first, TRUE, thrId);
    }
    while (lowerbound<upperbound);
    localVar[thrId].payOff=g;
    temp=localVar[thrId].movePly[localVar[thrId].iniDepth];
    last=localVar[thrId].movePly[localVar[thrId].iniDepth].last;
    noMoves=last+1;
    ind=2;
    localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4]=mv;
    futp->cards=1;
    if (localVar[thrId].payOff<=0) {
      futp->score[0]=0;
      futp->suit[0]=localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[0].suit;
      futp->rank[0]=localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[0].rank;
	futp->equals[0]=(localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[0].sequence)<<2;
    }
    else {
      futp->score[0]=localVar[thrId].payOff;
      futp->suit[0]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].suit;
      futp->rank[0]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].rank;
	futp->equals[0]=(localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].sequence)<<2;
    }
    localVar[thrId].tricksTarget=localVar[thrId].payOff;
  }

  if ((solutions==2)&&(localVar[thrId].payOff>0)) {
    forb=1;
    ind=forb;
    while ((localVar[thrId].payOff==localVar[thrId].tricksTarget)&&(ind<(temp.last+1))) {
      localVar[thrId].forbiddenMoves[forb].suit=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].suit;
      localVar[thrId].forbiddenMoves[forb].rank=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].rank;
      forb++; ind++;
      /* All moves before bestMove in the move list shall be
      moved to the forbidden moves list, since none of them reached
      the target */
      mcurr=localVar[thrId].movePly[localVar[thrId].iniDepth].current;
      for (k=0; k<=localVar[thrId].movePly[localVar[thrId].iniDepth].last; k++)
        if ((localVar[thrId].bestMove[localVar[thrId].iniDepth].suit==
			localVar[thrId].movePly[localVar[thrId].iniDepth].move[k].suit)
          &&(localVar[thrId].bestMove[localVar[thrId].iniDepth].rank==
		    localVar[thrId].movePly[localVar[thrId].iniDepth].move[k].rank))
          break;
      for (i=0; i<k; i++) {  /* All moves until best move */
        flag=FALSE;
        for (j=0; j<forb; j++) {
          if ((localVar[thrId].movePly[localVar[thrId].iniDepth].move[i].suit==localVar[thrId].forbiddenMoves[j].suit)
            &&(localVar[thrId].movePly[localVar[thrId].iniDepth].move[i].rank==localVar[thrId].forbiddenMoves[j].rank)) {
            /* If the move is already in the forbidden list */
            flag=TRUE;
            break;
          }
        }
        if (!flag) {
          localVar[thrId].forbiddenMoves[forb]=localVar[thrId].movePly[localVar[thrId].iniDepth].move[i];
          forb++;
        }
      }
      if (1/*(winSetSize<winSetFill)&&(nodeSetSize<nodeSetFill)*/)
        InitSearch(&localVar[thrId].iniPosition, localVar[thrId].game.noOfCards-4,
          localVar[thrId].initialMoves, first, TRUE, thrId);
      else
        InitSearch(&localVar[thrId].iniPosition, localVar[thrId].game.noOfCards-4,
          localVar[thrId].initialMoves, first, FALSE, thrId);
      localVar[thrId].val=ABsearch(&localVar[thrId].lookAheadPos, localVar[thrId].tricksTarget, 
		  localVar[thrId].iniDepth, thrId);
      
      localVar[thrId].hiwinSetSize=localVar[thrId].winSetSize;
      localVar[thrId].hinodeSetSize=localVar[thrId].nodeSetSize;
	  localVar[thrId].hilenSetSize=localVar[thrId].lenSetSize;
      if (localVar[thrId].nodeSetSize>localVar[thrId].MaxnodeSetSize)
        localVar[thrId].MaxnodeSetSize=localVar[thrId].nodeSetSize;
      if (localVar[thrId].winSetSize>localVar[thrId].MaxwinSetSize)
        localVar[thrId].MaxwinSetSize=localVar[thrId].winSetSize;
	if (localVar[thrId].lenSetSize>localVar[thrId].MaxlenSetSize)
        localVar[thrId].MaxlenSetSize=localVar[thrId].lenSetSize;
      if (localVar[thrId].val==TRUE) {
        localVar[thrId].payOff=localVar[thrId].tricksTarget;
        futp->cards=ind;
        futp->suit[ind-1]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].suit;
        futp->rank[ind-1]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].rank;
	  futp->equals[ind-1]=(localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].sequence)<<2;
        futp->score[ind-1]=localVar[thrId].payOff;
      }
      else
        localVar[thrId].payOff=0;
    }
  }
  else if ((solutions==2)&&(localVar[thrId].payOff==0)&&
	((target==-1)||(localVar[thrId].tricksTarget==1))) {
    futp->cards=noMoves;
    /* Find the cards that were in the initial move list
    but have not been listed in the current result */
    n=0;
    for (i=0; i<noMoves; i++) {
      found=FALSE;
      if ((temp.move[i].suit==futp->suit[0])&&
        (temp.move[i].rank==futp->rank[0])) {
          found=TRUE;
      }
      if (!found) {
        futp->suit[1+n]=temp.move[i].suit;
        futp->rank[1+n]=temp.move[i].rank;
	  futp->equals[1+n]=(temp.move[i].sequence)<<2;
        futp->score[1+n]=0;
        n++;
      }
    }
  }

  if ((solutions==3)&&(localVar[thrId].payOff>0)) {
    forb=1;
    ind=forb;
    for (i=0; i<last; i++) {
      localVar[thrId].forbiddenMoves[forb].suit=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].suit;
      localVar[thrId].forbiddenMoves[forb].rank=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].rank;
      forb++; ind++;

      g=localVar[thrId].payOff;
      upperbound=localVar[thrId].payOff;
      lowerbound=0;
      if (0/*(winSetSize>=winSetFill)||(nodeSetSize>=nodeSetFill)*/)
        InitSearch(&localVar[thrId].iniPosition, localVar[thrId].game.noOfCards-4,
          localVar[thrId].initialMoves, first, FALSE, thrId);
	else 
	  InitSearch(&localVar[thrId].iniPosition, localVar[thrId].game.noOfCards-4,
          localVar[thrId].initialMoves, first, TRUE, thrId);
      do {
        if (g==lowerbound)
          tricks=g+1;
        else
          tricks=g;
		assert((localVar[thrId].lookAheadPos.handRelFirst>=0)&&
		  (localVar[thrId].lookAheadPos.handRelFirst<=3));
        localVar[thrId].val=ABsearch(&localVar[thrId].lookAheadPos, tricks, 
			localVar[thrId].iniDepth, thrId);
        
        if (localVar[thrId].val==TRUE)
          mv=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4];
        localVar[thrId].hiwinSetSize=Max(localVar[thrId].hiwinSetSize, localVar[thrId].winSetSize);
        localVar[thrId].hinodeSetSize=Max(localVar[thrId].hinodeSetSize, localVar[thrId].nodeSetSize);
	localVar[thrId].hilenSetSize=Max(localVar[thrId].hilenSetSize, localVar[thrId].lenSetSize);
        if (localVar[thrId].nodeSetSize>localVar[thrId].MaxnodeSetSize)
          localVar[thrId].MaxnodeSetSize=localVar[thrId].nodeSetSize;
        if (localVar[thrId].winSetSize>localVar[thrId].MaxwinSetSize)
          localVar[thrId].MaxwinSetSize=localVar[thrId].winSetSize;
		if (localVar[thrId].lenSetSize>localVar[thrId].MaxlenSetSize)
          localVar[thrId].MaxlenSetSize=localVar[thrId].lenSetSize;
        if (localVar[thrId].val==FALSE) {
	    upperbound=tricks-1;
	    g=upperbound;
	  }	
	  else {
	    lowerbound=tricks;
	    g=lowerbound;
	  }
        if (0/*(winSetSize>=winSetFill)||(nodeSetSize>=nodeSetFill)*/)
          InitSearch(&localVar[thrId].iniPosition, localVar[thrId].game.noOfCards-4,
            localVar[thrId].initialMoves, first, FALSE, thrId);
        else
          InitSearch(&localVar[thrId].iniPosition, localVar[thrId].game.noOfCards-4,
            localVar[thrId].initialMoves, first, TRUE, thrId);
      }
      while (lowerbound<upperbound);
      localVar[thrId].payOff=g;
      if (localVar[thrId].payOff==0) {
        last=localVar[thrId].movePly[localVar[thrId].iniDepth].last;
        futp->cards=temp.last+1;
        for (j=0; j<=last; j++) {
          futp->suit[ind-1+j]=localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[j].suit;
          futp->rank[ind-1+j]=localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[j].rank;
	    futp->equals[ind-1+j]=(localVar[thrId].movePly[localVar[thrId].game.noOfCards-4].move[j].sequence)<<2;
          futp->score[ind-1+j]=localVar[thrId].payOff;
        }
        break;
      }
      else {
        localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4]=mv;

        futp->cards=ind;
        futp->suit[ind-1]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].suit;
        futp->rank[ind-1]=localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].rank;
	  futp->equals[ind-1]=(localVar[thrId].bestMove[localVar[thrId].game.noOfCards-4].sequence)<<2;
        futp->score[ind-1]=localVar[thrId].payOff;
      }   
    }
  }
  else if ((solutions==3)&&(localVar[thrId].payOff==0)) {
    futp->cards=noMoves;
    /* Find the cards that were in the initial move list
    but have not been listed in the current result */
    n=0;
    for (i=0; i<noMoves; i++) {
      found=FALSE;
      if ((temp.move[i].suit==futp->suit[0])&&
        (temp.move[i].rank==futp->rank[0])) {
          found=TRUE;
      }
      if (!found) {
        futp->suit[1+n]=temp.move[i].suit;
        futp->rank[1+n]=temp.move[i].rank;
	  futp->equals[1+n]=(temp.move[i].sequence)<<2;
        futp->score[1+n]=0;
        n++;
      }
    }
  }

  for (k=0; k<=13; k++) {
    localVar[thrId].forbiddenMoves[k].suit=0;
    localVar[thrId].forbiddenMoves[k].rank=0;
  }

  futp->nodes=localVar[thrId].trickNodes;
  #ifdef BENCH
  futp->totalNodes=localVar[thrId].nodes;
  #endif
  /*if ((wcount>0)||(ncount>0)||(lcount>0)) {
    localVar[thrId].fp2=fopen("dyn.txt", "a");
	fprintf(localVar[thrId].fp2, "wcount=%d, ncount=%d, lcount=%d\n", 
	  wcount, ncount, lcount);
    fprintf(localVar[thrId].fp2, "winSetSize=%d, nodeSetSize=%d, lenSetSize=%d\n", 
	  winSetSize, nodeSetSize, lenSetSize);
	fprintf(localVar[thrId].fp2, "\n");
    fclose(localVar[thrId].fp2);
  }*/


  return 1;
}


int _initialized=0;

void InitStart(void) {
  int k, r, i, j, m;
  unsigned short int res;

  if (_initialized)
      return;
  _initialized = 1;

  ttCollect=FALSE;
  suppressTTlog=FALSE;
  lastTTstore=0;
  ttStore = (struct ttStoreType *)calloc(SEARCHSIZE, sizeof(struct ttStoreType));
  if (ttStore==NULL)
    exit(1);

#ifdef _WIN32

  SYSTEM_INFO temp;

  GetSystemInfo(&temp);
  noOfThreads=Min(MAXNOOFTHREADS, temp.dwNumberOfProcessors);

  long double pcmem;
  /*FILE *fp;*/

  MEMORYSTATUS stat;

  GlobalMemoryStatus (&stat);

  pcmem=stat.dwTotalPhys/1024;
#endif

  for (k=0; k<MAXNOOFTHREADS; k++) {
    localVar[k].trump=-1;
    localVar[k].hiwinSetSize=0; 
    localVar[k].hinodeSetSize=0;
    localVar[k].hilenSetSize=0;
    localVar[k].MaxnodeSetSize=0;
    localVar[k].MaxwinSetSize=0;
    localVar[k].MaxlenSetSize=0;
    localVar[k].nodeSetSizeLimit=0;
    localVar[k].winSetSizeLimit=0;
    localVar[k].lenSetSizeLimit=0;
    localVar[k].clearTTflag=FALSE;
    localVar[k].windex=-1;
    localVar[k].suitLengths=0;

    localVar[k].nodeSetSize=0; /* Index with range 0 to nodeSetSizeLimit */
    localVar[k].winSetSize=0;  /* Index with range 0 to winSetSizeLimit */
    localVar[k].lenSetSize=0;  /* Index with range 0 to lenSetSizeLimit */

    localVar[k].nodeSetSizeLimit=NINIT;
    localVar[k].winSetSizeLimit=WINIT;
    localVar[k].lenSetSizeLimit=LINIT;

    localVar[k].maxmem=5000001*sizeof(struct nodeCardsType)+
		   15000001*sizeof(struct winCardType)+
		   200001*sizeof(struct posSearchType);

#ifdef _WIN32

    localVar[k].maxmem = (DDS_LONGLONG)(pcmem-32678) * (700/MAXNOOFTHREADS);  
	/* Linear calculation of maximum memory, formula by Michiel de Bondt */

    if (localVar[k].maxmem < 10485760) exit (1);
  
  /*if (pcmem > 450000) {
	maxmem=5000000*sizeof(struct nodeCardsType)+
		   15000000*sizeof(struct winCardType)+
		   200000*sizeof(struct posSearchType);
  }
  else if (pcmem > 240000) {
	maxmem=3200000*sizeof(struct nodeCardsType)+
		   6400000*sizeof(struct winCardType)+
		   200000*sizeof(struct posSearchType);
  }
  else if (pcmem > 100000) {
	maxmem=800000*sizeof(struct nodeCardsType)+
		   2000000*sizeof(struct winCardType)+
		   100000*sizeof(struct posSearchType);
  }
  else {
	maxmem=400000*sizeof(struct nodeCardsType)+
		   1000000*sizeof(struct winCardType)+
		   50000*sizeof(struct posSearchType);
  }*/

  /*fp=fopen("mem.txt", "w");

  fprintf (fp, "The MEMORYSTATUS structure is %ld bytes long; it should be %d.\n\n", 
	    stat.dwLength, sizeof (stat));
  fprintf (fp, "There is  %ld percent of memory in use.\n",
          stat.dwMemoryLoad);
  fprintf (fp, "There are %*ld total Kbytes of physical memory.\n",
          7, stat.dwTotalPhys/1024);
  fprintf (fp, "There are %*ld free Kbytes of physical memory.\n",
          7, stat.dwAvailPhys/1024);
  fprintf (fp, "There are %*ld total Kbytes of paging file.\n",
          7, stat.dwTotalPageFile/1024);
  fprintf (fp, "There are %*ld free Kbytes of paging file.\n",
          7, stat.dwAvailPageFile/1024);
  fprintf (fp, "There are %*ld total Kbytes of virtual memory.\n",
          7, stat.dwTotalVirtual/1024);
  fprintf (fp, "There are %*ld free Kbytes of virtual memory.\n",
          7, stat.dwAvailVirtual/1024);
  fprintf(fp, "\n");
  fprintf(fp, "nsize=%d wsize=%d lsize=%d\n", nodeSetSizeLimit, winSetSizeLimit,
    lenSetSizeLimit);

  fclose(fp);*/

  #endif
  }

  bitMapRank[15]=0x2000;
  bitMapRank[14]=0x1000;
  bitMapRank[13]=0x0800;
  bitMapRank[12]=0x0400;
  bitMapRank[11]=0x0200;
  bitMapRank[10]=0x0100;
  bitMapRank[9]=0x0080;
  bitMapRank[8]=0x0040;
  bitMapRank[7]=0x0020;
  bitMapRank[6]=0x0010;
  bitMapRank[5]=0x0008;
  bitMapRank[4]=0x0004;
  bitMapRank[3]=0x0002;
  bitMapRank[2]=0x0001;
  bitMapRank[1]=0;
  bitMapRank[0]=0;

  lho[0]=1; lho[1]=2; lho[2]=3; lho[3]=0;
  rho[0]=3; rho[1]=0; rho[2]=1; rho[3]=2;
  partner[0]=2; partner[1]=3; partner[2]=0; partner[3]=1;

  cardRank[2]='2'; cardRank[3]='3'; cardRank[4]='4'; cardRank[5]='5';
  cardRank[6]='6'; cardRank[7]='7'; cardRank[8]='8'; cardRank[9]='9';
  cardRank[10]='T'; cardRank[11]='J'; cardRank[12]='Q'; cardRank[13]='K';
  cardRank[14]='A';

  cardSuit[0]='S'; cardSuit[1]='H'; cardSuit[2]='D'; cardSuit[3]='C';
  cardSuit[4]='N';

  cardHand[0]='N'; cardHand[1]='E'; cardHand[2]='S'; cardHand[3]='W';

  for (k=0; k<MAXNOOFTHREADS; k++) {
    localVar[k].summem=(WINIT+1)*sizeof(struct winCardType)+
	     (NINIT+1)*sizeof(struct nodeCardsType)+
		 (LINIT+1)*sizeof(struct posSearchType);
    localVar[k].wmem=(WSIZE+1)*sizeof(struct winCardType);
    localVar[k].nmem=(NSIZE+1)*sizeof(struct nodeCardsType);
    localVar[k].lmem=(LSIZE+1)*sizeof(struct posSearchType);
    localVar[k].maxIndex=(int)(
	  localVar[k].maxmem-localVar[k].summem)/((WSIZE+1) * sizeof(struct winCardType)); 

    localVar[k].pw = (struct winCardType **)calloc(localVar[k].maxIndex+1, sizeof(struct winCardType *));
    if (localVar[k].pw==NULL)
      exit(1);
    localVar[k].pn = (struct nodeCardsType **)calloc(localVar[k].maxIndex+1, sizeof(struct nodeCardsType *));
    if (localVar[k].pn==NULL)
      exit(1);
    localVar[k].pl = (struct posSearchType **)calloc(localVar[k].maxIndex+1, sizeof(struct posSearchType *));
    if (localVar[k].pl==NULL)
      exit(1);
    for (i=0; i<=localVar[k].maxIndex; i++) {
      if (localVar[k].pw[i])
	    free(localVar[k].pw[i]);
	    localVar[k].pw[i]=NULL;
    }
    for (i=0; i<=localVar[k].maxIndex; i++) {
	  if (localVar[k].pn[i])
	    free(localVar[k].pn[i]);
      localVar[k].pn[i]=NULL;
    }
    for (i=0; i<=localVar[k].maxIndex; i++) {
	  if (localVar[k].pl[i])
	    free(localVar[k].pl[i]);
	  localVar[k].pl[i]=NULL;
    }

    localVar[k].pw[0] = (struct winCardType *)calloc(localVar[k].winSetSizeLimit+1, sizeof(struct winCardType));
    if (localVar[k].pw[0]==NULL) 
      exit(1);
    localVar[k].allocmem=(localVar[k].winSetSizeLimit+1)*sizeof(struct winCardType);
    localVar[k].winCards=localVar[k].pw[0];
    localVar[k].pn[0] = (struct nodeCardsType *)calloc(localVar[k].nodeSetSizeLimit+1, sizeof(struct nodeCardsType));
    if (localVar[k].pn[0]==NULL)
      exit(1);
    localVar[k].allocmem+=(localVar[k].nodeSetSizeLimit+1)*sizeof(struct nodeCardsType);
    localVar[k].nodeCards=localVar[k].pn[0];
    localVar[k].pl[0] = 
	  (struct posSearchType *)calloc(localVar[k].lenSetSizeLimit+1, sizeof(struct posSearchType));
    if (localVar[k].pl[0]==NULL)
     exit(1);
    localVar[k].allocmem+=(localVar[k].lenSetSizeLimit+1)*sizeof(struct posSearchType);
    localVar[k].posSearch=localVar[k].pl[0];
    localVar[k].wcount=0; localVar[k].ncount=0; localVar[k].lcount=0;

    localVar[k].rel = (struct relRanksType *)calloc(8192, sizeof(struct relRanksType));
    if (localVar[k].rel==NULL)
      exit(1);

    localVar[k].adaptWins = (struct adaptWinRanksType *)calloc(8192, 
	  sizeof(struct adaptWinRanksType));
	if (localVar[k].adaptWins==NULL)
      exit(1);
  }

  highestRank = (int *)calloc(8192, sizeof(int));
  if (highestRank==NULL)
    exit(1);

  highestRank[0]=0;
  for (k=1; k<8192; k++) {
    for (r=14; r>=2; r--) {
      if ((k & bitMapRank[r])!=0) {
	highestRank[k]=r;
	  break;
      }
    }
  }

  /* The use of the counttable to give the number of bits set to
  one in an integer follows an implementation by Thomas Andrews. */

  counttable = (int *)calloc(8192, sizeof(int));
  if (counttable==NULL)
    exit(1);

  for (i=0; i<8192; i++) {	
    counttable[i]=0;
    for (j=0; j<13; j++) {
      if (i & (1<<j)) {counttable[i]++;}
    }
  }


  for (i=0; i<8192; i++)
    for (j=0; j<14; j++) {
      res=0;
      if (j==0) {
	for (m=0; m<MAXNOOFTHREADS; m++)
	  localVar[m].adaptWins[i].winRanks[j]=0;
      }
      else {
	k=1;
	for (r=14; r>=2; r--) {
	  if ((i & bitMapRank[r])!=0) {
	    if (k <= j) {
	      res|=bitMapRank[r];
	      k++;
	    }
	    else
	      break;
	  }
	}
	for (m=0; m<MAXNOOFTHREADS; m++)
	  localVar[m].adaptWins[i].winRanks[j]=res;
      }
    }
 
  /*localVar[thrId].fp2=fopen("dyn.txt", "w");
  fclose(localVar[thrId].fp2);*/
  /*localVar[thrId].fp2=fopen("dyn.txt", "a");
  fprintf(localVar[thrId].fp2, "maxIndex=%ld\n", maxIndex);
  fclose(localVar[thrId].fp2);*/

  return;
}


void InitGame(int gameNo, int moveTreeFlag, int first, int handRelFirst, int thrId) {

  int k, s, h, m;
  unsigned int topBitRank=1;
  unsigned short int ind;

  #ifdef STAT
    localVar[thrId].fp2=fopen("stat.txt","w");
  #endif

  #ifdef TTDEBUG
  if (!suppressTTlog) {
    localVar[thrId].fp7=fopen("storett.txt","w");
    localVar[thrId].fp11=fopen("rectt.txt", "w");
    fclose(localVar[thrId].fp11);
    ttCollect=TRUE;
  }
  #endif	
  

  if (localVar[thrId].newDeal) {

    /* Initialization of the rel structure is implemented
       according to a solution given by Thomas Andrews */ 

    for (k=0; k<=3; k++)
      for (m=0; m<=3; m++)
        localVar[thrId].iniPosition.rankInSuit[k][m]=localVar[thrId].game.suit[k][m];

    for (s=0; s<4; s++) {
      localVar[thrId].rel[0].aggrRanks[s]=0;
      localVar[thrId].rel[0].winMask[s]=0;
    }
  
    for (ind=1; ind<8192; ind++) {
      if (ind>=(topBitRank+topBitRank)) {
       /* Next top bit */
        topBitRank <<=1;
      }

      localVar[thrId].rel[ind]=localVar[thrId].rel[ind ^ topBitRank];

      for (s=0; s<4; s++) {
	for (h=0; h<4; h++) {
	  if (localVar[thrId].game.suit[h][s] & topBitRank) {
	    localVar[thrId].rel[ind].aggrRanks[s]=
	          (localVar[thrId].rel[ind].aggrRanks[s]>>2)|(h<<24);
	    localVar[thrId].rel[ind].winMask[s]=
	          (localVar[thrId].rel[ind].winMask[s]>>2)|(3<<24);
	    break;
	  }
	}
      }
    }
  }

  localVar[thrId].iniPosition.first[localVar[thrId].game.noOfCards-4]=first;
  localVar[thrId].iniPosition.handRelFirst=handRelFirst;
  localVar[thrId].lookAheadPos=localVar[thrId].iniPosition;
  
  localVar[thrId].estTricks[1]=6;
  localVar[thrId].estTricks[3]=6;
  localVar[thrId].estTricks[0]=7;
  localVar[thrId].estTricks[2]=7;

  #ifdef STAT
  fprintf(localVar[thrId].fp2, "Estimated tricks for hand to play:\n");	
  fprintf(localVar[thrId].fp2, "hand=%d  est tricks=%d\n", 
	  localVar[thrId].handToPlay, localVar[thrId].estTricks[localVar[thrId].handToPlay]);
  #endif

  InitSearch(&localVar[thrId].lookAheadPos, localVar[thrId].game.noOfCards-4, 
	localVar[thrId].initialMoves, first, moveTreeFlag, thrId);
  return;
}


void InitSearch(struct pos * posPoint, int depth, struct moveType startMoves[], 
	int first, int mtd, int thrId)  {

  int s, d, h, handRelFirst, maxAgg, maxHand=0;
  int k, noOfStartMoves;       /* Number of start moves in the 1st trick */
  int hand[3], suit[3], rank[3];
  struct moveType move;
  unsigned short int startMovesBitMap[4][4]; /* Indices are hand and suit */
  unsigned short int aggHand[4][4];

  for (h=0; h<=3; h++)
    for (s=0; s<=3; s++)
      startMovesBitMap[h][s]=0;

  handRelFirst=posPoint->handRelFirst;
  noOfStartMoves=handRelFirst;

  for (k=0; k<=2; k++) {
    hand[k]=handId(first, k);
    suit[k]=startMoves[k].suit;
    rank[k]=startMoves[k].rank;
    if (k<noOfStartMoves)
      startMovesBitMap[hand[k]][suit[k]]|=bitMapRank[rank[k]];
  }

  for (d=0; d<=49; d++) {
    /*bestMove[d].suit=0;*/
    localVar[thrId].bestMove[d].rank=0;
    localVar[thrId].bestMoveTT[d].rank=0;
    /*bestMove[d].weight=0;
    bestMove[d].sequence=0; 0315 */
  }

  if (((handId(first, handRelFirst))==0)||
    ((handId(first, handRelFirst))==2)) {
    localVar[thrId].nodeTypeStore[0]=MAXNODE;
    localVar[thrId].nodeTypeStore[1]=MINNODE;
    localVar[thrId].nodeTypeStore[2]=MAXNODE;
    localVar[thrId].nodeTypeStore[3]=MINNODE;
  }
  else {
    localVar[thrId].nodeTypeStore[0]=MINNODE;
    localVar[thrId].nodeTypeStore[1]=MAXNODE;
    localVar[thrId].nodeTypeStore[2]=MINNODE;
    localVar[thrId].nodeTypeStore[3]=MAXNODE;
  }

  k=noOfStartMoves;
  posPoint->first[depth]=first;
  posPoint->handRelFirst=k;
  assert((posPoint->handRelFirst>=0)&&(posPoint->handRelFirst<=3));
  posPoint->tricksMAX=0;

  if (k>0) {
    posPoint->move[depth+k]=startMoves[k-1];
    move=startMoves[k-1];
  }

  posPoint->high[depth+k]=first;

  while (k>0) {
    localVar[thrId].movePly[depth+k].current=0;
    localVar[thrId].movePly[depth+k].last=0;
    localVar[thrId].movePly[depth+k].move[0].suit=startMoves[k-1].suit;
    localVar[thrId].movePly[depth+k].move[0].rank=startMoves[k-1].rank;
    if (k<noOfStartMoves) {     /* If there is more than one start move */
      if (WinningMove(&startMoves[k-1], &move, thrId)) {
        posPoint->move[depth+k].suit=startMoves[k-1].suit;
        posPoint->move[depth+k].rank=startMoves[k-1].rank;
        posPoint->high[depth+k]=handId(first, noOfStartMoves-k);
        move=posPoint->move[depth+k];
      }
      else {
        posPoint->move[depth+k]=posPoint->move[depth+k+1];
        posPoint->high[depth+k]=posPoint->high[depth+k+1];
      }
    }
    k--;
  }

  for (s=0; s<=3; s++)
    posPoint->removedRanks[s]=0;

  for (s=0; s<=3; s++)       /* Suit */
    for (h=0; h<=3; h++)     /* Hand */
      posPoint->removedRanks[s]|=
        posPoint->rankInSuit[h][s];
  for (s=0; s<=3; s++)
    posPoint->removedRanks[s]=~(posPoint->removedRanks[s]);

  for (s=0; s<=3; s++)       /* Suit */
    for (h=0; h<=3; h++)     /* Hand */
      posPoint->removedRanks[s]&=
        (~startMovesBitMap[h][s]);
        
  for (s=0; s<=3; s++)
    localVar[thrId].iniRemovedRanks[s]=posPoint->removedRanks[s];

  /*for (d=0; d<=49; d++) {
    for (s=0; s<=3; s++)
      posPoint->winRanks[d][s]=0;
  }*/

  /* Initialize winning and second best ranks */
  for (s=0; s<=3; s++) {
    maxAgg=0;
    for (h=0; h<=3; h++) {
      aggHand[h][s]=startMovesBitMap[h][s] | localVar[thrId].game.suit[h][s];
      if (aggHand[h][s]>maxAgg) {
	    maxAgg=aggHand[h][s];
	    maxHand=h;
      }
    }
    if (maxAgg!=0) {
      posPoint->winner[s].hand=maxHand;
      k=highestRank[aggHand[maxHand][s]];
      posPoint->winner[s].rank=k;
     
      maxAgg=0;
      for (h=0; h<=3; h++) { 
	    aggHand[h][s]&=(~bitMapRank[k]);
        if (aggHand[h][s]>maxAgg) {
	      maxAgg=aggHand[h][s];
	      maxHand=h;
	    }
      }
      if (maxAgg>0) {
	    posPoint->secondBest[s].hand=maxHand;
	    posPoint->secondBest[s].rank=highestRank[aggHand[maxHand][s]];
      }
      else {
	    posPoint->secondBest[s].hand=-1;
        posPoint->secondBest[s].rank=0;
      }
    }
    else {
      posPoint->winner[s].hand=-1;
      posPoint->winner[s].rank=0;
      posPoint->secondBest[s].hand=-1;
      posPoint->secondBest[s].rank=0;
    }
  }


  for (s=0; s<=3; s++)
    for (h=0; h<=3; h++)
      posPoint->length[h][s]=
	(unsigned char)counttable[posPoint->rankInSuit[h][s]];

  #ifdef STAT
  for (d=0; d<=49; d++) {
    score1Counts[d]=0;
    score0Counts[d]=0;
    c1[d]=0;  c2[d]=0;  c3[d]=0;  c4[d]=0;  c5[d]=0;  c6[d]=0; c7[d]=0;
    c8[d]=0;
    localVar[thrId].no[d]=0;
  }
  #endif

  if (!mtd) {
    localVar[thrId].lenSetSize=0;  
    for (k=0; k<=13; k++) { 
      for (h=0; h<=3; h++) {
	localVar[thrId].rootnp[k][h]=&localVar[thrId].posSearch[localVar[thrId].lenSetSize];
	localVar[thrId].posSearch[localVar[thrId].lenSetSize].suitLengths=0;
	localVar[thrId].posSearch[localVar[thrId].lenSetSize].posSearchPoint=NULL;
	localVar[thrId].posSearch[localVar[thrId].lenSetSize].left=NULL;
	localVar[thrId].posSearch[localVar[thrId].lenSetSize].right=NULL;
	localVar[thrId].lenSetSize++;
      }
    }
    localVar[thrId].nodeSetSize=0;
    localVar[thrId].winSetSize=0;
  }
  
  #ifdef TTDEBUG
  if (!suppressTTlog) 
    lastTTstore=0;
  #endif

  return;
}

#ifdef STAT
int score1Counts[50], score0Counts[50];
int sumScore1Counts, sumScore0Counts, dd, suit, rank, order;
int c1[50], c2[50], c3[50], c4[50], c5[50], c6[50], c7[50], c8[50], c9[50];
int sumc1, sumc2, sumc3, sumc4, sumc5, sumc6, sumc7, sumc8, sumc9;
#endif

int ABsearch(struct pos * posPoint, int target, int depth, int thrId) {
    /* posPoint points to the current look-ahead position,
       target is number of tricks to take for the player,
       depth is the remaining search length, must be positive,
       the value of the subtree is returned.  */

  int moveExists, mexists, value, hand, scoreFlag, found;
  int ready, hfirst, hh, ss, rr, mcurrent, qtricks, tricks, res, k;
  unsigned short int makeWinRank[4];
  struct nodeCardsType * cardsP;
  struct evalType evalData;
  struct winCardType * np;
  struct posSearchType * pp;
  /*struct nodeCardsType * sopP;*/
  struct nodeCardsType  * tempP;
  unsigned short int aggr[4];
  unsigned short int ranks;

  struct evalType Evaluate(struct pos * posPoint, int thrId);
  void Make(struct pos * posPoint, unsigned short int trickCards[4], 
    int depth, int thrId);
  void Undo(struct pos * posPoint, int depth, int thrId);

  /*cardsP=NULL;*/
  assert((posPoint->handRelFirst>=0)&&(posPoint->handRelFirst<=3));
  hand=handId(posPoint->first[depth], posPoint->handRelFirst);
  localVar[thrId].nodes++;
  if (posPoint->handRelFirst==0) {
    localVar[thrId].trickNodes++;
    if (posPoint->tricksMAX>=target) {
      for (ss=0; ss<=3; ss++)
        posPoint->winRanks[depth][ss]=0;

        #ifdef STAT
        c1[depth]++;
        
        score1Counts[depth]++;
        if (depth==localVar[thrId].iniDepth) {
          fprintf(localVar[thrId].fp2, "score statistics:\n");
          for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
            fprintf(localVar[thrId].fp2, "d=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
            score1Counts[dd], score0Counts[dd], c1[dd], c2[dd],
              c3[dd], c4[dd]);
            fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd],
              c6[dd], c7[dd], c8[dd]);
          }
        }
        #endif
   
      return TRUE;
    }
    if (((posPoint->tricksMAX+(depth>>2)+1)<target)/*&&(depth>0)*/) {
      for (ss=0; ss<=3; ss++)
        posPoint->winRanks[depth][ss]=0;

 #ifdef STAT
        c2[depth]++;
        score0Counts[depth]++;
        if (depth==localVar[thrId].iniDepth) {
          fprintf(localVar[thrId].fp2, "score statistics:\n");
          for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
            fprintf(localVar[thrId].fp2, "d=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
            score1Counts[dd], score0Counts[dd], c1[dd], c2[dd],
              c3[dd], c4[dd]);
            fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd],
              c6[dd], c7[dd], c8[dd]);
          }
        }
 #endif

      return FALSE;
    }
    	
    if (localVar[thrId].nodeTypeStore[hand]==MAXNODE) {
      qtricks=QuickTricks(posPoint, hand, depth, target, &res, thrId);
      if (res) {
	if (qtricks==0)
	  return FALSE;
	else
          return TRUE;
 #ifdef STAT
          c3[depth]++;
          score1Counts[depth]++;
          if (depth==localVar[thrId].iniDepth) {
            fprintf(localVar[thrId].fp2, "score statistics:\n");
            for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
              fprintf(localVar[thrId].fp2, "d=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
              score1Counts[dd], score0Counts[dd], c1[dd], c2[dd],
                c3[dd], c4[dd]);
              fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd],
                c6[dd], c7[dd], c8[dd]);
            }
          }
 #endif
      }
      if (!LaterTricksMIN(posPoint,hand,depth,target, thrId))
	return FALSE;
    }
    else {
      qtricks=QuickTricks(posPoint, hand, depth, target, &res, thrId);
      if (res) {
        if (qtricks==0)
	  return TRUE;
	else
          return FALSE;
 #ifdef STAT
          c4[depth]++;
          score0Counts[depth]++;
          if (depth==localVar[thrId].iniDepth) {
            fprintf(localVar[thrId].fp2, "score statistics:\n");
            for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
              fprintf(localVar[thrId].fp2, "d=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
              score1Counts[dd], score0Counts[dd], c1[dd], c2[dd],
                c3[dd], c4[dd]);
              fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd],
                c6[dd], c7[dd], c8[dd]);
            }
          }
 #endif
      }
      if (LaterTricksMAX(posPoint,hand,depth,target,thrId))
	return TRUE;
    }
  }
  
  else if (posPoint->handRelFirst==1) {
    ss=posPoint->move[depth+1].suit;
    ranks=posPoint->rankInSuit[hand][ss] |
      posPoint->rankInSuit[partner[hand]][ss];
    found=FALSE; rr=0; qtricks=0; 
    if ( ranks >(bitMapRank[posPoint->move[depth+1].rank] |
	posPoint->rankInSuit[lho[hand]][ss])) {
	/* Own side has highest card in suit */
      if ((localVar[thrId].trump==4) || ((ss==localVar[thrId].trump)||
        (posPoint->rankInSuit[lho[hand]][localVar[thrId].trump]==0)
	   || (posPoint->rankInSuit[lho[hand]][ss]!=0))) { 
	    rr=highestRank[ranks];
	    if (rr!=0) {
	      found=TRUE;
	      qtricks=1;
	    }
	    else
	      found=FALSE;
      }
    }		  
    else if ((localVar[thrId].trump!=4) && (ss!=localVar[thrId].trump) && 
      (((posPoint->rankInSuit[hand][ss]==0)
	&& (posPoint->rankInSuit[hand][localVar[thrId].trump]!=0))|| 
	((posPoint->rankInSuit[partner[hand]][ss]==0)
	&& (posPoint->rankInSuit[partner[hand]][localVar[thrId].trump]!=0))))  {
	/* Own side can ruff */
      if ((posPoint->rankInSuit[lho[hand]][ss]!=0)||
         (posPoint->rankInSuit[lho[hand]][localVar[thrId].trump]==0)) {
	  found=TRUE;
        qtricks=1;
      }
    }		
    if (localVar[thrId].nodeTypeStore[hand]==MAXNODE) {
      if ((posPoint->tricksMAX+qtricks>=target)&&(found)&&
        (depth!=localVar[thrId].iniDepth)) {
         for (k=0; k<=3; k++)
	   posPoint->winRanks[depth][k]=0;
	 if (rr!=0)
	   posPoint->winRanks[depth][ss]=
	      posPoint->winRanks[depth][ss] | bitMapRank[rr];
         return TRUE;
      }
    }
    else {
      if (((posPoint->tricksMAX+((depth)>>2)+3-qtricks)<=target)&&
        (found)&&(depth!=localVar[thrId].iniDepth)) {
        for (k=0; k<=3; k++)
	    posPoint->winRanks[depth][k]=0;
        if (rr!=0)
	  posPoint->winRanks[depth][ss]=
	    posPoint->winRanks[depth][ss] | bitMapRank[rr];
        return FALSE;
      }
    }
  }
  
  if ((posPoint->handRelFirst==0)&&
    (depth!=localVar[thrId].iniDepth)) {  
    for (ss=0; ss<=3; ss++) {
      aggr[ss]=0;
      for (hh=0; hh<=3; hh++)
	aggr[ss]=aggr[ss] | posPoint->rankInSuit[hh][ss];
	  /* New algo */
      posPoint->orderSet[ss]=localVar[thrId].rel[aggr[ss]].aggrRanks[ss];
    }
    tricks=depth>>2;
    localVar[thrId].suitLengths=0; 
    for (ss=0; ss<=2; ss++)
      for (hh=0; hh<=3; hh++) {
	localVar[thrId].suitLengths=localVar[thrId].suitLengths<<4;
	localVar[thrId].suitLengths|=posPoint->length[hh][ss];
      }
    	
    pp=SearchLenAndInsert(localVar[thrId].rootnp[tricks][hand], 
		localVar[thrId].suitLengths, FALSE, &res, thrId);
	/* Find node that fits the suit lengths */
    if (pp!=NULL) {
      np=pp->posSearchPoint;
	  if (np==NULL)
        cardsP=NULL;
      else 
        cardsP=FindSOP(posPoint, np, hand, target, tricks, &scoreFlag, thrId);
      
      if (cardsP!=NULL) {
        if (scoreFlag==1) {
	  for (ss=0; ss<=3; ss++)
	    posPoint->winRanks[depth][ss]=
	      localVar[thrId].adaptWins[aggr[ss]].winRanks[(int)cardsP->leastWin[ss]];
		    
          if (cardsP->bestMoveRank!=0) {
            localVar[thrId].bestMoveTT[depth].suit=cardsP->bestMoveSuit;
            localVar[thrId].bestMoveTT[depth].rank=cardsP->bestMoveRank;
          }
 #ifdef STAT
          c5[depth]++;
          if (scoreFlag==1)
            score1Counts[depth]++;
          else
            score0Counts[depth]++;
          if (depth==localVar[thrId].iniDepth) {
            fprintf(localVar[thrId].fp2, "score statistics:\n");
            for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
              fprintf(localVar[thrId].fp2, "d=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
                score1Counts[dd], score0Counts[dd], c1[dd], c2[dd],c3[dd], c4[dd]);
              fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd],
                c6[dd], c7[dd], c8[dd]);
            }
          }
 #endif
 #ifdef TTDEBUG
          if (!suppressTTlog) { 
            if (lastTTstore<SEARCHSIZE) 
              ReceiveTTstore(posPoint, cardsP, target, depth, thrId);
            else 
              ttCollect=FALSE;
	  }
 #endif 
          return TRUE;
	}
        else {
	  for (ss=0; ss<=3; ss++)
	    posPoint->winRanks[depth][ss]=
	      localVar[thrId].adaptWins[aggr[ss]].winRanks[(int)cardsP->leastWin[ss]];

          if (cardsP->bestMoveRank!=0) {
            localVar[thrId].bestMoveTT[depth].suit=cardsP->bestMoveSuit;
            localVar[thrId].bestMoveTT[depth].rank=cardsP->bestMoveRank;
          }
 #ifdef STAT
          c6[depth]++;
          if (scoreFlag==1)
            score1Counts[depth]++;
          else
            score0Counts[depth]++;
          if (depth==localVar[thrId].iniDepth) {
            fprintf(localVar[thrId].fp2, "score statistics:\n");
            for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
              fprintf(localVar[thrId].fp2, "d=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
                score1Counts[dd], score0Counts[dd], c1[dd], c2[dd], c3[dd],
                  c4[dd]);
              fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd],
                  c6[dd], c7[dd], c8[dd]);
            }
          }
 #endif

 #ifdef TTDEBUG
          if (!suppressTTlog) {
            if (lastTTstore<SEARCHSIZE) 
              ReceiveTTstore(posPoint, cardsP, target, depth, thrId);
            else 
              ttCollect=FALSE;
          }
 #endif 
          return FALSE;
	}  
      }
    }
  }

  if (depth==0) {                    /* Maximum depth? */
    evalData=Evaluate(posPoint, thrId);        /* Leaf node */
    if (evalData.tricks>=target)
      value=TRUE;
    else
      value=FALSE;
    for (ss=0; ss<=3; ss++) {
      posPoint->winRanks[depth][ss]=evalData.winRanks[ss];

 #ifdef STAT
        c7[depth]++;
        if (value==1)
          score1Counts[depth]++;
        else
          score0Counts[depth]++;
        if (depth==localVar[thrId].iniDepth) {
          fprintf(localVar[thrId].fp2, "score statistics:\n");
          for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
            fprintf(localVar[thrId].fp2, "d=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
              score1Counts[dd], score0Counts[dd], c1[dd], c2[dd], c3[dd],
              c4[dd]);
            fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd],
              c6[dd], c7[dd], c8[dd]);
          }
        }
 #endif
    }
    return value;
  }  
  else {
    moveExists=MoveGen(posPoint, depth, thrId);

	/*#if 0*/
	if ((posPoint->handRelFirst==3)&&(depth>=/*29*/33/*37*/)
		&&(depth!=localVar[thrId].iniDepth)) {
	  localVar[thrId].movePly[depth].current=0;
	  mexists=TRUE;
	  ready=FALSE;
	  while (mexists) {
	    Make(posPoint, makeWinRank, depth, thrId);
	    depth--;

	    for (ss=0; ss<=3; ss++) {
	      aggr[ss]=0;
	      for (hh=0; hh<=3; hh++)
		aggr[ss]=aggr[ss] | posPoint->rankInSuit[hh][ss];
	      /* New algo */
	      posPoint->orderSet[ss]=localVar[thrId].rel[aggr[ss]].aggrRanks[ss];
	    }
	    tricks=depth>>2;
	    hfirst=posPoint->first[depth];
	    localVar[thrId].suitLengths=0;
	    for (ss=0; ss<=2; ss++)
	      for (hh=0; hh<=3; hh++) {
		 localVar[thrId].suitLengths=localVar[thrId].suitLengths<<4;
		 localVar[thrId].suitLengths|=posPoint->length[hh][ss];
	      }

	    pp=SearchLenAndInsert(localVar[thrId].rootnp[tricks][hfirst], 
			localVar[thrId].suitLengths, FALSE, &res, thrId);
		 /* Find node that fits the suit lengths */
	    if (pp!=NULL) {
	      np=pp->posSearchPoint;
	      if (np==NULL)
		tempP=NULL;
	      else
		tempP=FindSOP(posPoint, np, hfirst, target, tricks, &scoreFlag, thrId);

	      if (tempP!=NULL) {
		if ((localVar[thrId].nodeTypeStore[hand]==MAXNODE)&&(scoreFlag==1)) {
		  for (ss=0; ss<=3; ss++)
		    posPoint->winRanks[depth+1][ss]=
			  localVar[thrId].adaptWins[aggr[ss]].winRanks[(int)tempP->leastWin[ss]];
		  if (tempP->bestMoveRank!=0) {
		    localVar[thrId].bestMoveTT[depth+1].suit=tempP->bestMoveSuit;
		    localVar[thrId].bestMoveTT[depth+1].rank=tempP->bestMoveRank;
		  }
		  for (ss=0; ss<=3; ss++)
		    posPoint->winRanks[depth+1][ss]=posPoint->winRanks[depth+1][ss]
		      | makeWinRank[ss];
		  Undo(posPoint, depth+1, thrId);
		  return TRUE;
		}
		else if ((localVar[thrId].nodeTypeStore[hand]==MINNODE)&&(scoreFlag==0)) {
		  for (ss=0; ss<=3; ss++)
		    posPoint->winRanks[depth+1][ss]=
			  localVar[thrId].adaptWins[aggr[ss]].winRanks[(int)tempP->leastWin[ss]];
		  if (tempP->bestMoveRank!=0) {
		    localVar[thrId].bestMoveTT[depth+1].suit=tempP->bestMoveSuit;
		    localVar[thrId].bestMoveTT[depth+1].rank=tempP->bestMoveRank;
		  }
		  for (ss=0; ss<=3; ss++)
		    posPoint->winRanks[depth+1][ss]=posPoint->winRanks[depth+1][ss]
		      | makeWinRank[ss];
		  Undo(posPoint, depth+1, thrId);
		  return FALSE;
		}
		else {
		  localVar[thrId].movePly[depth+1].move[localVar[thrId].movePly[depth+1].current].weight+=100;
		  ready=TRUE;
		}
	      }
	    }
	    depth++;
	    Undo(posPoint, depth, thrId);
	    if (ready)
	      break;
	    if (localVar[thrId].movePly[depth].current<=localVar[thrId].movePly[depth].last-1) {
	      localVar[thrId].movePly[depth].current++;
	      mexists=TRUE;
	    }
	    else
	      mexists=FALSE;
	  }
	  if (ready)
	    InsertSort(localVar[thrId].movePly[depth].last+1, depth, thrId);
	}
	/*#endif*/

    localVar[thrId].movePly[depth].current=0;
    if (localVar[thrId].nodeTypeStore[hand]==MAXNODE) {
      value=FALSE;
      for (ss=0; ss<=3; ss++)
        posPoint->winRanks[depth][ss]=0;

      while (moveExists)  {
        Make(posPoint, makeWinRank, depth, thrId);        /* Make current move */

        assert((posPoint->handRelFirst>=0)&&(posPoint->handRelFirst<=3));
		value=ABsearch(posPoint, target, depth-1, thrId);
          
        Undo(posPoint, depth, thrId);      /* Retract current move */
        if (value==TRUE) {
        /* A cut-off? */
	    for (ss=0; ss<=3; ss++)
            posPoint->winRanks[depth][ss]=posPoint->winRanks[depth-1][ss] |
              makeWinRank[ss];
	    mcurrent=localVar[thrId].movePly[depth].current;
          localVar[thrId].bestMove[depth]=localVar[thrId].movePly[depth].move[mcurrent];
          goto ABexit;
        }  
        for (ss=0; ss<=3; ss++)
          posPoint->winRanks[depth][ss]=posPoint->winRanks[depth][ss] |
           posPoint->winRanks[depth-1][ss] | makeWinRank[ss];

        moveExists=NextMove(posPoint, depth, thrId);
      }
    }
    else {                          /* A minnode */
      value=TRUE;
      for (ss=0; ss<=3; ss++)
        posPoint->winRanks[depth][ss]=0;
        
      while (moveExists)  {
        Make(posPoint, makeWinRank, depth, thrId);        /* Make current move */
        
        value=ABsearch(posPoint, target, depth-1, thrId);
      
        Undo(posPoint, depth, thrId);       /* Retract current move */
        if (value==FALSE) {
        /* A cut-off? */
	    for (ss=0; ss<=3; ss++)
            posPoint->winRanks[depth][ss]=posPoint->winRanks[depth-1][ss] |
              makeWinRank[ss];
	    mcurrent=localVar[thrId].movePly[depth].current;
          localVar[thrId].bestMove[depth]=localVar[thrId].movePly[depth].move[mcurrent];
          goto ABexit;
        }
        for (ss=0; ss<=3; ss++)
          posPoint->winRanks[depth][ss]=posPoint->winRanks[depth][ss] |
           posPoint->winRanks[depth-1][ss] | makeWinRank[ss];

        moveExists=NextMove(posPoint, depth, thrId);
      }
    }
  }
  ABexit:
  if (depth>=4) {
    if(posPoint->handRelFirst==0) { 
      tricks=depth>>2;
      /*hand=posPoint->first[depth-1];*/
      if (value)
	k=target;
      else
	k=target-1;
      BuildSOP(posPoint, tricks, hand, target, depth,
        value, k, thrId);
      if (localVar[thrId].clearTTflag) {
         /* Wipe out the TT dynamically allocated structures
	    except for the initially allocated structures.
	    Set the TT limits to the initial values.
	    Reset TT array indices to zero.
	    Reset memory chunk indices to zero.
	    Set allocated memory to the initial value. */
        /*localVar[thrId].fp2=fopen("dyn.txt", "a");
	fprintf(localVar[thrId].fp2, "Clear TT:\n");
	fprintf(localVar[thrId].fp2, "wcount=%d, ncount=%d, lcount=%d\n", 
	       wcount, ncount, lcount);
        fprintf(localVar[thrId].fp2, "winSetSize=%d, nodeSetSize=%d, lenSetSize=%d\n", 
	       winSetSize, nodeSetSize, lenSetSize);
	fprintf(localVar[thrId].fp2, "\n");
        fclose(localVar[thrId].fp2);*/

        Wipe(thrId);
	localVar[thrId].winSetSizeLimit=WINIT;
	localVar[thrId].nodeSetSizeLimit=NINIT;
	localVar[thrId].lenSetSizeLimit=LINIT;
	localVar[thrId].lcount=0;  
	localVar[thrId].allocmem=(localVar[thrId].lenSetSizeLimit+1)*sizeof(struct posSearchType);
	localVar[thrId].lenSetSize=0;
	localVar[thrId].posSearch=localVar[thrId].pl[localVar[thrId].lcount];  
	for (k=0; k<=13; k++) { 
	  for (hh=0; hh<=3; hh++) {
	    localVar[thrId].rootnp[k][hh]=&localVar[thrId].posSearch[localVar[thrId].lenSetSize];
	    localVar[thrId].posSearch[localVar[thrId].lenSetSize].suitLengths=0;
	    localVar[thrId].posSearch[localVar[thrId].lenSetSize].posSearchPoint=NULL;
	    localVar[thrId].posSearch[localVar[thrId].lenSetSize].left=NULL;
	    localVar[thrId].posSearch[localVar[thrId].lenSetSize].right=NULL;
	    localVar[thrId].lenSetSize++;
	  }
	}
        localVar[thrId].nodeSetSize=0;
        localVar[thrId].winSetSize=0;
	localVar[thrId].wcount=0; localVar[thrId].ncount=0; 
	localVar[thrId].allocmem+=(localVar[thrId].winSetSizeLimit+1)*sizeof(struct winCardType);
        localVar[thrId].winCards=localVar[thrId].pw[localVar[thrId].wcount];
	localVar[thrId].allocmem+=(localVar[thrId].nodeSetSizeLimit+1)*sizeof(struct nodeCardsType);
	localVar[thrId].nodeCards=localVar[thrId].pn[localVar[thrId].ncount];
	localVar[thrId].clearTTflag=FALSE;
	localVar[thrId].windex=-1;
      }
    } 
  }

 #ifdef STAT
  c8[depth]++;
  if (value==1)
    score1Counts[depth]++;
  else
    score0Counts[depth]++;
  if (depth==localVar[thrId].iniDepth) {
    if (localVar[thrId].fp2==NULL)
      exit(0);		  
    fprintf(localVar[thrId].fp2, "\n");
    fprintf(localVar[thrId].fp2, "top level cards:\n");
    for (hh=0; hh<=3; hh++) {
      fprintf(localVar[thrId].fp2, "hand=%c\n", cardHand[hh]);
      for (ss=0; ss<=3; ss++) {
        fprintf(localVar[thrId].fp2, "suit=%c", cardSuit[ss]);
        for (rr=14; rr>=2; rr--)
          if (posPoint->rankInSuit[hh][ss] & bitMapRank[rr])
            fprintf(localVar[thrId].fp2, " %c", cardRank[rr]);
        fprintf(localVar[thrId].fp2, "\n");
      }
      fprintf(localVar[thrId].fp2, "\n");
    }
    fprintf(localVar[thrId].fp2, "top level winning cards:\n");
    for (ss=0; ss<=3; ss++) {
      fprintf(localVar[thrId].fp2, "suit=%c", cardSuit[ss]);
      for (rr=14; rr>=2; rr--)
        if (posPoint->winRanks[depth][ss] & bitMapRank[rr])
          fprintf(localVar[thrId].fp2, " %c", cardRank[rr]);
      fprintf(localVar[thrId].fp2, "\n");
    }
    fprintf(localVar[thrId].fp2, "\n");
    fprintf(localVar[thrId].fp2, "\n");

    fprintf(localVar[thrId].fp2, "score statistics:\n");
    sumScore0Counts=0;
    sumScore1Counts=0;
    sumc1=0; sumc2=0; sumc3=0; sumc4=0;
    sumc5=0; sumc6=0; sumc7=0; sumc8=0; sumc9=0;
    for (dd=localVar[thrId].iniDepth; dd>=0; dd--) {
      fprintf(localVar[thrId].fp2, "depth=%d s1=%d s0=%d c1=%d c2=%d c3=%d c4=%d", dd,
          score1Counts[dd], score0Counts[dd], c1[dd], c2[dd], c3[dd], c4[dd]);
      fprintf(localVar[thrId].fp2, " c5=%d c6=%d c7=%d c8=%d\n", c5[dd], c6[dd],
          c7[dd], c8[dd]);
      sumScore0Counts=sumScore0Counts+score0Counts[dd];
      sumScore1Counts=sumScore1Counts+score1Counts[dd];
      sumc1=sumc1+c1[dd];
      sumc2=sumc2+c2[dd];
      sumc3=sumc3+c3[dd];
      sumc4=sumc4+c4[dd];
      sumc5=sumc5+c5[dd];
      sumc6=sumc6+c6[dd];
      sumc7=sumc7+c7[dd];
      sumc8=sumc8+c8[dd];
      sumc9=sumc9+c9[dd];
    } 
    fprintf(localVar[thrId].fp2, "\n");
    fprintf(localVar[thrId].fp2, "score sum statistics:\n");
    fprintf(localVar[thrId].fp2, "\n");
    fprintf(localVar[thrId].fp2, "sumScore0Counts=%d sumScore1Counts=%d\n",
        sumScore0Counts, sumScore1Counts);
    fprintf(localVar[thrId].fp2, "nodeSetSize=%d  winSetSize=%d\n", localVar[thrId].nodeSetSize,
        localVar[thrId].winSetSize);
    fprintf(localVar[thrId].fp2, "sumc1=%d sumc2=%d sumc3=%d sumc4=%d\n",
        sumc1, sumc2, sumc3, sumc4);
    fprintf(localVar[thrId].fp2, "sumc5=%d sumc6=%d sumc7=%d sumc8=%d sumc9=%d\n",
        sumc5, sumc6, sumc7, sumc8, sumc9);
    fprintf(localVar[thrId].fp2, "\n");	
    fprintf(localVar[thrId].fp2, "\n");
    fprintf(localVar[thrId].fp2, "No of searched nodes per depth:\n");
    for (dd=localVar[thrId].iniDepth; dd>=0; dd--)
      fprintf(localVar[thrId].fp2, "depth=%d  nodes=%d\n", dd, localVar[thrId].no[dd]);
    fprintf(localVar[thrId].fp2, "\n");
    fprintf(localVar[thrId].fp2, "Total nodes=%d\n", localVar[thrId].nodes);
  }
 #endif
    
  return value;
}


void Make(struct pos * posPoint, unsigned short int trickCards[4], 
  int depth, int thrId)  {
  int r, s, t, u, w, firstHand;
  int suit, count, mcurr, h, q, done;
  struct moveType mo1, mo2;

  assert((posPoint->handRelFirst>=0)&&(posPoint->handRelFirst<=3));
  for (suit=0; suit<=3; suit++)
    trickCards[suit]=0;

  firstHand=posPoint->first[depth];
  r=localVar[thrId].movePly[depth].current;

  if (posPoint->handRelFirst==3)  {         /* This hand is last hand */
    mo1=localVar[thrId].movePly[depth].move[r];
    mo2=posPoint->move[depth+1];
    if (mo1.suit==mo2.suit) {
      if (mo1.rank>mo2.rank) {
	posPoint->move[depth]=mo1;
        posPoint->high[depth]=handId(firstHand, 3);
      }
      else {
        posPoint->move[depth]=posPoint->move[depth+1];
        posPoint->high[depth]=posPoint->high[depth+1];
      }
    }
    else if ((localVar[thrId].trump!=4) && (mo1.suit==localVar[thrId].trump)) {
      posPoint->move[depth]=mo1;
      posPoint->high[depth]=handId(firstHand, 3);
    }  
    else {
      posPoint->move[depth]=posPoint->move[depth+1];
      posPoint->high[depth]=posPoint->high[depth+1];
    }

    /* Is the trick won by rank? */
    suit=posPoint->move[depth].suit;
    count=0;
    for (h=0; h<=3; h++) {
      mcurr=localVar[thrId].movePly[depth+h].current;
      if (localVar[thrId].movePly[depth+h].move[mcurr].suit==suit)
          count++;
    }

    if (localVar[thrId].nodeTypeStore[posPoint->high[depth]]==MAXNODE)
      posPoint->tricksMAX++;
    posPoint->first[depth-1]=posPoint->high[depth];   /* Defines who is first
        in the next move */

    t=handId(firstHand, 3);
    posPoint->handRelFirst=0;      /* Hand pointed to by posPoint->first
                                    will lead the next trick */

    done=FALSE;
    for (s=3; s>=0; s--) {
      q=handId(firstHand, 3-s);
    /* Add the moves to removed ranks */
      r=localVar[thrId].movePly[depth+s].current;
      w=localVar[thrId].movePly[depth+s].move[r].rank;
      u=localVar[thrId].movePly[depth+s].move[r].suit;
      posPoint->removedRanks[u]|=bitMapRank[w];

      if (s==0)
        posPoint->rankInSuit[t][u]&=(~bitMapRank[w]);

      if (w==posPoint->winner[u].rank) 
        UpdateWinner(posPoint, u);
      else if (w==posPoint->secondBest[u].rank) 
        UpdateSecondBest(posPoint, u);

    /* Determine win-ranked cards */
      if ((q==posPoint->high[depth])&&(!done)) {
        done=TRUE;
        if (count>=2) {
          trickCards[u]=bitMapRank[w];
          /* Mark ranks as winning if they are part of a sequence */
          trickCards[u]|=localVar[thrId].movePly[depth+s].move[r].sequence; 
        }
      }
    }
  }
  else if (posPoint->handRelFirst==0) {   /* Is it the 1st hand? */
    posPoint->first[depth-1]=firstHand;   /* First hand is not changed in
                                            next move */
    posPoint->high[depth]=firstHand;
    posPoint->move[depth]=localVar[thrId].movePly[depth].move[r];
    t=firstHand;
    posPoint->handRelFirst=1;
    r=localVar[thrId].movePly[depth].current;
    u=localVar[thrId].movePly[depth].move[r].suit;
    w=localVar[thrId].movePly[depth].move[r].rank;
    posPoint->rankInSuit[t][u]&=(~bitMapRank[w]);
  }
  else {
    mo1=localVar[thrId].movePly[depth].move[r];
    mo2=posPoint->move[depth+1];
    r=localVar[thrId].movePly[depth].current;
    u=localVar[thrId].movePly[depth].move[r].suit;
    w=localVar[thrId].movePly[depth].move[r].rank;
    if (mo1.suit==mo2.suit) {
      if (mo1.rank>mo2.rank) {
	posPoint->move[depth]=mo1;
        posPoint->high[depth]=handId(firstHand, posPoint->handRelFirst);
      }
      else {
	posPoint->move[depth]=posPoint->move[depth+1];
        posPoint->high[depth]=posPoint->high[depth+1];
      }
    }
    else if ((localVar[thrId].trump!=4) && (mo1.suit==localVar[thrId].trump)) {
      posPoint->move[depth]=mo1;
      posPoint->high[depth]=handId(firstHand, posPoint->handRelFirst);
    }  
    else {
      posPoint->move[depth]=posPoint->move[depth+1];
      posPoint->high[depth]=posPoint->high[depth+1];
    }
    
    t=handId(firstHand, posPoint->handRelFirst);
    posPoint->handRelFirst++;               /* Current hand is stepped */
    assert((posPoint->handRelFirst>=0)&&(posPoint->handRelFirst<=3));
    posPoint->first[depth-1]=firstHand;     /* First hand is not changed in
                                            next move */
    
    posPoint->rankInSuit[t][u]&=(~bitMapRank[w]);
  }

  posPoint->length[t][u]--;

#ifdef STAT
  localVar[thrId].no[depth]++;
#endif
    
  return;
}


void Undo(struct pos * posPoint, int depth, int thrId)  {
  int r, s, t, u, w, firstHand;

  assert((posPoint->handRelFirst>=0)&&(posPoint->handRelFirst<=3));

  firstHand=posPoint->first[depth];

  switch (posPoint->handRelFirst) {
    case 3: case 2: case 1:
     posPoint->handRelFirst--;
     break;
    case 0:
     posPoint->handRelFirst=3;
  }

  if (posPoint->handRelFirst==0) {          /* 1st hand which won the previous
                                            trick */
    t=firstHand;
    r=localVar[thrId].movePly[depth].current;
    u=localVar[thrId].movePly[depth].move[r].suit;
    w=localVar[thrId].movePly[depth].move[r].rank;
  }
  else if (posPoint->handRelFirst==3)  {    /* Last hand */
    for (s=3; s>=0; s--) {
    /* Delete the moves from removed ranks */
      r=localVar[thrId].movePly[depth+s].current;
      w=localVar[thrId].movePly[depth+s].move[r].rank;
      u=localVar[thrId].movePly[depth+s].move[r].suit;

	  posPoint->removedRanks[u]&= (~bitMapRank[w]);		

      if (w>posPoint->winner[u].rank) {
        posPoint->secondBest[u].rank=posPoint->winner[u].rank;
        posPoint->secondBest[u].hand=posPoint->winner[u].hand;
        posPoint->winner[u].rank=w;
        posPoint->winner[u].hand=handId(firstHand, 3-s);
		
      }
      else if (w>posPoint->secondBest[u].rank) {
        posPoint->secondBest[u].rank=w;
        posPoint->secondBest[u].hand=handId(firstHand, 3-s);
      }
    }
    t=handId(firstHand, 3);

        
    if (localVar[thrId].nodeTypeStore[posPoint->first[depth-1]]==MAXNODE)   /* First hand
                                            of next trick is winner of the
                                            current trick */
      posPoint->tricksMAX--;
  }
  else {
    t=handId(firstHand, posPoint->handRelFirst);
    r=localVar[thrId].movePly[depth].current;
    u=localVar[thrId].movePly[depth].move[r].suit;
    w=localVar[thrId].movePly[depth].move[r].rank;
  }    

  posPoint->rankInSuit[t][u]|=bitMapRank[w];

  posPoint->length[t][u]++;

  return;
}



struct evalType Evaluate(struct pos * posPoint, int thrId)  {
  int s, smax=0, max, /*i, j, r, mcurr,*/ k, firstHand, count;
  struct evalType eval;

  firstHand=posPoint->first[0];
  assert((firstHand >= 0)&&(firstHand <= 3));

  for (s=0; s<=3; s++)
    eval.winRanks[s]=0;

  /* Who wins the last trick? */
  if (localVar[thrId].trump!=4)  {            /* Highest trump card wins */
    max=0;
    count=0;
    for (s=0; s<=3; s++) {
      if (posPoint->rankInSuit[s][localVar[thrId].trump]!=0)
        count++;
      if (posPoint->rankInSuit[s][localVar[thrId].trump]>max) {
        smax=s;
        max=posPoint->rankInSuit[s][localVar[thrId].trump];
      }
    }

    if (max>0) {        /* Trumpcard wins */
      if (count>=2)
        eval.winRanks[localVar[thrId].trump]=max;

      if (localVar[thrId].nodeTypeStore[smax]==MAXNODE)
        goto maxexit;
      else
        goto minexit;
    }
  }

  /* Who has the highest card in the suit played by 1st hand? */

  k=0;
  while (k<=3)  {           /* Find the card the 1st hand played */
    if (posPoint->rankInSuit[firstHand][k]!=0)      /* Is this the card? */
      break;
    k++;
  } 

  assert(k < 4);

  count=0;
  max=0; 
  for (s=0; s<=3; s++)  {
    if (posPoint->rankInSuit[s][k]!=0)
        count++;
    if (posPoint->rankInSuit[s][k]>max)  {
      smax=s;
      max=posPoint->rankInSuit[s][k];
    }
  }

  if (count>=2)
    eval.winRanks[k]=max;

  if (localVar[thrId].nodeTypeStore[smax]==MAXNODE)
    goto maxexit;
  else
    goto minexit;

  maxexit:
  eval.tricks=posPoint->tricksMAX+1;
  return eval;

  minexit:
  eval.tricks=posPoint->tricksMAX;
  return eval;
}



void UpdateWinner(struct pos * posPoint, int suit) {
  int k, h, hmax=0;
  unsigned short int sb, sbmax;

  posPoint->winner[suit]=posPoint->secondBest[suit];
  if (posPoint->winner[suit].hand==-1)
    return;

  sbmax=0;
  for (h=0; h<=3; h++) {
    sb=posPoint->rankInSuit[h][suit] & (~bitMapRank[posPoint->winner[suit].rank]);
    if (sb>sbmax) { 
      hmax=h;
      sbmax=sb;
    }
  }
  k=highestRank[sbmax];
  if (k!=0) {
    posPoint->secondBest[suit].hand=hmax;
    posPoint->secondBest[suit].rank=k;
  }
  else {
    posPoint->secondBest[suit].hand=-1;
    posPoint->secondBest[suit].rank=0;
  }

  return;
}


void UpdateSecondBest(struct pos * posPoint, int suit) {
  int k, h, hmax=0;
  unsigned short int sb, sbmax;

  sbmax=0;
  for (h=0; h<=3; h++) {
    sb=posPoint->rankInSuit[h][suit] & (~bitMapRank[posPoint->winner[suit].rank]);
    if (sb>sbmax) { 
      hmax=h;
      sbmax=sb;
    }
  }
  k=highestRank[sbmax];
  if (k!=0) {
    posPoint->secondBest[suit].hand=hmax;
    posPoint->secondBest[suit].rank=k;
  }
  else {
    posPoint->secondBest[suit].hand=-1;
    posPoint->secondBest[suit].rank=0;
  }  
    
  return;
}


int QuickTricks(struct pos * posPoint, int hand, 
	int depth, int target, int *result, int thrId) {
  unsigned short int ranks;
  int suit, sum, qtricks, commPartner, commRank=0, commSuit=-1, s, found=FALSE;
  int opps;
  int countLho, countRho, countPart, countOwn, lhoTrumpRanks=0, rhoTrumpRanks=0;
  int cutoff, k, hh, ss, rr, lowestQtricks=0, count=0;
  int trump;

  trump=localVar[thrId].trump;
  
  *result=TRUE;
  qtricks=0;
  for (s=0; s<=3; s++)   
    posPoint->winRanks[depth][s]=0;

  if ((depth<=0)||(depth==localVar[thrId].iniDepth)) {
    *result=FALSE;
    return qtricks;
  }

  if (localVar[thrId].nodeTypeStore[hand]==MAXNODE) 
    cutoff=target-posPoint->tricksMAX;
  else
    cutoff=posPoint->tricksMAX-target+(depth>>2)+2;
      
  commPartner=FALSE;
  for (s=0; s<=3; s++) {
    if ((trump!=4)&&(trump!=s)) {
      if (posPoint->winner[s].hand==partner[hand]) {
        /* Partner has winning card */
        if (posPoint->rankInSuit[hand][s]!=0) {
        /* Own hand has card in suit */
          if (((posPoint->rankInSuit[lho[hand]][s]!=0) ||
          /* LHO not void */
          (posPoint->rankInSuit[lho[hand]][trump]==0))
          /* LHO has no trump */
          && ((posPoint->rankInSuit[rho[hand]][s]!=0) ||
          /* RHO not void */
          (posPoint->rankInSuit[rho[hand]][trump]==0))) {
          /* RHO has no trump */
            commPartner=TRUE;
            commSuit=s;
            commRank=posPoint->winner[s].rank;
            break;
          }  
        }
      }
      else if (posPoint->secondBest[s].hand==partner[hand]) {
        if ((posPoint->winner[s].hand==hand)&&
	  (posPoint->length[hand][s]>=2)&&(posPoint->length[partner[hand]][s]>=2)) {
	  if (((posPoint->rankInSuit[lho[hand]][s]!=0) ||
            (posPoint->rankInSuit[lho[hand]][trump]==0))
            && ((posPoint->rankInSuit[rho[hand]][s]!=0) ||
            (posPoint->rankInSuit[rho[hand]][trump]==0))) {
	    commPartner=TRUE;
            commSuit=s;
            commRank=posPoint->secondBest[s].rank;
            break;
	  }
	}
      }
    }
    else if (trump==4) {
      if (posPoint->winner[s].hand==partner[hand]) {
        /* Partner has winning card */
        if (posPoint->rankInSuit[hand][s]!=0) {
        /* Own hand has card in suit */
          commPartner=TRUE;
          commSuit=s;
          commRank=posPoint->winner[s].rank;
          break;
        }
      }
      else if (posPoint->secondBest[s].hand==partner[hand]) { 
        if ((posPoint->winner[s].hand==hand)&&
	  (posPoint->length[hand][s]>=2)&&(posPoint->length[partner[hand]][s]>=2)) {
	  commPartner=TRUE;
          commSuit=s;
          commRank=posPoint->secondBest[s].rank;
          break;
	}
      }
    }
  }

  if ((trump!=4) && (!commPartner) && 
	  (posPoint->rankInSuit[hand][trump]!=0) && 
	  (posPoint->winner[trump].hand==partner[hand])) {
    commPartner=TRUE;
    commSuit=trump;
    commRank=posPoint->winner[trump].rank;
  }


  if (trump!=4) {
    suit=trump;
    lhoTrumpRanks=posPoint->length[lho[hand]][trump];
    rhoTrumpRanks=posPoint->length[rho[hand]][trump];
  }
  else
    suit=0;   

  do {
    countOwn=posPoint->length[hand][suit];
    countLho=posPoint->length[lho[hand]][suit];
    countRho=posPoint->length[rho[hand]][suit];
    countPart=posPoint->length[partner[hand]][suit];
    opps=countLho | countRho;

    if (!opps && (countPart==0)) {
      if (countOwn==0) {
	if ((trump!=4) && (suit==trump)) {
          if (trump==0)
            suit=1;
          else
            suit=0;
        }
        else {
          suit++;
          if ((trump!=4) && (suit==trump))
            suit++;
        }
	continue;
      }
      if ((trump!=4) && (trump!=suit)) {
        if ((lhoTrumpRanks==0) &&
          /* LHO has no trump */
          (rhoTrumpRanks==0)) {
          /* RHO has no trump */
          qtricks=qtricks+countOwn;
		  if (qtricks>=cutoff) 
            return qtricks;
          suit++;
          if ((trump!=4) && (suit==trump))
            suit++;
		  continue;
        }
        else {
          suit++;
          if ((trump!=4) && (suit==trump))
            suit++;
	  continue;
        }
      }
      else {
        qtricks=qtricks+countOwn;
	if (qtricks>=cutoff) 
          return qtricks;
        
        if ((trump!=4) && (suit==trump)) {
          if (trump==0)
            suit=1;
          else
            suit=0;
        }
        else {
          suit++;
          if ((trump!=4) && (suit==trump))
            suit++;
        }
	continue;
      }
    }
    else {
      if (!opps && (trump!=4) && (suit==trump)) {
        sum=Max(countOwn, countPart);
	for (s=0; s<=3; s++) {
	  if ((sum>0)&&(s!=trump)&&(countOwn>=countPart)&&(posPoint->length[hand][s]>0)&&
	    (posPoint->length[partner[hand]][s]==0)) {
	    sum++;
	    break;
	  }
	}
	if (sum>=cutoff) 
	  return sum;
      }
      else if (!opps) {
	sum=Min(countOwn,countPart);
	if (trump==4) {
	  if (sum>=cutoff) 
	    return sum;
	}
	else if ((suit!=trump)&&(lhoTrumpRanks==0)&&(rhoTrumpRanks==0)) {
	  if (sum>=cutoff) 
	    return sum;
	}
      }

      if (commPartner) {
	if (!opps && (countOwn==0)) {
          if ((trump!=4) && (trump!=suit)) {
            if ((lhoTrumpRanks==0) &&
            /* LHO has no trump */
              (rhoTrumpRanks==0)) {
            /* RHO has no trump */
              qtricks=qtricks+countPart;
	      posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                 bitMapRank[commRank];
	      if (qtricks>=cutoff) 
                return qtricks;
              suit++;
              if ((trump!=4) && (suit==trump))
                suit++;
	      continue;
            }
            else {
              suit++;
              if ((trump!=4) && (suit==trump))
                suit++;
	      continue;
            }
          }
          else {
            qtricks=qtricks+countPart;
            posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                bitMapRank[commRank];
	    if (qtricks>=cutoff) 
              return qtricks;
            if ((trump!=4) && (suit==trump)) {
              if (trump==0)
                suit=1;
              else
                suit=0;
            }
            else {
              suit++;
              if ((trump!=4) && (suit==trump))
                suit++;
            }
	    continue;
          }
        }
	else {
	  if (!opps && (trump!=4) && (suit==trump)) {
	    sum=Max(countOwn, countPart);
	    for (s=0; s<=3; s++) {
	      if ((sum>0)&&(s!=trump)&&(countOwn<=countPart)&&(posPoint->length[partner[hand]][s]>0)&&
		(posPoint->length[hand][s]==0)) {
		sum++;
		break;
	      }
	    }
            if (sum>=cutoff) {
	      posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                bitMapRank[commRank];
	      return sum;
	    }
	  }
	  else if (!opps) {
	    sum=Min(countOwn,countPart);
	    if (trump==4) {
	      if (sum>=cutoff) 
		return sum;
	    }
	    else if ((suit!=trump)&&(lhoTrumpRanks==0)&&(rhoTrumpRanks==0)) {
	      if (sum>=cutoff) 
		return sum;
	    }
	  }
	} 
      }
	}
    /* 08-01-30 */
    if (posPoint->winner[suit].rank==0) {
	  if ((trump!=4) && (suit==trump)) {
        if (trump==0)
          suit=1;
        else
          suit=0;
      }
      else {
        suit++;
        if ((trump!=4) && (suit==trump))
          suit++;
      }
      continue;
    }

    if (posPoint->winner[suit].hand==hand) {
      /* Winner found in own hand */
      if ((trump!=4)&&(trump!=suit)) {
        if (((countLho!=0) ||
          /* LHO not void */
          (lhoTrumpRanks==0))
          /* LHO has no trump */
          && ((countRho!=0) ||
          /* RHO not void */
	  (rhoTrumpRanks==0))) {
          /* RHO has no trump */
          posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
            | bitMapRank[posPoint->winner[suit].rank];
          qtricks++;   /* A trick can be taken */
          /* 06-12-14 */
	  if (qtricks>=cutoff) 
            return qtricks;

          if ((countLho<=1)&&(countRho<=1)&&(countPart<=1)&&
            (lhoTrumpRanks==0)&&(rhoTrumpRanks==0)) {
            qtricks=qtricks+countOwn-1;
	    if (qtricks>=cutoff) 
              return qtricks;
            suit++;
            if ((trump!=4) && (suit==trump))
              suit++;
	    continue;
          }
        }

        if (posPoint->secondBest[suit].hand==hand) {
          /* Second best found in own hand */
          if ((lhoTrumpRanks==0)&&
             (rhoTrumpRanks==0)) {
            /* Opponents have no trump */
            posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
              | bitMapRank[posPoint->secondBest[suit].rank];
            qtricks++;
            if ((countLho<=2)&&(countRho<=2)&&(countPart<=2)) {
              qtricks=qtricks+countOwn-2;
	      if (qtricks>=cutoff) 
                return qtricks;
              suit++;
              if ((trump!=4) && (suit==trump))
                suit++;
	      continue;
            }
          }
        }
        /* 06-08-19 */
        else if ((posPoint->secondBest[suit].hand==partner[hand])
          &&(countOwn>1)&&(countPart>1)) {
	    /* Second best at partner and suit length of own
	      hand and partner > 1 */
          if ((lhoTrumpRanks==0)&&
             (rhoTrumpRanks==0)) {
          /* Opponents have no trump */
            posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
                | bitMapRank[posPoint->secondBest[suit].rank];
            qtricks++;
            if ((countLho<=2)&&(countRho<=2)&&((countPart<=2)||(countOwn<=2))) { 
				/* 07-06-10 */
              qtricks=qtricks+Max(countOwn-2, countPart-2);
	      if (qtricks>=cutoff) 
                return qtricks;
              suit++;
              if ((trump!=4) && (suit==trump))
                suit++;
	      continue;
            }
          }
        }
      }
      else {
        posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
           | bitMapRank[posPoint->winner[suit].rank];
        qtricks++;
        /* 06-12-14 */
	if (qtricks>=cutoff) 
          return qtricks;

        if ((countLho<=1)&&(countRho<=1)&&(countPart<=1)) {
          qtricks=qtricks+countOwn-1;
	  if (qtricks>=cutoff) 
            return qtricks;
          if ((trump!=4) && (trump==suit)) {
            if (trump==0)
              suit=1;
            else
              suit=0;
          }
          else {
            suit++;
            if ((trump!=4) && (suit==trump))
              suit++;
          }
	  continue;
        }
        
        if (posPoint->secondBest[suit].hand==hand) {
          /* Second best found in own hand */
          posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
              | bitMapRank[posPoint->secondBest[suit].rank];
          qtricks++;
          if ((countLho<=2)&&(countRho<=2)&&(countPart<=2)) {
            qtricks=qtricks+countOwn-2;
	    if (qtricks>=cutoff) 
              return qtricks;
            if ((trump!=4) && (suit==trump)) {
              if (trump==0)
                suit=1;
              else
                suit=0;
            }
            else {
              suit++;
              if ((trump!=4) && (suit==trump))
                suit++;
            }
	    continue;
          }
        }
        /* 06-08-19 */
        else if ((posPoint->secondBest[suit].hand==partner[hand])
            &&(countOwn>1)&&(countPart>1)) {
			/* Second best at partner and suit length of own
			   hand and partner > 1 */
          posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
              | bitMapRank[posPoint->secondBest[suit].rank];
          qtricks++;
          if ((countLho<=2)&&(countRho<=2)&&((countPart<=2)||(countOwn<=2))) {  
		/* 07-06-10 */
	    qtricks=qtricks+Max(countOwn-2,countPart-2);
	    if (qtricks>=cutoff) 
              return qtricks;
            if ((trump!=4) && (suit==trump)) {
              if (trump==0)
                suit=1;
              else
                suit=0;
            }
            else {
              suit++;
              if ((trump!=4) && (suit==trump))
                suit++;
            }
	    continue;
          }
        }
      }
    }
    /* It was not possible to take a quick trick by own winning card in
    the suit */
    else {
    /* Partner winning card? */
      if ((posPoint->winner[suit].hand==partner[hand])&&(countPart>0)) {
        /* Winner found at partner*/
        if (commPartner) {
        /* There is communication with the partner */
          if ((trump!=4)&&(trump!=suit)) {
            if (((countLho!=0) ||
              /* LHO not void */
            (lhoTrumpRanks==0))
              /* LHO has no trump */
             && ((countRho!=0) ||
              /* RHO not void */
             (rhoTrumpRanks==0)))
              /* RHO has no trump */
              {
                posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
                  | bitMapRank[posPoint->winner[suit].rank];
                posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                   bitMapRank[commRank];
                qtricks++;   /* A trick can be taken */
                /* 06-12-14 */
		if (qtricks>=cutoff) 
                  return qtricks;
                if ((countLho<=1)&&(countRho<=1)&&(countOwn<=1)&&
                  (lhoTrumpRanks==0)&&
                  (rhoTrumpRanks==0)) {
                   qtricks=qtricks+countPart-1;
		   if (qtricks>=cutoff) 
                     return qtricks;
                   suit++;
                   if ((trump!=4) && (suit==trump))
                     suit++;
		   continue;
                }
              }
              if (posPoint->secondBest[suit].hand==partner[hand]) {
               /* Second best found in partners hand */
                if ((lhoTrumpRanks==0)&&
                 (rhoTrumpRanks==0)) {
                /* Opponents have no trump */
                  posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
                   | bitMapRank[posPoint->secondBest[suit].rank];
                  posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                    bitMapRank[commRank];
                  qtricks++;
                  if ((countLho<=2)&&(countRho<=2)&&(countOwn<=2)) {
                    qtricks=qtricks+countPart-2;
		    if (qtricks>=cutoff) 
                      return qtricks;
                    suit++;
                    if ((trump!=4) && (suit==trump))
                      suit++;
		    continue;
                  }
                }
              }
              /* 06-08-19 */
              else if ((posPoint->secondBest[suit].hand==hand)&&
                  (countPart>1)&&(countOwn>1)) {
               /* Second best found in own hand and suit
		     lengths of own hand and partner > 1*/
                if ((lhoTrumpRanks==0)&&
                 (rhoTrumpRanks==0)) {
                /* Opponents have no trump */
                  posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
                   | bitMapRank[posPoint->secondBest[suit].rank];
                  posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                    bitMapRank[commRank];
                  qtricks++;
                  if ((countLho<=2)&&(countRho<=2)&&
		     ((countOwn<=2)||(countPart<=2))) {  /* 07-06-10 */
                    qtricks=qtricks+
                      Max(countPart-2,countOwn-2);
		    if (qtricks>=cutoff) 
                      return qtricks;
                    suit++;
                    if ((trump!=4) && (suit==trump))
                      suit++;
		    continue;
                  }
                }
              }
              /* 06-08-24 */
              else if ((suit==commSuit)&&(posPoint->secondBest[suit].hand
		    ==lho[hand])&&((countLho>=2)||(lhoTrumpRanks==0))&&
                ((countRho>=2)||(rhoTrumpRanks==0))) {
                ranks=0;
                for (k=0; k<=3; k++)
                  ranks=ranks | posPoint->rankInSuit[k][suit];
                for (rr=posPoint->secondBest[suit].rank-1; rr>=2; rr--) {
                  /* 3rd best at partner? */
                  if ((ranks & bitMapRank[rr])!=0) {
                    if ((posPoint->rankInSuit[partner[hand]][suit] &
                      bitMapRank[rr])!=0) {
                      found=TRUE;
                      break;
                    }
                    else {
                      found=FALSE;
                      break;
                    }
                  }
                  found=FALSE;
                }
                if (found) {
                  posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit] | bitMapRank[rr];
                  posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                     bitMapRank[commRank];
                  qtricks++;
                  if ((countOwn<=2)&&(countLho<=2)&&(countRho<=2)&&
                    (lhoTrumpRanks==0)&&(rhoTrumpRanks==0)) 
                    qtricks=qtricks+countPart-2;
                }
              } 
          }
          else {
            posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
             | bitMapRank[posPoint->winner[suit].rank];
            posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
              bitMapRank[commRank];
            qtricks++;
            /* 06-12-14 */
	    if (qtricks>=cutoff) 
              return qtricks;

            if ((countLho<=1)&&(countRho<=1)&&(countOwn<=1)) {
              qtricks=qtricks+countPart-1;
	      if (qtricks>=cutoff) 
                return qtricks;
              if ((trump!=4) && (suit==trump)) {
                if (trump==0)
                  suit=1;
                else
                  suit=0;
              }
              else {
                suit++;
                if ((trump!=4) && (suit==trump))
                  suit++;
              }
	      continue;
            }
            if ((posPoint->secondBest[suit].hand==partner[hand])&&(countPart>0)) {
              /* Second best found in partners hand */
              posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
                | bitMapRank[posPoint->secondBest[suit].rank];
              posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                bitMapRank[commRank];
              qtricks++;
              if ((countLho<=2)&&(countRho<=2)&&(countOwn<=2)) {
                qtricks=qtricks+countPart-2;
		if (qtricks>=cutoff)
                  return qtricks;
                if ((trump!=4) && (suit==trump)) {
                  if (trump==0)
                    suit=1;
                  else
                    suit=0;
                }
                else {
                  suit++;
                  if ((trump!=4) && (suit==trump))
                    suit++;
                }
		continue;
              }
            }
	    /* 06-08-19 */
	    else if ((posPoint->secondBest[suit].hand==hand)
		  &&(countPart>1)&&(countOwn>1)) {
               /* Second best found in own hand and own and
			partner's suit length > 1 */
              posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit]
               | bitMapRank[posPoint->secondBest[suit].rank];
              posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                bitMapRank[commRank];
              qtricks++;
              if ((countLho<=2)&&(countRho<=2)&&((countOwn<=2)||(countPart<=2))) {  /* 07-06-10 */
		qtricks=qtricks+Max(countPart-2,countOwn-2);
		if (qtricks>=cutoff)
		  return qtricks;
                if ((trump!=4) && (suit==trump)) {
                  if (trump==0)
                    suit=1;
                  else
                    suit=0;
                }
                else {
                  suit++;
                  if ((trump!=4) && (suit==trump))
                    suit++;
                }
		continue;
              }
            }
            /* 06-08-24 */
            else if ((suit==commSuit)&&(posPoint->secondBest[suit].hand
		  ==lho[hand])) {
              ranks=0;
              for (k=0; k<=3; k++)
                ranks=ranks | posPoint->rankInSuit[k][suit];
              for (rr=posPoint->secondBest[suit].rank-1; rr>=2; rr--) {
                  /* 3rd best at partner? */
                if ((ranks & bitMapRank[rr])!=0) {
                  if ((posPoint->rankInSuit[partner[hand]][suit] &
                    bitMapRank[rr])!=0) {
                    found=TRUE;
                    break;
                  }
                  else {
                    found=FALSE;
                    break;
                  }
                }
                found=FALSE;
              }
              if (found) {
                posPoint->winRanks[depth][suit]=posPoint->winRanks[depth][suit] | bitMapRank[rr];
                posPoint->winRanks[depth][commSuit]=posPoint->winRanks[depth][commSuit] |
                  bitMapRank[commRank];
                qtricks++;
		if ((countOwn<=2)&&(countLho<=2)&&(countRho<=2)) { 
                  qtricks=qtricks+countPart-2;
				}
              }
            } 
          }
        }
      }
    }
    if ((trump!=4) &&(suit!=trump)&&
		(countOwn>0)&&(lowestQtricks==0)&&
		((qtricks==0)||((posPoint->winner[suit].hand!=hand)&&
		(posPoint->winner[suit].hand!=partner[hand])&&
		(posPoint->winner[trump].hand!=hand)&&
		(posPoint->winner[trump].hand!=partner[hand])))) {
      if ((countPart==0)&&(posPoint->length[partner[hand]][trump]>0)) {
        if (((countRho>0)||(posPoint->length[rho[hand]][trump]==0))&&
	  ((countLho>0)||(posPoint->length[lho[hand]][trump]==0))) {
	  lowestQtricks=1; 
	  if (1>=cutoff) 
	    return 1;
	  suit++;
	  if ((trump!=4) && (suit==trump))
            suit++;
	  continue;
	}
        else if ((countRho==0)&&(countLho==0)) {
	  if ((posPoint->rankInSuit[lho[hand]][trump] |
	     posPoint->rankInSuit[rho[hand]][trump]) <
	     posPoint->rankInSuit[partner[hand]][trump]) {
            lowestQtricks=1;
 
            rr=highestRank[posPoint->rankInSuit[partner[hand]][trump]];
	    if (rr!=0) {
	      posPoint->winRanks[depth][trump]|=bitMapRank[rr];
	      if (1>=cutoff) 
		return 1;
	    }
          }
	  suit++;
	  if ((trump!=4) && (suit==trump))
            suit++;
	  continue;
	}
	else if (countLho==0) {
          if (posPoint->rankInSuit[lho[hand]][trump] <
		posPoint->rankInSuit[partner[hand]][trump]) {
	    lowestQtricks=1; 
	    for (rr=14; rr>=2; rr--) {
	      if ((posPoint->rankInSuit[partner[hand]][trump] &
		bitMapRank[rr])!=0) {
		posPoint->winRanks[depth][trump]=
		  posPoint->winRanks[depth][trump] | bitMapRank[rr];
		break;
	      }
	    }
	    if (1>=cutoff) 
	      return 1;
	  }
	  suit++;
	  if ((trump!=4) && (suit==trump))
            suit++;
	  continue;
        }
	else if (countRho==0) {
          if (posPoint->rankInSuit[rho[hand]][trump] <
	    posPoint->rankInSuit[partner[hand]][trump]) {
	    lowestQtricks=1; 
	    for (rr=14; rr>=2; rr--) {
	      if ((posPoint->rankInSuit[partner[hand]][trump] &
		bitMapRank[rr])!=0) {
		posPoint->winRanks[depth][trump]=
		  posPoint->winRanks[depth][trump] | bitMapRank[rr];
		break;
	      }
	    }
	    if (1>=cutoff) 
	      return 1;
	  }
	  suit++;
	  if ((trump!=4) && (suit==trump))
            suit++;
	  continue;
	}
      }
    }
    if (qtricks>=cutoff) 
      return qtricks;
    if ((trump!=4) && (suit==trump)) {
      if (trump==0)
        suit=1;
      else
        suit=0;
    }
    else {
      suit++;
      if ((trump!=4) && (suit==trump))
        suit++;
    }
  }
  while (suit<=3);

  if (qtricks==0) {
    if ((trump==4)||(posPoint->winner[trump].rank==0)) {
      found=FALSE;
      for (ss=0; ss<=3; ss++) {
        if (posPoint->winner[ss].rank==0)
	  continue;
        hh=posPoint->winner[ss].hand;
	if (localVar[thrId].nodeTypeStore[hh]==localVar[thrId].nodeTypeStore[hand]) {
          if (posPoint->length[hand][ss]>0) {
            found=TRUE;   
            break;
          }
        }
	else if ((posPoint->length[partner[hand]][ss]>0)&&
	      (posPoint->length[hand][ss]>0)&&
	      (posPoint->length[partner[hh]][ss]>0)) { 
          count++;
	}
      }
      if (!found) {
	if (localVar[thrId].nodeTypeStore[hand]==MAXNODE) {
          if ((posPoint->tricksMAX+(depth>>2)-Max(0,count-1))<target) {
            for (ss=0; ss<=3; ss++) {
	      if (posPoint->winner[ss].hand==-1)
	        posPoint->winRanks[depth][ss]=0;
              else if ((posPoint->length[hand][ss]>0)
                 &&(localVar[thrId].nodeTypeStore[posPoint->winner[ss].hand]==MINNODE))
                posPoint->winRanks[depth][ss]=
		        bitMapRank[posPoint->winner[ss].rank];
              else
                posPoint->winRanks[depth][ss]=0;
	    }
	    return 0;
	  }
        }
	else {
	  if ((posPoint->tricksMAX+1+Max(0,count-1))>=target) {
          for (ss=0; ss<=3; ss++) {
	    if (posPoint->winner[ss].hand==-1)
	      posPoint->winRanks[depth][ss]=0;
            else if ((posPoint->length[hand][ss]>0) 
               &&(localVar[thrId].nodeTypeStore[posPoint->winner[ss].hand]==MAXNODE))
              posPoint->winRanks[depth][ss]=
		           bitMapRank[posPoint->winner[ss].rank];
              else
                posPoint->winRanks[depth][ss]=0;
	    }
	    return 0;
	  }
        }
      }
    }
  }

  *result=FALSE;
  return qtricks;
}

int LaterTricksMIN(struct pos *posPoint, int hand, int depth, int target, int thrId) {
  int hh, ss, sum=0;
  int trump;

  trump=localVar[thrId].trump;

  if ((trump==4)||(posPoint->winner[trump].rank==0)) {
    for (ss=0; ss<=3; ss++) {
      hh=posPoint->winner[ss].hand;
      if (hh!=-1) {
        if (localVar[thrId].nodeTypeStore[hh]==MAXNODE)
          sum+=Max(posPoint->length[hh][ss], posPoint->length[partner[hh]][ss]);
      }
    }
    if ((posPoint->tricksMAX+sum<target)&&
      (sum>0)&&(depth>0)&&(depth!=localVar[thrId].iniDepth)) {
      if ((posPoint->tricksMAX+(depth>>2)<target)) {
	for (ss=0; ss<=3; ss++) {
	  if (posPoint->winner[ss].hand==-1)
	    posPoint->winRanks[depth][ss]=0;
          else if (localVar[thrId].nodeTypeStore[posPoint->winner[ss].hand]==MINNODE)  
            posPoint->winRanks[depth][ss]=bitMapRank[posPoint->winner[ss].rank];
          else
            posPoint->winRanks[depth][ss]=0;
	}
	return FALSE;
      }
    } 
  }
  else if ((trump!=4) && (posPoint->winner[trump].rank!=0) && 
    (localVar[thrId].nodeTypeStore[posPoint->winner[trump].hand]==MINNODE)) {
    if ((posPoint->length[hand][trump]==0)&&
      (posPoint->length[partner[hand]][trump]==0)) {
      if (((posPoint->tricksMAX+(depth>>2)+1-
	  Max(posPoint->length[lho[hand]][trump],
	  posPoint->length[rho[hand]][trump]))<target)
          &&(depth>0)&&(depth!=localVar[thrId].iniDepth)) {
        for (ss=0; ss<=3; ss++)
          posPoint->winRanks[depth][ss]=0;
	return FALSE;
      }
    }    
    else if (((posPoint->tricksMAX+(depth>>2))<target)&&
      (depth>0)&&(depth!=localVar[thrId].iniDepth)) {
      for (ss=0; ss<=3; ss++)
        posPoint->winRanks[depth][ss]=0;
      posPoint->winRanks[depth][trump]=
	  bitMapRank[posPoint->winner[trump].rank];
      return FALSE;
    }
    else {
      hh=posPoint->secondBest[trump].hand;
      if (hh!=-1) {
        if ((localVar[thrId].nodeTypeStore[hh]==MINNODE)&&(posPoint->secondBest[trump].rank!=0))  {
          if (((posPoint->length[hh][trump]>1) ||
            (posPoint->length[partner[hh]][trump]>1))&&
            ((posPoint->tricksMAX+(depth>>2)-1)<target)&&(depth>0)
		      &&(depth!=localVar[thrId].iniDepth)) {
            for (ss=0; ss<=3; ss++)
              posPoint->winRanks[depth][ss]=0;
	    posPoint->winRanks[depth][trump]=
	        bitMapRank[posPoint->winner[trump].rank] | 
              bitMapRank[posPoint->secondBest[trump].rank] ;
	         return FALSE;
	  }
        }
      }
    }	
  }
  else if (trump!=4) {
    hh=posPoint->secondBest[trump].hand;
    if (hh!=-1) {
      if ((localVar[thrId].nodeTypeStore[hh]==MINNODE)&&
        (posPoint->length[hh][trump]>1)&&
	    (posPoint->winner[trump].hand==rho[hh])
        &&(posPoint->secondBest[trump].rank!=0)) {
        if (((posPoint->tricksMAX+(depth>>2))<target)&&
          (depth>0)&&(depth!=localVar[thrId].iniDepth)) {
          for (ss=0; ss<=3; ss++)
            posPoint->winRanks[depth][ss]=0;
	      posPoint->winRanks[depth][trump]=
            bitMapRank[posPoint->secondBest[trump].rank] ; 
          return FALSE;
	}
      }
    }
  }
  return TRUE;
}


int LaterTricksMAX(struct pos *posPoint, int hand, int depth, int target, int thrId) {
  int hh, ss, sum=0;
  int trump;

  trump=localVar[thrId].trump;
	
  if ((trump==4)||(posPoint->winner[trump].rank==0)) {
    for (ss=0; ss<=3; ss++) {
      hh=posPoint->winner[ss].hand;
      if (hh!=-1) {
        if (localVar[thrId].nodeTypeStore[hh]==MINNODE)
          sum+=Max(posPoint->length[hh][ss], posPoint->length[partner[hh]][ss]);
      }
    }
    if ((posPoint->tricksMAX+(depth>>2)+1-sum>=target)&&
	(sum>0)&&(depth>0)&&(depth!=localVar[thrId].iniDepth)) {
      if ((posPoint->tricksMAX+1>=target)) {
	for (ss=0; ss<=3; ss++) {
	  if (posPoint->winner[ss].hand==-1)
	    posPoint->winRanks[depth][ss]=0;
          else if (localVar[thrId].nodeTypeStore[posPoint->winner[ss].hand]==MAXNODE)  
            posPoint->winRanks[depth][ss]=bitMapRank[posPoint->winner[ss].rank];
          else
            posPoint->winRanks[depth][ss]=0;
	}
	return TRUE;
      }
    }
  }
  else if ((trump!=4) && (posPoint->winner[trump].rank!=0) &&
    (localVar[thrId].nodeTypeStore[posPoint->winner[trump].hand]==MAXNODE)) {
    if ((posPoint->length[hand][trump]==0)&&
      (posPoint->length[partner[hand]][trump]==0)) {
      if (((posPoint->tricksMAX+Max(posPoint->length[lho[hand]][trump],
        posPoint->length[rho[hand]][trump]))>=target)
        &&(depth>0)&&(depth!=localVar[thrId].iniDepth)) {
        for (ss=0; ss<=3; ss++)
          posPoint->winRanks[depth][ss]=0;
	return TRUE;
      }
    }    
    else if (((posPoint->tricksMAX+1)>=target)
      &&(depth>0)&&(depth!=localVar[thrId].iniDepth)) {
      for (ss=0; ss<=3; ss++)
        posPoint->winRanks[depth][ss]=0;
      posPoint->winRanks[depth][trump]=
	  bitMapRank[posPoint->winner[trump].rank];
      return TRUE;
    }
    else {
      hh=posPoint->secondBest[trump].hand;
      if (hh!=-1) {
        if ((localVar[thrId].nodeTypeStore[hh]==MAXNODE)&&(posPoint->secondBest[trump].rank!=0))  {
          if (((posPoint->length[hh][trump]>1) ||
            (posPoint->length[partner[hh]][trump]>1))&&
            ((posPoint->tricksMAX+2)>=target)&&(depth>0)
	    &&(depth!=localVar[thrId].iniDepth)) {
            for (ss=0; ss<=3; ss++)
              posPoint->winRanks[depth][ss]=0;
	    posPoint->winRanks[depth][trump]=
	        bitMapRank[posPoint->winner[trump].rank] | 
            bitMapRank[posPoint->secondBest[trump].rank];
	    return TRUE;
	  }
 	}
      }
    }
  }
  else if (trump!=4) {
    hh=posPoint->secondBest[trump].hand;
    if (hh!=-1) {
      if ((localVar[thrId].nodeTypeStore[hh]==MAXNODE)&&
        (posPoint->length[hh][trump]>1)&&(posPoint->winner[trump].hand==rho[hh])
        &&(posPoint->secondBest[trump].rank!=0)) {
        if (((posPoint->tricksMAX+1)>=target)&&(depth>0)
		  &&(depth!=localVar[thrId].iniDepth)) {
          for (ss=0; ss<=3; ss++)
            posPoint->winRanks[depth][ss]=0;
	  posPoint->winRanks[depth][trump]=
            bitMapRank[posPoint->secondBest[trump].rank] ;  
          return TRUE;
	}
      }
    }
  }
  return FALSE;
}


int MoveGen(struct pos * posPoint, int depth, int thrId) {
  int suit, k, m, n, r, s, t, q, first, state;
  unsigned short ris;
  int scount[4];
  int WeightAlloc(struct pos *, struct moveType * mp, int depth,
    unsigned short notVoidInSuit, int thrId);

  for (k=0; k<4; k++) 
    localVar[thrId].lowestWin[depth][k]=0;
  
  m=0;
  r=posPoint->handRelFirst;
  assert((r>=0)&&(r<=3));
  first=posPoint->first[depth];
  q=handId(first, r);
  
  s=localVar[thrId].movePly[depth+r].current;             /* Current move of first hand */
  t=localVar[thrId].movePly[depth+r].move[s].suit;        /* Suit played by first hand */
  ris=posPoint->rankInSuit[q][t];

  if ((r!=0)&&(ris!=0)) {
  /* Not first hand and not void in suit */
    k=14;   state=MOVESVALID;
    while (k>=2) {
      if ((ris & bitMapRank[k])&&(state==MOVESVALID)) {
           /* Only first move in sequence is generated */
        localVar[thrId].movePly[depth].move[m].suit=t;
        localVar[thrId].movePly[depth].move[m].rank=k;
        localVar[thrId].movePly[depth].move[m].sequence=0;
        m++;
        state=MOVESLOCKED;
      }
      else if (state==MOVESLOCKED)
        if ((posPoint->removedRanks[t] & bitMapRank[k])==0) {
          if ((ris & bitMapRank[k])==0)
          /* If the card still exists and it is not in own hand */
            state=MOVESVALID;
          else
          /* If the card still exists and it is in own hand */
            localVar[thrId].movePly[depth].move[m-1].sequence|=bitMapRank[k];
        }
      k--;
    }
    if (m!=1) {
      for (k=0; k<=m-1; k++) 
        localVar[thrId].movePly[depth].move[k].weight=WeightAlloc(posPoint,
          &localVar[thrId].movePly[depth].move[k], depth, ris, thrId);
    }

    localVar[thrId].movePly[depth].last=m-1;
    if (m!=1)
      InsertSort(m, depth, thrId);
    if (depth!=localVar[thrId].iniDepth)
      return m;
    else {
      m=AdjustMoveList(thrId);
      return m;
    }
  }
  else {                  /* First hand or void in suit */
    for (suit=0; suit<=3; suit++)  {
      k=14;  state=MOVESVALID;
      while (k>=2) {
        if ((posPoint->rankInSuit[q][suit] & bitMapRank[k])&&
            (state==MOVESVALID)) {
           /* Only first move in sequence is generated */
          localVar[thrId].movePly[depth].move[m].suit=suit;
          localVar[thrId].movePly[depth].move[m].rank=k;
          localVar[thrId].movePly[depth].move[m].sequence=0;
          m++;
          state=MOVESLOCKED;
        }
        else if (state==MOVESLOCKED)
          if ((posPoint->removedRanks[suit] & bitMapRank[k])==0) {
            if ((posPoint->rankInSuit[q][suit] & bitMapRank[k])==0)
            /* If the card still exists and it is not in own hand */
              state=MOVESVALID;
            else
            /* If the card still exists and it is in own hand */
              localVar[thrId].movePly[depth].move[m-1].sequence|=bitMapRank[k];
          }
        k--;
      }
    }

    for (k=0; k<=m-1; k++) 
        localVar[thrId].movePly[depth].move[k].weight=WeightAlloc(posPoint,
          &localVar[thrId].movePly[depth].move[k], depth, ris, thrId);
  
    localVar[thrId].movePly[depth].last=m-1;
    InsertSort(m, depth, thrId);
    if (r==0) {
      for (n=0; n<=3; n++)
        scount[n]=0;
      for (k=0; k<=m-1; k++) {
        if (scount[localVar[thrId].movePly[depth].move[k].suit]==1/*2*/) 
          continue;
        else {
          localVar[thrId].movePly[depth].move[k].weight+=500;
          scount[localVar[thrId].movePly[depth].move[k].suit]++;
        }
      }
      InsertSort(m, depth, thrId);
    }
    else {
      for (n=0; n<=3; n++)
        scount[n]=0;
      for (k=0; k<=m-1; k++) {
        if (scount[localVar[thrId].movePly[depth].move[k].suit]==1) 
          continue;
        else {
          localVar[thrId].movePly[depth].move[k].weight+=500;
          scount[localVar[thrId].movePly[depth].move[k].suit]++;
        }
      }
      InsertSort(m, depth, thrId);
    }
    if (depth!=localVar[thrId].iniDepth)
     return m;
    else {
      m=AdjustMoveList(thrId);
      return m;
    }  
  }
}


int WeightAlloc(struct pos * posPoint, struct moveType * mp, int depth,
  unsigned short notVoidInSuit, int thrId) {
  int weight=0, k, l, kk, ll, suit, suitAdd=0, leadSuit;
  int suitWeightDelta, first, q, trump;
  int suitBonus=0;
  int winMove=FALSE;
  unsigned short suitCount, suitCountLH, suitCountRH;
  int countLH, countRH;

  first=posPoint->first[depth];
  q=handId(first, posPoint->handRelFirst);
  suit=mp->suit;
  trump=localVar[thrId].trump;

  if (!notVoidInSuit) {
    suitCount=posPoint->length[q][suit];
    /*suitAdd=suitCount+suitCount;*/
    suitAdd=(suitCount<<6)/36;
    if ((suitCount==2)&&(posPoint->secondBest[suit].hand==q))
      suitAdd-=2;
  }

  switch (posPoint->handRelFirst) {
    case 0:
      suitCount=posPoint->length[q][suit];
      suitCountLH=posPoint->length[lho[q]][suit];
      suitCountRH=posPoint->length[rho[q]][suit];

      if ((trump!=4) && (suit!=trump) &&
          (((posPoint->rankInSuit[lho[q]][suit]==0) &&
          (posPoint->rankInSuit[lho[q]][trump]!=0)) ||
          ((posPoint->rankInSuit[rho[q]][suit]==0) &&
          (posPoint->rankInSuit[rho[q]][trump]!=0))))
        suitBonus=-10/*12*//*15*/;

      if ((posPoint->winner[suit].hand==rho[q])||
	  ((posPoint->secondBest[suit].hand!=-1)&&
          (posPoint->secondBest[suit].hand==rho[q]))) {
	  suitBonus-=18;
      }

      if ((trump!=4)&&(suit!=trump)&&(suitCount==1)&&
	    (posPoint->length[q][trump]>0)&&
	    (posPoint->length[partner[q]][suit]>1)&&
	    (posPoint->winner[suit].hand==partner[q]))
	suitBonus+=16;

      if (suitCountLH!=0)
        countLH=(suitCountLH<<2);
      else
        countLH=depth+4;
      if (suitCountRH!=0)
        countRH=(suitCountRH<<2);
      else
        countRH=depth+4;

      suitWeightDelta=suitBonus-((countLH+countRH)<<5)/15;

      if (posPoint->winner[suit].rank==mp->rank) {
        if ((trump!=4)&&(suit!=trump)) {
	  if ((posPoint->length[partner[first]][suit]!=0)||
	    (posPoint->length[partner[first]][trump]==0)) {
	    if (((posPoint->length[lho[first]][suit]!=0)||
		  (posPoint->length[lho[first]][trump]==0))&&
		  ((posPoint->length[rho[first]][suit]!=0)||
		  (posPoint->length[rho[first]][trump]==0)))
	      winMove=TRUE;
	  }
	  else if (((posPoint->length[lho[first]][suit]!=0)||
               (posPoint->rankInSuit[partner[first]][trump]>
                posPoint->rankInSuit[lho[first]][trump]))&&
		((posPoint->length[rho[first]][suit]!=0)||
		(posPoint->rankInSuit[partner[first]][trump]>
		 posPoint->rankInSuit[rho[first]][trump])))
	    winMove=TRUE;
	}
        else 
          winMove=TRUE;			   
      }
      else if (posPoint->rankInSuit[partner[first]][suit] >
	(posPoint->rankInSuit[lho[first]][suit] |
	posPoint->rankInSuit[rho[first]][suit])) {
	if ((trump!=4) && (suit!=trump)) {
	  if (((posPoint->length[lho[first]][suit]!=0)||
	      (posPoint->length[lho[first]][trump]==0))&&
	      ((posPoint->length[rho[first]][suit]!=0)||
	      (posPoint->length[rho[first]][trump]==0)))
	    winMove=TRUE;
	}
	else
	  winMove=TRUE;
      }			
      else if ((trump!=4)&&(suit!=trump)) {
        if ((posPoint->length[partner[first]][suit]==0)&&
            (posPoint->length[partner[first]][trump]!=0)) {
	  if ((posPoint->length[lho[first]][suit]==0)&&
              (posPoint->length[lho[first]][trump]!=0)&&
		  (posPoint->length[rho[first]][suit]==0)&&
              (posPoint->length[rho[first]][trump]!=0)) {
	    if (posPoint->rankInSuit[partner[first]][trump]>
	       (posPoint->rankInSuit[lho[first]][trump] |
		posPoint->rankInSuit[rho[first]][trump]))
	      winMove=TRUE;
	  }
	  else if ((posPoint->length[lho[first]][suit]==0)&&
              (posPoint->length[lho[first]][trump]!=0)) {
	    if (posPoint->rankInSuit[partner[first]][trump]
		    > posPoint->rankInSuit[lho[first]][trump])
	        winMove=TRUE;
	  }	
	  else if ((posPoint->length[rho[first]][suit]==0)&&
              (posPoint->length[rho[first]][trump]!=0)) {
	    if (posPoint->rankInSuit[partner[first]][trump]
		    > posPoint->rankInSuit[rho[first]][trump])
	      winMove=TRUE;
	  }	
          else
	    winMove=TRUE;
	}
      }
              
      if (winMove) {
	if (((suitCountLH==1)&&(posPoint->winner[suit].hand==lho[first]))
            ||((suitCountRH==1)&&(posPoint->winner[suit].hand==rho[first])))
          weight=suitWeightDelta+40-(mp->rank);
        else if (posPoint->winner[suit].hand==first) {
          if ((posPoint->secondBest[suit].hand!=-1)&&
	     (posPoint->secondBest[suit].hand==partner[first]))
            weight=suitWeightDelta+50-(mp->rank);
	  else if (posPoint->winner[suit].rank==mp->rank) 
            weight=suitWeightDelta+31;
          else
            weight=suitWeightDelta+19-(mp->rank);
        }
        else if (posPoint->winner[suit].hand==partner[first]) {
          /* If partner has winning card */
          if (posPoint->secondBest[suit].hand==first)
            weight=suitWeightDelta+50-(mp->rank);
          else 
            weight=suitWeightDelta+35-(mp->rank);  
        } 
        else if ((mp->sequence)&&
          (mp->rank==posPoint->secondBest[suit].rank))			
          weight=suitWeightDelta+40/*-(mp->rank)*/;
        else
          weight=suitWeightDelta+30-(mp->rank);

	if ((localVar[thrId].bestMove[depth].suit==mp->suit)&&
            (localVar[thrId].bestMove[depth].rank==mp->rank)) 
          weight+=52;
	else if ((localVar[thrId].bestMoveTT[depth].suit==mp->suit)&&
            (localVar[thrId].bestMoveTT[depth].rank==mp->rank)) 
          weight+=11;
	/*else if (localVar[thrId].bestMove[depth].suit==mp->suit)
	    weight+=10;*/
      }
      else {
        if (((suitCountLH==1)&&(posPoint->winner[suit].hand==lho[first]))
            ||((suitCountRH==1)&&(posPoint->winner[suit].hand==rho[first])))
          weight=suitWeightDelta+29-(mp->rank);
        else if (posPoint->winner[suit].hand==first) {
          if ((posPoint->secondBest[suit].rank!=0)&&
	     (posPoint->secondBest[suit].hand==partner[first]))
            weight=suitWeightDelta+44-(mp->rank);
          else if (posPoint->winner[suit].rank==mp->rank) 
            weight=suitWeightDelta+25;
          else
            weight=suitWeightDelta+13-(mp->rank);
        }
        else if (posPoint->winner[suit].hand==partner[first]) {
          /* If partner has winning card */
          if (posPoint->secondBest[suit].hand==first)
            weight=suitWeightDelta+44-(mp->rank);
          else 
            weight=suitWeightDelta+29-(mp->rank);  
        } 
        else if ((mp->sequence)&&
          (mp->rank==posPoint->secondBest[suit].rank)) 
          weight=suitWeightDelta+29;
        else 
          weight=suitWeightDelta+13-(mp->rank); 
	
	if ((localVar[thrId].bestMove[depth].suit==mp->suit)&&
            (localVar[thrId].bestMove[depth].rank==mp->rank)) 
          weight+=20;
	else if ((localVar[thrId].bestMoveTT[depth].suit==mp->suit)&&
            (localVar[thrId].bestMoveTT[depth].rank==mp->rank)) 
          weight+=9;
	/*else if (localVar[thrId].bestMove[depth].suit==mp->suit)
	    weight+=10;*/
      }
        
      break;

    case 1:
      leadSuit=posPoint->move[depth+1].suit;
      if (leadSuit==suit) {
	if (bitMapRank[mp->rank]>
	    (bitMapRank[posPoint->move[depth+1].rank] |
	    posPoint->rankInSuit[partner[first]][suit])) {
	  if ((trump!=4) && (suit!=trump)) {
	    if ((posPoint->length[partner[first]][suit]!=0)||
		  (posPoint->length[partner[first]][trump]==0))
	      winMove=TRUE;
	    else if ((posPoint->length[rho[first]][suit]==0)
                &&(posPoint->length[rho[first]][trump]!=0)
                &&(posPoint->rankInSuit[rho[first]][trump]>
                 posPoint->rankInSuit[partner[first]][trump]))
	      winMove=TRUE;
	  }
          else
            winMove=TRUE;
        }
	else if (posPoint->rankInSuit[rho[first]][suit]>
	    (bitMapRank[posPoint->move[depth+1].rank] |
	    posPoint->rankInSuit[partner[first]][suit])) {	 
	  if ((trump!=4) && (suit!=trump)) {
	    if ((posPoint->length[partner[first]][suit]!=0)||
		  (posPoint->length[partner[first]][trump]==0))
	      winMove=TRUE;
	  }
          else
            winMove=TRUE;
	} 
	else if (bitMapRank[posPoint->move[depth+1].rank] >
	    (posPoint->rankInSuit[rho[first]][suit] |
	     posPoint->rankInSuit[partner[first]][suit] |
	     bitMapRank[mp->rank])) {  
	  if ((trump!=4) && (suit!=trump)) {
	    if ((posPoint->length[rho[first]][suit]==0)&&
		  (posPoint->length[rho[first]][trump]!=0)) {
	      if ((posPoint->length[partner[first]][suit]!=0)||
		    (posPoint->length[partner[first]][trump]==0))
		winMove=TRUE;
	      else if (posPoint->rankInSuit[rho[first]][trump]
                  > posPoint->rankInSuit[partner[first]][trump])
		winMove=TRUE;
	    }	  
	  }
	}	
	else {   /* winnerHand is partner to first */
	  if ((trump!=4) && (suit!=trump)) {
	    if ((posPoint->length[rho[first]][suit]==0)&&
		  (posPoint->length[rho[first]][trump]!=0))
	       winMove=TRUE;
	  }  
	}
      }
      else {
	 /* Leading suit differs from suit played by LHO */
	if ((trump!=4) && (suit==trump)) {
	  if (posPoint->length[partner[first]][leadSuit]!=0)
	    winMove=TRUE;
	  else if (bitMapRank[mp->rank]>
		posPoint->rankInSuit[partner[first]][trump]) 
	    winMove=TRUE;
	  else if ((posPoint->length[rho[first]][leadSuit]==0)
              &&(posPoint->length[rho[first]][trump]!=0)&&
              (posPoint->rankInSuit[rho[first]][trump] >
              posPoint->rankInSuit[partner[first]][trump]))
            winMove=TRUE;
        }	
        else if ((trump!=4) && (leadSuit!=trump)) {
          /* Neither suit nor leadSuit is trump */
          if (posPoint->length[partner[first]][leadSuit]!=0) {
            if (posPoint->rankInSuit[rho[first]][leadSuit] >
              (posPoint->rankInSuit[partner[first]][leadSuit] |
              bitMapRank[posPoint->move[depth+1].rank]))
              winMove=TRUE;
	    else if ((posPoint->length[rho[first]][leadSuit]==0)
		  &&(posPoint->length[rho[first]][trump]!=0))
	      winMove=TRUE;
	  }
	  /* Partner to leading hand is void in leading suit */
	  else if ((posPoint->length[rho[first]][leadSuit]==0)
		&&(posPoint->rankInSuit[rho[first]][trump]>
	      posPoint->rankInSuit[partner[first]][trump]))
	    winMove=TRUE;
	  else if ((posPoint->length[partner[first]][trump]==0)
	      &&(posPoint->rankInSuit[rho[first]][leadSuit] >
		bitMapRank[posPoint->move[depth+1].rank]))
	    winMove=TRUE;
        }
        else {
	  /* Either no trumps or leadSuit is trump, side with
		highest rank in leadSuit wins */
	  if (posPoint->rankInSuit[rho[first]][leadSuit] >
            (posPoint->rankInSuit[partner[first]][leadSuit] |
             bitMapRank[posPoint->move[depth+1].rank]))
            winMove=TRUE;			   
        }			  
      }
      
      kk=posPoint->rankInSuit[partner[first]][leadSuit];
      ll=posPoint->rankInSuit[rho[first]][leadSuit];
      k=kk & (-kk); l=ll & (-ll);  /* Only least significant 1 bit */
      if (winMove) {
        if (!notVoidInSuit) { 
          if ((trump!=4) && (suit==trump))  
            weight=25/*30*/-(mp->rank)+suitAdd;
          else
            weight=60-(mp->rank)+suitAdd;  /* Better discard than ruff since rho
								wins anyway */
        }
        else if (k > bitMapRank[mp->rank])
          weight=45-(mp->rank);    /* If lowest card for partner to leading hand 
					is higher than lho played card, playing as low as 
					possible will give the cheapest win */
        else if ((ll > bitMapRank[posPoint->move[depth+1].rank])&&
          (posPoint->rankInSuit[first][leadSuit] > ll)) 
          weight=60-(mp->rank);    /* If rho has a card in the leading suit that
                                    is higher than the trick leading card but lower
                                    than the highest rank of the leading hand, then
                                    lho playing the lowest card will be the cheapest
                                    win */
	    else if (mp->rank > posPoint->move[depth+1].rank) {
          if (bitMapRank[mp->rank] < ll) 
            weight=75-(mp->rank);  /* If played card is lower than any of the cards of
					rho, it will be the cheapest win */
          else if (bitMapRank[mp->rank] > kk)
            weight=70-(mp->rank);  /* If played card is higher than any cards at partner
						of the leading hand, rho can play low, under the
                                    condition that he has a lower card than lho played */ 
          else {
            if (mp->sequence)
              weight=60-(mp->rank); 
            else
              weight=45-(mp->rank);
          }
        } 
        else if (posPoint->length[rho[first]][leadSuit]>0) {
          if (mp->sequence)
            weight=50-(mp->rank);  /* Playing a card in a sequence may promote a winner */
          else
            weight=45-(mp->rank);
        }
        else
          weight=45-(mp->rank);
      }
      else {
        if (!notVoidInSuit) {
	  if ((trump!=4) && (suit==trump)) {
             weight=15-(mp->rank)+suitAdd;  /* Ruffing is preferred, makes the trick
						costly for the opponents */
	  }
          else
            weight=-(mp->rank)+suitAdd;
        }
        else if ((k > bitMapRank[mp->rank])||
          (l > bitMapRank[mp->rank])) 
          weight=-(mp->rank);  /* If lowest rank for either partner to leading hand 
				or rho is higher than played card for lho,
				lho should play as low card as possible */			
        else if (mp->rank > posPoint->move[depth+1].rank) {		  
          if (mp->sequence) 
            weight=20-(mp->rank);
          else 
            weight=10-(mp->rank);
        }          
        else
          weight=-(mp->rank);  
      }

      break;

    case 2:
            
      leadSuit=posPoint->move[depth+2].suit;
      if (WinningMove(mp, &(posPoint->move[depth+1]), thrId)) {
	if (suit==leadSuit) {
	  if ((trump!=4) && (leadSuit!=trump)) {
	    if (((posPoint->length[rho[first]][suit]!=0)||
		  (posPoint->length[rho[first]][trump]==0))&&
		  (bitMapRank[mp->rank] >
		   posPoint->rankInSuit[rho[first]][suit]))
	      winMove=TRUE;
	  }	
	  else if (bitMapRank[mp->rank] >
	    posPoint->rankInSuit[rho[first]][suit])
	    winMove=TRUE;
	}
	else {  /* Suit is trump */
	  if (posPoint->length[rho[first]][leadSuit]==0) {
	    if (bitMapRank[mp->rank] >
		  posPoint->rankInSuit[rho[first]][trump])
	      winMove=TRUE;
	  }
	  else
	    winMove=TRUE;
	}
      }	
      else if (posPoint->high[depth+1]==first) {
	if (posPoint->length[rho[first]][leadSuit]!=0) {
	  if (posPoint->rankInSuit[rho[first]][leadSuit]
		 < bitMapRank[posPoint->move[depth+2].rank])	
	    winMove=TRUE;
	}
	else if (trump==4)
	  winMove=TRUE;
	else if ((trump!=4) && (leadSuit==trump))
          winMove=TRUE;
	else if ((trump!=4) && (leadSuit!=trump) &&
	    (posPoint->length[rho[first]][trump]==0))
	  winMove=TRUE;
      }
      
      if (winMove) {
        if (!notVoidInSuit) {
          if (posPoint->high[depth+1]==first) {
            if ((trump!=4) && (suit==trump)) 
              weight=30-(mp->rank)+suitAdd; /* Ruffs partner's winner */
	    else
              weight=60-(mp->rank)+suitAdd;
          } 
          else if (WinningMove(mp, &(posPoint->move[depth+1]), thrId))
             /* Own hand on top by ruffing */
            weight=70-(mp->rank)+suitAdd;
          else if ((trump!=4) && (suit==trump))
            /* Discard a trump but still losing */
            weight=15-(mp->rank)+suitAdd;
          else
            weight=30-(mp->rank)+suitAdd;
        }
        else 
          weight=60-(mp->rank); 
      }
      else {
        if (!notVoidInSuit) {
          if (WinningMove(mp, &(posPoint->move[depth+1]), thrId))
             /* Own hand on top by ruffing */
            weight=40-(mp->rank)+suitAdd;
          else if ((trump!=4) && (suit==trump))
            /* Discard a trump but still losing */
            weight=-15-(mp->rank)+suitAdd;
          else
            weight=-(mp->rank)+suitAdd;
        }
        else {
          if (WinningMove(mp, &(posPoint->move[depth+1]), thrId)) {
            if (mp->rank==posPoint->secondBest[leadSuit].rank)
              weight=25/*35*/;
            else if (mp->sequence)
              weight=20/*30*/-(mp->rank);
            else
              weight=10/*20*/-(mp->rank);
          }
          else  
            weight=-10/*0*/-(mp->rank);  
        } 
      }
            
      break;

    case 3:
      if (!notVoidInSuit) {
        if ((posPoint->high[depth+1])==lho[first]) {
          /* If the current winning move is given by the partner */
          if ((trump!=4) && (suit==trump))
            /* Ruffing partners winner? */
            weight=14-(mp->rank)+suitAdd;
          else 
            weight=30-(mp->rank)+suitAdd;
        }
        else if (WinningMove(mp, &(posPoint->move[depth+1]), thrId)) 
          /* Own hand ruffs */
          weight=30-(mp->rank)+suitAdd;
        else if (suit==trump) 
          weight=-(mp->rank);
        else 
          weight=14-(mp->rank)+suitAdd;  
      }
      else if ((posPoint->high[depth+1])==(lho[first])) {
        /* If the current winning move is given by the partner */
        if ((trump!=4) && (suit==trump))
        /* Ruffs partners winner */
          weight=24-(mp->rank);
        else 
          weight=30-(mp->rank);
      }
      else if (WinningMove(mp, &(posPoint->move[depth+1]), thrId))
        /* If present move is superior to current winning move and the
        current winning move is not given by the partner */
        weight=30-(mp->rank);
      else {
        /* If present move is not superior to current winning move and the
        current winning move is not given by the partner */
        if ((trump!=4) && (suit==trump))
          /* Ruffs but still loses */
          weight=-(mp->rank);
        else 
          weight=14-(mp->rank);
      }
  }
  return weight;
}

/* Shell-1 */
/* K&R page 62: */
/*void shellSort(int n, int depth) {
  int gap, i, j;
  struct moveType temp;

  if (n==2) {
    if (movePly[depth].move[0].weight<movePly[depth].move[1].weight) {
      temp=movePly[depth].move[0];
      movePly[depth].move[0]=movePly[depth].move[1];
      movePly[depth].move[1]=temp;
      return;
    }
    else
      return;
  }
  for (gap=n>>1; gap>0; gap>>=1)
    for (i=gap; i<n; i++)
      for (j=i-gap; j>=0 && movePly[depth].move[j].weight<
         movePly[depth].move[j+gap].weight; j-=gap) {
        temp=movePly[depth].move[j];
        movePly[depth].move[j]=movePly[depth].move[j+gap];
        movePly[depth].move[j+gap]=temp;
      }
} */

/* Shell-2 */
/*void shellSort(int n, int depth)
{
  int i, j, increment;
  struct moveType temp;

  if (n==2) {
    if (movePly[depth].move[0].weight<movePly[depth].move[1].weight) {
      temp=movePly[depth].move[0];
      movePly[depth].move[0]=movePly[depth].move[1];
      movePly[depth].move[1]=temp;
      return;
    }
    else
      return;
  }
  increment = 3;
  while (increment > 0)
  {
    for (i=0; i < n; i++)
    {
      j = i;
      temp = movePly[depth].move[i];
      while ((j >= increment) && (movePly[depth].move[j-increment].weight < temp.weight))
      {
        movePly[depth].move[j] = movePly[depth].move[j - increment];
        j = j - increment;
      }
      movePly[depth].move[j] = temp;
    }
    if ((increment>>1) != 0)
      increment>>=1;
    else if (increment == 1)
      increment = 0;
    else
      increment = 1;
  }
} */


/* Insert-1 */
void InsertSort(int n, int depth, int thrId) {
  int i, j;
  struct moveType a, temp;

  if (n==2) {
    if (localVar[thrId].movePly[depth].move[0].weight<
      localVar[thrId].movePly[depth].move[1].weight) {
      temp=localVar[thrId].movePly[depth].move[0];
      localVar[thrId].movePly[depth].move[0]=localVar[thrId].movePly[depth].move[1];
      localVar[thrId].movePly[depth].move[1]=temp;
      return;
    }
    else
      return;
  }

  a=localVar[thrId].movePly[depth].move[0];
  for (i=1; i<=n-1; i++) 
    if (localVar[thrId].movePly[depth].move[i].weight>a.weight) {
      temp=a;
      a=localVar[thrId].movePly[depth].move[i];
      localVar[thrId].movePly[depth].move[i]=temp;
    }
  localVar[thrId].movePly[depth].move[0]=a; 
  for (i=2; i<=n-1; i++) {  
    j=i;
    a=localVar[thrId].movePly[depth].move[i];
    while (a.weight>localVar[thrId].movePly[depth].move[j-1].weight) {
      localVar[thrId].movePly[depth].move[j]=localVar[thrId].movePly[depth].move[j-1];
      j--;
    }
    localVar[thrId].movePly[depth].move[j]=a;
  }
}  

/* Insert-2 */
/*void InsertSort(int n, int depth) {
  int i, j;
  struct moveType a;

  if (n==2) {
    if (movePly[depth].move[0].weight<movePly[depth].move[1].weight) {
      a=movePly[depth].move[0];
      movePly[depth].move[0]=movePly[depth].move[1];
      movePly[depth].move[1]=a;
      return;
    }
    else
      return;
  }
  for (j=1; j<=n-1; j++) {
    a=movePly[depth].move[j];
    i=j-1;
    while ((i>=0)&&(movePly[depth].move[i].weight<a.weight)) {
      movePly[depth].move[i+1]=movePly[depth].move[i];
      i--;
    }
    movePly[depth].move[i+1]=a;
  }
}  */


int AdjustMoveList(int thrId) {
  int k, r, n, rank, suit;

  for (k=1; k<=13; k++) {
    suit=localVar[thrId].forbiddenMoves[k].suit;
    rank=localVar[thrId].forbiddenMoves[k].rank;
    for (r=0; r<=localVar[thrId].movePly[localVar[thrId].iniDepth].last; r++) {
      if ((suit==localVar[thrId].movePly[localVar[thrId].iniDepth].move[r].suit)&&
        (rank!=0)&&(rank==localVar[thrId].movePly[localVar[thrId].iniDepth].move[r].rank)) {
        /* For the forbidden move r: */
        for (n=r; n<=localVar[thrId].movePly[localVar[thrId].iniDepth].last; n++)
          localVar[thrId].movePly[localVar[thrId].iniDepth].move[n]=
		  localVar[thrId].movePly[localVar[thrId].iniDepth].move[n+1];
        localVar[thrId].movePly[localVar[thrId].iniDepth].last--;
      }  
    }
  }
  return localVar[thrId].movePly[localVar[thrId].iniDepth].last+1;
}


int InvBitMapRank(unsigned short bitMap) {

  switch (bitMap) {
    case 0x1000: return 14;
    case 0x0800: return 13;
    case 0x0400: return 12;
    case 0x0200: return 11;
    case 0x0100: return 10;
    case 0x0080: return 9;
    case 0x0040: return 8;
    case 0x0020: return 7;
    case 0x0010: return 6;
    case 0x0008: return 5;
    case 0x0004: return 4;
    case 0x0002: return 3;
    case 0x0001: return 2;
    default: return 0;
  }
}

int InvWinMask(int mask) {

  switch (mask) {
    case 0x01000000: return 1;
    case 0x00400000: return 2;
    case 0x00100000: return 3;
    case 0x00040000: return 4;
    case 0x00010000: return 5;
    case 0x00004000: return 6;
    case 0x00001000: return 7;
    case 0x00000400: return 8;
    case 0x00000100: return 9;
    case 0x00000040: return 10;
    case 0x00000010: return 11;
    case 0x00000004: return 12;
    case 0x00000001: return 13;
    default: return 0;
  }
}
	  

int WinningMove(struct moveType * mvp1, struct moveType * mvp2, int thrId) {
/* Return TRUE if move 1 wins over move 2, with the assumption that
move 2 is the presently winning card of the trick */

  if (mvp1->suit==mvp2->suit) {
    if ((mvp1->rank)>(mvp2->rank))
      return TRUE;
    else
      return FALSE;
  }    
  else if ((localVar[thrId].trump!=4) && (mvp1->suit)==localVar[thrId].trump)
    return TRUE;
  else
    return FALSE;
}


struct nodeCardsType * CheckSOP(struct pos * posPoint, struct nodeCardsType
  * nodep, int target, int tricks, int * result, int *value, int thrId) {
    /* Check SOP if it matches the
    current position. If match, pointer to the SOP node is returned and
    result is set to TRUE, otherwise pointer to SOP node is returned
    and result set to FALSE. */

  /* 07-04-22 */ 
  if (localVar[thrId].nodeTypeStore[0]==MAXNODE) {
    if (nodep->lbound==-1) {  /* This bound values for
      this leading hand has not yet been determined */
      *result=FALSE;
      return nodep;
    }	
    else if ((posPoint->tricksMAX + nodep->lbound)>=target) {
      *value=TRUE;
      *result=TRUE;
      return nodep;
    }
    else if ((posPoint->tricksMAX + nodep->ubound)<target) {
      *value=FALSE;
      *result=TRUE;
      return nodep;
    }
  }
  else {
    if (nodep->ubound==-1) {  /* This bound values for
      this leading hand has not yet been determined */
      *result=FALSE;
      return nodep;
    }	
    else if ((posPoint->tricksMAX + (tricks + 1 - nodep->ubound))>=target) {
      *value=TRUE;
      *result=TRUE;
      return nodep;
    }
    else if ((posPoint->tricksMAX + (tricks + 1 - nodep->lbound))<target) {
      *value=FALSE;
      *result=TRUE;
      return nodep;
    }
  }

  *result=FALSE;
  return nodep;          /* No matching node was found */
}


struct nodeCardsType * UpdateSOP(struct pos * posPoint, struct nodeCardsType
  * nodep) {
    /* Update SOP node with new values for upper and lower
	  bounds. */
	
    if ((posPoint->lbound > nodep->lbound) ||
		(nodep->lbound==-1))
      nodep->lbound=posPoint->lbound;
    if ((posPoint->ubound < nodep->ubound) ||
		(nodep->ubound==-1))
      nodep->ubound=posPoint->ubound;

    nodep->bestMoveSuit=posPoint->bestMoveSuit;
    nodep->bestMoveRank=posPoint->bestMoveRank;

    return nodep;
}


struct nodeCardsType * FindSOP(struct pos * posPoint,
  struct winCardType * nodeP, int firstHand, 
	int target, int tricks, int * valp, int thrId) {
  struct nodeCardsType * sopP;
  struct winCardType * np;
  int s, res;

  np=nodeP; s=0;
  while ((np!=NULL)&&(s<4)) {
    if ((np->winMask & posPoint->orderSet[s])==
       np->orderSet)  {
      /* Winning rank set fits position */
      if (s==3) {
	sopP=CheckSOP(posPoint, np->first, target, tricks, &res, valp, thrId);
	if (res) {
	  return sopP;
	}
	else {
	  if (np->next!=NULL) {
	    np=np->next;
	  }
	  else {
	    np=np->prevWin;
	    s--;
	    if (np==NULL)
	      return NULL;
	    while (np->next==NULL) {
	      np=np->prevWin;
	      s--;
	      if (np==NULL)  /* Previous node is header node? */
				return NULL;
	    }
	    np=np->next;
	  }
	}
      }
      else if (s<4) {
	np=np->nextWin;
	s++;
      }
    }
    else {
      if (np->next!=NULL) {
	np=np->next;
      }
      else {
	np=np->prevWin;
	s--;
	if (np==NULL)
	  return NULL;
	while (np->next==NULL) {
	  np=np->prevWin;
	  s--;
	  if (np==NULL)  /* Previous node is header node? */
	    return NULL;
	}
	np=np->next;
      }
    }
  }
  return NULL;
}


struct nodeCardsType * BuildPath(struct pos * posPoint, 
  struct posSearchType *nodep, int * result, int thrId) {
  /* If result is TRUE, a new SOP has been created and BuildPath returns a
  pointer to it. If result is FALSE, an existing SOP is used and BuildPath
  returns a pointer to the SOP */

  int found, suit;
  struct winCardType * np, * p2, /* * sp2,*/ * nprev, * fnp, *pnp;
  struct winCardType temp;
  struct nodeCardsType * sopP=0, * p/*, * sp*/;

  np=nodep->posSearchPoint;
  nprev=NULL;
  suit=0;

  /* If winning node has a card that equals the next winning card deduced
  from the position, then there already exists a (partial) path */

  if (np==NULL) {   /* There is no winning list created yet */
   /* Create winning nodes */
    p2=&localVar[thrId].winCards[localVar[thrId].winSetSize];
    AddWinSet(thrId);
    p2->next=NULL;
    p2->nextWin=NULL;
    p2->prevWin=NULL;
    nodep->posSearchPoint=p2;
    p2->winMask=posPoint->winMask[suit];
    p2->orderSet=posPoint->winOrderSet[suit];
    p2->first=NULL;
    np=p2;           /* Latest winning node */
    suit++;
    while (suit<4) {
      p2=&localVar[thrId].winCards[localVar[thrId].winSetSize];
      AddWinSet(thrId);
      np->nextWin=p2;
      p2->prevWin=np;
      p2->next=NULL;
      p2->nextWin=NULL;
      p2->winMask=posPoint->winMask[suit];
      p2->orderSet=posPoint->winOrderSet[suit];
      p2->first=NULL;
      np=p2;         /* Latest winning node */
      suit++;
    }
    p=&localVar[thrId].nodeCards[localVar[thrId].nodeSetSize];
    AddNodeSet(thrId);
    np->first=p;
    *result=TRUE;
    return p;
  }
  else {   /* Winning list exists */
    while (1) {   /* Find all winning nodes that correspond to current
		position */
      found=FALSE;
      while (1) {    /* Find node amongst alternatives */
	if ((np->winMask==posPoint->winMask[suit])&&
	   (np->orderSet==posPoint->winOrderSet[suit])) {
	   /* Part of path found */
	  found=TRUE;
	  nprev=np;
	  break;
	}
	if (np->next!=NULL)
	  np=np->next;
	else
	  break;
      }
      if (found) {
	suit++;
	if (suit>3) {
	  sopP=UpdateSOP(posPoint, np->first);

	  if (np->prevWin!=NULL) {
	    pnp=np->prevWin;
	    fnp=pnp->nextWin;
	  }
	  else 
	    fnp=nodep->posSearchPoint;

	  temp.orderSet=np->orderSet;
	  temp.winMask=np->winMask;
	  temp.first=np->first;
	  temp.nextWin=np->nextWin;
	  np->orderSet=fnp->orderSet;
	  np->winMask=fnp->winMask;
	  np->first=fnp->first;
	  np->nextWin=fnp->nextWin;
	  fnp->orderSet=temp.orderSet;
	  fnp->winMask=temp.winMask;
	  fnp->first=temp.first;
	  fnp->nextWin=temp.nextWin;

	  *result=FALSE;
	  return sopP;
	}
	else {
	  np=np->nextWin;       /* Find next winning node  */
	  continue;
	}
      }
      else
	break;                    /* Node was not found */
    }               /* End outer while */

    /* Create additional node, coupled to existing node(s) */
    p2=&localVar[thrId].winCards[localVar[thrId].winSetSize];
    AddWinSet(thrId);
    p2->prevWin=nprev;
    if (nprev!=NULL) {
      p2->next=nprev->nextWin;
      nprev->nextWin=p2;
    }
    else {
      p2->next=nodep->posSearchPoint;
      nodep->posSearchPoint=p2;
    }
    p2->nextWin=NULL;
    p2->winMask=posPoint->winMask[suit];
    p2->orderSet=posPoint->winOrderSet[suit];
    p2->first=NULL;
    np=p2;          /* Latest winning node */
    suit++;

    /* Rest of path must be created */
    while (suit<4) {
      p2=&localVar[thrId].winCards[localVar[thrId].winSetSize];
      AddWinSet(thrId);
      np->nextWin=p2;
      p2->prevWin=np;
      p2->next=NULL;
      p2->winMask=posPoint->winMask[suit];
      p2->orderSet=posPoint->winOrderSet[suit];
      p2->first=NULL;
      p2->nextWin=NULL;
      np=p2;         /* Latest winning node */
      suit++;
    }

  /* All winning nodes in SOP have been traversed and new nodes created */
    p=&localVar[thrId].nodeCards[localVar[thrId].nodeSetSize];
    AddNodeSet(thrId);
    np->first=p;
    *result=TRUE;
    return p;
  }  
}


struct posSearchType * SearchLenAndInsert(struct posSearchType
	* rootp, DDS_LONGLONG key, int insertNode, int *result, int thrId) {
/* Search for node which matches with the suit length combination 
   given by parameter key. If no such node is found, NULL is 
  returned if parameter insertNode is FALSE, otherwise a new 
  node is inserted with suitLengths set to key, the pointer to
  this node is returned.
  The algorithm used is defined in Knuth "The art of computer
  programming", vol.3 "Sorting and searching", 6.2.2 Algorithm T,
  page 424. */
 
  struct posSearchType *np, *p, *sp;

  if (insertNode)
    sp=&localVar[thrId].posSearch[localVar[thrId].lenSetSize];
		
  np=rootp;
  while (1) {
    if (key==np->suitLengths) {
      *result=TRUE;	
      return np;
    }  
    else if (key < np->suitLengths) {
      if (np->left!=NULL)
        np=np->left;
      else if (insertNode) {
	p=sp;
	AddLenSet(thrId);
	np->left=p;
	p->posSearchPoint=NULL;
	p->suitLengths=key;
	p->left=NULL; p->right=NULL;
	*result=TRUE;
	return p;
      }
      else {
	*result=FALSE;
        return NULL;
      }	
    }  
    else {      /* key > suitLengths */
      if (np->right!=NULL)
        np=np->right;
      else if (insertNode) {
	p=sp;
	AddLenSet(thrId);
	np->right=p;
	p->posSearchPoint=NULL;
	p->suitLengths=key;
	p->left=NULL; p->right=NULL;
	*result=TRUE;
	return p;
      }
      else {
	*result=FALSE;
        return NULL;
      }	
    } 
  }
}



void BuildSOP(struct pos * posPoint, int tricks, int firstHand, int target,
  int depth, int scoreFlag, int score, int thrId) {
  int ss, hh, res, wm;
  unsigned short int w;
  unsigned short int temp[4][4];
  unsigned short int aggr[4];
  struct nodeCardsType * cardsP;
  struct posSearchType * np;
#ifdef TTDEBUG
  int k, mcurrent, rr;
  mcurrent=localVar[thrId].movePly[depth].current;
#endif

  for (ss=0; ss<=3; ss++) {
    w=posPoint->winRanks[depth][ss];
    if (w==0) {
      posPoint->winMask[ss]=0;
      posPoint->winOrderSet[ss]=0;
      posPoint->leastWin[ss]=0;
      for (hh=0; hh<=3; hh++)
        temp[hh][ss]=0;
    }
    else {
      w=w & (-w);       /* Only lowest win */
      for (hh=0; hh<=3; hh++)
	temp[hh][ss]=posPoint->rankInSuit[hh][ss] & (-w);

      aggr[ss]=0;
      for (hh=0; hh<=3; hh++)
	aggr[ss]=aggr[ss] | temp[hh][ss];
      posPoint->winMask[ss]=localVar[thrId].rel[aggr[ss]].winMask[ss];
      posPoint->winOrderSet[ss]=localVar[thrId].rel[aggr[ss]].aggrRanks[ss];
      wm=posPoint->winMask[ss];
      wm=wm & (-wm);
      posPoint->leastWin[ss]=InvWinMask(wm);
    }
  }

  /* 07-04-22 */
  if (scoreFlag) {
    if (localVar[thrId].nodeTypeStore[0]==MAXNODE) {
      posPoint->ubound=tricks+1;
      posPoint->lbound=target-posPoint->tricksMAX;
    }
    else {
      posPoint->ubound=tricks+1-target+posPoint->tricksMAX;
      posPoint->lbound=0;
    }
  }
  else {
    if (localVar[thrId].nodeTypeStore[0]==MAXNODE) {
      posPoint->ubound=target-posPoint->tricksMAX-1;
      posPoint->lbound=0;
    }
    else {
      posPoint->ubound=tricks+1;
      posPoint->lbound=tricks+1-target+posPoint->tricksMAX+1;
    }
  }	

  localVar[thrId].suitLengths=0; 
  for (ss=0; ss<=2; ss++)
    for (hh=0; hh<=3; hh++) {
      localVar[thrId].suitLengths=localVar[thrId].suitLengths<<4;
      localVar[thrId].suitLengths|=posPoint->length[hh][ss];
    }
  
  np=SearchLenAndInsert(localVar[thrId].rootnp[tricks][firstHand], 
	  localVar[thrId].suitLengths, TRUE, &res, thrId);
  
  cardsP=BuildPath(posPoint, np, &res, thrId);
  if (res) {
    cardsP->ubound=posPoint->ubound;
    cardsP->lbound=posPoint->lbound;
    if (((localVar[thrId].nodeTypeStore[firstHand]==MAXNODE)&&(scoreFlag))||
	((localVar[thrId].nodeTypeStore[firstHand]==MINNODE)&&(!scoreFlag))) {
      cardsP->bestMoveSuit=localVar[thrId].bestMove[depth].suit;
      cardsP->bestMoveRank=localVar[thrId].bestMove[depth].rank;
    }
    else {
      cardsP->bestMoveSuit=0;
      cardsP->bestMoveRank=0;
    }
    posPoint->bestMoveSuit=localVar[thrId].bestMove[depth].suit;
    posPoint->bestMoveRank=localVar[thrId].bestMove[depth].rank;
    for (ss=0; ss<=3; ss++) 
      cardsP->leastWin[ss]=posPoint->leastWin[ss];
  }
      		
  #ifdef STAT
    c9[depth]++;
  #endif  

  #ifdef TTDEBUG
  if ((res) && (ttCollect) && (!suppressTTlog)) {
    fprintf(localVar[thrId].fp7, "cardsP=%d\n", (int)cardsP);
    fprintf(localVar[thrId].fp7, "nodeSetSize=%d\n", localVar[thrId].nodeSetSize);
    fprintf(localVar[thrId].fp7, "ubound=%d\n", cardsP->ubound);
    fprintf(localVar[thrId].fp7, "lbound=%d\n", cardsP->lbound);
    fprintf(localVar[thrId].fp7, "target=%d\n", target);
    fprintf(localVar[thrId].fp7, "first=%c nextFirst=%c\n",
      cardHand[posPoint->first[depth]], cardHand[posPoint->first[depth-1]]);
    fprintf(localVar[thrId].fp7, "bestMove:  suit=%c rank=%c\n", cardSuit[localVar[thrId].bestMove[depth].suit],
      cardRank[localVar[thrId].bestMove[depth].rank]);
    fprintf(localVar[thrId].fp7, "\n");
    fprintf(localVar[thrId].fp7, "Last trick:\n");
    fprintf(localVar[thrId].fp7, "1st hand=%c\n", cardHand[posPoint->first[depth+3]]);
    for (k=3; k>=0; k--) {
      mcurrent=localVar[thrId].movePly[depth+k+1].current;
      fprintf(localVar[thrId].fp7, "suit=%c  rank=%c\n",
        cardSuit[localVar[thrId].movePly[depth+k+1].move[mcurrent].suit],
        cardRank[localVar[thrId].movePly[depth+k+1].move[mcurrent].rank]);
    }
    fprintf(localVar[thrId].fp7, "\n");
    for (hh=0; hh<=3; hh++) {
      fprintf(localVar[thrId].fp7, "hand=%c\n", cardHand[hh]);
      for (ss=0; ss<=3; ss++) {
	fprintf(localVar[thrId].fp7, "suit=%c", cardSuit[ss]);
	for (rr=14; rr>=2; rr--)
	  if (posPoint->rankInSuit[hh][ss] & bitMapRank[rr])
	    fprintf(localVar[thrId].fp7, " %c", cardRank[rr]);
	fprintf(localVar[thrId].fp7, "\n");
      }
      fprintf(localVar[thrId].fp7, "\n");
    }
    fprintf(localVar[thrId].fp7, "\n");
  }
  #endif  
}


int CheckDeal(struct moveType * cardp, int thrId) {
  int h, s, k, found;
  unsigned short int temp[4][4];

  for (h=0; h<=3; h++)
    for (s=0; s<=3; s++)
      temp[h][s]=localVar[thrId].game.suit[h][s];

  /* Check that all ranks appear only once within the same suit. */
  for (s=0; s<=3; s++)
    for (k=2; k<=14; k++) {
      found=FALSE;
      for (h=0; h<=3; h++) {
        if ((temp[h][s] & bitMapRank[k])!=0) {
          if (found) {
            cardp->suit=s;
            cardp->rank=k;
            return 1;
          }  
          else
            found=TRUE;
        }    
      }
    }

  return 0;
}


int NextMove(struct pos *posPoint, int depth, int thrId) {
  /* Returns TRUE if at least one move remains to be
  searched, otherwise FALSE is returned. */
  int mcurrent;
  unsigned short int lw;
  unsigned char suit;
  struct moveType currMove;
  
  mcurrent=localVar[thrId].movePly[depth].current;
  currMove=localVar[thrId].movePly[depth].move[mcurrent];

  if (localVar[thrId].lowestWin[depth][currMove.suit]==0) {
    /* A small card has not yet been identified for this suit. */
    lw=posPoint->winRanks[depth][currMove.suit];
    if (lw!=0)
      lw=lw & (-lw);  /* LSB */
    else
      lw=bitMapRank[15];
    if (bitMapRank[currMove.rank]<lw) {
       /* The current move has a small card. */
      localVar[thrId].lowestWin[depth][currMove.suit]=lw;
      while (localVar[thrId].movePly[depth].current<=(localVar[thrId].movePly[depth].last-1)) {
        localVar[thrId].movePly[depth].current++;
        mcurrent=localVar[thrId].movePly[depth].current;
        if (bitMapRank[localVar[thrId].movePly[depth].move[mcurrent].rank] >=
	    localVar[thrId].lowestWin[depth][localVar[thrId].movePly[depth].move[mcurrent].suit]) 
	  return TRUE;
      }
      return FALSE;
    }

    /* New: */
    else {
      while (localVar[thrId].movePly[depth].current<=localVar[thrId].movePly[depth].last-1) {
        localVar[thrId].movePly[depth].current++;
        mcurrent=localVar[thrId].movePly[depth].current;
        suit=localVar[thrId].movePly[depth].move[mcurrent].suit;
        if ((currMove.suit==suit) ||
	  (bitMapRank[localVar[thrId].movePly[depth].move[mcurrent].rank] >=
	    localVar[thrId].lowestWin[depth][suit]))
	  return TRUE;
      }
      return FALSE;
    }
  }
  else {
    while (localVar[thrId].movePly[depth].current<=localVar[thrId].movePly[depth].last-1) { 
      localVar[thrId].movePly[depth].current++;
      mcurrent=localVar[thrId].movePly[depth].current;
      if (bitMapRank[localVar[thrId].movePly[depth].move[mcurrent].rank] >=
	    localVar[thrId].lowestWin[depth][localVar[thrId].movePly[depth].move[mcurrent].suit])
	return TRUE;
    }
    return FALSE;
  }  
}


int DumpInput(int errCode, struct deal dl, int target,
    int solutions, int mode) {

  FILE *fp;
  int i, j, k;
  unsigned short ranks[4][4];

  fp=fopen("dump.txt", "w");
  if (fp==NULL)
    return -1;
  fprintf(fp, "Error code=%d\n", errCode);
  fprintf(fp, "\n");
  fprintf(fp, "Deal data:\n");
  if (dl.trump!=4)
    fprintf(fp, "trump=%c\n", cardSuit[dl.trump]);
  else
    fprintf(fp, "trump=N\n");
  fprintf(fp, "first=%c\n", cardHand[dl.first]);
  for (k=0; k<=2; k++)
    if (dl.currentTrickRank[k]!=0)
      fprintf(fp, "index=%d currentTrickSuit=%c currentTrickRank=%c\n",  
        k, cardSuit[dl.currentTrickSuit[k]], cardRank[dl.currentTrickRank[k]]);
  for (i=0; i<=3; i++)
	for (j=0; j<=3; j++) { 
      fprintf(fp, "index1=%d index2=%d remainCards=%d\n", 
        i, j, dl.remainCards[i][j]);
	  ranks[i][j]=dl.remainCards[i][3-j]>>2;
	}
  fprintf(fp, "\n");
  fprintf(fp, "target=%d\n", target);
  fprintf(fp, "solutions=%d\n", solutions);
  fprintf(fp, "mode=%d\n", mode);
  fprintf(fp, "\n");
  PrintDeal(fp, ranks);
  fclose(fp);
  return 0;
}

void PrintDeal(FILE *fp, unsigned short ranks[][4]) {
  int i, count, ec[4], trickCount=0, s, r;
  for (i=0; i<=3; i++) {
    count=counttable[ranks[3][i]];
    if (count>5)
      ec[i]=TRUE;
    else
      ec[i]=FALSE;
    trickCount=trickCount+count;
  }
  fprintf(fp, "\n");
  for (s=0; s<=3; s++) {
    fprintf(fp, "\t%c ", cardSuit[s]);
    if (!ranks[0][s])
      fprintf(fp, "--");
    else {
      for (r=14; r>=2; r--)
        if ((ranks[0][s] & bitMapRank[r])!=0)
          fprintf(fp, "%c", cardRank[r]);
    }
    fprintf(fp, "\n");
  }
  for (s=0; s<=3; s++) {
    fprintf(fp, "%c ", cardSuit[s]);
    if (!ranks[3][s])
      fprintf(fp, "--");
    else {
      for (r=14; r>=2; r--)
        if ((ranks[3][s] & bitMapRank[r])!=0)
          fprintf(fp, "%c", cardRank[r]);
    }
    if (ec[s])
      fprintf(fp, "\t\%c ", cardSuit[s]);
    else
      fprintf(fp, "\t\t\%c ", cardSuit[s]);
    if (!ranks[1][s])
      fprintf(fp, "--");
    else {
      for (r=14; r>=2; r--)
        if ((ranks[1][s] & bitMapRank[r])!=0)
            fprintf(fp, "%c", cardRank[r]);
    }
    fprintf(fp, "\n");
  }
  for (s=0; s<=3; s++) {
    fprintf(fp, "\t%c ", cardSuit[s]);
    if (!ranks[2][s])
      fprintf(fp, "--");
    else {
      for (r=14; r>=2; r--)
        if ((ranks[2][s] & bitMapRank[r])!=0)
          fprintf(fp, "%c", cardRank[r]);
    }
    fprintf(fp, "\n");
  }
  fprintf(fp, "\n");
  return;
}



void Wipe(int thrId) {
  int k;

  for (k=1; k<=localVar[thrId].wcount; k++) {
    if (localVar[thrId].pw[k])
      free(localVar[thrId].pw[k]);
    localVar[thrId].pw[k]=NULL;
  }
  for (k=1; k<=localVar[thrId].ncount; k++) {
    if (localVar[thrId].pn[k])
      free(localVar[thrId].pn[k]);
    localVar[thrId].pn[k]=NULL;
  }
  for (k=1; k<=localVar[thrId].lcount; k++) {
    if (localVar[thrId].pl[k])
      free(localVar[thrId].pl[k]);
    localVar[thrId].pl[k]=NULL;
  }
	
  localVar[thrId].allocmem=localVar[thrId].summem;

  return;
}


void AddWinSet(int thrId) {
  if (localVar[thrId].clearTTflag) {
    localVar[thrId].windex++;
    localVar[thrId].winSetSize=localVar[thrId].windex;
    /*localVar[thrId].fp2=fopen("dyn.txt", "a");
    fprintf(localVar[thrId].fp2, "windex=%d\n", windex);
    fclose(localVar[thrId].fp2);*/
    localVar[thrId].winCards=&localVar[thrId].temp_win[localVar[thrId].windex];
  }
  else if (localVar[thrId].winSetSize>=localVar[thrId].winSetSizeLimit) {
    /* The memory chunk for the winCards structure will be exceeded. */
    if ((localVar[thrId].allocmem+localVar[thrId].wmem)>localVar[thrId].maxmem) {
      /* Already allocated memory plus needed allocation overshot maxmem */
      localVar[thrId].windex++;
      localVar[thrId].winSetSize=localVar[thrId].windex;
      /*localVar[thrId].fp2=fopen("dyn.txt", "a");
      fprintf(localVar[thrId].fp2, "windex=%d\n", windex);
      fclose(localVar[thrId].fp2);*/
      localVar[thrId].clearTTflag=TRUE;
      localVar[thrId].winCards=&localVar[thrId].temp_win[localVar[thrId].windex];
    }
    else {
      localVar[thrId].wcount++; localVar[thrId].winSetSizeLimit=WSIZE; 
      localVar[thrId].pw[localVar[thrId].wcount] = 
		  (struct winCardType *)calloc(localVar[thrId].winSetSizeLimit+1, sizeof(struct winCardType));
      if (localVar[thrId].pw[localVar[thrId].wcount]==NULL) {
        localVar[thrId].clearTTflag=TRUE;
        localVar[thrId].windex++;
	localVar[thrId].winSetSize=localVar[thrId].windex;
	localVar[thrId].winCards=&localVar[thrId].temp_win[localVar[thrId].windex];
      }
      else {
	localVar[thrId].allocmem+=(localVar[thrId].winSetSizeLimit+1)*sizeof(struct winCardType);
	localVar[thrId].winSetSize=0;
	localVar[thrId].winCards=localVar[thrId].pw[localVar[thrId].wcount];
      }
    }
  }
  else
    localVar[thrId].winSetSize++;
  return;
}

void AddNodeSet(int thrId) {
  if (localVar[thrId].nodeSetSize>=localVar[thrId].nodeSetSizeLimit) {
    /* The memory chunk for the nodeCards structure will be exceeded. */
    if ((localVar[thrId].allocmem+localVar[thrId].nmem)>localVar[thrId].maxmem) {
      /* Already allocated memory plus needed allocation overshot maxmem */  
      localVar[thrId].clearTTflag=TRUE;
    }
    else {
      localVar[thrId].ncount++; localVar[thrId].nodeSetSizeLimit=NSIZE; 
      localVar[thrId].pn[localVar[thrId].ncount] = (struct nodeCardsType *)calloc(localVar[thrId].nodeSetSizeLimit+1, sizeof(struct nodeCardsType));
      if (localVar[thrId].pn[localVar[thrId].ncount]==NULL) {
        localVar[thrId].clearTTflag=TRUE;
      }
      else {
	localVar[thrId].allocmem+=(localVar[thrId].nodeSetSizeLimit+1)*sizeof(struct nodeCardsType);
	localVar[thrId].nodeSetSize=0;
	localVar[thrId].nodeCards=localVar[thrId].pn[localVar[thrId].ncount];
      }
    }
  }
  else
    localVar[thrId].nodeSetSize++;
  return;
}

void AddLenSet(int thrId) {
  if (localVar[thrId].lenSetSize>=localVar[thrId].lenSetSizeLimit) {
      /* The memory chunk for the posSearchType structure will be exceeded. */
    if ((localVar[thrId].allocmem+localVar[thrId].lmem)>localVar[thrId].maxmem) { 
       /* Already allocated memory plus needed allocation overshot maxmem */
      localVar[thrId].clearTTflag=TRUE;
    }
    else {
      localVar[thrId].lcount++; localVar[thrId].lenSetSizeLimit=LSIZE; 
      localVar[thrId].pl[localVar[thrId].lcount] = (struct posSearchType *)calloc(localVar[thrId].lenSetSizeLimit+1, sizeof(struct posSearchType));
      if (localVar[thrId].pl[localVar[thrId].lcount]==NULL) {
        localVar[thrId].clearTTflag=TRUE;
      }
      else {
	localVar[thrId].allocmem+=(localVar[thrId].lenSetSizeLimit+1)*sizeof(struct posSearchType);
	localVar[thrId].lenSetSize=0;
	localVar[thrId].posSearch=localVar[thrId].pl[localVar[thrId].lcount];
      }
    }
  }
  else
    localVar[thrId].lenSetSize++;
  return;
} 



#ifdef TTDEBUG

void ReceiveTTstore(struct pos *posPoint, struct nodeCardsType * cardsP, 
  int target, int depth, int thrId) {
  int tricksLeft, hh, ss, rr;
/* Stores current position information and TT position value in table
  ttStore with current entry lastTTStore. Also stores corresponding
  information in log rectt.txt. */
  tricksLeft=0;
  for (hh=0; hh<=3; hh++)
    for (ss=0; ss<=3; ss++)
      tricksLeft+=posPoint->length[hh][ss];
  tricksLeft=tricksLeft/4;
  ttStore[lastTTstore].tricksLeft=tricksLeft;
  ttStore[lastTTstore].cardsP=cardsP;
  ttStore[lastTTstore].first=posPoint->first[depth];
  if ((localVar[thrId].handToPlay==posPoint->first[depth])||
    (localVar[thrId].handToPlay==partner[posPoint->first[depth]])) {
    ttStore[lastTTstore].target=target-posPoint->tricksMAX;
    ttStore[lastTTstore].ubound=cardsP->ubound;
    ttStore[lastTTstore].lbound=cardsP->lbound;
  }
  else {
    ttStore[lastTTstore].target=tricksLeft-
      target+posPoint->tricksMAX+1;
  }
  for (hh=0; hh<=3; hh++)
    for (ss=0; ss<=3; ss++)
      ttStore[lastTTstore].suit[hh][ss]=
        posPoint->rankInSuit[hh][ss];
  localVar[thrId].fp11=fopen("rectt.txt", "a");
  if (lastTTstore<SEARCHSIZE) {
    fprintf(localVar[thrId].fp11, "lastTTstore=%d\n", lastTTstore);
    fprintf(localVar[thrId].fp11, "tricksMAX=%d\n", posPoint->tricksMAX);
    fprintf(localVar[thrId].fp11, "leftTricks=%d\n",
      ttStore[lastTTstore].tricksLeft);
    fprintf(localVar[thrId].fp11, "cardsP=%d\n",
      ttStore[lastTTstore].cardsP);
    fprintf(localVar[thrId].fp11, "ubound=%d\n",
      ttStore[lastTTstore].ubound);
    fprintf(localVar[thrId].fp11, "lbound=%d\n",
      ttStore[lastTTstore].lbound);
    fprintf(localVar[thrId].fp11, "first=%c\n",
      cardHand[ttStore[lastTTstore].first]);
    fprintf(localVar[thrId].fp11, "target=%d\n",
      ttStore[lastTTstore].target);
    fprintf(localVar[thrId].fp11, "\n");
    for (hh=0; hh<=3; hh++) {
      fprintf(localVar[thrId].fp11, "hand=%c\n", cardHand[hh]);
      for (ss=0; ss<=3; ss++) {
        fprintf(localVar[thrId].fp11, "suit=%c", cardSuit[ss]);
        for (rr=14; rr>=2; rr--)
          if (ttStore[lastTTstore].suit[hh][ss]
            & bitMapRank[rr])
            fprintf(localVar[thrId].fp11, " %c", cardRank[rr]);
         fprintf(localVar[thrId].fp11, "\n");
      }
      fprintf(localVar[thrId].fp11, "\n");
    }
  }
  fclose(localVar[thrId].fp11);
  lastTTstore++;
}
#endif

#if defined(_MSC_VER)
HANDLE solveAllEvents[MAXNOOFTHREADS];
struct paramType param;
LONG volatile threadIndex;
LONG volatile current;
int timeOut;

const long chunk = 4;

DWORD CALLBACK SolveChunkDDtable (void *) {
  struct futureTricks *futp[MAXNOOFBOARDS];
  struct futureTricks fut[MAXNOOFBOARDS];
  int thid;
  long j;
  clock_t tstop;

  EnterCriticalSection(&solv_crit);
  __try
  {
    threadIndex++;
	thid=threadIndex;
  }
  __finally 
  {
    LeaveCriticalSection(&solv_crit);
  }

  while ((j=_InterlockedExchangeAdd(&current, chunk))<param.noOfBoards) {

    for (int k=0; k<chunk && j+k<param.noOfBoards; k++) {
      if ((param.remainTime!=-1)&&(param.solvedp->noOfBoards!=0)) {
        tstop=clock();
        if (((int)tstop - param.tstart) > param.remainTime) {
	  timeOut=TRUE;
	  break;
        }
      }

      futp[j+k]=&fut[j+k];
      int res=SolveBoard(param.bop->deals[j+k], param.bop->target[j+k],
	  param.bop->solutions[j+k], param.bop->mode[j+k], futp[j+k], thid);
      if (res==1) {
	param.solvedp->solvedBoard[j+k]=fut[j+k];
      }
      else {
	return 0;
      }
    }
  }

  if (SetEvent(solveAllEvents[thid])==0) {
    int errCode=GetLastError();
    return 0;
  }
  
  return 1;

}

int SolveAllBoards4(struct boards *bop, struct solvedBoards *solvedp,
  int remainTime) {
  int k, errCode;
  DWORD res;
  DWORD solveAllWaitResult;

  current=0;

  threadIndex=-1;
  
  timeOut=FALSE;

  if (bop->noOfBoards > MAXNOOFBOARDS)
    return -4;
  
  (int)param.tstart=clock(); param.remainTime=remainTime;
  for (k=0; k<noOfThreads; k++) {
    solveAllEvents[k]=CreateEvent(NULL, FALSE, FALSE, 0);
    if (solveAllEvents[k]==0) {
      errCode=GetLastError();
      return -1;
    }
  }
  
  param.bop=bop; param.solvedp=solvedp; param.noOfBoards=bop->noOfBoards;

  for (k=0; k<MAXNOOFBOARDS; k++)
    solvedp->solvedBoard[k].cards=0;

  for (k=0; k<noOfThreads; k++) {
    res=QueueUserWorkItem(SolveChunkDDtable, NULL, WT_EXECUTELONGFUNCTION);
    if (res==0) { 
      errCode=GetLastError();
      return -2;
    }
  }

  solveAllWaitResult = WaitForMultipleObjects(noOfThreads, 
	  solveAllEvents, TRUE, INFINITE);
  if (solveAllWaitResult!=WAIT_OBJECT_0) {
    errCode=GetLastError();
    return -3;
  }

  for (k=0; k<noOfThreads; k++) {
    CloseHandle(solveAllEvents[k]);
  }

  /* Calculate number of solved boards. */

  solvedp->noOfBoards=0;
  for (k=0; k<MAXNOOFBOARDS; k++) {
    if (solvedp->solvedBoard[k].cards!=0)
      solvedp->noOfBoards++;
  }

  if (timeOut)
    solvedp->timeOut=TRUE;
  else
    solvedp->timeOut=FALSE;
    
  return 1;
}

int STDCALL CalcDDtable(struct ddTableDeal tableDeal, struct ddTableResults * tablep) {

  int h, s, k, ind, tr, first, res;
  struct deal dl;
  struct boards bo;
  struct solvedBoards solved;

  for (h=0; h<=3; h++)
    for (s=0; s<=3; s++)
      dl.remainCards[h][s]=tableDeal.cards[h][s];

  for (k=0; k<=2; k++) {
    dl.currentTrickRank[k]=0;
    dl.currentTrickSuit[k]=0;
  }

  ind=0; bo.noOfBoards=20;

  for (tr=4; tr>=0; tr--) 
    for (first=0; first<=3; first++) {
      dl.first=first;
      dl.trump=tr;
      bo.deals[ind]=dl;
      bo.target[ind]=-1;
      bo.solutions[ind]=1;
      bo.mode[ind]=1;
      ind++;
    }

  res=SolveAllBoards4(&bo, &solved, -1);
  if (res==1) {
    for (ind=0; ind<20; ind++) {
      tablep->resTable[bo.deals[ind].trump][rho[bo.deals[ind].first]]=
	    13-solved.solvedBoard[ind].score[0];
    }
    return 1;
  }

  return res;
}
#endif



