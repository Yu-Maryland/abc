/**CFile****************************************************************

  FileName    [abcMap.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Interface with the SC mapping package.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcMap.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "map/mio/mio.h"
#include "map/mapper/mapper.h"
#include "misc/util/utilNam.h"
#include "map/scl/sclCon.h"
#include "map/scl/sclLib.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

extern Map_Man_t *  Abc_NtkToMap( Abc_Ntk_t * pNtk, double DelayTarget, int fRecovery, float * pSwitching, int fVerbose );
extern Abc_Ntk_t *  Abc_NtkFromMap( Map_Man_t * pMan, Abc_Ntk_t * pNtk, int fUseBuffs );
static Abc_Obj_t *  Abc_NodeFromMap_rec( Abc_Ntk_t * pNtkNew, Map_Node_t * pNodeMap, int fPhase );
static Abc_Obj_t *  Abc_NodeFromMapPhase_rec( Abc_Ntk_t * pNtkNew, Map_Node_t * pNodeMap, int fPhase );

static Abc_Ntk_t *  Abc_NtkFromMapSuperChoice( Map_Man_t * pMan, Abc_Ntk_t * pNtk );
static void         Abc_NodeSuperChoice( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pNode );
static void         Abc_NodeFromMapCutPhase( Abc_Ntk_t * pNtkNew, Map_Cut_t * pCut, int fPhase );
static Abc_Obj_t *  Abc_NodeFromMapSuperChoice_rec( Abc_Ntk_t * pNtkNew, Map_Super_t * pSuper, Abc_Obj_t * pNodePis[], int nNodePis );

 
////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

#define ABC_STMAP77_MAX_WATCH_AIGS 4
static int s_fStmap77ReconstructionDiag = 0;
static int s_fStmap77ReconstructionActive = 0;
static const char * s_pStmap77ReconstructionLabel = "stmap77";
static int s_nStmap77ReconstructionWatchAigs = 0;
static int s_Stmap77ReconstructionWatchAigIds[ABC_STMAP77_MAX_WATCH_AIGS] = { -1, -1, -1, -1 };
static int s_nStmap77ReconstructionRows = 0;
static int s_nStmap77ReconstructionDemands = 0;
static int s_nStmap77ReconstructionRequests = 0;
static int s_nStmap77ReconstructionDirectEmits = 0;
static int s_nStmap77ReconstructionInverters = 0;
static int s_nStmap77ReconstructionCacheHits = 0;
static int s_Stmap77ReconstructionRequestPhase[2] = { 0, 0 };
static int s_Stmap77ReconstructionDirectPhase[2] = { 0, 0 };
static int s_Stmap77ReconstructionInverterPhase[2] = { 0, 0 };
static int s_Stmap77ReconstructionCachePhase[2] = { 0, 0 };

static void Abc_Stmap77ResetReconstructionCounters( void )
{
    int i;
    s_nStmap77ReconstructionRows = 0;
    s_nStmap77ReconstructionDemands = 0;
    s_nStmap77ReconstructionRequests = 0;
    s_nStmap77ReconstructionDirectEmits = 0;
    s_nStmap77ReconstructionInverters = 0;
    s_nStmap77ReconstructionCacheHits = 0;
    for ( i = 0; i < 2; i++ )
    {
        s_Stmap77ReconstructionRequestPhase[i] = 0;
        s_Stmap77ReconstructionDirectPhase[i] = 0;
        s_Stmap77ReconstructionInverterPhase[i] = 0;
        s_Stmap77ReconstructionCachePhase[i] = 0;
    }
}

static void Abc_Stmap77ClearReconstructionDiag( void )
{
    int i;
    s_fStmap77ReconstructionDiag = 0;
    s_fStmap77ReconstructionActive = 0;
    s_pStmap77ReconstructionLabel = "stmap77";
    s_nStmap77ReconstructionWatchAigs = 0;
    for ( i = 0; i < ABC_STMAP77_MAX_WATCH_AIGS; i++ )
        s_Stmap77ReconstructionWatchAigIds[i] = -1;
    Abc_Stmap77ResetReconstructionCounters();
}

void Abc_Stmap77SetReconstructionDiag( int fEnable, const char * pLabel, int * pWatchAigIds, int nWatchAigs )
{
    int i;
    Abc_Stmap77ClearReconstructionDiag();
    s_pStmap77ReconstructionLabel = pLabel && pLabel[0] ? pLabel : "stmap77";
    if ( !fEnable || pWatchAigIds == NULL || nWatchAigs <= 0 )
        return;
    for ( i = 0; i < nWatchAigs && s_nStmap77ReconstructionWatchAigs < ABC_STMAP77_MAX_WATCH_AIGS; i++ )
        if ( pWatchAigIds[i] >= 0 )
            s_Stmap77ReconstructionWatchAigIds[s_nStmap77ReconstructionWatchAigs++] = pWatchAigIds[i];
    if ( s_nStmap77ReconstructionWatchAigs == 0 )
        return;
    s_fStmap77ReconstructionDiag = 1;
}

int Abc_Stmap77ReconstructionDiagConfigured( void )
{
    return s_fStmap77ReconstructionDiag;
}

void Abc_Stmap77SetReconstructionActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap77ReconstructionDiag )
        return;
    if ( fActive )
        Abc_Stmap77ResetReconstructionCounters();
    s_fStmap77ReconstructionActive = fActive;
    (void)pPassLabel;
}

static int Abc_Stmap77ReconstructionWatchIndex( Map_Node_t * pNodeMap, int * pAigId )
{
    Map_Node_t * pNodeRegular;
    int i, AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( !s_fStmap77ReconstructionDiag || !s_fStmap77ReconstructionActive || pNodeMap == NULL )
        return -1;
    pNodeRegular = Map_Regular( pNodeMap );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return -1;
    AigId = Map_NodeReadAigId( pNodeRegular );
    if ( pAigId )
        *pAigId = AigId;
    for ( i = 0; i < s_nStmap77ReconstructionWatchAigs; i++ )
        if ( s_Stmap77ReconstructionWatchAigIds[i] == AigId )
            return i;
    return -1;
}

static void Abc_Stmap77RecordReconstructionDemand( const char * pKind, int CoIndex, Map_Node_t * pNodeMap, int fPhase )
{
    int AigId, WatchIndex;
    WatchIndex = Abc_Stmap77ReconstructionWatchIndex( pNodeMap, &AigId );
    if ( WatchIndex < 0 )
        return;
    s_nStmap77ReconstructionRows++;
    s_nStmap77ReconstructionDemands++;
    printf( "%s reconstruct-demand: index = %d  watch-index = %d  co-kind = %s  co-index = %d  node = %d  aig-id = %d  requested-phase = %d  complemented-driver = %d\n",
        s_pStmap77ReconstructionLabel, s_nStmap77ReconstructionRows, WatchIndex, pKind ? pKind : "?",
        CoIndex, Map_NodeReadNum(Map_Regular(pNodeMap)), AigId, fPhase, Map_IsComplement(pNodeMap) );
}

static void Abc_Stmap77RecordReconstructionRequest( Map_Node_t * pNodeMap, int fPhase )
{
    int AigId, WatchIndex;
    WatchIndex = Abc_Stmap77ReconstructionWatchIndex( pNodeMap, &AigId );
    if ( WatchIndex < 0 )
        return;
    s_nStmap77ReconstructionRows++;
    s_nStmap77ReconstructionRequests++;
    if ( fPhase >= 0 && fPhase < 2 )
        s_Stmap77ReconstructionRequestPhase[fPhase]++;
    printf( "%s reconstruct-request: index = %d  watch-index = %d  node = %d  aig-id = %d  requested-phase = %d  has-cut-requested = %d  has-cut-opposite = %d  cached-requested = %d  cached-opposite = %d  level = %d\n",
        s_pStmap77ReconstructionLabel, s_nStmap77ReconstructionRows, WatchIndex,
        Map_NodeReadNum(Map_Regular(pNodeMap)), AigId, fPhase,
        Map_NodeReadCutBest(Map_Regular(pNodeMap), fPhase) != NULL,
        Map_NodeReadCutBest(Map_Regular(pNodeMap), !fPhase) != NULL,
        Map_NodeReadData(Map_Regular(pNodeMap), fPhase) != NULL,
        Map_NodeReadData(Map_Regular(pNodeMap), !fPhase) != NULL,
        Map_NodeReadLevel(Map_Regular(pNodeMap)) );
}

static void Abc_Stmap77RecordReconstructionCache( Map_Node_t * pNodeMap, int fPhase, Abc_Obj_t * pNodeNew, const char * pVia )
{
    int AigId, WatchIndex;
    WatchIndex = Abc_Stmap77ReconstructionWatchIndex( pNodeMap, &AigId );
    if ( WatchIndex < 0 )
        return;
    s_nStmap77ReconstructionRows++;
    s_nStmap77ReconstructionCacheHits++;
    if ( fPhase >= 0 && fPhase < 2 )
        s_Stmap77ReconstructionCachePhase[fPhase]++;
    printf( "%s reconstruct-cache: index = %d  watch-index = %d  node = %d  aig-id = %d  phase = %d  cached-node = %d  via = %s\n",
        s_pStmap77ReconstructionLabel, s_nStmap77ReconstructionRows, WatchIndex,
        Map_NodeReadNum(Map_Regular(pNodeMap)), AigId, fPhase,
        pNodeNew ? Abc_ObjId(pNodeNew) : -1, pVia ? pVia : "?" );
}

static void Abc_Stmap77RecordReconstructionEmit( Map_Node_t * pNodeMap, int fPhase, Abc_Obj_t * pNodeNew, Map_Cut_t * pCutBest, Map_Super_t * pSuperBest, unsigned uPhaseBest )
{
    Map_Node_t ** ppLeaves;
    Mio_Gate_t * pGate;
    int AigId, WatchIndex, nLeaves, i, LeafAigId[6], LeafPhase[6];
    WatchIndex = Abc_Stmap77ReconstructionWatchIndex( pNodeMap, &AigId );
    if ( WatchIndex < 0 )
        return;
    nLeaves = pCutBest ? Map_CutReadLeavesNum( pCutBest ) : 0;
    ppLeaves = pCutBest ? Map_CutReadLeaves( pCutBest ) : NULL;
    for ( i = 0; i < 6; i++ )
    {
        LeafAigId[i] = -1;
        LeafPhase[i] = -1;
        if ( ppLeaves != NULL && i < nLeaves )
        {
            LeafAigId[i] = Map_NodeReadAigId( Map_Regular(ppLeaves[i]) );
            LeafPhase[i] = ((uPhaseBest & (1 << i)) > 0) ? 0 : 1;
        }
    }
    pGate = pNodeNew ? (Mio_Gate_t *)pNodeNew->pData : NULL;
    s_nStmap77ReconstructionRows++;
    s_nStmap77ReconstructionDirectEmits++;
    if ( fPhase >= 0 && fPhase < 2 )
        s_Stmap77ReconstructionDirectPhase[fPhase]++;
    printf( "%s reconstruct-emit: index = %d  watch-index = %d  node = %d  aig-id = %d  phase = %d  new-node = %d  gate = %s  leaves = %d  u-phase-best = %u  fanout-limit = %d  leaf0-aig-id = %d  leaf0-phase = %d  leaf1-aig-id = %d  leaf1-phase = %d  leaf2-aig-id = %d  leaf2-phase = %d  leaf3-aig-id = %d  leaf3-phase = %d  leaf4-aig-id = %d  leaf4-phase = %d  leaf5-aig-id = %d  leaf5-phase = %d\n",
        s_pStmap77ReconstructionLabel, s_nStmap77ReconstructionRows, WatchIndex,
        Map_NodeReadNum(Map_Regular(pNodeMap)), AigId, fPhase,
        pNodeNew ? Abc_ObjId(pNodeNew) : -1, pGate ? Mio_GateReadName(pGate) : "?",
        nLeaves, uPhaseBest, pSuperBest ? Map_SuperReadFanoutLimit(pSuperBest) : -1,
        LeafAigId[0], LeafPhase[0], LeafAigId[1], LeafPhase[1], LeafAigId[2], LeafPhase[2],
        LeafAigId[3], LeafPhase[3], LeafAigId[4], LeafPhase[4], LeafAigId[5], LeafPhase[5] );
}

static void Abc_Stmap77RecordReconstructionInverter( Map_Node_t * pNodeMap, int fPhase, Abc_Obj_t * pNodeSource, Abc_Obj_t * pNodeInv )
{
    Mio_Gate_t * pSourceGate, * pInvGate;
    int AigId, WatchIndex;
    WatchIndex = Abc_Stmap77ReconstructionWatchIndex( pNodeMap, &AigId );
    if ( WatchIndex < 0 )
        return;
    pSourceGate = pNodeSource ? (Mio_Gate_t *)pNodeSource->pData : NULL;
    pInvGate = pNodeInv ? (Mio_Gate_t *)pNodeInv->pData : NULL;
    s_nStmap77ReconstructionRows++;
    s_nStmap77ReconstructionInverters++;
    if ( fPhase >= 0 && fPhase < 2 )
        s_Stmap77ReconstructionInverterPhase[fPhase]++;
    printf( "%s reconstruct-inverter: index = %d  watch-index = %d  node = %d  aig-id = %d  requested-phase = %d  source-phase = %d  source-node = %d  source-gate = %s  inverter-node = %d  inverter-gate = %s\n",
        s_pStmap77ReconstructionLabel, s_nStmap77ReconstructionRows, WatchIndex,
        Map_NodeReadNum(Map_Regular(pNodeMap)), AigId, fPhase, !fPhase,
        pNodeSource ? Abc_ObjId(pNodeSource) : -1, pSourceGate ? Mio_GateReadName(pSourceGate) : "?",
        pNodeInv ? Abc_ObjId(pNodeInv) : -1, pInvGate ? Mio_GateReadName(pInvGate) : "?" );
}

void Abc_Stmap77PrintReconstructionSummary( void )
{
    printf( "%s reconstruct stats: watched-aigs = %d  rows = %d  demands = %d  requests = %d  direct-emits = %d  inverters = %d  cache-hits = %d  phase0-requests = %d  phase1-requests = %d  phase0-direct = %d  phase1-direct = %d  phase0-inverters = %d  phase1-inverters = %d  phase0-cache = %d  phase1-cache = %d\n",
        s_pStmap77ReconstructionLabel, s_nStmap77ReconstructionWatchAigs, s_nStmap77ReconstructionRows,
        s_nStmap77ReconstructionDemands, s_nStmap77ReconstructionRequests,
        s_nStmap77ReconstructionDirectEmits, s_nStmap77ReconstructionInverters,
        s_nStmap77ReconstructionCacheHits, s_Stmap77ReconstructionRequestPhase[0],
        s_Stmap77ReconstructionRequestPhase[1], s_Stmap77ReconstructionDirectPhase[0],
        s_Stmap77ReconstructionDirectPhase[1], s_Stmap77ReconstructionInverterPhase[0],
        s_Stmap77ReconstructionInverterPhase[1], s_Stmap77ReconstructionCachePhase[0],
        s_Stmap77ReconstructionCachePhase[1] );
}

#define ABC_STMAP78_MAX_PATH 256
static int s_fStmap78DemandPathDiag = 0;
static int s_fStmap78DemandPathActive = 0;
static const char * s_pStmap78DemandPathLabel = "stmap78";
static int s_Stmap78WatchAigId = -1;
static int s_Stmap78DownstreamAigId = -1;
static const char * s_pStmap78RootKind = "?";
static int s_iStmap78RootCo = -1;
static int s_nStmap78DemandRows = 0;
static int s_nStmap78TargetRequests = 0;
static int s_nStmap78TargetViaDownstream = 0;
static int s_nStmap78LeafDemands = 0;
static int s_nStmap78LeafViaDownstream = 0;
static int s_Stmap78TargetPhaseRequests[2] = { 0, 0 };
static int s_Stmap78LeafPhaseRequests[2] = { 0, 0 };
static int s_nStmap78Phase0Direct = 0;
static int s_nStmap78Phase0Invert = 0;
static int s_nStmap78Stack = 0;
static int s_Stmap78PathNodes[ABC_STMAP78_MAX_PATH];
static int s_Stmap78PathAigs[ABC_STMAP78_MAX_PATH];
static int s_Stmap78PathPhases[ABC_STMAP78_MAX_PATH];

static void Abc_Stmap78ResetDemandPathCounters( void )
{
    s_pStmap78RootKind = "?";
    s_iStmap78RootCo = -1;
    s_nStmap78DemandRows = 0;
    s_nStmap78TargetRequests = 0;
    s_nStmap78TargetViaDownstream = 0;
    s_nStmap78LeafDemands = 0;
    s_nStmap78LeafViaDownstream = 0;
    s_Stmap78TargetPhaseRequests[0] = 0;
    s_Stmap78TargetPhaseRequests[1] = 0;
    s_Stmap78LeafPhaseRequests[0] = 0;
    s_Stmap78LeafPhaseRequests[1] = 0;
    s_nStmap78Phase0Direct = 0;
    s_nStmap78Phase0Invert = 0;
    s_nStmap78Stack = 0;
}

static void Abc_Stmap78ClearDemandPathDiag( void )
{
    s_fStmap78DemandPathDiag = 0;
    s_fStmap78DemandPathActive = 0;
    s_pStmap78DemandPathLabel = "stmap78";
    s_Stmap78WatchAigId = -1;
    s_Stmap78DownstreamAigId = -1;
    Abc_Stmap78ResetDemandPathCounters();
}

void Abc_Stmap78SetDemandPathDiag( int fEnable, const char * pLabel, int WatchAigId, int DownstreamAigId )
{
    Abc_Stmap78ClearDemandPathDiag();
    s_pStmap78DemandPathLabel = pLabel && pLabel[0] ? pLabel : "stmap78";
    if ( !fEnable || WatchAigId < 0 )
        return;
    s_fStmap78DemandPathDiag = 1;
    s_Stmap78WatchAigId = WatchAigId;
    s_Stmap78DownstreamAigId = DownstreamAigId;
}

int Abc_Stmap78DemandPathDiagConfigured( void )
{
    return s_fStmap78DemandPathDiag;
}

void Abc_Stmap78SetDemandPathActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap78DemandPathDiag )
        return;
    if ( fActive )
        Abc_Stmap78ResetDemandPathCounters();
    s_fStmap78DemandPathActive = fActive;
    (void)pPassLabel;
}

static const char * Abc_Stmap78PhaseAction( Map_Node_t * pNodeMap, int fPhase )
{
    Map_Node_t * pNodeRegular;
    if ( pNodeMap == NULL )
        return "unavailable";
    pNodeRegular = Map_Regular( pNodeMap );
    if ( Map_NodeReadData( pNodeRegular, fPhase ) != NULL )
        return "cached";
    if ( Map_NodeReadCutBest( pNodeRegular, fPhase ) != NULL )
        return "direct-cut";
    if ( Map_NodeReadCutBest( pNodeRegular, !fPhase ) != NULL )
        return "inverter-from-opposite";
    return "unavailable";
}

static const char * Abc_Stmap78PhaseGateName( Map_Node_t * pNodeMap, int fPhase )
{
    Map_Cut_t * pCutBest;
    Map_Super_t * pSuperBest;
    Mio_Gate_t * pGate;
    if ( pNodeMap == NULL )
        return "?";
    pCutBest = Map_NodeReadCutBest( Map_Regular(pNodeMap), fPhase );
    if ( pCutBest == NULL )
        return "?";
    pSuperBest = Map_CutReadSuperBest( pCutBest, fPhase );
    pGate = pSuperBest ? Map_SuperReadRoot( pSuperBest ) : NULL;
    return pGate ? Mio_GateReadName( pGate ) : "?";
}

static void Abc_Stmap78AppendValue( char * pBuffer, int nBuffer, int * pPos, int Value )
{
    int nWritten;
    if ( *pPos >= nBuffer )
        return;
    nWritten = snprintf( pBuffer + *pPos, nBuffer - *pPos, *pPos ? ">%d" : "%d", Value );
    if ( nWritten < 0 )
        return;
    *pPos += nWritten;
    if ( *pPos >= nBuffer )
        *pPos = nBuffer - 1;
}

static int Abc_Stmap78PathContainsAig( int AigId )
{
    int i, Limit = s_nStmap78Stack < ABC_STMAP78_MAX_PATH ? s_nStmap78Stack : ABC_STMAP78_MAX_PATH;
    for ( i = 0; i < Limit; i++ )
        if ( s_Stmap78PathAigs[i] == AigId )
            return 1;
    return 0;
}

static void Abc_Stmap78FormatPath( char * pAigs, char * pNodes, char * pPhases, int nBuffer )
{
    int i, Limit = s_nStmap78Stack < ABC_STMAP78_MAX_PATH ? s_nStmap78Stack : ABC_STMAP78_MAX_PATH;
    int nAigPos = 0, nNodePos = 0, nPhasePos = 0;
    pAigs[0] = pNodes[0] = pPhases[0] = 0;
    for ( i = 0; i < Limit; i++ )
    {
        Abc_Stmap78AppendValue( pAigs, nBuffer, &nAigPos, s_Stmap78PathAigs[i] );
        Abc_Stmap78AppendValue( pNodes, nBuffer, &nNodePos, s_Stmap78PathNodes[i] );
        Abc_Stmap78AppendValue( pPhases, nBuffer, &nPhasePos, s_Stmap78PathPhases[i] );
    }
    if ( pAigs[0] == 0 )
    {
        pAigs[0] = '-'; pAigs[1] = 0;
        pNodes[0] = '-'; pNodes[1] = 0;
        pPhases[0] = '-'; pPhases[1] = 0;
    }
}

static void Abc_Stmap78RecordTargetRequest( Map_Node_t * pNodeMap, int fPhase )
{
    char pAigs[1024], pNodes[1024], pPhases[1024];
    Map_Node_t * pNodeRegular = Map_Regular( pNodeMap );
    int Limit = s_nStmap78Stack < ABC_STMAP78_MAX_PATH ? s_nStmap78Stack : ABC_STMAP78_MAX_PATH;
    int ParentAig = Limit >= 2 ? s_Stmap78PathAigs[Limit - 2] : -1;
    int ParentNode = Limit >= 2 ? s_Stmap78PathNodes[Limit - 2] : -1;
    int ParentPhase = Limit >= 2 ? s_Stmap78PathPhases[Limit - 2] : -1;
    int fViaDownstream = Abc_Stmap78PathContainsAig( s_Stmap78DownstreamAigId );
    const char * pPhase0Action = Abc_Stmap78PhaseAction( pNodeRegular, 0 );
    const char * pPhase1Action = Abc_Stmap78PhaseAction( pNodeRegular, 1 );
    Abc_Stmap78FormatPath( pAigs, pNodes, pPhases, 1024 );
    s_nStmap78DemandRows++;
    s_nStmap78TargetRequests++;
    if ( fPhase >= 0 && fPhase < 2 )
        s_Stmap78TargetPhaseRequests[fPhase]++;
    if ( fViaDownstream )
        s_nStmap78TargetViaDownstream++;
    if ( !strcmp( pPhase0Action, "direct-cut" ) || !strcmp( pPhase0Action, "cached" ) )
        s_nStmap78Phase0Direct++;
    else if ( !strcmp( pPhase0Action, "inverter-from-opposite" ) )
        s_nStmap78Phase0Invert++;
    printf( "%s demand-path: index = %d  co-kind = %s  co-index = %d  watch-aig = %d  downstream-aig = %d  requested-phase = %d  stack-depth = %d  via-downstream = %d  parent-node = %d  parent-aig-id = %d  parent-phase = %d  parent-is-downstream = %d  phase0-action = %s  phase1-action = %s  phase0-has-cut = %d  phase1-has-cut = %d  phase0-cached = %d  phase1-cached = %d  phase0-gate = %s  phase1-gate = %s  path-aigs = %s  path-nodes = %s  path-phases = %s\n",
        s_pStmap78DemandPathLabel, s_nStmap78DemandRows, s_pStmap78RootKind,
        s_iStmap78RootCo, s_Stmap78WatchAigId, s_Stmap78DownstreamAigId,
        fPhase, s_nStmap78Stack, fViaDownstream, ParentNode, ParentAig,
        ParentPhase, ParentAig == s_Stmap78DownstreamAigId, pPhase0Action,
        pPhase1Action, Map_NodeReadCutBest(pNodeRegular, 0) != NULL,
        Map_NodeReadCutBest(pNodeRegular, 1) != NULL,
        Map_NodeReadData(pNodeRegular, 0) != NULL,
        Map_NodeReadData(pNodeRegular, 1) != NULL,
        Abc_Stmap78PhaseGateName( pNodeRegular, 0 ),
        Abc_Stmap78PhaseGateName( pNodeRegular, 1 ),
        pAigs, pNodes, pPhases );
}

static void Abc_Stmap78DemandPathPush( Map_Node_t * pNodeMap, int fPhase )
{
    Map_Node_t * pNodeRegular;
    int AigId;
    if ( !s_fStmap78DemandPathDiag || !s_fStmap78DemandPathActive || pNodeMap == NULL )
        return;
    pNodeRegular = Map_Regular( pNodeMap );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return;
    AigId = Map_NodeReadAigId( pNodeRegular );
    if ( s_nStmap78Stack < ABC_STMAP78_MAX_PATH )
    {
        s_Stmap78PathNodes[s_nStmap78Stack] = Map_NodeReadNum( pNodeRegular );
        s_Stmap78PathAigs[s_nStmap78Stack] = AigId;
        s_Stmap78PathPhases[s_nStmap78Stack] = fPhase;
    }
    s_nStmap78Stack++;
    if ( AigId == s_Stmap78WatchAigId )
        Abc_Stmap78RecordTargetRequest( pNodeRegular, fPhase );
}

static void Abc_Stmap78DemandPathPop( void )
{
    if ( !s_fStmap78DemandPathDiag || !s_fStmap78DemandPathActive )
        return;
    if ( s_nStmap78Stack > 0 )
        s_nStmap78Stack--;
}

static int Abc_Stmap78DemandPathSetTopPhase( int fPhase )
{
    int iTop, fOldPhase;
    if ( !s_fStmap78DemandPathDiag || !s_fStmap78DemandPathActive )
        return -1;
    if ( s_nStmap78Stack <= 0 || s_nStmap78Stack > ABC_STMAP78_MAX_PATH )
        return -1;
    iTop = s_nStmap78Stack - 1;
    fOldPhase = s_Stmap78PathPhases[iTop];
    s_Stmap78PathPhases[iTop] = fPhase;
    return fOldPhase;
}

static void Abc_Stmap78DemandPathSetRoot( const char * pKind, int CoIndex )
{
    if ( !s_fStmap78DemandPathDiag || !s_fStmap78DemandPathActive )
        return;
    s_pStmap78RootKind = pKind ? pKind : "?";
    s_iStmap78RootCo = CoIndex;
    s_nStmap78Stack = 0;
}

static void Abc_Stmap78RecordLeafDemand( Map_Node_t * pParentMap, int ParentPhase, Map_Node_t * pLeafMap, int LeafPhase, int LeafIndex, Map_Cut_t * pCutBest, Map_Super_t * pSuperBest, unsigned uPhaseBest )
{
    char pAigs[1024], pNodes[1024], pPhases[1024];
    Map_Node_t * pParentRegular, * pLeafRegular;
    Mio_Gate_t * pParentGate;
    int ParentAig, LeafAig, fParentIsDownstream;
    if ( !s_fStmap78DemandPathDiag || !s_fStmap78DemandPathActive || pParentMap == NULL || pLeafMap == NULL )
        return;
    pParentRegular = Map_Regular( pParentMap );
    pLeafRegular = Map_Regular( pLeafMap );
    if ( Map_NodeIsConst(pParentRegular) || Map_NodeIsConst(pLeafRegular) )
        return;
    ParentAig = Map_NodeReadAigId( pParentRegular );
    LeafAig = Map_NodeReadAigId( pLeafRegular );
    if ( LeafAig != s_Stmap78WatchAigId )
        return;
    pParentGate = pSuperBest ? Map_SuperReadRoot( pSuperBest ) : NULL;
    fParentIsDownstream = ParentAig == s_Stmap78DownstreamAigId;
    Abc_Stmap78FormatPath( pAigs, pNodes, pPhases, 1024 );
    s_nStmap78DemandRows++;
    s_nStmap78LeafDemands++;
    if ( LeafPhase >= 0 && LeafPhase < 2 )
        s_Stmap78LeafPhaseRequests[LeafPhase]++;
    if ( fParentIsDownstream )
        s_nStmap78LeafViaDownstream++;
    printf( "%s demand-leaf: index = %d  co-kind = %s  co-index = %d  parent-node = %d  parent-aig-id = %d  parent-phase = %d  parent-is-downstream = %d  parent-gate = %s  leaf-index = %d  leaf-aig-id = %d  leaf-requested-phase = %d  leaf-inverted-pin = %d  parent-cut-leaves = %d  u-phase-best = %u  path-aigs = %s  path-nodes = %s  path-phases = %s\n",
        s_pStmap78DemandPathLabel, s_nStmap78DemandRows, s_pStmap78RootKind,
        s_iStmap78RootCo, Map_NodeReadNum(pParentRegular), ParentAig,
        ParentPhase, fParentIsDownstream, pParentGate ? Mio_GateReadName(pParentGate) : "?",
        LeafIndex, LeafAig, LeafPhase, !LeafPhase,
        pCutBest ? Map_CutReadLeavesNum(pCutBest) : 0, uPhaseBest,
        pAigs, pNodes, pPhases );
}

void Abc_Stmap78PrintDemandPathSummary( void )
{
    printf( "%s demand-path stats: watch-aig = %d  downstream-aig = %d  rows = %d  target-requests = %d  target-phase0 = %d  target-phase1 = %d  target-via-downstream = %d  leaf-demands = %d  leaf-phase0 = %d  leaf-phase1 = %d  leaf-via-downstream = %d  phase0-direct-or-cached = %d  phase0-inverter-from-opposite = %d\n",
        s_pStmap78DemandPathLabel, s_Stmap78WatchAigId, s_Stmap78DownstreamAigId,
        s_nStmap78DemandRows, s_nStmap78TargetRequests,
        s_Stmap78TargetPhaseRequests[0], s_Stmap78TargetPhaseRequests[1],
        s_nStmap78TargetViaDownstream, s_nStmap78LeafDemands,
        s_Stmap78LeafPhaseRequests[0], s_Stmap78LeafPhaseRequests[1],
        s_nStmap78LeafViaDownstream, s_nStmap78Phase0Direct,
        s_nStmap78Phase0Invert );
}

static int s_fStmap79ParentCutDiag = 0;
static int s_fStmap79ParentCutActive = 0;
static const char * s_pStmap79ParentCutLabel = "stmap79";
static int s_Stmap79ChildAigId = -1;
static int s_Stmap79ParentAigId = -1;
static int s_nStmap79ParentCutRows = 0;
static int s_nStmap79ParentCutRequests = 0;
static int s_Stmap79ParentCutLogged[2] = { 0, 0 };
static int s_Stmap79ParentCutHasCut[2] = { 0, 0 };
static int s_Stmap79ParentCutChildPhase[2] = { 0, 0 };
static int s_nStmap79ParentCutChildMissing = 0;

static void Abc_Stmap79ResetParentCutCounters( void )
{
    s_nStmap79ParentCutRows = 0;
    s_nStmap79ParentCutRequests = 0;
    s_Stmap79ParentCutLogged[0] = 0;
    s_Stmap79ParentCutLogged[1] = 0;
    s_Stmap79ParentCutHasCut[0] = 0;
    s_Stmap79ParentCutHasCut[1] = 0;
    s_Stmap79ParentCutChildPhase[0] = 0;
    s_Stmap79ParentCutChildPhase[1] = 0;
    s_nStmap79ParentCutChildMissing = 0;
}

static void Abc_Stmap79ClearParentCutDiag( void )
{
    s_fStmap79ParentCutDiag = 0;
    s_fStmap79ParentCutActive = 0;
    s_pStmap79ParentCutLabel = "stmap79";
    s_Stmap79ChildAigId = -1;
    s_Stmap79ParentAigId = -1;
    Abc_Stmap79ResetParentCutCounters();
}

void Abc_Stmap79SetParentCutDiag( int fEnable, const char * pLabel, int ChildAigId, int ParentAigId )
{
    Abc_Stmap79ClearParentCutDiag();
    s_pStmap79ParentCutLabel = pLabel && pLabel[0] ? pLabel : "stmap79";
    if ( !fEnable || ChildAigId < 0 || ParentAigId < 0 )
        return;
    s_fStmap79ParentCutDiag = 1;
    s_Stmap79ChildAigId = ChildAigId;
    s_Stmap79ParentAigId = ParentAigId;
}

int Abc_Stmap79ParentCutDiagConfigured( void )
{
    return s_fStmap79ParentCutDiag;
}

void Abc_Stmap79SetParentCutActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap79ParentCutDiag )
        return;
    if ( fActive )
        Abc_Stmap79ResetParentCutCounters();
    s_fStmap79ParentCutActive = fActive;
    (void)pPassLabel;
}

static void Abc_Stmap79RecordParentCutPhase( Map_Node_t * pParentMap, int fPhase )
{
    Map_Cut_t * pCutBest;
    Map_Super_t * pSuperBest;
    Map_Node_t ** ppLeaves;
    Mio_Gate_t * pGate;
    unsigned uPhaseBest = 0;
    int nLeaves = 0, i, ChildLeaf = -1, ChildPhase = -1, LeafAigId[6], LeafPhase[6];
    if ( fPhase < 0 || fPhase > 1 || s_Stmap79ParentCutLogged[fPhase] )
        return;
    s_Stmap79ParentCutLogged[fPhase] = 1;
    for ( i = 0; i < 6; i++ )
    {
        LeafAigId[i] = -1;
        LeafPhase[i] = -1;
    }
    pCutBest = Map_NodeReadCutBest( Map_Regular(pParentMap), fPhase );
    if ( pCutBest )
    {
        s_Stmap79ParentCutHasCut[fPhase] = 1;
        pSuperBest = Map_CutReadSuperBest( pCutBest, fPhase );
        pGate = pSuperBest ? Map_SuperReadRoot( pSuperBest ) : NULL;
        uPhaseBest = Map_CutReadPhaseBest( pCutBest, fPhase );
        nLeaves = Map_CutReadLeavesNum( pCutBest );
        ppLeaves = Map_CutReadLeaves( pCutBest );
        for ( i = 0; i < nLeaves && i < 6; i++ )
        {
            LeafAigId[i] = Map_NodeReadAigId( Map_Regular(ppLeaves[i]) );
            LeafPhase[i] = ((uPhaseBest & (1 << i)) > 0) ? 0 : 1;
            if ( LeafAigId[i] == s_Stmap79ChildAigId )
            {
                ChildLeaf = i;
                ChildPhase = LeafPhase[i];
            }
        }
    }
    else
    {
        pSuperBest = NULL;
        pGate = NULL;
    }
    if ( ChildPhase >= 0 && ChildPhase < 2 )
        s_Stmap79ParentCutChildPhase[ChildPhase]++;
    else if ( pCutBest )
        s_nStmap79ParentCutChildMissing++;
    s_nStmap79ParentCutRows++;
    printf( "%s parent-cut: index = %d  parent-aig = %d  child-aig = %d  phase = %d  has-cut = %d  gate = %s  leaves = %d  u-phase-best = %u  fanout-limit = %d  child-leaf-index = %d  child-requested-phase = %d  child-inverted-pin = %d  leaf0-aig-id = %d  leaf0-phase = %d  leaf1-aig-id = %d  leaf1-phase = %d  leaf2-aig-id = %d  leaf2-phase = %d  leaf3-aig-id = %d  leaf3-phase = %d  leaf4-aig-id = %d  leaf4-phase = %d  leaf5-aig-id = %d  leaf5-phase = %d\n",
        s_pStmap79ParentCutLabel, s_nStmap79ParentCutRows,
        s_Stmap79ParentAigId, s_Stmap79ChildAigId, fPhase, pCutBest != NULL,
        pGate ? Mio_GateReadName(pGate) : "?", nLeaves, uPhaseBest,
        pSuperBest ? Map_SuperReadFanoutLimit(pSuperBest) : -1, ChildLeaf,
        ChildPhase, ChildPhase >= 0 ? !ChildPhase : -1,
        LeafAigId[0], LeafPhase[0], LeafAigId[1], LeafPhase[1],
        LeafAigId[2], LeafPhase[2], LeafAigId[3], LeafPhase[3],
        LeafAigId[4], LeafPhase[4], LeafAigId[5], LeafPhase[5] );
}

static void Abc_Stmap79RecordParentCuts( Map_Node_t * pNodeMap )
{
    Map_Node_t * pNodeRegular;
    if ( !s_fStmap79ParentCutDiag || !s_fStmap79ParentCutActive || pNodeMap == NULL )
        return;
    pNodeRegular = Map_Regular( pNodeMap );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return;
    if ( Map_NodeReadAigId( pNodeRegular ) != s_Stmap79ParentAigId )
        return;
    s_nStmap79ParentCutRequests++;
    Abc_Stmap79RecordParentCutPhase( pNodeRegular, 0 );
    Abc_Stmap79RecordParentCutPhase( pNodeRegular, 1 );
}

void Abc_Stmap79PrintParentCutSummary( void )
{
    printf( "%s parent-cut stats: child-aig = %d  parent-aig = %d  requests = %d  rows = %d  phase0-has-cut = %d  phase1-has-cut = %d  child-phase0 = %d  child-phase1 = %d  child-missing = %d\n",
        s_pStmap79ParentCutLabel, s_Stmap79ChildAigId, s_Stmap79ParentAigId,
        s_nStmap79ParentCutRequests, s_nStmap79ParentCutRows,
        s_Stmap79ParentCutHasCut[0], s_Stmap79ParentCutHasCut[1],
        s_Stmap79ParentCutChildPhase[0], s_Stmap79ParentCutChildPhase[1],
        s_nStmap79ParentCutChildMissing );
}

/**Function*************************************************************

  Synopsis    [Interface with the mapping package.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkMap( Abc_Ntk_t * pNtk, Mio_Library_t* userLib, double DelayTarget, double AreaMulti, double DelayMulti, float LogFan, float Slew, float Gain, int nGatesMin, int fRecovery, int fSwitching, int fSkipFanout, int fUseProfile, int fUseBuffs, int fVerbose )
{
    static int fUseMulti = 0;
    int fShowSwitching = 1;
    Abc_Ntk_t * pNtkNew;
    Map_Man_t * pMan;
    Vec_Int_t * vSwitching = NULL;
    float * pSwitching = NULL;
    abctime clk, clkTotal = Abc_Clock();
    Mio_Library_t * pLib = (Mio_Library_t *)Abc_FrameReadLibGen();

    assert( Abc_NtkIsStrash(pNtk) );
    // derive library from SCL
    // if the library is created here, it will be deleted when pSuperLib is deleted in Map_SuperLibFree()
    if ( Abc_FrameReadLibScl() && Abc_SclHasDelayInfo( Abc_FrameReadLibScl() ) )
    {
        if ( pLib && Mio_LibraryHasProfile(pLib) )
            pLib = Abc_SclDeriveGenlib( Abc_FrameReadLibScl(), pLib, Slew, Gain, nGatesMin, fVerbose );
        else
            pLib = Abc_SclDeriveGenlib( Abc_FrameReadLibScl(), NULL, Slew, Gain, nGatesMin, fVerbose );
        if ( Abc_FrameReadLibGen() )
        {
            Mio_LibraryTransferDelays( (Mio_Library_t *)Abc_FrameReadLibGen(), pLib );
            Mio_LibraryTransferProfile( pLib, (Mio_Library_t *)Abc_FrameReadLibGen() );
        }
        // remove supergate library
        Map_SuperLibFree( (Map_SuperLib_t *)Abc_FrameReadLibSuper() );
        Abc_FrameSetLibSuper( NULL );
    }

    if ( userLib != NULL ) {
        pLib = userLib;
    }
 
    // quit if there is no library
    if ( pLib == NULL )
    {
        printf( "The current library is not available.\n" );
        return 0;
    }
    if ( AreaMulti != 0.0 )
        fUseMulti = 1, printf( "The cell areas are multiplied by the factor: <num_fanins> ^ (%.2f).\n", AreaMulti );
    if ( DelayMulti != 0.0 )
        fUseMulti = 1, printf( "The cell delays are multiplied by the factor: <num_fanins> ^ (%.2f).\n", DelayMulti );

    // penalize large gates by increasing their area
    if ( AreaMulti != 0.0 )
        Mio_LibraryMultiArea( pLib, AreaMulti );
    if ( DelayMulti != 0.0 )
        Mio_LibraryMultiDelay( pLib, DelayMulti );

    // derive the supergate library
    if ( fUseMulti || Abc_FrameReadLibSuper() == NULL )
    {
        if ( fVerbose )
            printf( "Converting \"%s\" into supergate library \"%s\".\n", 
                Mio_LibraryReadName(pLib), Extra_FileNameGenericAppend(Mio_LibraryReadName(pLib), ".super") );
        // compute supergate library to be used for mapping
        if ( Mio_LibraryHasProfile(pLib) )
            printf( "Abc_NtkMap(): Genlib library has profile.\n" );
        Map_SuperLibDeriveFromGenlib( pLib, fVerbose );
    }

    // return the library to normal
    if ( AreaMulti != 0.0 )
        Mio_LibraryMultiArea( (Mio_Library_t *)Abc_FrameReadLibGen(), -AreaMulti );
    if ( DelayMulti != 0.0 )
        Mio_LibraryMultiDelay( (Mio_Library_t *)Abc_FrameReadLibGen(), -DelayMulti );

    // print a warning about choice nodes
    if ( fVerbose && Abc_NtkGetChoiceNum( pNtk ) )
        printf( "Performing mapping with choices.\n" );

    // compute switching activity
    fShowSwitching |= fSwitching;
    if ( fShowSwitching )
    {
        extern Vec_Int_t * Sim_NtkComputeSwitching( Abc_Ntk_t * pNtk, int nPatterns );
        vSwitching = Sim_NtkComputeSwitching( pNtk, 4096 );
        pSwitching = (float *)vSwitching->pArray;
    }

    // perform the mapping
    pMan = Abc_NtkToMap( pNtk, DelayTarget, fRecovery, pSwitching, fVerbose );
    if ( pSwitching ) Vec_IntFree( vSwitching );
    if ( pMan == NULL )
        return NULL;
clk = Abc_Clock();
    Map_ManSetSwitching( pMan, fSwitching );
    Map_ManSetSkipFanout( pMan, fSkipFanout );
    if ( fUseProfile )
        Map_ManSetUseProfile( pMan );
    if ( LogFan != 0 )
        Map_ManCreateNodeDelays( pMan, LogFan );
    if ( !Map_Mapping( pMan ) )
    {
        Map_ManFree( pMan );
        return NULL;
    }
//    Map_ManPrintStatsToFile( pNtk->pSpec, Map_ManReadAreaFinal(pMan), Map_ManReadRequiredGlo(pMan), Abc_Clock()-clk );

    // reconstruct the network after mapping (use buffers when user requested or in the area mode)
    pNtkNew = Abc_NtkFromMap( pMan, pNtk, fUseBuffs || (DelayTarget == (double)ABC_INFINITY) );
    if ( Mio_LibraryHasProfile(pLib) )
        Mio_LibraryTransferProfile2( (Mio_Library_t *)Abc_FrameReadLibGen(), pLib );
    Map_ManFree( pMan );
    if ( pNtkNew == NULL )
        return NULL;

    if ( pNtk->pExdc )
        pNtkNew->pExdc = Abc_NtkDup( pNtk->pExdc );
if ( fVerbose )
{
ABC_PRT( "Total runtime", Abc_Clock() - clkTotal );
}

    // make sure that everything is okay
    if ( !Abc_NtkCheck( pNtkNew ) )
    {
        printf( "Abc_NtkMap: The network check has failed.\n" );
        Abc_NtkDelete( pNtkNew );
        return NULL;
    }
    return pNtkNew;
}

/**Function*************************************************************

  Synopsis    [Load the network into manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Map_Time_t * Abc_NtkMapCopyCiArrival( Abc_Ntk_t * pNtk, Abc_Time_t * ppTimes )
{
    Map_Time_t * p;
    int i;
    p = ABC_CALLOC( Map_Time_t, Abc_NtkCiNum(pNtk) );
    for ( i = 0; i < Abc_NtkCiNum(pNtk); i++ )
    {
        p[i].Fall = ppTimes[i].Fall;
        p[i].Rise = ppTimes[i].Rise;
        p[i].Worst = Abc_MaxFloat( p[i].Fall, p[i].Rise );
    }
    ABC_FREE( ppTimes );
    return p;
}
Map_Time_t * Abc_NtkMapCopyCoRequired( Abc_Ntk_t * pNtk, Abc_Time_t * ppTimes )
{
    Map_Time_t * p;
    int i;
    p = ABC_CALLOC( Map_Time_t, Abc_NtkCoNum(pNtk) );
    for ( i = 0; i < Abc_NtkCoNum(pNtk); i++ )
    {
        p[i].Fall = ppTimes[i].Fall;
        p[i].Rise = ppTimes[i].Rise;
        p[i].Worst = Abc_MaxFloat( p[i].Fall, p[i].Rise );
    }
    ABC_FREE( ppTimes );
    return p;
}

/**Function*************************************************************

  Synopsis    [Load the network into manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Map_Time_t * Abc_NtkMapCopyCiArrivalCon( Abc_Ntk_t * pNtk )
{
    Map_Time_t * p; int i;
    p = ABC_CALLOC( Map_Time_t, Abc_NtkCiNum(pNtk) );
    for ( i = 0; i < Abc_NtkCiNum(pNtk); i++ )
        p[i].Fall = p[i].Rise = p[i].Worst = Scl_Int2Flt( Scl_ConGetInArr(i) );
    return p;
}
Map_Time_t * Abc_NtkMapCopyCoRequiredCon( Abc_Ntk_t * pNtk )
{
    Map_Time_t * p; int i;
    p = ABC_CALLOC( Map_Time_t, Abc_NtkCoNum(pNtk) );
    for ( i = 0; i < Abc_NtkCoNum(pNtk); i++ )
        p[i].Fall = p[i].Rise = p[i].Worst = Scl_Int2Flt( Scl_ConGetOutReq(i) );
    return p;
}

/**Function*************************************************************

  Synopsis    [Load the network into manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Map_Man_t * Abc_NtkToMap( Abc_Ntk_t * pNtk, double DelayTarget, int fRecovery, float * pSwitching, int fVerbose )
{
    Map_Man_t * pMan;
    Map_Node_t * pNodeMap;
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pNode, * pFanin, * pPrev;
    int i;

    assert( Abc_NtkIsStrash(pNtk) );

    // start the mapping manager and set its parameters
    pMan = Map_ManCreate( Abc_NtkPiNum(pNtk) + Abc_NtkLatchNum(pNtk) - pNtk->nBarBufs, Abc_NtkPoNum(pNtk) + Abc_NtkLatchNum(pNtk) - pNtk->nBarBufs, fVerbose );
    if ( pMan == NULL )
        return NULL;
    Map_ManSetAreaRecovery( pMan, fRecovery );
    Map_ManSetOutputNames( pMan, Abc_NtkCollectCioNames(pNtk, 1) );
    Map_ManSetDelayTarget( pMan, (float)DelayTarget );
    Map_ManCreateAigIds( pMan, Abc_NtkObjNumMax(pNtk) );

    // set arrival and requireds
    if ( Scl_ConIsRunning() && Scl_ConHasInArrs() )
        Map_ManSetInputArrivals( pMan, Abc_NtkMapCopyCiArrivalCon(pNtk) );
    else
        Map_ManSetInputArrivals( pMan, Abc_NtkMapCopyCiArrival(pNtk, Abc_NtkGetCiArrivalTimes(pNtk)) );
    if ( Scl_ConIsRunning() && Scl_ConHasOutReqs() )
        Map_ManSetOutputRequireds( pMan, Abc_NtkMapCopyCoRequiredCon(pNtk) );
    else
        Map_ManSetOutputRequireds( pMan, Abc_NtkMapCopyCoRequired(pNtk, Abc_NtkGetCoRequiredTimes(pNtk)) );

    // create PIs and remember them in the old nodes
    Abc_NtkCleanCopy( pNtk );
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)Map_ManReadConst1(pMan);
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        if ( i == Abc_NtkCiNum(pNtk) - pNtk->nBarBufs )
            break;
        pNodeMap = Map_ManReadInputs(pMan)[i];
        pNode->pCopy = (Abc_Obj_t *)pNodeMap;
        if ( pSwitching )
            Map_NodeSetSwitching( pNodeMap, pSwitching[pNode->Id] );
        Map_NodeSetAigId( pNodeMap, pNode->Id );
    }

    // load the AIG into the mapper
    vNodes = Abc_AigDfsMap( pNtk );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
    {
        if ( Abc_ObjIsLatch(pNode) )
        {
            pFanin = Abc_ObjFanin0(pNode);
            pNodeMap = Map_NodeBuf( pMan, Map_NotCond( Abc_ObjFanin0(pFanin)->pCopy, (int)Abc_ObjFaninC0(pFanin) ) );
            Abc_ObjFanout0(pNode)->pCopy = (Abc_Obj_t *)pNodeMap;
            continue;
        }
        assert( Abc_ObjIsNode(pNode) );
        // add the node to the mapper
        pNodeMap = Map_NodeAnd( pMan, 
            Map_NotCond( Abc_ObjFanin0(pNode)->pCopy, (int)Abc_ObjFaninC0(pNode) ),
            Map_NotCond( Abc_ObjFanin1(pNode)->pCopy, (int)Abc_ObjFaninC1(pNode) ) );
        assert( pNode->pCopy == NULL );
        // remember the node
        pNode->pCopy = (Abc_Obj_t *)pNodeMap;
        if ( pSwitching )
            Map_NodeSetSwitching( pNodeMap, pSwitching[pNode->Id] );
        // set up the choice node
        if ( Abc_AigNodeIsChoice( pNode ) )
            for ( pPrev = pNode, pFanin = (Abc_Obj_t *)pNode->pData; pFanin; pPrev = pFanin, pFanin = (Abc_Obj_t *)pFanin->pData )
            {
                Map_NodeSetNextE( (Map_Node_t *)pPrev->pCopy, (Map_Node_t *)pFanin->pCopy );
                Map_NodeSetRepr( (Map_Node_t *)pFanin->pCopy, (Map_Node_t *)pNode->pCopy );
            }
        Map_NodeSetAigId( pNodeMap, pNode->Id );
    }
    assert( Map_ManReadBufNum(pMan) == pNtk->nBarBufs );
    Vec_PtrFree( vNodes );

    // set the primary outputs in the required phase
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        if ( i == Abc_NtkCoNum(pNtk) - pNtk->nBarBufs )
            break;
        Map_ManReadOutputs(pMan)[i] = Map_NotCond( (Map_Node_t *)Abc_ObjFanin0(pNode)->pCopy, (int)Abc_ObjFaninC0(pNode) );
    }
    return pMan;
}

/**Function*************************************************************

  Synopsis    [Creates the mapped network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NodeFromMapSuper_rec( Abc_Ntk_t * pNtkNew, Map_Node_t * pNodeMap, Map_Super_t * pSuper, Abc_Obj_t * pNodePis[], int nNodePis )
{
    Mio_Library_t * pLib = (Mio_Library_t *)Abc_FrameReadLibGen();
    Mio_Gate_t * pRoot;
    Map_Super_t ** ppFanins;
    Abc_Obj_t * pNodeNew, * pNodeFanin;
    int nFanins, Number, i;

    // get the parameters of the supergate
    pRoot = Map_SuperReadRoot(pSuper);
    if ( pRoot == NULL )
    {
        Number = Map_SuperReadNum(pSuper);
        if ( Number < nNodePis )  
        {
            return pNodePis[Number];
        }
        else
        {  
//            assert( 0 );
            /* It might happen that a super gate with 5 inputs is constructed that
             * actually depends only on the first four variables; i.e the fifth is a
             * don't care -- in that case we connect constant node for the fifth
             * (since the cut only has 4 variables). An interesting question is what
             * if the first variable (and not the fifth one is the redundant one;
             * can that happen?) */
            return Abc_NtkCreateNodeConst0(pNtkNew);
        }
    }
    pRoot = Mio_LibraryReadGateByName( pLib, Mio_GateReadName(pRoot), NULL );

    // get information about the fanins of the supergate
    nFanins  = Map_SuperReadFaninNum( pSuper );
    ppFanins = Map_SuperReadFanins( pSuper );
    // create a new node with these fanins
    pNodeNew = Abc_NtkCreateNode( pNtkNew );
    for ( i = 0; i < nFanins; i++ )
    {
        pNodeFanin = Abc_NodeFromMapSuper_rec( pNtkNew, pNodeMap, ppFanins[i], pNodePis, nNodePis );
        Abc_ObjAddFanin( pNodeNew, pNodeFanin );
    }
    pNodeNew->pData = pRoot;
    return pNodeNew;
}
Abc_Obj_t * Abc_NodeFromMapPhase_rec( Abc_Ntk_t * pNtkNew, Map_Node_t * pNodeMap, int fPhase )
{
    Abc_Obj_t * pNodePIs[10];
    Abc_Obj_t * pNodeNew;
    Map_Node_t ** ppLeaves;
    Map_Cut_t * pCutBest;
    Map_Super_t * pSuperBest;
    unsigned uPhaseBest;
    int i, fInvPin, nLeaves;

    // make sure the node can be implemented in this phase
    assert( Map_NodeReadCutBest(pNodeMap, fPhase) != NULL || Map_NodeIsConst(pNodeMap) );
    // check if the phase is already implemented
    pNodeNew = (Abc_Obj_t *)Map_NodeReadData( pNodeMap, fPhase );
    if ( pNodeNew )
    {
        Abc_Stmap77RecordReconstructionCache( pNodeMap, fPhase, pNodeNew, "phase" );
        return pNodeNew;
    }

    // get the information about the best cut 
    pCutBest   = Map_NodeReadCutBest( pNodeMap, fPhase );
    pSuperBest = Map_CutReadSuperBest( pCutBest, fPhase );
    uPhaseBest = Map_CutReadPhaseBest( pCutBest, fPhase );
    nLeaves    = Map_CutReadLeavesNum( pCutBest );
    ppLeaves   = Map_CutReadLeaves( pCutBest );
    //Vec_Ptr_t * vAnds = Map_CutInternalNodes( pNodeMap, pCutBest );

    // collect the PI nodes
    for ( i = 0; i < nLeaves; i++ )
    {
        fInvPin = ((uPhaseBest & (1 << i)) > 0);
        Abc_Stmap78RecordLeafDemand( pNodeMap, fPhase, ppLeaves[i], !fInvPin, i, pCutBest, pSuperBest, uPhaseBest );
        pNodePIs[i] = Abc_NodeFromMap_rec( pNtkNew, ppLeaves[i], !fInvPin );
        assert( pNodePIs[i] != NULL );
    }

    // implement the supergate
    pNodeNew = Abc_NodeFromMapSuper_rec( pNtkNew, pNodeMap, pSuperBest, pNodePIs, nLeaves );
    Vec_IntWriteEntry( pNtkNew->vOrigNodeIds, pNodeNew->Id, Abc_Var2Lit( Map_NodeReadAigId(pNodeMap), fPhase ) );
    Map_NodeSetData( pNodeMap, fPhase, (char *)pNodeNew );
    Abc_Stmap77RecordReconstructionEmit( pNodeMap, fPhase, pNodeNew, pCutBest, pSuperBest, uPhaseBest );
    return pNodeNew;
}
Abc_Obj_t * Abc_NodeFromMap_rec( Abc_Ntk_t * pNtkNew, Map_Node_t * pNodeMap, int fPhase )
{
    Abc_Obj_t * pNodeNew, * pNodeInv;
    int fPushed = 0, fOldPathPhase = -1;

    // check the case of constant node
    if ( Map_NodeIsConst(pNodeMap) )
    {
        pNodeNew = fPhase? Abc_NtkCreateNodeConst1(pNtkNew) : Abc_NtkCreateNodeConst0(pNtkNew);
        if ( pNodeNew->pData == NULL )
            printf( "Error creating mapped network: Library does not have a constant %d gate.\n", fPhase );
        return pNodeNew;
    }

    Abc_Stmap79RecordParentCuts( pNodeMap );
    Abc_Stmap78DemandPathPush( pNodeMap, fPhase );
    fPushed = s_fStmap78DemandPathDiag && s_fStmap78DemandPathActive;
    Abc_Stmap77RecordReconstructionRequest( pNodeMap, fPhase );

    // check if the phase is already implemented
    pNodeNew = (Abc_Obj_t *)Map_NodeReadData( pNodeMap, fPhase );
    if ( pNodeNew )
    {
        Abc_Stmap77RecordReconstructionCache( pNodeMap, fPhase, pNodeNew, "request" );
        if ( fPushed )
            Abc_Stmap78DemandPathPop();
        return pNodeNew;
    }

    // implement the node if the best cut is assigned
    if ( Map_NodeReadCutBest(pNodeMap, fPhase) != NULL )
    {
        pNodeNew = Abc_NodeFromMapPhase_rec( pNtkNew, pNodeMap, fPhase );
        if ( fPushed )
            Abc_Stmap78DemandPathPop();
        return pNodeNew;
    }

    // if the cut is not assigned, implement the node
    assert( Map_NodeReadCutBest(pNodeMap, !fPhase) != NULL || Map_NodeIsConst(pNodeMap) );
    if ( fPushed )
        fOldPathPhase = Abc_Stmap78DemandPathSetTopPhase( !fPhase );
    pNodeNew = Abc_NodeFromMapPhase_rec( pNtkNew, pNodeMap, !fPhase );
    if ( fPushed && fOldPathPhase >= 0 )
        Abc_Stmap78DemandPathSetTopPhase( fOldPathPhase );

    // add the inverter
    pNodeInv = Abc_NtkCreateNode( pNtkNew );
    Vec_IntWriteEntry( pNtkNew->vOrigNodeIds, pNodeInv->Id, Abc_Var2Lit( Map_NodeReadAigId(pNodeMap), fPhase ) );
    Abc_ObjAddFanin( pNodeInv, pNodeNew );
    pNodeInv->pData = Mio_LibraryReadInv((Mio_Library_t *)Abc_FrameReadLibGen());

    // set the inverter
    Map_NodeSetData( pNodeMap, fPhase, (char *)pNodeInv );
    Abc_Stmap77RecordReconstructionInverter( pNodeMap, fPhase, pNodeNew, pNodeInv );
    if ( fPushed )
        Abc_Stmap78DemandPathPop();
    return pNodeInv;
}
Abc_Ntk_t * Abc_NtkFromMap( Map_Man_t * pMan, Abc_Ntk_t * pNtk, int fUseBuffs )
{
    Abc_Ntk_t * pNtkNew;
    Map_Node_t * pNodeMap;
    Abc_Obj_t * pNode, * pNodeNew;
    int i, nDupGates;
    assert( Map_ManReadBufNum(pMan) == pNtk->nBarBufs );
    // create the new network
    pNtkNew = Abc_NtkStartFrom( pNtk, ABC_NTK_LOGIC, ABC_FUNC_MAP );
    pNtkNew->vOrigNodeIds = Vec_IntStartFull( 2 * Abc_NtkObjNumMax(pNtk) );
    // make the mapper point to the new network
    Map_ManCleanData( pMan );
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        if ( i >= Abc_NtkCiNum(pNtk) - pNtk->nBarBufs )
            break;
        Map_NodeSetData( Map_ManReadInputs(pMan)[i], 1, (char *)pNode->pCopy );
    }
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        if ( i < Abc_NtkCiNum(pNtk) - pNtk->nBarBufs )
            continue;
        Map_NodeSetData( Map_ManReadBufs(pMan)[i - (Abc_NtkCiNum(pNtk) - pNtk->nBarBufs)], 1, (char *)pNode->pCopy );
    }
    // assign the mapping of the required phase to the POs
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        if ( i < Abc_NtkCoNum(pNtk) - pNtk->nBarBufs )
            continue;
        pNodeMap = Map_ManReadBufDriver( pMan, i - (Abc_NtkCoNum(pNtk) - pNtk->nBarBufs) );
        Abc_Stmap77RecordReconstructionDemand( "barbuf", i, pNodeMap, !Map_IsComplement(pNodeMap) );
        Abc_Stmap78DemandPathSetRoot( "barbuf", i );
        pNodeNew = Abc_NodeFromMap_rec( pNtkNew, Map_Regular(pNodeMap), !Map_IsComplement(pNodeMap) );
        assert( !Abc_ObjIsComplement(pNodeNew) );
        Abc_ObjAddFanin( pNode->pCopy, pNodeNew );
    }
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        if ( i >= Abc_NtkCoNum(pNtk) - pNtk->nBarBufs )
            break;
        pNodeMap = Map_ManReadOutputs(pMan)[i];
        Abc_Stmap77RecordReconstructionDemand( "co", i, pNodeMap, !Map_IsComplement(pNodeMap) );
        Abc_Stmap78DemandPathSetRoot( "co", i );
        pNodeNew = Abc_NodeFromMap_rec( pNtkNew, Map_Regular(pNodeMap), !Map_IsComplement(pNodeMap) );
        assert( !Abc_ObjIsComplement(pNodeNew) );
        Abc_ObjAddFanin( pNode->pCopy, pNodeNew );
    }
    // decouple the PO driver nodes to reduce the number of levels
    nDupGates = Abc_NtkLogicMakeSimpleCos( pNtkNew, !fUseBuffs );
