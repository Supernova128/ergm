/*  File src/MHProposals.c in package ergm, part of the Statnet suite
 *  of packages for network analysis, http://statnet.org .
 *
 *  This software is distributed under the GPL-3 license.  It is free,
 *  open source, and has the attribution requirements (GPL Section 7) at
 *  http://statnet.org/attribution
 *
 *  Copyright 2003-2017 Statnet Commons
 */
#include "MHproposals.h"
#include "ergm_edgelist.h"
#include "ergm_rlebdm.h"
#include "ergm_changestat.h"

/* Shorthand. */
#define Mtail (MHp->toggletail)
#define Mhead (MHp->togglehead)

/*********************
 void MH_randomtoggle

 Default MH algorithm
*********************/
void MH_randomtoggle (MHProposal *MHp, Network *nwp)  {  

  /* *** don't forget tail-> head now */

  if(MHp->ntoggles == 0) { /* Initialize randomtoggle */
    MHp->ntoggles=1;
    return;
  }
  
  BD_LOOP({
      GetRandDyad(Mtail, Mhead, nwp);
    });
}

/********************
   void MH_TNT
   Tie/no tie:  Gives at least 50% chance of
   proposing a toggle of an existing edge, as opposed
   to simple random toggles that rarely do so in sparse 
   networks
***********************/
void MH_TNT (MHProposal *MHp, Network *nwp) 
{
  /* *** don't forget tail-> head now */
  
  Edge nedges=nwp->nedges;
  static double comp=0.5;
  static double odds;
  static Dyad ndyads;
  
  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=1;
    odds = comp/(1.0-comp);
    ndyads = DYADCOUNT(nwp->nnodes, nwp->bipartite, nwp->directed_flag);
    return;
  }

  double logratio=0;
  BD_LOOP({
      if (unif_rand() < comp && nedges > 0) { /* Select a tie at random */
	GetRandEdge(Mtail, Mhead, nwp);
	/* Thanks to Robert Goudie for pointing out an error in the previous 
	   version of this sampler when proposing to go from nedges==0 to nedges==1 
	   or vice versa.  Note that this happens extremely rarely unless the 
	   network is small or the parameter values lead to extremely sparse 
	   networks.  */
	logratio = log((nedges==1 ? 1.0/(comp*ndyads + (1.0-comp)) :
			      nedges / (odds*ndyads + nedges)));
      }else{ /* Select a dyad at random */
	GetRandDyad(Mtail, Mhead, nwp);
	
	if(EdgetreeSearch(Mtail[0],Mhead[0],nwp->outedges)!=0){
	  logratio = log((nedges==1 ? 1.0/(comp*ndyads + (1.0-comp)) :
				nedges / (odds*ndyads + nedges)));
	}else{
	  logratio = log((nedges==0 ? comp*ndyads + (1.0-comp) :
				1.0 + (odds*ndyads)/(nedges + 1)));
	}
      }
    });
  MHp->logratio += logratio;
}

/********************
   void MH_TNT10
   Attempts to do 10 TNT steps at once, but this seems flawed currently
   because it does not correctly update network quantities like nedges
   after each of the 10 proposed toggles.
***********************/
void MH_TNT10 (MHProposal *MHp, Network *nwp) 
{
  /* *** don't forget tail-> head now */
  
  Edge nedges=nwp->nedges;
  static double comp=0.5;
  static double odds;
  static Dyad ndyads;
  
  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=10;
    odds = comp/(1.0-comp);
    ndyads = DYADCOUNT(nwp->nnodes, nwp->bipartite, nwp->directed_flag);
    return;
  }
  
  double logratio;
  BD_LOOP({
      logratio = 0;
      for(unsigned int n = 0; n < 10; n++){
	if (unif_rand() < comp && nedges > 0) { /* Select a tie at random */
	  GetRandEdge(Mtail, Mhead, nwp);
	  logratio += log(nedges  / (odds*ndyads + nedges));
	}else{ /* Select a dyad at random */
	  GetRandDyad(Mtail+n, Mhead+n, nwp);
	  if(EdgetreeSearch(Mtail[n],Mhead[n],nwp->outedges)!=0){
	    logratio += log((nedges==1 ? 1.0/(comp*ndyads + (1.0-comp)) :
				  nedges / (odds*ndyads + nedges)));
	  }else{
	    logratio += log((nedges==0 ? comp*ndyads + (1.0-comp) :
				  1.0 + (odds*ndyads)/(nedges + 1)));
	  }
	} 
      }
    });
  MHp->logratio += logratio;
}

/*********************
 void MH_constantedges
 propose pairs of toggles that keep number of edges
 the same.  This is done by (a) choosing an existing edge
 at random; (b) repeatedly choosing dyads at random until 
 one is found that does not have an edge; and (c) proposing
 toggling both these dyads.  Note that step (b) will be very 
 inefficient if the network is nearly complete, so this proposal is
 NOT recommended for such networks.  However, most network
 datasets are sparse, so this is not likely to be an issue.
*********************/
void MH_ConstantEdges (MHProposal *MHp, Network *nwp)  {  
  /* *** don't forget tail-> head now */
  
  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=2;    
    return;
  }
  /* Note:  This proposal cannot be used for full or empty observed graphs.
     If desired, we could check for this at initialization phase. 
     (For now, however, no way to easily return an error message and stop.)*/
  BD_LOOP({
      /* First, select edge at random */
      GetRandEdge(Mtail, Mhead, nwp);
      /* Second, select dyad at random until it has no edge */
      do{
	GetRandDyad(Mtail+1, Mhead+1, nwp);
      }while(EdgetreeSearch(Mtail[1], Mhead[1], nwp->outedges) != 0);
    });
}

