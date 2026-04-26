/**CFile****************************************************************

  FileName    [abcStmap_65.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap65 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "map/mio/mio.h"
#include "map/scl/sclLib.h"
#include "map/scl/sclSize.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

#define ABC_STMAP65_TOP_PRESSURES 10
#define ABC_STMAP65_TRACK_AIG_ID 27645
#define ABC_STMAP65_TRACK_NODE 23628
#define ABC_STMAP65_BOUND_RAW_WEIGHT 0.72f
#define ABC_STMAP65_BOUND_RATIO_CAP 2.10f
#define ABC_STMAP65_SEVERE_THRESHOLD 0.85f
#define ABC_STMAP65_PRESSURE_ENTRY_THRESHOLD 8000

typedef struct Abc_Stmap65LoadStats_t_
{
    int fAvailable;
    int nNodes;
    int nMatched;
    int nPins;
    int nOver;
    int nHotspotRoots;
    int nHotspotFanin;
    int nHotspotFanout;
    int nPressureEntries;
    int nPressureUpdates;
    int nPressureMissed;
    int nSinkPressureEntries;
    int nSinkPressureUpdates;
    int nAigPressureRatios;
    int nCriticalNodes;
    int TrackedNode;
    float RatioSum;
    float RatioMax;
    float TrackedRatio;
    float TrackedSinkRatio;
    float TimingDelay;
    float CriticalitySum;
    float CriticalityMax;
    float * pAigPressureRatios;
    float * pAigSinkPressureRatios;
    float TopRatios[5];
    float TopLoads[5];
    float TopCaps[5];
    const char * pTopGates[5];
    int TopNodes[5];
    int TopFanouts[5];
    int TopPressureAigIds[ABC_STMAP65_TOP_PRESSURES];
    int TopPressureNodes[ABC_STMAP65_TOP_PRESSURES];
    float TopPressureRatios[ABC_STMAP65_TOP_PRESSURES];
} Abc_Stmap65LoadStats_t;

typedef struct Abc_Stmap65BoundedStats_t_
{
    int fSevereTransfer;
    int nEntries;
    int nLifted;
    int nCapped;
    int nRawOnly;
    int nSinkFallback;
    float TrackedBoundedRatio;
} Abc_Stmap65BoundedStats_t;

static void Abc_Stmap65LoadStatsClear( Abc_Stmap65LoadStats_t * pStats )
{
    memset( pStats, 0, sizeof(Abc_Stmap65LoadStats_t) );
    pStats->TrackedNode = -1;
}

static void Abc_Stmap65LoadStatsFree( Abc_Stmap65LoadStats_t * pStats )
{
    ABC_FREE( pStats->pAigPressureRatios );
    ABC_FREE( pStats->pAigSinkPressureRatios );
    pStats->nAigPressureRatios = 0;
}

/**Function*************************************************************

  Synopsis    [Inserts one mapped-load diagnostic into the top list.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Abc_Stmap65TopInsert( float * pRatios, int * pNodes, const char ** ppGates, int * pFanouts, float * pLoads, float * pCaps, float Ratio, int Node, const char * pGate, int Fanouts, float Load, float Cap )
{
    int i, k;
    if ( Ratio <= pRatios[4] )
        return;
    for ( i = 0; i < 5; i++ )
    {
        if ( Ratio <= pRatios[i] )
            continue;
        for ( k = 4; k > i; k-- )
        {
            pRatios[k] = pRatios[k-1];
            pNodes[k] = pNodes[k-1];
            ppGates[k] = ppGates[k-1];
            pFanouts[k] = pFanouts[k-1];
            pLoads[k] = pLoads[k-1];
            pCaps[k] = pCaps[k-1];
        }
        pRatios[i] = Ratio;
        pNodes[i] = Node;
        ppGates[i] = pGate;
        pFanouts[i] = Fanouts;
        pLoads[i] = Load;
        pCaps[i] = Cap;
        return;
    }
}

static int Abc_Stmap65MappedObjAigId( Abc_Ntk_t * pNtk, Abc_Obj_t * pObj )
{
    int Lit;
    if ( pNtk == NULL || pObj == NULL || pNtk->vOrigNodeIds == NULL )
        return -1;
    if ( Abc_ObjId(pObj) >= Vec_IntSize(pNtk->vOrigNodeIds) )
        return -1;
    Lit = Vec_IntEntry( pNtk->vOrigNodeIds, Abc_ObjId(pObj) );
    return Lit >= 0 ? Abc_Lit2Var(Lit) : -1;
}

static void Abc_Stmap65PressureTopInsert( Abc_Stmap65LoadStats_t * pStats, int AigId, int NodeId, float Ratio )
{
    int i, k;
    if ( Ratio <= pStats->TopPressureRatios[ABC_STMAP65_TOP_PRESSURES-1] )
        return;
    for ( i = 0; i < ABC_STMAP65_TOP_PRESSURES; i++ )
    {
        if ( Ratio <= pStats->TopPressureRatios[i] )
            continue;
        for ( k = ABC_STMAP65_TOP_PRESSURES - 1; k > i; k-- )
        {
            pStats->TopPressureRatios[k] = pStats->TopPressureRatios[k-1];
            pStats->TopPressureAigIds[k] = pStats->TopPressureAigIds[k-1];
            pStats->TopPressureNodes[k] = pStats->TopPressureNodes[k-1];
        }
        pStats->TopPressureRatios[i] = Ratio;
        pStats->TopPressureAigIds[i] = AigId;
        pStats->TopPressureNodes[i] = NodeId;
        return;
    }
}

static int Abc_Stmap65EnsurePressureCapacity( Abc_Stmap65LoadStats_t * pStats, int AigId )
{
    float * pNew, * pNewSink;
    int i, nOld, nNew;
    if ( AigId < 0 )
        return 0;
    if ( AigId < pStats->nAigPressureRatios )
        return 1;
    nOld = pStats->nAigPressureRatios;
    nNew = nOld ? nOld : 1024;
    while ( nNew <= AigId )
        nNew *= 2;
    pNew = ABC_REALLOC( float, pStats->pAigPressureRatios, nNew );
    if ( pNew == NULL )
        return 0;
    pStats->pAigPressureRatios = pNew;
    pNewSink = ABC_REALLOC( float, pStats->pAigSinkPressureRatios, nNew );
    if ( pNewSink == NULL )
        return 0;
    pStats->pAigSinkPressureRatios = pNewSink;
    for ( i = nOld; i < nNew; i++ )
    {
        pStats->pAigPressureRatios[i] = 0.0;
        pStats->pAigSinkPressureRatios[i] = 0.0;
    }
    pStats->nAigPressureRatios = nNew;
    return 1;
}

static float Abc_Stmap65ObjCriticality( SC_Man * pTime, Abc_Obj_t * pObj )
{
    float Slack, Window, Criticality;
    if ( pTime == NULL || pObj == NULL || pTime->MaxDelay <= 0.0 )
        return 0.0;
    if ( Abc_ObjId(pObj) < 0 || Abc_ObjId(pObj) >= pTime->nObjs )
        return 0.0;
    Slack = Abc_SclObjGetSlack( pTime, pObj, pTime->MaxDelay );
    if ( Slack < 0.0 )
        Slack = 0.0;
    Window = 0.18 * pTime->MaxDelay;
    if ( Window < 10.0 )
        Window = 10.0;
    Criticality = 1.0 - Slack / Window;
    if ( Criticality < 0.0 )
        return 0.0;
    return Criticality > 1.0 ? 1.0 : Criticality;
}

static void Abc_Stmap65PressureInsert( Abc_Stmap65LoadStats_t * pStats, int AigId, int NodeId, float Ratio, float SinkCriticality )
{
    float OldRatio, OldSinkRatio, SinkWeight, SinkRatio;
    if ( AigId < 0 || Ratio <= 1.0 )
        return;
    if ( SinkCriticality < 0.0 )
        SinkCriticality = 0.0;
    if ( SinkCriticality > 1.0 )
        SinkCriticality = 1.0;
    SinkWeight = 0.35 + 0.65 * SinkCriticality;
    SinkRatio = 1.0 + (Ratio - 1.0) * SinkWeight;
    if ( AigId == ABC_STMAP65_TRACK_AIG_ID && Ratio > pStats->TrackedRatio )
    {
        pStats->TrackedRatio = Ratio;
        pStats->TrackedNode = NodeId;
    }
    if ( AigId == ABC_STMAP65_TRACK_AIG_ID && SinkRatio > pStats->TrackedSinkRatio )
        pStats->TrackedSinkRatio = SinkRatio;
    if ( !Abc_Stmap65EnsurePressureCapacity( pStats, AigId ) )
    {
        pStats->nPressureMissed++;
        return;
    }
    OldRatio = pStats->pAigPressureRatios[AigId];
    OldSinkRatio = pStats->pAigSinkPressureRatios[AigId];
    if ( OldRatio == 0.0 )
        pStats->nPressureEntries++;
    else if ( Ratio > OldRatio )
        pStats->nPressureUpdates++;
    if ( Ratio > OldRatio )
        pStats->pAigPressureRatios[AigId] = Ratio;
    if ( SinkRatio <= 1.0 || SinkRatio <= OldSinkRatio )
        return;
    if ( OldSinkRatio == 0.0 )
        pStats->nSinkPressureEntries++;
    else
        pStats->nSinkPressureUpdates++;
    pStats->pAigSinkPressureRatios[AigId] = SinkRatio;
    Abc_Stmap65PressureTopInsert( pStats, AigId, NodeId, SinkRatio );
}

static void Abc_Stmap65InsertConsumerFanins( Abc_Ntk_t * pNtk, Abc_Obj_t * pObj, Abc_Stmap65LoadStats_t * pStats, float Ratio, float SinkCriticality, int Depth )
{
    Abc_Obj_t * pFanin;
    int i, AigId;
    if ( pObj == NULL || Ratio <= 1.0 || Depth <= 0 )
        return;
    Abc_ObjForEachFanin( pObj, pFanin, i )
    {
        AigId = Abc_Stmap65MappedObjAigId( pNtk, pFanin );
        if ( AigId >= 0 )
        {
            pStats->nHotspotFanin++;
            Abc_Stmap65PressureInsert( pStats, AigId, Abc_ObjId(pFanin), Ratio, SinkCriticality );
        }
        Abc_Stmap65InsertConsumerFanins( pNtk, pFanin, pStats, 0.78 * Ratio, SinkCriticality, Depth - 1 );
    }
}

static void Abc_Stmap65CollectHotspotCone( Abc_Ntk_t * pNtk, Abc_Obj_t * pObj, Abc_Stmap65LoadStats_t * pStats, SC_Man * pTime, float Ratio )
{
    Abc_Obj_t * pFanout, * pFanout2;
    float FanoutCriticality;
    int i, k, AigId;
    if ( Ratio <= 1.0 )
        return;
    AigId = Abc_Stmap65MappedObjAigId( pNtk, pObj );
    if ( AigId >= 0 )
    {
        pStats->nHotspotRoots++;
        Abc_Stmap65PressureInsert( pStats, AigId, Abc_ObjId(pObj), 0.90 * Ratio, Abc_Stmap65ObjCriticality( pTime, pObj ) );
    }
    Abc_ObjForEachFanout( pObj, pFanout, i )
    {
        FanoutCriticality = Abc_Stmap65ObjCriticality( pTime, pFanout );
        AigId = Abc_Stmap65MappedObjAigId( pNtk, pFanout );
        if ( AigId >= 0 )
        {
            pStats->nHotspotFanout++;
            Abc_Stmap65PressureInsert( pStats, AigId, Abc_ObjId(pFanout), Ratio, FanoutCriticality );
        }
        Abc_Stmap65InsertConsumerFanins( pNtk, pFanout, pStats, 0.86 * Ratio, FanoutCriticality, 3 );
        Abc_ObjForEachFanout( pFanout, pFanout2, k )
        {
            FanoutCriticality = Abc_Stmap65ObjCriticality( pTime, pFanout2 );
            AigId = Abc_Stmap65MappedObjAigId( pNtk, pFanout2 );
            if ( AigId < 0 )
                continue;
            pStats->nHotspotFanout++;
            Abc_Stmap65PressureInsert( pStats, AigId, Abc_ObjId(pFanout2), 0.66 * Ratio, FanoutCriticality );
        }
    }
}

/**Function*************************************************************

  Synopsis    [Returns direct SCL input-pin load driven by a mapped node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Abc_Stmap65FanoutLoad( SC_Lib * pLib, Abc_Obj_t * pObj, int * pFanouts, int * pPins )
{
    Abc_Obj_t * pFanout;
    Mio_Gate_t * pGateFan;
    SC_Cell * pCellFan;
    float Load = 0.0;
    int i, iFanin, CellId;
    *pFanouts = 0;
    *pPins = 0;
    Abc_ObjForEachFanout( pObj, pFanout, i )
    {
        (*pFanouts)++;
        if ( !Abc_ObjIsNode(pFanout) || pFanout->pData == NULL )
            continue;
        iFanin = Abc_NodeFindFanin( pFanout, pObj );
        if ( iFanin < 0 )
            continue;
        pGateFan = (Mio_Gate_t *)pFanout->pData;
        CellId = Abc_SclCellFind( pLib, Mio_GateReadName(pGateFan) );
        if ( CellId < 0 )
            continue;
        pCellFan = SC_LibCell( pLib, CellId );
        if ( pCellFan == NULL || iFanin >= pCellFan->n_inputs )
            continue;
        Load += SC_CellPinCap( pCellFan, iFanin );
        (*pPins)++;
    }
    return Load;
}

/**Function*************************************************************

  Synopsis    [Prints direct SCL load diagnostics for the mapped network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Abc_Stmap65CollectSclLoadStats( Abc_Ntk_t * pNtk, Abc_Stmap65LoadStats_t * pStats )
{
    SC_Lib * pLib = (SC_Lib *)Abc_FrameReadLibScl();
    SC_Man * pTime = NULL;
    Abc_Obj_t * pObj;
    Mio_Gate_t * pGate;
    SC_Cell * pCell;
    float Load, MaxCap, Ratio, Criticality;
    int i, CellId, Fanouts, Pins;
    Abc_Stmap65LoadStatsClear( pStats );
    if ( pLib == NULL || !Abc_SclHasDelayInfo(pLib) )
        return;
    pStats->fAvailable = 1;
    if ( Abc_NtkHasMapping(pNtk) )
    {
        pTime = Abc_SclManStart( pLib, pNtk, 0, 1, 0, 0 );
        pStats->TimingDelay = pTime->MaxDelay;
    }
    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        pStats->nNodes++;
        if ( pObj->pData == NULL )
            continue;
        pGate = (Mio_Gate_t *)pObj->pData;
        CellId = Abc_SclCellFind( pLib, Mio_GateReadName(pGate) );
        if ( CellId < 0 )
            continue;
        pCell = SC_LibCell( pLib, CellId );
        if ( pCell == NULL || pCell->n_outputs == 0 )
            continue;
        MaxCap = SC_CellPin( pCell, pCell->n_inputs )->max_out_cap;
        if ( MaxCap <= 0.0 )
            continue;
        Load = Abc_Stmap65FanoutLoad( pLib, pObj, &Fanouts, &Pins );
        Ratio = Load / MaxCap;
        pStats->RatioSum += Ratio;
        if ( pStats->RatioMax < Ratio )
            pStats->RatioMax = Ratio;
        if ( Ratio > 1.0 )
            pStats->nOver++;
        pStats->nMatched++;
        pStats->nPins += Pins;
        Criticality = Abc_Stmap65ObjCriticality( pTime, pObj );
        pStats->CriticalitySum += Criticality;
        if ( Criticality > 0.0 )
            pStats->nCriticalNodes++;
        if ( pStats->CriticalityMax < Criticality )
            pStats->CriticalityMax = Criticality;
        Abc_Stmap65TopInsert( pStats->TopRatios, pStats->TopNodes, pStats->pTopGates, pStats->TopFanouts, pStats->TopLoads, pStats->TopCaps,
            Ratio, Abc_ObjId(pObj), Mio_GateReadName(pGate), Fanouts, Load, MaxCap );
        Abc_Stmap65CollectHotspotCone( pNtk, pObj, pStats, pTime, Ratio );
    }
    if ( pTime )
        Abc_SclManFree( pTime );
}

static void Abc_Stmap65PrintSclLoadStats( const char * pLabel, Abc_Stmap65LoadStats_t * pStats )
{
    int i;
    if ( !pStats->fAvailable )
    {
        printf( "stmap65 %s stats: unavailable = 1\n", pLabel );
        return;
    }
    printf( "stmap65 %s stats: nodes = %d  matched = %d  fanout-pins = %d  over-max = %d  avg-load-ratio = %.3f  max-load-ratio = %.3f\n",
        pLabel, pStats->nNodes, pStats->nMatched, pStats->nPins, pStats->nOver,
        pStats->nMatched ? pStats->RatioSum / (float)pStats->nMatched : 0.0, pStats->RatioMax );
    printf( "stmap65 %s criticality: timing-delay = %.3f  critical-nodes = %d  avg-criticality = %.3f  max-criticality = %.3f\n",
        pLabel, pStats->TimingDelay, pStats->nCriticalNodes,
        pStats->nMatched ? pStats->CriticalitySum / (float)pStats->nMatched : 0.0,
        pStats->CriticalityMax );
    for ( i = 0; i < 5 && pStats->TopRatios[i] > 0.0; i++ )
        printf( "stmap65 %s top: rank = %d  node = %d  gate = %s  fanouts = %d  load = %.3f  max-cap = %.3f  load-ratio = %.3f\n",
            pLabel, i + 1, pStats->TopNodes[i], pStats->pTopGates[i] ? pStats->pTopGates[i] : "?",
            pStats->TopFanouts[i], pStats->TopLoads[i], pStats->TopCaps[i], pStats->TopRatios[i] );
}

static void Abc_Stmap65PrintHotspots( Abc_Stmap65LoadStats_t * pStats )
{
    int i;
    if ( !pStats->fAvailable )
        return;
    printf( "stmap65 sink-pressure stats: roots = %d  consumer-fanin = %d  consumer-fanout = %d  raw-pressure-entries = %d  raw-pressure-updates = %d  sink-pressure-entries = %d  sink-pressure-updates = %d  pressure-missed = %d  pressure-capacity = %d  tracked-aig = %d  tracked-node = %d  tracked-raw-ratio = %.3f  tracked-sink-ratio = %.3f\n",
        pStats->nHotspotRoots, pStats->nHotspotFanin, pStats->nHotspotFanout,
        pStats->nPressureEntries, pStats->nPressureUpdates,
        pStats->nSinkPressureEntries, pStats->nSinkPressureUpdates,
        pStats->nPressureMissed,
        pStats->nAigPressureRatios, ABC_STMAP65_TRACK_AIG_ID, pStats->TrackedNode,
        pStats->TrackedRatio, pStats->TrackedSinkRatio );
    for ( i = 0; i < ABC_STMAP65_TOP_PRESSURES && pStats->TopPressureRatios[i] > 0.0; i++ )
        printf( "stmap65 sink-pressure: rank = %d  aig-id = %d  mapped-node = %d  sink-pressure-ratio = %.3f\n",
            i + 1, pStats->TopPressureAigIds[i], pStats->TopPressureNodes[i], pStats->TopPressureRatios[i] );
}

static float Abc_Stmap65FeedbackSeverity( Abc_Stmap65LoadStats_t * pStats )
{
    float OverFrac, MaxPart, OverPart, Severity;
    if ( !pStats->fAvailable || pStats->nMatched == 0 )
        return 0.0;
    OverFrac = (float)pStats->nOver / (float)pStats->nMatched;
    MaxPart = pStats->RatioMax > 4.0 ? (pStats->RatioMax - 4.0) / 8.0 : 0.0;
    OverPart = OverFrac > 0.002 ? (OverFrac - 0.002) / 0.006 : 0.0;
    if ( MaxPart > 1.0 )
        MaxPart = 1.0;
    if ( OverPart > 1.0 )
        OverPart = 1.0;
    Severity = 0.65 * MaxPart + 0.35 * OverPart;
    return Severity > 1.0 ? 1.0 : Severity;
}

static float * Abc_Stmap65BuildBoundedPressures( Abc_Stmap65LoadStats_t * pStats, int fSevereTransfer, Abc_Stmap65BoundedStats_t * pBounded )
{
    float * pRatios;
    float RawRatio, SinkRatio, BoundedRatio, Candidate;
    int i;
    memset( pBounded, 0, sizeof(Abc_Stmap65BoundedStats_t) );
    pBounded->fSevereTransfer = fSevereTransfer;
    if ( pStats->nAigPressureRatios <= 0 )
        return NULL;
    pRatios = ABC_ALLOC( float, pStats->nAigPressureRatios );
    if ( pRatios == NULL )
        return NULL;
    for ( i = 0; i < pStats->nAigPressureRatios; i++ )
    {
        RawRatio = pStats->pAigPressureRatios ? pStats->pAigPressureRatios[i] : 0.0f;
        SinkRatio = pStats->pAigSinkPressureRatios ? pStats->pAigSinkPressureRatios[i] : 0.0f;
        BoundedRatio = SinkRatio;
        if ( fSevereTransfer && BoundedRatio > ABC_STMAP65_BOUND_RATIO_CAP )
        {
            BoundedRatio = ABC_STMAP65_BOUND_RATIO_CAP;
            pBounded->nCapped++;
        }
        if ( fSevereTransfer && RawRatio > 1.0f )
        {
            Candidate = 1.0f + ABC_STMAP65_BOUND_RAW_WEIGHT * (RawRatio - 1.0f);
            if ( Candidate > ABC_STMAP65_BOUND_RATIO_CAP )
                Candidate = ABC_STMAP65_BOUND_RATIO_CAP;
            if ( Candidate > BoundedRatio )
            {
                BoundedRatio = Candidate;
                if ( Candidate >= ABC_STMAP65_BOUND_RATIO_CAP )
                    pBounded->nCapped++;
                if ( SinkRatio > 0.0f )
                    pBounded->nLifted++;
                else
                    pBounded->nRawOnly++;
            }
            else if ( SinkRatio > 0.0f )
                pBounded->nSinkFallback++;
        }
        else if ( SinkRatio > 0.0f )
            pBounded->nSinkFallback++;
        pRatios[i] = BoundedRatio;
        if ( BoundedRatio > 0.0f )
            pBounded->nEntries++;
        if ( i == ABC_STMAP65_TRACK_AIG_ID )
            pBounded->TrackedBoundedRatio = BoundedRatio;
    }
    return pRatios;
}

static void Abc_Stmap65PrintBoundedPressures( Abc_Stmap65LoadStats_t * pStats, Abc_Stmap65BoundedStats_t * pBounded )
{
    printf( "stmap65 bounded-pressure stats: severe-transfer = %d  raw-weight = %.2f  ratio-cap = %.3f  bounded-pressure-entries = %d  lifted = %d  capped = %d  raw-only = %d  sink-fallback = %d  tracked-aig = %d  tracked-raw-ratio = %.3f  tracked-sink-ratio = %.3f  tracked-bounded-ratio = %.3f\n",
        pBounded->fSevereTransfer, ABC_STMAP65_BOUND_RAW_WEIGHT, ABC_STMAP65_BOUND_RATIO_CAP,
        pBounded->nEntries, pBounded->nLifted, pBounded->nCapped,
        pBounded->nRawOnly, pBounded->nSinkFallback, ABC_STMAP65_TRACK_AIG_ID,
        pStats->TrackedRatio, pStats->TrackedSinkRatio, pBounded->TrackedBoundedRatio );
}

/**Function*************************************************************

  Synopsis    [Runs the stmap65 mapper probe.]

  Description [stmap65 repeats the stmap64 bounded transfer but tightens the
  command-scoped load-drop cut-only guard to require stronger node pressure
  before admitting a one-sided cut-pressure seed.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap65( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkMap, * pNtkRes, * pNtkFirst;
    Abc_Stmap65LoadStats_t LoadStats;
    Abc_Stmap65BoundedStats_t BoundedStats;
    float * pBoundedPressureRatios = NULL;
    char Buffer[100];
    double DelayTarget;
    double AreaMulti;
    double DelayMulti;
    float LogFan = 0;
    float Slew = 0; // choose based on the library
    float Gain = 250; // base SCL/genlib gain for low and moderate pressure
    float LowGain, SelectedGain;
    int nGatesMin = 0;
    int fAreaOnly;
    int fRecovery;
    int fSweep;
    int fSwitching;
    int fSkipFanout;
    int fUseProfile;
    int fUseBuffs;
    int fVerbose;
    int c, fTempMap = 0;
    int fUseReducedGain, fSevereTransfer;
    float OverFrac, Feedback;
    extern Abc_Ntk_t * Abc_NtkMap( Abc_Ntk_t * pNtk, Mio_Library_t* userLib, double DelayTarget, double AreaMulti, double DelayMulti, float LogFan, float Slew, float Gain, int nGatesMin, int fRecovery, int fSwitching, int fSkipFanout, int fUseProfile, int fUseBuffs, int fVerbose );
    extern int Abc_NtkFraigSweep( Abc_Ntk_t * pNtk, int fUseInv, int fExdc, int fVerbose, int fVeryVerbose );
    extern void Map_Stmap45SetSclLoadFeedbackWithEntries( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios, int nPressureEntries );
    extern void Map_Stmap60SetNearMissLeafDiag( int fEnable, int TrackNode );
    extern void Map_Stmap61SetCutOnlyGateDiag( int fEnable, int TrackNode );
    extern void Map_Stmap62SetCutOnlyOrdering( int fEnable );
    extern void Map_Stmap65SetStrongNodeLoadDropGuard( int fEnable );
    extern int Abc_Stmap77ReconstructionDiagConfigured( void );
    extern void Abc_Stmap77SetReconstructionActive( int fActive, const char * pPassLabel );
    extern int Abc_Stmap78DemandPathDiagConfigured( void );
    extern void Abc_Stmap78SetDemandPathActive( int fActive, const char * pPassLabel );
    extern int Abc_Stmap79ParentCutDiagConfigured( void );
    extern void Abc_Stmap79SetParentCutActive( int fActive, const char * pPassLabel );

    pNtk = Abc_FrameReadNtk(pAbc);
    DelayTarget = -1;
    AreaMulti   = 0;
    DelayMulti  = 0;
    fAreaOnly   = 0;
    fRecovery   = 1;
    fSweep      = 0;
    fSwitching  = 0;
    fSkipFanout = 57; // stmap65 reuses reviewed mode 57 plus a stricter command-scoped load-drop guard; -f disables it
    fUseProfile = 0;
    fUseBuffs   = 0;
    fVerbose    = 0;
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "DABFSGMarspfuovh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'D':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-D\" should be followed by a floating point number.\n" );
                goto usage;
            }
            DelayTarget = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( DelayTarget <= 0.0 )
                goto usage;
            break;
        case 'A':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-A\" should be followed by a floating point number.\n" );
                goto usage;
            }
            AreaMulti = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            break;
        case 'B':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-B\" should be followed by a floating point number.\n" );
                goto usage;
            }
            DelayMulti = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            break;
        case 'F':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-F\" should be followed by a floating point number.\n" );
                goto usage;
            }
            LogFan = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( LogFan < 0.0 )
                goto usage;
            break;
        case 'S':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-S\" should be followed by a floating point number.\n" );
                goto usage;
            }
            Slew = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( Slew <= 0.0 )
                goto usage;
            break;
        case 'G':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-G\" should be followed by a floating point number.\n" );
                goto usage;
            }
            Gain = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( Gain <= 0.0 )
                goto usage;
            break;
        case 'M':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-M\" should be followed by a positive integer.\n" );
                goto usage;
            }
            nGatesMin = atoi(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( nGatesMin < 0 )
                goto usage;
            break;
        case 'a':
            fAreaOnly ^= 1;
            break;
        case 'r':
            fRecovery ^= 1;
            break;
        case 's':
            fSweep ^= 1;
            break;
        case 'p':
            fSwitching ^= 1;
            break;
        case 'f':
            fSkipFanout = fSkipFanout ? 0 : 57;
            break;
        case 'u':
            fUseProfile ^= 1;
            break;
        case 'o':
            fUseBuffs ^= 1;
            break;
        case 'v':
            fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

    if ( fAreaOnly )
        DelayTarget = ABC_INFINITY;

    Map_Stmap45SetSclLoadFeedbackWithEntries( 0.0, 0.0, 0.0, NULL, 0, 0 );
    Map_Stmap60SetNearMissLeafDiag( 0, -1 );
    Map_Stmap61SetCutOnlyGateDiag( 0, -1 );
    Map_Stmap62SetCutOnlyOrdering( 0 );
    Map_Stmap65SetStrongNodeLoadDropGuard( 0 );
    pNtkMap = pNtk;
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        pNtkMap = Abc_NtkStrash( pNtk, 0, 0, 0 );
        if ( pNtkMap == NULL )
        {
            Abc_Print( -1, "Strashing before stmap65 has failed.\n" );
            return 1;
        }
        pNtkMap = Abc_NtkBalance( pNtkRes = pNtkMap, 0, 0, 1 );
        Abc_NtkDelete( pNtkRes );
        if ( pNtkMap == NULL )
        {
            Abc_Print( -1, "Balancing before stmap65 has failed.\n" );
            return 1;
        }
        fTempMap = 1;
        Abc_Print( 0, "The network was strashed and balanced before stmap65.\n" );
    }

    if ( fSkipFanout )
    {
        if ( Abc_Stmap77ReconstructionDiagConfigured() )
            Abc_Stmap77SetReconstructionActive( 0, "feedback" );
        if ( Abc_Stmap78DemandPathDiagConfigured() )
            Abc_Stmap78SetDemandPathActive( 0, "feedback" );
        if ( Abc_Stmap79ParentCutDiagConfigured() )
            Abc_Stmap79SetParentCutActive( 0, "feedback" );
        pNtkFirst = Abc_NtkMap( pNtkMap, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, 36, fUseProfile, fUseBuffs, fVerbose );
        if ( pNtkFirst == NULL )
        {
            if ( fTempMap )
                Abc_NtkDelete( pNtkMap );
            Abc_Print( -1, "stmap65 has failed.\n" );
            return 1;
        }
        Abc_Stmap65CollectSclLoadStats( pNtkFirst, &LoadStats );
        Abc_Stmap65PrintSclLoadStats( "feedback-load", &LoadStats );
        Abc_Stmap65PrintHotspots( &LoadStats );
        OverFrac = LoadStats.nMatched ? (float)LoadStats.nOver / (float)LoadStats.nMatched : 0.0;
        Feedback = Abc_Stmap65FeedbackSeverity( &LoadStats );
        LowGain = 0.90 * Gain;
        SelectedGain = Gain;
        fUseReducedGain = Feedback >= ABC_STMAP65_SEVERE_THRESHOLD && LoadStats.nPressureEntries >= ABC_STMAP65_PRESSURE_ENTRY_THRESHOLD;
        fSevereTransfer = fUseReducedGain;
        if ( fUseReducedGain && LowGain > 0.0 )
            SelectedGain = LowGain;
        pBoundedPressureRatios = Abc_Stmap65BuildBoundedPressures( &LoadStats, fSevereTransfer, &BoundedStats );
        if ( pBoundedPressureRatios == NULL && LoadStats.nAigPressureRatios > 0 )
        {
            Abc_NtkDelete( pNtkFirst );
            Abc_Stmap65LoadStatsFree( &LoadStats );
            if ( fTempMap )
                Abc_NtkDelete( pNtkMap );
            Abc_Print( -1, "stmap65 bounded-pressure allocation has failed.\n" );
            return 1;
        }
        printf( "stmap65 feedback: remap = 1  active = %d  max-load-ratio = %.3f  over-max-frac = %.6f  severity = %.3f  raw-pressure-entries = %d  sink-pressure-entries = %d  pressure-capacity = %d\n",
            Feedback > 0.0 && LoadStats.nPressureEntries > 0, LoadStats.RatioMax, OverFrac,
            Feedback, LoadStats.nPressureEntries, LoadStats.nSinkPressureEntries,
            LoadStats.nAigPressureRatios );
        printf( "stmap65 blended-gain: base-gain = %.2f  blend-gain = %.2f  selected-gain = %.2f  severity-threshold = 0.850  raw-pressure-threshold = 8000  selected-blend-gain = %d\n",
            Gain, LowGain, SelectedGain, fUseReducedGain );
        Abc_Stmap65PrintBoundedPressures( &LoadStats, &BoundedStats );
        printf( "stmap65 mapper-mode: bounded-pressure-transfer-mode = 57  cut-only-before-moderate = 0  load-drop-cut-only-guard = 0  strong-node-load-drop-guard = 1  strong-node-pressure-min = 1.250  blended-gain = %.2f  pressure-feed = %s  raw-weight = %.2f  bounded-ratio-cap = %.3f  inherited-cut-only-area-save-cap = 1.35x-inverter  tracked-near-miss-node = %d  cut-only-gate-diag = 1\n",
            SelectedGain, fSevereTransfer ? "bounded-raw" : "sink-fallback",
            ABC_STMAP65_BOUND_RAW_WEIGHT, ABC_STMAP65_BOUND_RATIO_CAP,
            ABC_STMAP65_TRACK_NODE );
        Abc_NtkDelete( pNtkFirst );
        Map_Stmap45SetSclLoadFeedbackWithEntries( LoadStats.RatioMax, OverFrac, Feedback, pBoundedPressureRatios, LoadStats.nAigPressureRatios, BoundedStats.nEntries );
        Map_Stmap60SetNearMissLeafDiag( 1, ABC_STMAP65_TRACK_NODE );
        Map_Stmap61SetCutOnlyGateDiag( 1, ABC_STMAP65_TRACK_NODE );
        Map_Stmap62SetCutOnlyOrdering( 0 );
        Map_Stmap65SetStrongNodeLoadDropGuard( 1 );
        if ( Abc_Stmap77ReconstructionDiagConfigured() )
            Abc_Stmap77SetReconstructionActive( 1, "final" );
        if ( Abc_Stmap78DemandPathDiagConfigured() )
            Abc_Stmap78SetDemandPathActive( 1, "final" );
        if ( Abc_Stmap79ParentCutDiagConfigured() )
            Abc_Stmap79SetParentCutActive( 1, "final" );
        pNtkRes = Abc_NtkMap( pNtkMap, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, SelectedGain, nGatesMin, fRecovery, fSwitching, 57, fUseProfile, fUseBuffs, fVerbose );
        if ( Abc_Stmap77ReconstructionDiagConfigured() )
            Abc_Stmap77SetReconstructionActive( 0, "final" );
        if ( Abc_Stmap78DemandPathDiagConfigured() )
            Abc_Stmap78SetDemandPathActive( 0, "final" );
        if ( Abc_Stmap79ParentCutDiagConfigured() )
            Abc_Stmap79SetParentCutActive( 0, "final" );
        Map_Stmap65SetStrongNodeLoadDropGuard( 0 );
        Map_Stmap62SetCutOnlyOrdering( 0 );
        Map_Stmap61SetCutOnlyGateDiag( 0, -1 );
        Map_Stmap60SetNearMissLeafDiag( 0, -1 );
        Map_Stmap45SetSclLoadFeedbackWithEntries( 0.0, 0.0, 0.0, NULL, 0, 0 );
        ABC_FREE( pBoundedPressureRatios );
        Abc_Stmap65LoadStatsFree( &LoadStats );
        if ( pNtkRes == NULL )
        {
            if ( fTempMap )
                Abc_NtkDelete( pNtkMap );
            Abc_Print( -1, "stmap65 feedback remap has failed.\n" );
            return 1;
        }
    }
    else
    {
        if ( Abc_Stmap77ReconstructionDiagConfigured() )
            Abc_Stmap77SetReconstructionActive( 1, "final" );
        if ( Abc_Stmap78DemandPathDiagConfigured() )
            Abc_Stmap78SetDemandPathActive( 1, "final" );
        if ( Abc_Stmap79ParentCutDiagConfigured() )
            Abc_Stmap79SetParentCutActive( 1, "final" );
        pNtkRes = Abc_NtkMap( pNtkMap, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, 0, fUseProfile, fUseBuffs, fVerbose );
        if ( Abc_Stmap77ReconstructionDiagConfigured() )
            Abc_Stmap77SetReconstructionActive( 0, "final" );
        if ( Abc_Stmap78DemandPathDiagConfigured() )
            Abc_Stmap78SetDemandPathActive( 0, "final" );
        if ( Abc_Stmap79ParentCutDiagConfigured() )
            Abc_Stmap79SetParentCutActive( 0, "final" );
        if ( pNtkRes == NULL )
        {
            if ( fTempMap )
                Abc_NtkDelete( pNtkMap );
            Abc_Print( -1, "stmap65 has failed.\n" );
            return 1;
        }
    }
    if ( fTempMap )
        Abc_NtkDelete( pNtkMap );

    if ( fSweep )
    {
        Abc_NtkFraigSweep( pNtkRes, 0, 0, 0, 0 );
        if ( Abc_NtkHasMapping(pNtkRes) )
        {
            pNtkRes = Abc_NtkDupDfs( pNtk = pNtkRes );
            Abc_NtkDelete( pNtk );
        }
    }

    Abc_Stmap65CollectSclLoadStats( pNtkRes, &LoadStats );
    Abc_Stmap65PrintSclLoadStats( "scl-load", &LoadStats );
    Abc_Stmap65LoadStatsFree( &LoadStats );
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtkRes );
    return 0;

usage:
    if ( DelayTarget == -1 )
        sprintf(Buffer, "not used" );
    else
        sprintf(Buffer, "%.3f", DelayTarget );
    Abc_Print( -2, "usage: stmap65 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap65 bounded-pressure transfer with a stronger node-pressure load-drop guard\n" );
    Abc_Print( -2, "\t-D float : sets the global required times [default = %s]\n", Buffer );
    Abc_Print( -2, "\t-A float : \"area multiplier\" to bias gate selection [default = %.2f]\n", AreaMulti );
    Abc_Print( -2, "\t-B float : \"delay multiplier\" to bias gate selection [default = %.2f]\n", DelayMulti );
    Abc_Print( -2, "\t-F float : the logarithmic fanout delay parameter [default = %.2f]\n", LogFan );
    Abc_Print( -2, "\t-S float : the slew parameter used to generate the library [default = %.2f]\n", Slew );
    Abc_Print( -2, "\t-G float : the SCL/genlib gain parameter used to generate the library [default = %.2f]\n", Gain );
    Abc_Print( -2, "\t-M num   : skip gate classes whose size is less than this [default = %d]\n", nGatesMin );
    Abc_Print( -2, "\t-a       : toggles area-only mapping [default = %s]\n", fAreaOnly? "yes": "no" );
    Abc_Print( -2, "\t-r       : toggles area recovery [default = %s]\n", fRecovery? "yes": "no" );
    Abc_Print( -2, "\t-s       : toggles sweep after mapping [default = %s]\n", fSweep? "yes": "no" );
    Abc_Print( -2, "\t-p       : optimizes power by minimizing switching [default = %s]\n", fSwitching? "yes": "no" );
    Abc_Print( -2, "\t-f       : disables the bounded-pressure SCL feedback mapper mode [default = %s]\n", fSkipFanout? "yes": "no" );
    Abc_Print( -2, "\t-u       : use standard-cell profile [default = %s]\n", fUseProfile? "yes": "no" );
    Abc_Print( -2, "\t-o       : toggles using buffers to decouple combinational outputs [default = %s]\n", fUseBuffs? "yes": "no" );
    Abc_Print( -2, "\t-v       : toggles verbose output [default = %s]\n", fVerbose? "yes": "no" );
    Abc_Print( -2, "\t-h       : print the command usage\n");
    return 1;
}

ABC_NAMESPACE_IMPL_END