//    if ( nDupGates && Map_ManReadVerbose(pMan) )
//        printf( "Duplicated %d gates to decouple the CO drivers.\n", nDupGates );
    return pNtkNew;
}

/**Function*************************************************************

  Synopsis    [Interface with the mapping package.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkSuperChoice( Abc_Ntk_t * pNtk )
{
    Abc_Ntk_t * pNtkNew;

    Map_Man_t * pMan;

    assert( Abc_NtkIsStrash(pNtk) );

    // check that the library is available
    if ( Abc_FrameReadLibGen() == NULL )
    {
        printf( "The current library is not available.\n" );
        return 0;
    }

    // derive the supergate library
    if ( Abc_FrameReadLibSuper() == NULL && Abc_FrameReadLibGen() )
    {
//        printf( "A simple supergate library is derived from gate library \"%s\".\n", 
//            Mio_LibraryReadName((Mio_Library_t *)Abc_FrameReadLibGen()) );
        Map_SuperLibDeriveFromGenlib( (Mio_Library_t *)Abc_FrameReadLibGen(), 0 );
    }

    // print a warning about choice nodes
    if ( Abc_NtkGetChoiceNum( pNtk ) )
        printf( "Performing mapping with choices.\n" );

    // perform the mapping
    pMan = Abc_NtkToMap( pNtk, -1, 1, NULL, 0 );
    if ( pMan == NULL )
        return NULL;
    if ( !Map_Mapping( pMan ) )
    {
        Map_ManFree( pMan );
        return NULL;
    }

    // reconstruct the network after mapping
    pNtkNew = Abc_NtkFromMapSuperChoice( pMan, pNtk );
    if ( pNtkNew == NULL )
        return NULL;
    Map_ManFree( pMan );

    // make sure that everything is okay
    if ( !Abc_NtkCheck( pNtkNew ) )
    {
        printf( "Abc_NtkMap: The network check has failed.\n" );
        Abc_NtkDelete( pNtkNew );
        return NULL;
    }
    return pNtkNew;
}


/**Function*************************************************************

  Synopsis    [Creates the mapped network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkFromMapSuperChoice( Map_Man_t * pMan, Abc_Ntk_t * pNtk )
{
    extern Abc_Ntk_t * Abc_NtkMulti( Abc_Ntk_t * pNtk, int nThresh, int nFaninMax, int fCnf, int fMulti, int fSimple, int fFactor );
    ProgressBar * pProgress;
    Abc_Ntk_t * pNtkNew, * pNtkNew2;
    Abc_Obj_t * pNode;
    int i;

    // save the pointer to the mapped nodes
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pNext = pNode->pCopy;
    Abc_NtkForEachPo( pNtk, pNode, i )
        pNode->pNext = pNode->pCopy;
    Abc_NtkForEachNode( pNtk, pNode, i )
        pNode->pNext = pNode->pCopy;

    // duplicate the network
    pNtkNew2 = Abc_NtkDup( pNtk );
    pNtkNew  = Abc_NtkMulti( pNtkNew2, 0, 20, 0, 0, 1, 0 );
    if ( !Abc_NtkBddToSop( pNtkNew, -1, ABC_INFINITY, 1 ) )
    {
        printf( "Abc_NtkFromMapSuperChoice(): Converting to SOPs has failed.\n" );
        return NULL;
    }

    // set the old network to point to the new network
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pCopy = pNode->pCopy->pCopy;
    Abc_NtkForEachPo( pNtk, pNode, i )
        pNode->pCopy = pNode->pCopy->pCopy;
    Abc_NtkForEachNode( pNtk, pNode, i )
        pNode->pCopy = pNode->pCopy->pCopy;
    Abc_NtkDelete( pNtkNew2 );

    // set the pointers from the mapper to the new nodes
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        Map_NodeSetData( Map_ManReadInputs(pMan)[i], 0, (char *)Abc_NtkCreateNodeInv(pNtkNew,pNode->pCopy) );
        Map_NodeSetData( Map_ManReadInputs(pMan)[i], 1, (char *)pNode->pCopy );
    }
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        Map_NodeSetData( (Map_Node_t *)pNode->pNext, 0, (char *)Abc_NtkCreateNodeInv(pNtkNew,pNode->pCopy) );
        Map_NodeSetData( (Map_Node_t *)pNode->pNext, 1, (char *)pNode->pCopy );
    }

    // assign the mapping of the required phase to the POs
    pProgress = Extra_ProgressBarStart( stdout, Abc_NtkObjNumMax(pNtk) );
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        Extra_ProgressBarUpdate( pProgress, i, NULL );
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        Abc_NodeSuperChoice( pNtkNew, pNode );
    }
    Extra_ProgressBarStop( pProgress );
    return pNtkNew;
}


/**Function*************************************************************

  Synopsis    [Creates the mapped network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeSuperChoice( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pNode )
{
    Map_Node_t * pMapNode = (Map_Node_t *)pNode->pNext;
    Map_Cut_t * pCuts, * pTemp;

    pCuts = Map_NodeReadCuts(pMapNode);
    for ( pTemp = Map_CutReadNext(pCuts); pTemp; pTemp = Map_CutReadNext(pTemp) )
    {
        Abc_NodeFromMapCutPhase( pNtkNew, pTemp, 0 );
        Abc_NodeFromMapCutPhase( pNtkNew, pTemp, 1 );
    }
}


/**Function*************************************************************

  Synopsis    [Constructs the nodes corrresponding to one node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeFromMapCutPhase( Abc_Ntk_t * pNtkNew, Map_Cut_t * pCut, int fPhase )
{
    Abc_Obj_t * pNodePIs[10];
    Map_Node_t ** ppLeaves;
    Map_Super_t * pSuperBest;
    unsigned uPhaseBest;
    int i, fInvPin, nLeaves;

    pSuperBest = Map_CutReadSuperBest( pCut, fPhase );
    if ( pSuperBest == NULL )
        return;

    // get the information about the best cut 
    uPhaseBest = Map_CutReadPhaseBest( pCut, fPhase );
    nLeaves    = Map_CutReadLeavesNum( pCut );
    ppLeaves   = Map_CutReadLeaves( pCut );

    // collect the PI nodes
    for ( i = 0; i < nLeaves; i++ )
    {
        fInvPin = ((uPhaseBest & (1 << i)) > 0);
        pNodePIs[i] = (Abc_Obj_t *)Map_NodeReadData( ppLeaves[i], !fInvPin );
        assert( pNodePIs[i] != NULL );
    }

    // implement the supergate
    Abc_NodeFromMapSuperChoice_rec( pNtkNew, pSuperBest, pNodePIs, nLeaves );
}


/**Function*************************************************************

  Synopsis    [Constructs the nodes corrresponding to one supergate.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NodeFromMapSuperChoice_rec( Abc_Ntk_t * pNtkNew, Map_Super_t * pSuper, Abc_Obj_t * pNodePis[], int nNodePis )
{
    Mio_Library_t * pLib = (Mio_Library_t *)Abc_FrameReadLibGen();
    Mio_Gate_t * pRoot;
    Map_Super_t ** ppFanins;
    Abc_Obj_t * pNodeNew, * pNodeFanin;
    int nFanins, Number, i;

    // get the parameters of the supergate
    pRoot = Map_SuperReadRoot(pSuper);
    if ( pRoot == NULL )
    {
        Number = Map_SuperReadNum(pSuper);
        if ( Number < nNodePis )  
        {
            return pNodePis[Number];
        }
        else
        {  
//            assert( 0 );
            /* It might happen that a super gate with 5 inputs is constructed that
             * actually depends only on the first four variables; i.e the fifth is a
             * don't care -- in that case we connect constant node for the fifth
             * (since the cut only has 4 variables). An interesting question is what
             * if the first variable (and not the fifth one is the redundant one;
             * can that happen?) */
            return Abc_NtkCreateNodeConst0(pNtkNew);
        }
    }
    pRoot = Mio_LibraryReadGateByName( pLib, Mio_GateReadName(pRoot), NULL );

    // get information about the fanins of the supergate
    nFanins  = Map_SuperReadFaninNum( pSuper );
    ppFanins = Map_SuperReadFanins( pSuper );
    // create a new node with these fanins
    pNodeNew = Abc_NtkCreateNode( pNtkNew );
    for ( i = 0; i < nFanins; i++ )
    {
        pNodeFanin = Abc_NodeFromMapSuperChoice_rec( pNtkNew, ppFanins[i], pNodePis, nNodePis );
        Abc_ObjAddFanin( pNodeNew, pNodeFanin );
    }
    pNodeNew->pData = Abc_SopRegister( (Mem_Flex_t *)pNtkNew->pManFunc, Mio_GateReadSop(pRoot) );
    return pNodeNew;
}