/*********************
 void MH_CondDegreeDist
 It used to be called  MH_CondDegDistSwapToggles
*********************/
void MH_CondDegreeDist (MHProposal *MHp, Network *nwp) {  
  int noutedge=0, ninedge=0, k, fvalid;
  int k0, j0, j1, k1;
  int j0h, j1h;
  int trynode;
  Vertex e, alter, tail=0, head, head1;
  
  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=2;    
    return;
  }

  fvalid = 0;
  trynode = 0;
  while(fvalid==0 && trynode < 500){

  trynode++;
  /* select a node at random */
  while(noutedge+ninedge==0){
    /* select a node at random */
    tail = 1 + unif_rand() * nwp->nnodes;
    ninedge  = nwp->indegree[tail];
    noutedge = nwp->outdegree[tail];
  }

  /* choose a edge of the node at random */
    /* *** don't forget tail-> head now */

  k0 = (int)(unif_rand() * (noutedge+ninedge)); 
  if (k0 < noutedge){
    k=0;
    for(e = EdgetreeMinimum(nwp->outedges, tail);
    ((head = nwp->outedges[e].value) != 0 && k<k0);
    e = EdgetreeSuccessor(nwp->outedges, e)){++k;}
  }else{
    k=0;
    for(e = EdgetreeMinimum(nwp->inedges, tail);
    ((head = nwp->inedges[e].value) != 0 && k<(k0-noutedge));
    e = EdgetreeSuccessor(nwp->inedges, e)){++k;}
  }

  if ( (!nwp->directed_flag && tail > head) ||
  (nwp->directed_flag && k0 >= noutedge) ) {
    Mtail[0] = head;
    Mhead[0] = tail;
  }else{
    Mtail[0] = tail;
    Mhead[0] = head;
  }
  
  k1=0;
  fvalid=0;
  while(fvalid==0 && k1 < 100){
    while((alter = 1 + unif_rand() * nwp->nnodes) == tail);
    fvalid=1;
    if(alter == head){fvalid=0;}
    if (k0 < noutedge || !nwp->directed_flag){
      for(e = EdgetreeMinimum(nwp->outedges, tail);
      (fvalid==1 && ((head1 = nwp->outedges[e].value) != 0));
      e = EdgetreeSuccessor(nwp->outedges, e)){
        if(alter==head1){fvalid=0;}}
    }
    if (k0 >= noutedge || !nwp->directed_flag){
      for(e = EdgetreeMinimum(nwp->inedges, tail);
      (fvalid==1 && ((head1 = nwp->inedges[e].value) != 0));
      e = EdgetreeSuccessor(nwp->inedges, e)){
        if(alter==head1){fvalid=0;}}
    }
    k1++;
  }

  if (k1 == 100){
    fvalid=0;
    continue;
  }
  
  if ( (!nwp->directed_flag && alter > tail) ||
       (nwp->directed_flag && k0 < noutedge) )
    {
      Mtail[1] = tail;
      Mhead[1] = alter;
    }else{
      Mtail[1] = alter;
      Mhead[1] = tail;
    }
  
  if (!nwp->directed_flag){
    /* Check undirected degrees */
    k0 =nwp->outdegree[tail]  + nwp->indegree[tail];
    j0h=nwp->outdegree[head]  + nwp->indegree[head];
    j1h=nwp->outdegree[alter] + nwp->indegree[alter];
    
    j0=j0h-1;
    j1=j1h+1;
    
    if( ( (j0==j1h) && (j1==j0h) ) ){
      fvalid = 1;
    }else{
      fvalid = 0;
    }
  }else{
    /* Check directed degrees */
   if(k0 < noutedge){
     /* Check indegrees */
     j0h=nwp->indegree[head];
     j1h=nwp->indegree[alter];
   }else{
     /* Check outdegrees */
     j0h=nwp->outdegree[head];
     j1h=nwp->outdegree[alter];
   }
   j0=j0h-1;
   j1=j1h+1;
   
   if( ( (j0==j1h) && (j1==j0h) ) ){
     fvalid = 1;
   }else{
     fvalid = 0;
   }
  }
  
  }

  if (trynode==500){
    Mtail[1] = Mtail[0];
    Mhead[1] = Mhead[0];
  }
}

/*********************
 void MH_CondOutDegreeDist
*********************/
void MH_CondOutDegreeDist (MHProposal *MHp, Network *nwp) {  
  int noutedge=0, k, fvalid=0;
  int k0, k1;
  int trynode;
  Vertex e, alter, tail=0, head, head1;
  
  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=2;    
    return;
  }

  fvalid = 0;
  trynode = 0;
  while(fvalid==0 && trynode < 1500){

  trynode++;

  while(noutedge==0){
    /* select a node at random */
    tail = 1 + unif_rand() * nwp->nnodes;
    noutedge = nwp->outdegree[tail];
  }
  
  k0 = (int)(unif_rand() * noutedge); 
  k=0;
  for(e = EdgetreeMinimum(nwp->outedges, tail);
      ((head = nwp->outedges[e].value) != 0 && k<k0);
      e = EdgetreeSuccessor(nwp->outedges, e)){++k;}
  Mtail[0] = tail;
  Mhead[0] = head;
  
  k1=0;
  fvalid=0;
  while(fvalid==0 && k1 < 100){
    while((alter = 1 + unif_rand() * nwp->nnodes) == tail);
    fvalid=1;
    if(alter == head){fvalid=0;}
    for(e = EdgetreeMinimum(nwp->outedges, tail);
	(fvalid==1 && ((head1 = nwp->outedges[e].value) != 0));
	e = EdgetreeSuccessor(nwp->outedges, e)){
      if(alter==head1){fvalid=0;}}
    k1++;
  }
  if (k1 == 100){
    fvalid=0;
    continue;
  }
  
  Mtail[1] = tail;
  Mhead[1] = alter;
  }
  
  if(trynode==1500 || !CheckTogglesValid(MHp, nwp)){
      Mtail[0] = 1;
      Mhead[0] = 2;
      Mtail[1] = 1;
      Mhead[1] = 2;
  }
  

}

/*********************
 void MH_CondInDegreeDist
*********************/
void MH_CondInDegreeDist (MHProposal *MHp, Network *nwp) {  
  int ninedge=0, k, fvalid=0;
  int k0, k1;
  int trynode;
  Vertex e, alter, tail=0, head, head1;

  /* *** don't forget tail-> head now */

  
  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=2;    
    return;
  }

  fvalid = 0;
  trynode = 0;
  while(fvalid==0 && trynode < 1500){

  trynode++;

  while(ninedge==0){
    /* select a node at random */
    tail = 1 + unif_rand() * nwp->nnodes;
    ninedge = nwp->indegree[tail];
  }
  
  k0 = (int)(unif_rand() * ninedge); 
  k=0;
  for(e = EdgetreeMinimum(nwp->inedges, tail);
      ((head = nwp->inedges[e].value) != 0 && k<k0);
      e = EdgetreeSuccessor(nwp->inedges, e)){++k;}
  Mtail[0] = head;
  Mhead[0] = tail;
  
  k1=0;
  fvalid=0;
  while(fvalid==0 && k1 < 100){
    while((alter = 1 + unif_rand() * nwp->nnodes) == tail);
    fvalid=1;
    if(alter == head){fvalid=0;}
    for(e = EdgetreeMinimum(nwp->inedges, tail);
	(fvalid==1 && ((head1 = nwp->inedges[e].value) != 0));
	e = EdgetreeSuccessor(nwp->inedges, e)){
      if(alter==head1){fvalid=0;}}
    k1++;
  }
  if (k1 == 100){
    fvalid=0;
    continue;
  }
  
  Mtail[1] = alter;
  Mhead[1] = tail;
  
  }
  
  if(trynode==1500){
      Mtail[0] = 1;
      Mhead[0] = 2;
      Mtail[1] = 1;
      Mhead[1] = 2;
  }
}

/*********************
 void MH_TwoRandomToggles
*********************/
void MH_TwoRandomToggles (MHProposal *MHp, Network *nwp) {  
  Vertex tail, head;
  int i;

  /* *** don't forget tail-> head now */
  
  if(MHp->ntoggles == 0) { /* Initialize OneRandomToggle */
    MHp->ntoggles=2;
    return;
  }

  for (i = 0; i < 2; i++){
   tail = 1 + unif_rand() * nwp->nnodes;
   while ((head = 1 + unif_rand() * nwp->nnodes) == tail);
   if (!nwp->directed_flag && tail > head) {
     Mtail[i] = head;
     Mhead[i] = tail;
   }else{
     Mtail[i] = tail;
     Mhead[i] = head;
   }
  }
}

/*********************
 void MH_RandomNode
*********************/
void MH_randomnode (MHProposal *MHp, Network *nwp) {
  
  Vertex root, alter;
  int j;
  
  if(MHp->ntoggles == 0) { /* Initialize OneRandomToggle */
    MHp->ntoggles= nwp->nnodes - 1;
    return;
  }

  root = 1 + unif_rand() * nwp->nnodes;
  
  j = 0;
  for (alter = 1; alter <= nwp->nnodes; alter++)
    {
      /* there is never an edge (root, root) */
      if (alter != root) {
       if (!nwp->directed_flag && root > alter) {
        Mtail[j] = alter;
        Mhead[j] = root;
       }else{
        Mtail[j] = root;
        Mhead[j] = alter;
       }
       j++;
      }
    }
}

/********************
   void MH_randomtoggleList
   Propose ONLY edges on a static list
***********************/
void MH_randomtoggleList (MHProposal *MHp, Network *nwp) 
{  
  static Edge nedges0;

  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=1;
    nedges0 = MHp->inputs[0];
    return;
  }
  
  if(nedges0==0){ /* Attempting dissolution on a complete graph. */
    Mtail[0]=MH_FAILED;
    Mhead[0]=MH_IMPOSSIBLE;
    return;
  }

  BD_LOOP({
      /* Select a dyad at random that is in the reference graph. (We
	 have a convenient sampling frame.) */
      /* Generate. */
      Edge rane = 1 + unif_rand() * nedges0;
      Mtail[0]=MHp->inputs[rane];
      Mhead[0]=MHp->inputs[nedges0+rane];
    });
}


/********************
   void MH_randomtoggleRLE
   Propose ONLY edges on an RLE-compressed list
***********************/
void MH_RLE (MHProposal *MHp, Network *nwp) 
{  
  static RLEBDM1D r;

  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=1;
    double *inputs = MHp->inputs;
    r = unpack_RLEBDM1D(&inputs, nwp->nnodes);
    return;
  }
  
  if(r.ndyads==0){ /* No dyads to toggle. */
    Mtail[0]=MH_FAILED;
    Mhead[0]=MH_IMPOSSIBLE;
    return;
  }

  BD_LOOP({
      /* Select a dyad at random that is in the reference graph. (We
	 have a convenient sampling frame.) */
      /* Generate. */
      GetRandRLEBDM1D_RS(Mtail, Mhead, &r);
    });
}


/********************
   void MH_listTNT
   Propose ONLY edges on a static list
   Use TNT weights.
   This is a fusion of MH_DissolutionMLETNT and MH_TNT:

   A "discord" network is constructed that is the intersection of
   dyads on the static list and the edges present in nwp. Then,
   standard TNT procedure is followed, but the dyad space (and the
   number of dyads) is the number of dyads in the static list and the
   network for the ties is the ties in the discord network.
***********************/
void MH_listTNT (MHProposal *MHp, Network *nwp) 
{
  static Vertex nnodes;
  static double comp=0.5, odds;
  static Dyad ndyads;

  Network *discord;
  
  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=1;
    nnodes = nwp->nnodes;
    odds = comp/(1.0-comp);

    ndyads = MHp->inputs[0]; // Note that ndyads here is the number of dyads in the list.
    MHp->discord = (Network**) Calloc(2, Network*); // A space for the sentinel NULL pointer.
    MHp->discord[0] = discord = NetworkInitialize(NULL, NULL, 0, nnodes, nwp->directed_flag, nwp->bipartite, 0, 0, NULL);
    
    // Network containing edges that are present in the network AND are on the toggleable list.

   
    for(Edge i=0; i<ndyads; i++){
      Vertex tail=MHp->inputs[1+i], head=MHp->inputs[1+ndyads+i];
      if(EdgetreeSearch(tail, head,nwp->outedges)!=0)
	ToggleEdge(tail,head, discord);
    }

    return;
  }

  discord = MHp->discord[0];
  
  Edge nedges=discord->nedges;
  double logratio=0;
  BD_LOOP({
      if (unif_rand() < comp && nedges > 0) { /* Select a tie at random from the network of eligibles */
	GetRandEdge(Mtail, Mhead, discord);
	/* Thanks to Robert Goudie for pointing out an error in the previous 
	   version of this sampler when proposing to go from nedges==0 to nedges==1 
	   or vice versa.  Note that this happens extremely rarely unless the 
	   network is small or the parameter values lead to extremely sparse 
	   networks.  */
	logratio = log((nedges==1 ? 1.0/(comp*ndyads + (1.0-comp)) :
			      nedges / (odds*ndyads + nedges)));
      }else{ /* Select a dyad at random from the list */
	Edge rane = 1 + unif_rand() * ndyads;
	Mtail[0]=MHp->inputs[rane];
	Mhead[0]=MHp->inputs[ndyads+rane];
	
	if(EdgetreeSearch(Mtail[0],Mhead[0],discord->outedges)!=0){
	  logratio = log((nedges==1 ? 1.0/(comp*ndyads + (1.0-comp)) :
				nedges / (odds*ndyads + nedges)));
	}else{
	  logratio = log((nedges==0 ? comp*ndyads + (1.0-comp) :
				1.0 + (odds*ndyads)/(nedges + 1)));
	}
      }
    });
  MHp->logratio += logratio;
}

/********************
   void MH_RLETNT
   Propose ONLY edges on a static list
   Use TNT weights.
   This is a fusion of MH_DissolutionMLETNT and MH_TNT:

   A "discord" network is constructed that is the intersection of
   dyads on the static list and the edges present in nwp. Then,
   standard TNT procedure is followed, but the dyad space (and the
   number of dyads) is the number of dyads in the static list and the
   network for the ties is the ties in the discord network.
***********************/
void MH_RLETNT (MHProposal *MHp, Network *nwp) 
{
  static RLEBDM1D r;

  static Vertex nnodes;
  static double comp=0.5, odds;

  Network *discord;

  if(MHp->ntoggles == 0) { /* Initialize */
    MHp->ntoggles=1;
    nnodes = nwp->nnodes;
    odds = comp/(1.0-comp);

    double *inputs = MHp->inputs;
    r = unpack_RLEBDM1D(&inputs, nwp->nnodes);

    MHp->discord = (Network**) Calloc(2, Network*); // A space for the sentinel NULL pointer.
    MHp->discord[0] = discord = NetworkInitialize(NULL, NULL, 0, nnodes, nwp->directed_flag, nwp->bipartite, 0, 0, NULL);
;
    
    // Network containing edges that are present in the network AND are on the toggleable list.

    for(Vertex tail=1; tail<=nwp->nnodes; tail++){
      Vertex head;
      Edge e;
      STEP_THROUGH_OUTEDGES(tail, e, head){
	if(GetRLEBDM1D(tail, head, &r)){
	  ToggleEdge(tail,head, discord);
	}
      }
    }

    if(discord->nedges==nwp->nedges){ // There are no ties in the initial network that are fixed.
      NetworkDestroy(discord);
      Free(MHp->discord);
      MHp->discord = NULL;
    }
    return;
  }

  Network *nwp1 = MHp->discord? MHp->discord[0] : nwp;
  Edge nedges= nwp1->nedges;
  double logratio=0;
  BD_LOOP({
      if (unif_rand() < comp && nedges > 0) { /* Select a tie at random from the network of eligibles */
	GetRandEdge(Mtail, Mhead, nwp1);
	/* Thanks to Robert Goudie for pointing out an error in the previous 
	   version of this sampler when proposing to go from nedges==0 to nedges==1 
	   or vice versa.  Note that this happens extremely rarely unless the 
	   network is small or the parameter values lead to extremely sparse 
	   networks.  */
	logratio = log((nedges==1 ? 1.0/(comp*r.ndyads + (1.0-comp)) :
			      nedges / (odds*r.ndyads + nedges)));
      }else{ /* Select a dyad at random from the list */
	GetRandRLEBDM1D_RS(Mtail, Mhead, &r);
	
	if(EdgetreeSearch(Mtail[0],Mhead[0],nwp1->outedges)!=0){
	  logratio = log((nedges==1 ? 1.0/(comp*r.ndyads + (1.0-comp)) :
				nedges / (odds*r.ndyads + nedges)));
	}else{
	  logratio = log((nedges==0 ? comp*r.ndyads + (1.0-comp) :
				1.0 + (odds*r.ndyads)/(nedges + 1)));
	}
      }
    });
  MHp->logratio += logratio;
}