/**Function*************************************************************

  Synopsis    [Returns the twin node if it exists.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NtkFetchTwinNode( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNode2;
    Mio_Gate_t * pGate = (Mio_Gate_t *)pNode->pData;
    assert( Abc_NtkHasMapping(pNode->pNtk) );
    if ( pGate == NULL || Mio_GateReadTwin(pGate) == NULL )
        return NULL;
    // assuming the twin node is following next
    if ( (int)Abc_ObjId(pNode) == Abc_NtkObjNumMax(pNode->pNtk) - 1 )
        return NULL;
    pNode2 = Abc_NtkObj( pNode->pNtk, Abc_ObjId(pNode) + 1 );
    if ( pNode2 == NULL || !Abc_ObjIsNode(pNode2) || Abc_ObjFaninNum(pNode) != Abc_ObjFaninNum(pNode2) )
        return NULL;
    if ( Mio_GateReadTwin(pGate) != (Mio_Gate_t *)pNode2->pData )
        return NULL;
    return pNode2;
}


/**Function*************************************************************

  Synopsis    [Dumps mapped network in the mini-mapped format.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Abc_NtkWriteMiniMapping( Abc_Ntk_t * pNtk )
{
    Vec_Ptr_t * vNodes;
    Vec_Int_t * vMapping;
    Vec_Str_t * vGates;
    Abc_Obj_t * pObj, * pFanin;
    int i, k, nNodes, nFanins, nExtra, * pArray;
    assert( Abc_NtkHasMapping(pNtk) );
    // collect nodes in the DFS order
    vNodes = Abc_NtkDfs( pNtk, 0 );
    // assign unique numbers
    nNodes = nFanins = 0;
    Abc_NtkForEachCi( pNtk, pObj, i )
        pObj->iTemp = nNodes++;
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pObj, i )
        pObj->iTemp = nNodes++, nFanins += Abc_ObjFaninNum(pObj);
    // allocate attay to store mapping (4 counters + fanins for each node + PO drivers + gate names)
    vMapping = Vec_IntAlloc( 4 + Abc_NtkNodeNum(pNtk) + nFanins + Abc_NtkCoNum(pNtk) + 10000 );
    // write the numbers of CI/CO/Node/FF
    Vec_IntPush( vMapping, Abc_NtkCiNum(pNtk) );
    Vec_IntPush( vMapping, Abc_NtkCoNum(pNtk) );
    Vec_IntPush( vMapping, Vec_PtrSize(vNodes) );
    Vec_IntPush( vMapping, Abc_NtkLatchNum(pNtk) );
    // write the nodes
    vGates = Vec_StrAlloc( 10000 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pObj, i )
    {
        Vec_IntPush( vMapping, Abc_ObjFaninNum(pObj) );
        Abc_ObjForEachFanin( pObj, pFanin, k )
            Vec_IntPush( vMapping, pFanin->iTemp );
        // remember this gate (to be added to the mapping later)
        Vec_StrPrintStr( vGates, Mio_GateReadName((Mio_Gate_t *)pObj->pData) );
        Vec_StrPush( vGates, '\0' );
    }
    // write the COs literals
    Abc_NtkForEachCo( pNtk, pObj, i )
        Vec_IntPush( vMapping, Abc_ObjFanin0(pObj)->iTemp );
    // write signal names
    Abc_NtkForEachCi( pNtk, pObj, i ) {
        Vec_StrPrintStr( vGates, Abc_ObjName(pObj) );        
        Vec_StrPush( vGates, '\0' );
    }
    Abc_NtkForEachCo( pNtk, pObj, i ) {
        Vec_StrPrintStr( vGates, Abc_ObjName(pObj) );
        Vec_StrPush( vGates, '\0' );
    }
    // finish off the array
    nExtra = 4 - Vec_StrSize(vGates) % 4;
    for ( i = 0; i < nExtra; i++ )
        Vec_StrPush( vGates, '\0' );
    // add gates to the array
    assert( Vec_StrSize(vGates) % 4 == 0 );
    nExtra = Vec_StrSize(vGates) / 4;
    pArray = (int *)Vec_StrArray(vGates);
    for ( i = 0; i < nExtra; i++ )
        Vec_IntPush( vMapping, pArray[i] );
    // cleanup and return
    Vec_PtrFree( vNodes );
    Vec_StrFree( vGates );
    return vMapping;
}

/**Function*************************************************************

  Synopsis    [Build mapped network from the mini-mapped format.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkFromMiniMapping( int *pArray )
{
    if ( !pArray ) {
        printf("Mapping is not available.\n");
        return NULL;
    }
    Mio_Library_t * pLib = (Mio_Library_t *)Abc_FrameReadLibGen();
    if ( !pLib ) {
        printf("Library is not available.\n");
        return NULL;
    }
    Abc_Ntk_t *pNtkMapped = Abc_NtkAlloc( ABC_NTK_LOGIC, ABC_FUNC_MAP, 1 );
    pNtkMapped->pName = Extra_UtilStrsav( "mapped" );
    pNtkMapped->pManFunc = pLib;
    int nCis, nCos, nNodes, nFlops;
    int i, k, nLeaves, Pos = 4;
    char * pBuffer, * pName;
    Mio_Gate_t *pGate;
    Abc_Obj_t * pObj;
    nCis = pArray[0];
    nCos = pArray[1];
    nNodes = pArray[2];
    nFlops = pArray[3];
    // create pis
    for ( i = 0; i < nCis-nFlops; i++ )
        Abc_NtkCreatePi( pNtkMapped );
    // create nodes
    for ( i = 0; i < nNodes; i++ )
        Abc_NtkCreateNode( pNtkMapped );
    // create pos
    for ( i = 0; i < nCos-nFlops; i++ )
        Abc_NtkCreatePo( pNtkMapped );
    // create flops
    for ( i = 0; i < nFlops; i++ )
        Abc_NtkAddLatch( pNtkMapped, NULL, ABC_INIT_ZERO );
    // connect nodes
    for ( i = 0; i < nNodes; i++ )
    {
        nLeaves = pArray[Pos++];
        for ( k = 0; k < nLeaves; k++ )
            Abc_ObjAddFanin( Abc_NtkObj( pNtkMapped, nCis + i + 1 ), Abc_NtkObj( pNtkMapped, pArray[Pos++] + 1 ) );
    }
    for ( i = 0; i < nCos; i++ )
        Abc_ObjAddFanin( Abc_NtkCo( pNtkMapped, i ), Abc_NtkObj( pNtkMapped, pArray[Pos++] + 1 ) );

    pBuffer = (char *)(pArray + Pos);
    for ( i = 0; i < nNodes; i++ )
    {
        pName = pBuffer;
        pBuffer += strlen(pName) + 1;
        pGate = Mio_LibraryReadGateByName( pLib, pName, NULL );
        Abc_NtkObj( pNtkMapped, nCis + i + 1 )->pData = pGate;
    }

    assert( Abc_NtkCiNum(pNtkMapped) == nCis );
    Abc_NtkForEachCi( pNtkMapped, pObj, i ) {
        pName = pBuffer;
        pBuffer += strlen(pName) + 1;
        Abc_ObjAssignName( pObj, pName, NULL );
    }
    assert( Abc_NtkCoNum(pNtkMapped) == nCos );
    Abc_NtkForEachCo( pNtkMapped, pObj, i ) {
        pName = pBuffer;
        pBuffer += strlen(pName) + 1;
        Abc_ObjAssignName( pObj, pName, NULL );
    }

    if ( !Abc_NtkCheck( pNtkMapped ) ) {
        fprintf( stdout, "Abc_NtkFromMiniMapping(): Network check has failed.\n" );
    }

    return pNtkMapped;
}

/**Function*************************************************************

  Synopsis    [File IO.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkReadFromFile( char * pFileName )
{
    int nSize = Extra_FileSize( pFileName );
    if ( nSize == 0 )
        return NULL;
    FILE * pFile = fopen( pFileName, "rb" );
    char * pArray = ABC_ALLOC( char, nSize );
    int nSize2 = fread( pArray, sizeof(char), nSize, pFile );
    assert( nSize2 == nSize );
    fclose( pFile );
    Abc_Ntk_t * pNtk = Abc_NtkFromMiniMapping( (int*)pArray );
    ABC_FREE( pArray );
    return pNtk;
}
int Abc_NtkWriteToFile( char * pFileName, Abc_Ntk_t * pNtk )
{
    Vec_Int_t * vRes = Abc_NtkWriteMiniMapping( pNtk );
    FILE * pFile = fopen( pFileName, "wb" );
    if ( pFile == NULL ) { printf( "Cannot open input file \"%s\" for writing.\n", pFileName ); return 0; }
    int nSize = fwrite( Vec_IntArray(vRes), sizeof(int), Vec_IntSize(vRes), pFile );
    assert( nSize == Vec_IntSize(vRes) );
    Vec_IntFree( vRes );
    fclose( pFile );
    return 1;
}


/**Function*************************************************************

  Synopsis    [Prints mapped network represented in mini-mapped format.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkPrintMiniMapping( int * pArray )
{
    int nCis, nCos, nNodes, nFlops;
    int i, k, nLeaves, Pos = 4;
    char * pBuffer, * pName;
    nCis = pArray[0];
    nCos = pArray[1];
    nNodes = pArray[2];
    nFlops = pArray[3];
    printf( "Mapped network has %d CIs, %d COs, %d gates, and %d flops.\n", nCis, nCos, nNodes, nFlops );
    printf( "The first %d object IDs (from 0 to %d) are reserved for the CIs.\n", nCis, nCis - 1 );
    for ( i = 0; i < nNodes; i++ )
    {
        nLeaves = pArray[Pos++];
        printf( "Node %d has %d fanins {", nCis + i, nLeaves );
        for ( k = 0; k < nLeaves; k++ )
            printf( " %d", pArray[Pos++] );
        printf( " }\n" );
    }
    for ( i = 0; i < nCos; i++ )
        printf( "CO %d is driven by node %d\n", i, pArray[Pos++] );
    pBuffer = (char *)(pArray + Pos);
    for ( i = 0; i < nNodes; i++ )
    {
        pName = pBuffer;
        pBuffer += strlen(pName) + 1;
        printf( "Node %d has gate \"%s\"\n", nCis + i, pName );
    }
    for ( i = 0; i < nCis; i++ )
    {
        pName = pBuffer;
        pBuffer += strlen(pName) + 1;
        printf( "CI %d has name \"%s\"\n", i, pName );
    }
    for ( i = 0; i < nCos; i++ )
    {
        pName = pBuffer;
        pBuffer += strlen(pName) + 1;
        printf( "CO %d has name \"%s\"\n", i, pName );
    }
}

/**Function*************************************************************

  Synopsis    [Procedures to update internal ABC network using mini-mapped network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkInputMiniMapping( Abc_Frame_t * pAbc, void *p )
{
    Abc_Ntk_t * pNtk;
    if ( pAbc == NULL )
        printf( "ABC framework is not initialized by calling Abc_Start()\n" );
    pNtk = Abc_NtkFromMiniMapping( (int *)p );
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtk );
}

/**Function*************************************************************

  Synopsis    [This procedure outputs an array representing mini-mapped network.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int * Abc_NtkOutputMiniMapping( Abc_Frame_t * pAbc )
{
    //Abc_Frame_t * pAbc = (Abc_Frame_t *)pAbc0;
    Abc_Ntk_t * pNtk;
    Vec_Int_t * vMapping;
    int * pArray;
    if ( pAbc == NULL )
        printf( "ABC framework is not initialized by calling Abc_Start()\n" );
    pNtk = Abc_FrameReadNtk( pAbc );
    if ( pNtk == NULL )
        printf( "Current network in ABC framework is not defined.\n" );
    if ( !Abc_NtkHasMapping(pNtk) )
        printf( "Current network in ABC framework is not mapped.\n" );
    // derive mini-mapping
    vMapping = Abc_NtkWriteMiniMapping( pNtk );
    pArray = Vec_IntArray( vMapping );
    ABC_FREE( vMapping );
    // print mini-mapping (optional)
//    Abc_NtkPrintMiniMapping( pArray );
    // return the array representation of mini-mapping
    return pArray;
}

/**Function*************************************************************

  Synopsis    [Test for mini-mapped format.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkTestMiniMapping( Abc_Ntk_t * p )
{
    Vec_Int_t * vMapping;
    vMapping = Abc_NtkWriteMiniMapping( p );
    Abc_NtkPrintMiniMapping( Vec_IntArray(vMapping) );
    printf( "Array has size %d ints.\n", Vec_IntSize(vMapping) );
    Vec_IntFree( vMapping );
}

/**Function*************************************************************

  Synopsis    [These APIs set arrival/required times of CIs/COs.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkSetCiArrivalTime( Abc_Frame_t * pAbc, int iCi, float Rise, float Fall )
{
    //Abc_Frame_t * pAbc = (Abc_Frame_t *)pAbc0;
    Abc_Ntk_t * pNtk;
    Abc_Obj_t * pNode;
    if ( pAbc == NULL )
    {
        printf( "ABC framework is not initialized by calling Abc_Start()\n" );
        return;
    }
    pNtk = Abc_FrameReadNtk( pAbc );
    if ( pNtk == NULL )
    {
        printf( "Current network in ABC framework is not defined.\n" );
        return;
    }
    if ( iCi < 0 || iCi >= Abc_NtkCiNum(pNtk) )
    {
        printf( "CI index is not valid.\n" );
        return;
    }
    pNode = Abc_NtkCi( pNtk, iCi );
    Abc_NtkTimeSetArrival( pNtk, Abc_ObjId(pNode), Rise, Fall );
}
void Abc_NtkSetCoRequiredTime( Abc_Frame_t * pAbc, int iCo, float Rise, float Fall )
{
    //Abc_Frame_t * pAbc = (Abc_Frame_t *)pAbc0;
    Abc_Ntk_t * pNtk;
    Abc_Obj_t * pNode;
    if ( pAbc == NULL )\
    {
        printf( "ABC framework is not initialized by calling Abc_Start()\n" );
        return;
    }
    pNtk = Abc_FrameReadNtk( pAbc );
    if ( pNtk == NULL )
    {
        printf( "Current network in ABC framework is not defined.\n" );
        return;
    }
    if ( iCo < 0 || iCo >= Abc_NtkCoNum(pNtk) )
    {
        printf( "CO index is not valid.\n" );
        return;
    }
    pNode = Abc_NtkCo( pNtk, iCo );
    Abc_NtkTimeSetRequired( pNtk, Abc_ObjId(pNode), Rise, Fall );
}

/**Function*************************************************************

  Synopsis    [This APIs set AND gate delay.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkSetAndGateDelay( Abc_Frame_t * pAbc, float Delay )
{
    //Abc_Frame_t * pAbc = (Abc_Frame_t *)pAbc0;
    Abc_Ntk_t * pNtk;
    if ( pAbc == NULL )
    {
        printf( "ABC framework is not initialized by calling Abc_Start()\n" );
        return;
    }
    pNtk = Abc_FrameReadNtk( pAbc );
    if ( pNtk == NULL )
    {
        printf( "Current network in ABC framework is not defined.\n" );
        return;
    }
    pNtk->AndGateDelay = Delay;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END