/* The ones below have not been tested */

/*********************
 void MH_ConstrainedCondOutDegDist
*********************/
void MH_ConstrainedCondOutDegDist (MHProposal *MHp, Network *nwp){  
  int noutedge=0, k, fvalid=0;
  int k0, k1;
  Vertex e, alter, tail, head, head1;

  /* *** don't forget tail-> head now */

  while(noutedge==0){
    /* select a node at random */
    tail = 1 + unif_rand() * nwp->nnodes;
    noutedge = nwp->outdegree[tail];
  }
  
  k0 = (int)(unif_rand() * noutedge); 
  k=0;
  for(e = EdgetreeMinimum(nwp->outedges, tail);
      ((head = nwp->outedges[e].value) != 0 && k<k0);
      e = EdgetreeSuccessor(nwp->outedges, e)){++k;}
  Mtail[0] = tail;
  Mhead[0] = head;
  
  k1=0;
  fvalid=0;
  while(fvalid==0 && k1 < 100){
    while((alter = 1 + unif_rand() * nwp->nnodes) == tail);
    fvalid=1;
    if(alter == head){fvalid=0;}
    for(e = EdgetreeMinimum(nwp->outedges, tail);
	(fvalid==1 && ((head1 = nwp->outedges[e].value) != 0));
	e = EdgetreeSuccessor(nwp->outedges, e)){
      if(alter==head1){fvalid=0;}}
    k1++;
  }
  if (k1 == 100){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  
  Mtail[1] = tail;
  Mhead[1] = alter;
  
  if (!fvalid){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  
  for(k=0; k < 2; k++){
    if (dEdgeListSearch(Mtail[k], Mhead[k], MHp->inputs)==0){
      Mtail[0] = Mhead[0] = 0;
      Mtail[1] = Mhead[1] = 0;
    }
  }
}


void MH_NodePairedTiesToggles (MHProposal *MHp, Network *nwp) {  
  /* chooses a node and toggles all ties and
	 and toggles an equal number of matching nonties
	 for that node */
  int nedge=0,j,k;
  int fvalid = 1;
  Vertex e, tail, prop;

  /* *** don't forget tail-> head now */
  
  /* double to integer coercion */
  tail = 1 + unif_rand() * nwp->nnodes; 
  
  for(e = EdgetreeMinimum(nwp->outedges, tail);
      (prop = nwp->outedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail */
    {
      Mtail[nedge] = tail;
      Mhead[nedge] = prop;
      ++nedge;
    }
  for(e = EdgetreeMinimum(nwp->inedges, tail);
      (prop = nwp->inedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail */
    {
      Mhead[nedge] = tail;
      Mtail[nedge] = prop;
      ++nedge;
    }
  
  if(nedge > nwp->nnodes-nedge){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }  
  j = 0;
  while (j <=nedge)
    {
      prop = 1 + unif_rand() * nwp->nnodes; 
      k=0;
      fvalid=1;
      while(fvalid==1 && k<nedge+j){
	if(EdgetreeSearch( MIN(prop,Mtail[k]),
			   MAX(prop,Mtail[k]), nwp->outedges) +
	   EdgetreeSearch( MIN(prop,Mhead[k]),
			   MAX(prop,Mhead[k]), nwp->outedges)==0
	   ){++k;
	}else{
	  fvalid=0;
	}
      }
      if(prop>tail){
	Mtail[j+nedge] = tail;
	Mhead[j+nedge] = prop;
      }else{
	Mtail[j+nedge] = prop;
	Mhead[j+nedge] = tail;
      }
      ++j;
    }
  
  j = 2*nedge;
  if (!CheckTogglesValid(MHp, nwp))
    {
      *Mtail = *Mhead = 0;
    }
}

/*********************
 void MH_OneRandomTnTNode
*********************/
void MH_OneRandomTnTNode (MHProposal *MHp, Network *nwp) {  
  Vertex tail=0, head, e, head1;
  int noutedge=0, ninedge=0, k0=0, fvalid=0, k;
  /* int ndyad; */

  /* *** don't forget tail-> head now */
  
  /* if ( nwp->directed_flag )
    {
      ndyad = (nwp->nnodes - 1) * nwp->nnodes;
    }else{
      ndyad = (nwp->nnodes - 1) * nwp->nnodes / 2;
    } */

  double logratio=0;
  fvalid=0;
  while(fvalid==0){
    
    if ( unif_rand() < 0.5 && nwp->nedges > 0) 
      {
	
	/* select a tie */
	ninedge=0;
	noutedge=0;
	while(noutedge+ninedge==0){
	  /* select a node at random */
	  tail = 1 + unif_rand() * nwp->nnodes;
	  ninedge = nwp->indegree[tail];
	  noutedge = nwp->outdegree[tail];
	}
	
	k0 = (int)(unif_rand() * (noutedge+ninedge)); 
	if (k0 < noutedge){
	  k=0;
	  for(e = EdgetreeMinimum(nwp->outedges, tail);
	      ((head = nwp->outedges[e].value) != 0 && k<k0);
	      e = EdgetreeSuccessor(nwp->outedges, e)){++k;}
	}else{
	  k=0;
	  for(e = EdgetreeMinimum(nwp->inedges, tail);
	      ((head = nwp->inedges[e].value) != 0 && k<(k0-noutedge));
	      e = EdgetreeSuccessor(nwp->inedges, e)){++k;}
	}
	if ( (!nwp->directed_flag && tail > head) ||
	     (nwp->directed_flag && k0 >= noutedge) )
	  {
	    Mtail[0] = head;
	    Mhead[0] = tail;
	  }else{
	    Mtail[0] = tail;
	    Mhead[0] = head;
	  }
	
	logratio = log(((noutedge+ninedge)*1.0)/(nwp->nnodes-1-noutedge-ninedge-1));
	fvalid =1;
      }else{
	/* Choose random non-tie */

	/* select a node at random */
	ninedge=nwp->nnodes-1;
	noutedge=0;
	while(noutedge+ninedge>=(nwp->nnodes-1)){
	  ninedge=0;
	  /* select a node at random */
	  tail = 1 + unif_rand() * nwp->nnodes;
	  ninedge = nwp->indegree[tail];
	  noutedge = nwp->outdegree[tail];
	}
	
	fvalid=0;
	while(fvalid==0){
	  while ((head = 1 + unif_rand() * nwp->nnodes) == tail);
	  fvalid=1;
	  for(e = EdgetreeMinimum(nwp->outedges, tail);
	      (fvalid==1 && ((head1 = nwp->outedges[e].value) != 0));
	      e = EdgetreeSuccessor(nwp->outedges, e)){
	    if(head==head1){fvalid=0;}}
	  if (!(nwp->directed_flag)){
	    for(e = EdgetreeMinimum(nwp->inedges, tail);
		(fvalid==1 && ((head1 = nwp->inedges[e].value) != 0));
		e = EdgetreeSuccessor(nwp->inedges, e)){
	      if(head==head1){fvalid=0;}}
	  }
	}
	
	if ( (!nwp->directed_flag && tail > head) ||
	     (nwp->directed_flag && k0 >= noutedge) )
	  {
	    Mtail[0] = head;
	    Mhead[0] = tail;
	  }else{
	    Mtail[0] = tail;
	    Mhead[0] = head;
	  }
	
        if ( nwp->directed_flag )
	  {
	    logratio = log((nwp->nnodes-1-noutedge-ninedge)/(noutedge+ninedge+1.0));
	  }else{
	    logratio = log((nwp->nnodes-1-noutedge-ninedge)/(noutedge+ninedge+1.0));
	  }
      }
  }
  MHp->logratio += logratio;
}

/*********************
 void MH_ReallocateWithReplacement
*********************/
void MH_ReallocateWithReplacement (MHProposal *MHp, Network *nwp) {  
  int i;
  Vertex root;
  Vertex* edges;
  int edgecount = 0;
  
  /* select a node at random */
  root = 1 + unif_rand() * nwp->nnodes;

  edges = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  for (i = 0; i <= nwp->nnodes; i++)
    edges[i] = NO_EDGE;
  
  /* count current edges and mark them in an array */
  for (i = 1; i <= nwp->nnodes; i++)
    {
      if (root == i) continue;
      if (EdgetreeSearch(root, i, nwp->outedges) > 0)
	{
	  edges[i] = OLD_EDGE;
	  edgecount++;
	}
      if (!nwp->directed_flag && (root > i) &&
	  (EdgetreeSearch(i, root, nwp->outedges) > 0))
	{
	  edges[i] = OLD_EDGE;
	  edgecount++;
	}
    }
  
  /* select edgecount edges to create */
  for (i = 0; i < edgecount; i++)
    {
      Vertex newhead;
      /* get a new edge, neither the root nor something already chosen */
      while ((newhead = 1 + unif_rand() * nwp->nnodes) == root ||
	     (edges[newhead] & NEW_EDGE))
	;
      
      /* if this edge already exists - (OLD_EDGE | NEW_EDGE) == CAN_IGNORE */
      edges[newhead] = edges[newhead] | NEW_EDGE;
    }
  
  /* index into Mtail/Mhead is  */
  edgecount = 0;
  
  /* add to toggle list:  anything that is non zero in edges array
     should be toggled, whether on or off. */
  for (i = 0; i <= nwp->nnodes; i++)
    {
      if (edges[i] == NO_EDGE || edges[i] == CAN_IGNORE) continue;
      
      /* double to integer coercion */
      Mtail[edgecount] = root;
      Mhead[edgecount] = i;
      
      if (!nwp->directed_flag && (Mtail[edgecount] > Mhead[edgecount]))
	{
	  Vertex temp;
	  temp = Mtail[edgecount];
	  Mtail[edgecount] = Mhead[edgecount];
	  Mhead[edgecount] = temp;
	}
      edgecount++;
    }
  Free(edges);
}

/*********************
 void MH_AllTogglesForOneNode
*********************/
void MH_AllTogglesForOneNode (MHProposal *MHp, Network *nwp) {
  
  int i;
  int j;
  int root;
  
  root = 1 + unif_rand() * nwp->nnodes;
  
  j = 0;
  for (i = 1; i <= nwp->nnodes; i++)
    {
      /* probability here only do this with .8? */
      
      /* there is never an edge (root, root) */
      if (i == root)
	continue;
      
      /* double to integer coercion */
      Mtail[j] = root;
      Mhead[j] = i;
      
      if (!nwp->directed_flag && (Mtail[j] > Mhead[j]))
	{
	  Vertex temp;
	  temp = Mtail[j];
	  Mtail[j] = Mhead[j];
	  Mhead[j] = temp;
	}
      j++;
    }
}


/*********************
 void MH_SwitchLabelTwoNodesToggles
*********************/
void MH_SwitchLabelTwoNodesToggles (MHProposal *MHp, Network *nwp) {  
  int nedge1=0, nedge2=0, k, ntoggles;
  Vertex *edges1, *edges2;
  Vertex e, tail2, head2, tail1, head1;

  /* *** don't forget tail-> head now */
  
  /* select a node at random */
  edges1 = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  edges2 = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  
  while(nedge1==0){
    tail1 = 1 + unif_rand() * nwp->nnodes;
    
    for(e = EdgetreeMinimum(nwp->outedges, tail1);
	(head1 = nwp->outedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail1 */
      {
        edges1[nedge1] = head1;
	++nedge1;
      }
    for(e = EdgetreeMinimum(nwp->inedges, tail1);
	(head1 = nwp->inedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail1 */
      {
        edges1[nedge1] = head1;
	++nedge1;
      }
  }
  
  while((tail2 = 1 + unif_rand() * nwp->nnodes) == tail1);
  
  for(e = EdgetreeMinimum(nwp->outedges, tail2);
      (head2 = nwp->outedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail2 */
    {
      edges2[nedge2] = head2;
      ++nedge2;
    }
  for(e = EdgetreeMinimum(nwp->inedges, tail2);
      (head2 = nwp->inedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail2 */
    {
      edges2[nedge2] = head2;
      ++nedge2;
    }
  
  ntoggles = 0;
  for(k=0; k < nedge1; k++){
    if (tail1 > edges1[k])
      {
	Mtail[ntoggles] = edges1[k];
	Mhead[ntoggles] = tail1;
      }
    if (tail1 < edges1[k]){
      Mtail[ntoggles] = tail1;
      Mhead[ntoggles] = edges1[k];
    }
    if(tail1 != edges1[k]) ntoggles++;
  }
  
  for(k=0; k < nedge2; k++){
    if (tail1 > edges2[k])
      {
	Mtail[ntoggles] = edges2[k];
	Mhead[ntoggles] = tail1;
      }
    if (tail1 < edges2[k]){
      Mtail[ntoggles] = tail1;
      Mhead[ntoggles] = edges2[k];
    }
    if(tail1 != edges2[k]) ntoggles++;
  }
  
  for(k=0; k < nedge2; k++){
    if (tail2 > edges2[k])
      {
	Mtail[ntoggles] = edges2[k];
	Mhead[ntoggles] = tail2;
      }
    if (tail2 < edges2[k]){
      Mtail[ntoggles] = tail2;
      Mhead[ntoggles] = edges2[k];
    }
    if(tail2 != edges2[k]) ntoggles++;
  }
  
  for(k=0; k < nedge1; k++){
    if (tail2 > edges1[k])
      {
	Mtail[ntoggles] = edges1[k];
	Mhead[ntoggles] = tail2;
      }
    if (tail2 < edges1[k]){
      Mtail[ntoggles] = tail2;
      Mhead[ntoggles] = edges1[k];
    }
    if(tail2 != edges1[k]) ntoggles++;
  }
  Free(edges1);
  Free(edges2);
}


/*********************
 void MH_ConstrainedCondDegDist
*********************/
void MH_ConstrainedCondDegDist (MHProposal *MHp, Network *nwp)  {  
  int noutedge=0, ninedge=0, k, fvalid=0;
  int k0, j0, j1, k1;
  int j0h, j1h;
  Vertex *outedges, *inedges;
  Vertex e, alter, tail=0, head;

  /* *** don't forget tail-> head now */
  
  /* select a node at random */
  outedges = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  inedges = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  
  while(noutedge==0 && ninedge==0){
    tail = 1 + unif_rand() * nwp->nnodes;
    
    for(e = EdgetreeMinimum(nwp->outedges, tail);
	(head = nwp->outedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail */
      {
        outedges[noutedge] = head;
	++noutedge;
      }
    for(e = EdgetreeMinimum(nwp->inedges, tail);
	(head = nwp->inedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail */
      {
        inedges[ninedge] = head;
	++ninedge;
      }
  }
  
  k0 = (int)(unif_rand() * (noutedge+ninedge)); 
  if (k0 < noutedge){
    head = outedges[k0]; 
  }else{
    head = inedges[k0-noutedge]; 
  }
  if ( (!nwp->directed_flag && tail > head) ||
       (  nwp->directed_flag  && k0 >= noutedge) )
    {
      Mtail[0] = head;
      Mhead[0] = tail;
    }else{
      Mtail[0] = tail;
      Mhead[0] = head;
    }
  
  if (dEdgeListSearch(Mtail[0], Mhead[0], MHp->inputs)==0){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  
  fvalid=0;
  k1=0;
  while(fvalid==0 && k1 < 100){
    while((alter = 1 + unif_rand() * nwp->nnodes) == tail);
    if(alter != head){fvalid=1;}
    fvalid=1;
    if (k0 < noutedge || !(nwp->directed_flag)){
      k=0;
      while(fvalid==1 && noutedge > 0 && k <= noutedge-1){
	if(alter == outedges[k]){fvalid=0;}else{++k;}
      }
    }
    if (k0 >= noutedge || !(nwp->directed_flag)){
      k=0;
      while(fvalid==1 && ninedge > 0 && k <= ninedge-1){
	if(alter == inedges[k]){fvalid=0;}else{++k;}
      }
    }
    k1++;
  }
  
  if (k1 == 100){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  
  if ( (!nwp->directed_flag && alter > tail) ||
       (nwp->directed_flag && k0 < noutedge) )
    {
      Mtail[1] = tail;
      Mhead[1] = alter;
    }else{
      Mtail[1] = alter;
      Mhead[1] = tail;
    }
  
  if (dEdgeListSearch(Mtail[1], Mhead[1], MHp->inputs)==0){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  
  Free(outedges);
  Free(inedges);
  
  /* Check undirected degrees */

  /* *** don't forget tail-> head now */

  if (!nwp->directed_flag){
    k0=nwp->outdegree[tail]+ nwp->indegree[tail];
    j0h=nwp->outdegree[head]+ nwp->indegree[head];
    j1h=nwp->outdegree[alter]+ nwp->indegree[alter];
    
    j0=j0h-1;
    j1=j1h+1;
    
    if( ( (j0==j1h) && (j1==j0h) ) ){
      fvalid = 1;
    }else{
      fvalid = 0;
    }
  }else{
    if(k0 < noutedge){
      /* Check indegrees */
      j0h=nwp->indegree[head];
      j1h=nwp->indegree[alter];
    }else{
      /* Check outdegrees */
      j0h=nwp->outdegree[head];
      j1h=nwp->outdegree[alter];
    }
    j0=j0h-1;
    j1=j1h+1;
    
    if( ( (j0==j1h) && (j1==j0h) ) ){
      fvalid = 1;
    }else{
      fvalid = 0;
    }
  }
  
  if (!fvalid){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
}

void MH_ConstrainedNodePairedTiesToggles (MHProposal *MHp,
       	 Network *nwp) {  
  /* chooses a node and toggles all ties and
     and toggles an equal number of matching nonties
     for that node */
  int nedge=0,j,k;
  int fvalid = 1;
  Vertex e, tail, prop;

  /* *** don't forget tail-> head now */
  
  /* double to integer coercion */
  tail = 1 + unif_rand() * nwp->nnodes; 
  
  for(e = EdgetreeMinimum(nwp->outedges, tail);
      (prop = nwp->outedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail */
    {
      Mtail[nedge] = tail;
      Mhead[nedge] = prop;
      ++nedge;
    }
  for(e = EdgetreeMinimum(nwp->inedges, tail);
      (prop = nwp->inedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail */
    {
      Mhead[nedge] = tail;
      Mtail[nedge] = prop;
      ++nedge;
    }
  
  if(nedge > nwp->nnodes-nedge){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }  
  j = 0;
  while (j <=nedge)
    {
      prop = 1 + unif_rand() * nwp->nnodes; 
      k=0;
      fvalid=1;
      while(fvalid==1 && k<nedge+j){
	if(EdgetreeSearch( MIN(prop,Mtail[k]),
			   MAX(prop,Mtail[k]), nwp->outedges) +
	   EdgetreeSearch( MIN(prop,Mhead[k]),
			   MAX(prop,Mhead[k]), nwp->outedges)==0
	   ){++k;
	}else{
	  fvalid=0;}
      }
      if(prop>tail){
	Mtail[j+nedge] = tail;
	Mhead[j+nedge] = prop;
      }else{
	Mtail[j+nedge] = prop;
	Mhead[j+nedge] = tail;
      }
      ++j;
    }
  
  j = 2*nedge;
  if (!CheckConstrainedTogglesValid(MHp, nwp))
    {
      *Mtail = *Mhead = 0;
    }
}

/*********************
 void MH_ConstrainedReallocateWithReplacement
*********************/
void MH_ConstrainedReallocateWithReplacement (MHProposal *MHp,
       	 Network *nwp) {  
  int i;
  Vertex root;
  Vertex* edges;
  int edgecount = 0;
  
  /* select a node at random */
  root = 1 + unif_rand() * nwp->nnodes;

  edges = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  for (i = 0; i <= nwp->nnodes; i++)
    edges[i] = NO_EDGE;
  
  /* count current edges and mark them in an array */
  for (i = 1; i <= nwp->nnodes; i++)
    {
      if (root == i) continue;
      if (EdgetreeSearch(root, i, nwp->outedges) > 0)
	{
	  edges[i] = OLD_EDGE;
	  edgecount++;
	}
      if (!nwp->directed_flag && (root > i) &&
	  (EdgetreeSearch(i, root, nwp->outedges) > 0))
	{
	  edges[i] = OLD_EDGE;
	  edgecount++;
	}
    }
  
  /* select edgecount edges to create */
  for (i = 0; i < edgecount; i++)
    {
      Vertex newhead;
      
      /* get a new edge, neither the root nor something already chosen */
      while ((newhead = 1 + unif_rand() * nwp->nnodes) == root ||
	     (edges[newhead] & NEW_EDGE))
	;
      
      /* if this edge already exists - (OLD_EDGE | NEW_EDGE) == CAN_IGNORE */
      edges[newhead] = edges[newhead] | NEW_EDGE;
    }
  
  /* index into Mtail/Mhead is  */
  edgecount = 0;
  
  /* add to toggle list:  anything that is non zero in edges array
     should be toggled, whether on or off. */
  for (i = 0; i <= nwp->nnodes; i++)
    {
      if (edges[i] == NO_EDGE || edges[i] == CAN_IGNORE) continue;
      
      /* double to integer coercion */
      Mtail[edgecount] = root;
      Mhead[edgecount] = i;
      
      if (!nwp->directed_flag && (Mtail[edgecount] > Mhead[edgecount]))
	{
	  Vertex temp;
	  temp = Mtail[edgecount];
	  Mtail[edgecount] = Mhead[edgecount];
	  Mhead[edgecount] = temp;
	}
      edgecount++;
    }
  Free(edges);
}

/*********************
 void MH_ConstrainedAllTogglesForOneNode
*********************/
void MH_ConstrainedAllTogglesForOneNode (MHProposal *MHp,
					 Network *nwp) {
  int i;
  int j;
  int root;
  
  root = 1 + unif_rand() * nwp->nnodes;
  
  j = 0;
  for (i = 1; i <= nwp->nnodes; i++)
    {
      /* probability here only do this with .8? */
      
      /* there is never an edge (root, root) */
      if (i == root)
	continue;
      
      /* double to integer coercion */
      Mtail[j] = root;
      Mhead[j] = i;
      
      if (!nwp->directed_flag && (Mtail[j] > Mhead[j]))
	{
	  Vertex temp;
	  temp = Mtail[j];
	  Mtail[j] = Mhead[j];
	  Mhead[j] = temp;
	}
      j++;
    }
}

/*********************
 void MH_ConstrainedTwoRandomToggles
*********************/
void MH_ConstrainedTwoRandomToggles (MHProposal *MHp,
				 Network *nwp) {  
  int i;
  
  for (i = 0; i < 2; i++)
    {
      /* double to integer coercion */
      Mtail[i] = 1 + unif_rand() * nwp->nnodes; 
      while ((Mhead[i] = 1 + unif_rand() * nwp->nnodes) == Mtail[i]);
      
      while(dEdgeListSearch(Mtail[i], Mhead[i], MHp->inputs)==0){
	Mtail[i] = 1 + unif_rand() * nwp->nnodes; 
	while ((Mhead[i] = 1 + unif_rand() * nwp->nnodes) == Mtail[i]);
      }
      if (!nwp->directed_flag && Mtail[i] > Mhead[i]) 
	{
	  Vertex temp;
	  temp = Mtail[i];
	  Mtail[i] = Mhead[i];
	  Mhead[i] = temp;
	}
    }
  
  if (!CheckConstrainedTogglesValid(MHp, nwp))
    {
      Mtail[0] = Mhead[0] = 0;
      Mtail[1] = Mhead[1] = 0;
    }  
}

/*********************
 void MH_ConstrainedCondDeg
*********************/
void MH_ConstrainedCondDeg (MHProposal *MHp,
					 Network *nwp) {  
  /* WARNING: THIS NEEDS TO BE FIXED */
  int nedge1=0, nedge2=0, k, toomany, fvalid=0;
  Vertex *edges1, *edges2;
  Vertex e, tail2=0, head2, tail1, head1;
  
  /* select a node at random */
  edges1 = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  edges2 = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  
  while(nedge1==0){
    tail1 = 1 + unif_rand() * nwp->nnodes;
    
    for(e = EdgetreeMinimum(nwp->outedges, tail1);
	(head1 = nwp->outedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail1 */
      {
        edges1[nedge1] = head1;
	++nedge1;
      }
    for(e = EdgetreeMinimum(nwp->inedges, tail1);
	(head1 = nwp->inedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail1 */
      {
        edges1[nedge1] = head1;
	++nedge1;
      }
  }
  
  head1 = edges1[(int)(unif_rand() * nedge1)]; 
  if (tail1 > head1)
    {
      Mtail[0] = head1;
      Mhead[0] = tail1;
    }else{
      Mtail[0] = tail1;
      Mhead[0] = head1;
    }
   
  toomany = 0;
  while(nedge2==0 && toomany < 100){
    fvalid=0;
    while(fvalid==0){
      while((tail2 = 1 + unif_rand() * nwp->nnodes) == tail1);
      k=0;
      fvalid=1;
      while(fvalid==1 && k < nedge1){
	if(tail2 == edges1[k]){fvalid=0;}else{++k;}
      }
    }

    for(e = EdgetreeMinimum(nwp->outedges, tail2);
	(head2 = nwp->outedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail2 */
      {
        edges2[nedge2] = head2;
	++nedge2;
      }
    for(e = EdgetreeMinimum(nwp->inedges, tail2);
	(head2 = nwp->inedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail2 */
      {
        edges2[nedge2] = head2;
	++nedge2;
      }
    ++toomany;
  }
  if (toomany==100){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  toomany=0;
  fvalid=0;
  while(fvalid==0 && toomany < 10){
    while((head2 = edges2[(int)(unif_rand() * nedge2)]) == tail1);
    k=0;
    fvalid=1;
    while(fvalid==1 && k < nedge1){
      if(head2 == edges1[k]){fvalid=0;}else{++k;}
    }
    ++toomany;
  }
  if (!fvalid || toomany==10){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
    Free(edges1);
    Free(edges2);
      }
  if (tail2 > head2)
    {
      Mtail[1] = head2;
      Mhead[1] = tail2;
    }else{
      Mtail[1] = tail2;
      Mhead[1] = head2;
    }
  Free(edges1);
  Free(edges2);
}

/*********************
 void MH_ConstrainedSwitchLabelTwoNodesToggles
*********************/
void MH_ConstrainedSwitchLabelTwoNodesToggles (MHProposal *MHp,
       	 Network *nwp)  {  
  int nedge1=0, nedge2=0, k, ntoggles;
  Vertex *edges1, *edges2;
  Vertex e, tail2, head2, tail1, head1;

  /* *** don't forget tail-> head now */
  
  /* select a node at random */

  edges1 = (Vertex *) Calloc((nwp->nnodes+1), Vertex);
  edges2 = (Vertex *) Calloc((nwp->nnodes+1), Vertex);

  while(nedge1==0){
    tail1 = 1 + unif_rand() * nwp->nnodes;
    
    for(e = EdgetreeMinimum(nwp->outedges, tail1);
	(head1 = nwp->outedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail1 */
      {
        edges1[nedge1] = head1;
	++nedge1;
      }
    for(e = EdgetreeMinimum(nwp->inedges, tail1);
	(head1 = nwp->inedges[e].value) != 0; /* loop if */
	e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail1 */
      {
        edges1[nedge1] = head1;
	++nedge1;
      }
  }
  
  while((tail2 = 1 + unif_rand() * nwp->nnodes) == tail1);
  
  for(e = EdgetreeMinimum(nwp->outedges, tail2);
      (head2 = nwp->outedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->outedges, e)) /* step through outedges of tail2 */
    {
      edges2[nedge2] = head2;
      ++nedge2;
    }
  for(e = EdgetreeMinimum(nwp->inedges, tail2);
      (head2 = nwp->inedges[e].value) != 0; /* loop if */
      e = EdgetreeSuccessor(nwp->inedges, e)) /* step through inedges of tail2 */
    {
      edges2[nedge2] = head2;
      ++nedge2;
    }
  
  ntoggles = 0;
  for(k=0; k < nedge1; k++){
    if (tail1 > edges1[k])
      {
	Mtail[ntoggles] = edges1[k];
	Mhead[ntoggles] = tail1;
      }
    if (tail1 < edges1[k]){
      Mtail[ntoggles] = tail1;
      Mhead[ntoggles] = edges1[k];
    }
    if(tail1 != edges1[k]) ntoggles++;
  }
  
  for(k=0; k < nedge2; k++){
    if (tail1 > edges2[k])
      {
	Mtail[ntoggles] = edges2[k];
	Mhead[ntoggles] = tail1;
      }
    if (tail1 < edges2[k]){
      Mtail[ntoggles] = tail1;
      Mhead[ntoggles] = edges2[k];
    }
    if(tail1 != edges2[k]) ntoggles++;
  }
  
  for(k=0; k < nedge2; k++){
    if (tail2 > edges2[k])
      {
	Mtail[ntoggles] = edges2[k];
	Mhead[ntoggles] = tail2;
      }
    if (tail2 < edges2[k]){
      Mtail[ntoggles] = tail2;
      Mhead[ntoggles] = edges2[k];
    }
    if(tail2 != edges2[k]) ntoggles++;
  }
  
  for(k=0; k < nedge1; k++){
    if (tail2 > edges1[k])
      {
	Mtail[ntoggles] = edges1[k];
	Mhead[ntoggles] = tail2;
      }
    if (tail2 < edges1[k]){
      Mtail[ntoggles] = tail2;
      Mhead[ntoggles] = edges1[k];
    }
    if(tail2 != edges1[k]) ntoggles++;
  }
  Free(edges1);
  Free(edges2);
}

/*********************
 void MH_ConstantEdgesToggles
*********************/
void MH_ConstantEdgesToggles (MHProposal *MHp, Network *nwp)  {  
  int noutedge=0, ninedge=0, k, fvalid=0;
  int k0, k1;
  Vertex e, alter, tail, head, head1;

  /* *** don't forget tail-> head now */
  
  while(noutedge+ninedge==0){
    /* select a node at random */
    tail = 1 + unif_rand() * nwp->nnodes;
    ninedge  = nwp->indegree[tail];
    noutedge = nwp->outdegree[tail];
  }
  
  k0 = (int)(unif_rand() * (noutedge+ninedge)); 
  if (k0 < noutedge){
    k=0;
    for(e = EdgetreeMinimum(nwp->outedges, tail);
	((head = nwp->outedges[e].value) != 0 && k<k0);
	e = EdgetreeSuccessor(nwp->outedges, e)){++k;}
  }else{
    k=0;
    for(e = EdgetreeMinimum(nwp->inedges, tail);
	((head = nwp->inedges[e].value) != 0 && k<(k0-noutedge));
	e = EdgetreeSuccessor(nwp->inedges, e)){++k;}
  }
  
  if ( (!nwp->directed_flag && tail > head) ||
       (nwp->directed_flag && k0 >= noutedge) )
    {
      Mtail[0] = head;
      Mhead[0] = tail;
    }else{
      Mtail[0] = tail;
      Mhead[0] = head;
    }
  
  k1=0;
  fvalid=0;
  while(fvalid==0 && k1 < 100){
    while((alter = 1 + unif_rand() * nwp->nnodes) == tail);
    fvalid=1;
    if(alter == head){fvalid=0;}
    if (k0 < noutedge || !(nwp->directed_flag)){
      for(e = EdgetreeMinimum(nwp->outedges, tail);
	  (fvalid==1 && ((head1 = nwp->outedges[e].value) != 0));
	  e = EdgetreeSuccessor(nwp->outedges, e)){
	if(alter==head1){fvalid=0;}}
    }
    if (k0 >= noutedge || !(nwp->directed_flag)){
      for(e = EdgetreeMinimum(nwp->inedges, tail);
	  (fvalid==1 && ((head1 = nwp->inedges[e].value) != 0));
	  e = EdgetreeSuccessor(nwp->inedges, e)){
	if(alter==head1){fvalid=0;}}
    }
    k1++;
  }
  if (k1 == 100){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  
  if ( (!nwp->directed_flag && alter > tail) ||
       (nwp->directed_flag && k0 < noutedge) )
    {
      Mtail[1] = tail;
      Mhead[1] = alter;
    }else{
      Mtail[1] = alter;
      Mhead[1] = tail;
    }
  
  if (!fvalid){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }else{  
  }
}

/*********************
 void MH_CondDegSwitchToggles
*********************/
void MH_CondDegSwitchToggles (MHProposal *MHp, Network *nwp)  {  
  int noutedge, ninedge, i;
  int k, k0, toomany;
  Vertex e, tail, head;

  /* *** don't forget tail-> head now */
  
  /* select a node at random */
  for (i = 0; i < 2; i++){
    toomany=0;
    noutedge=0;
    ninedge=0;
    while(noutedge==0 && ninedge==0 && toomany < 100){
      tail = 1 + unif_rand() * nwp->nnodes;
      ninedge=0;
      noutedge=0;
      while(noutedge+ninedge==0){
	/* select a node at random */
	tail = 1 + unif_rand() * nwp->nnodes;
	ninedge = nwp->indegree[tail];
	noutedge = nwp->outdegree[tail];
      }
      ++toomany;
    }
    
    if (toomany == 100){
      Mtail[0] = Mhead[0] = 0;
      Mtail[1] = Mhead[1] = 0;
    }
    
    k0 = (int)(unif_rand() * (noutedge+ninedge)); 
    if (k0 < noutedge){
      k=0;
      for(e = EdgetreeMinimum(nwp->outedges, tail);
	  ((head = nwp->outedges[e].value) != 0 && k<k0);
	  e = EdgetreeSuccessor(nwp->outedges, e)){++k;}
    }else{
      k=0;
      for(e = EdgetreeMinimum(nwp->inedges, tail);
	  ((head = nwp->inedges[e].value) != 0 && k<(k0-noutedge));
	  e = EdgetreeSuccessor(nwp->inedges, e)){++k;}
    }
    if ( (!nwp->directed_flag && tail > head) ||
	 (nwp->directed_flag && k0 >= noutedge) )
      {
	Mtail[i] = head;
	Mhead[i] = tail;
      }else{
	Mtail[i] = tail;
	Mhead[i] = head;
      }
  }
  
  if (EdgetreeSearch( Mtail[0],Mhead[1], nwp->outedges) ||
      EdgetreeSearch( Mtail[1],Mhead[0], nwp->outedges) ){
    Mtail[0] = Mhead[0] = 0;
    Mtail[1] = Mhead[1] = 0;
  }
  
  if ( (!nwp->directed_flag && Mtail[0] > Mhead[1]) )
    {
      Mtail[2] = Mhead[1];
      Mhead[2] = Mtail[0];
    }else{
      Mtail[2] = Mtail[0];
      Mhead[2] = Mhead[1];
    }
  
  if ( (!nwp->directed_flag && Mtail[1] > Mhead[0]) )
    {
      Mtail[3] = Mhead[0];
      Mhead[3] = Mtail[1];
    }else{
      Mtail[3] = Mtail[1];
      Mhead[3] = Mhead[0];
    }
}


