/**CFile****************************************************************

  FileName    [mapperMatch.c]

  PackageName [MVSIS 1.3: Multi-valued logic synthesis system.]

  Synopsis    [Generic technology mapping engine.]

  Author      [MVSIS Group]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 2.0. Started - June 1, 2004.]

  Revision    [$Id: mapperMatch.c,v 1.7 2004/09/30 21:18:10 satrajit Exp $]

***********************************************************************/

#include "mapperInt.h"

#include "misc/util/utilNam.h"
#include "map/scl/sclCon.h"

ABC_NAMESPACE_IMPL_START


/*
    A potential improvement:
    When an internal node is not used in the mapping, its required times 
    are set to be +infinity. So when we recover area, we try to find the 
    best match for area and completely disregard the delay for the nodes
    that are not currently used in the mapping because any match whose 
    arrival times are less than the required times (+infinity) can be used.
    It may be possible to develop a better approach to recover area for
    the nodes that are not currently used in the mapping...
*/

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Cleans the match.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Map_MatchClean( Map_Match_t * pMatch )
{
    memset( pMatch, 0, sizeof(Map_Match_t) );
    pMatch->AreaFlow          = MAP_FLOAT_LARGE; // unassigned
    pMatch->tArrive.Rise   = MAP_FLOAT_LARGE; // unassigned
    pMatch->tArrive.Fall   = MAP_FLOAT_LARGE; // unassigned
    pMatch->tArrive.Worst  = MAP_FLOAT_LARGE; // unassigned
}

enum {
    MAP_STMAP80_REASON_SKIP_FANOUT = 0,
    MAP_STMAP80_REASON_NO_SUPERS,
    MAP_STMAP80_REASON_NO_SUPER_BEST,
    MAP_STMAP80_REASON_REQUIRED,
    MAP_STMAP80_REASON_AREA_SENSITIVE,
    MAP_STMAP80_REASON_ACCEPTED_BEST,
    MAP_STMAP80_REASON_NONSELECTED
};

static int s_fStmap80CandidateCutDiag = 0;
static int s_fStmap80CandidateCutActive = 0;
static const char * s_pStmap80CandidateCutLabel = "stmap80";
static int s_Stmap80ParentAigId = -1;
static int s_Stmap80ChildAigId = -1;
static int s_nStmap80CandidateCutRows = 0;
static int s_nStmap80CandidateCutViable = 0;
static int s_nStmap80CandidateCutAccepted = 0;
static int s_nStmap80CandidateChildPhase0 = 0;
static int s_nStmap80CandidateChildPhase1 = 0;
static int s_nStmap80CandidateChildUnknown = 0;
static int s_nStmap80CandidateChildMissing = 0;
static int s_nStmap80CandidateSkipFanout = 0;
static int s_nStmap80CandidateNoSupers = 0;
static int s_nStmap80CandidateNoSuperBest = 0;
static int s_nStmap80CandidateRequired = 0;
static int s_nStmap80CandidateAreaSensitive = 0;

static const char * Map_Stmap80ReasonName( int Reason )
{
    switch ( Reason )
    {
    case MAP_STMAP80_REASON_SKIP_FANOUT:
        return "skip-fanout";
    case MAP_STMAP80_REASON_NO_SUPERS:
        return "no-supers";
    case MAP_STMAP80_REASON_NO_SUPER_BEST:
        return "no-super-best";
    case MAP_STMAP80_REASON_REQUIRED:
        return "violates-required";
    case MAP_STMAP80_REASON_AREA_SENSITIVE:
        return "area-sensitive-skip";
    case MAP_STMAP80_REASON_ACCEPTED_BEST:
        return "accepted-best";
    case MAP_STMAP80_REASON_NONSELECTED:
        return "nonselected";
    default:
        return "unknown";
    }
}

static void Map_Stmap80ResetCandidateCutCounters( void )
{
    s_nStmap80CandidateCutRows = 0;
    s_nStmap80CandidateCutViable = 0;
    s_nStmap80CandidateCutAccepted = 0;
    s_nStmap80CandidateChildPhase0 = 0;
    s_nStmap80CandidateChildPhase1 = 0;
    s_nStmap80CandidateChildUnknown = 0;
    s_nStmap80CandidateChildMissing = 0;
    s_nStmap80CandidateSkipFanout = 0;
    s_nStmap80CandidateNoSupers = 0;
    s_nStmap80CandidateNoSuperBest = 0;
    s_nStmap80CandidateRequired = 0;
    s_nStmap80CandidateAreaSensitive = 0;
}

static void Map_Stmap80ClearCandidateCutDiag( void )
{
    s_fStmap80CandidateCutDiag = 0;
    s_fStmap80CandidateCutActive = 0;
    s_pStmap80CandidateCutLabel = "stmap80";
    s_Stmap80ParentAigId = -1;
    s_Stmap80ChildAigId = -1;
    Map_Stmap80ResetCandidateCutCounters();
}

void Map_Stmap80SetCandidateCutDiag( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId )
{
    Map_Stmap80ClearCandidateCutDiag();
    s_pStmap80CandidateCutLabel = pLabel && pLabel[0] ? pLabel : "stmap80";
    if ( !fEnable || ParentAigId < 0 || ChildAigId < 0 )
        return;
    s_fStmap80CandidateCutDiag = 1;
    s_Stmap80ParentAigId = ParentAigId;
    s_Stmap80ChildAigId = ChildAigId;
}

int Map_Stmap80CandidateCutDiagConfigured( void )
{
    return s_fStmap80CandidateCutDiag;
}

void Map_Stmap80SetCandidateCutActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap80CandidateCutDiag )
        return;
    if ( fActive )
        Map_Stmap80ResetCandidateCutCounters();
    s_fStmap80CandidateCutActive = fActive;
    (void)pPassLabel;
}

static void Map_Stmap80RecordCandidateCut( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, int CutOrdinal, Map_Match_t * pMatch, Map_Match_t * pBestBefore, int Reason, int fSelectedUpdate, int fViable )
{
    Map_Node_t * pNodeRegular;
    Map_Super_t * pSuperBest, * pSuperBestBefore;
    Mio_Gate_t * pGate, * pGateBefore;
    unsigned uPhaseBest;
    int AigId, nLeaves, i, ChildLeaf, ChildPhase, ChildInverted, LeafAigId[6], LeafPhase[6];
    float Arrive, Required, Slack, AreaFlow, BestArrive, BestArea;
    if ( !s_fStmap80CandidateCutDiag || !s_fStmap80CandidateCutActive || pNode == NULL || pCut == NULL )
        return;
    pNodeRegular = Map_Regular( pNode );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return;
    AigId = Map_NodeReadAigId( pNodeRegular );
    if ( AigId != s_Stmap80ParentAigId )
        return;
    pSuperBest = pMatch ? pMatch->pSuperBest : NULL;
    pSuperBestBefore = pBestBefore ? pBestBefore->pSuperBest : NULL;
    pGate = pSuperBest ? pSuperBest->pRoot : NULL;
    pGateBefore = pSuperBestBefore ? pSuperBestBefore->pRoot : NULL;
    uPhaseBest = pSuperBest && pMatch ? pMatch->uPhaseBest : 0;
    nLeaves = pCut->nLeaves;
    ChildLeaf = -1;
    ChildPhase = -1;
    for ( i = 0; i < 6; i++ )
    {
        LeafAigId[i] = -1;
        LeafPhase[i] = -1;
    }
    for ( i = 0; i < nLeaves && i < 6; i++ )
    {
        LeafAigId[i] = Map_NodeReadAigId( Map_Regular(pCut->ppLeaves[i]) );
        if ( pSuperBest )
            LeafPhase[i] = ((uPhaseBest & (1 << i)) > 0) ? 0 : 1;
        if ( LeafAigId[i] == s_Stmap80ChildAigId )
        {
            ChildLeaf = i;
            ChildPhase = LeafPhase[i];
        }
    }
    ChildInverted = ChildPhase >= 0 ? !ChildPhase : -1;
    Arrive = pMatch ? pMatch->tArrive.Worst : MAP_FLOAT_LARGE;
    AreaFlow = pMatch ? pMatch->AreaFlow : MAP_FLOAT_LARGE;
    Required = pNodeRegular->tRequired[fPhase].Worst;
    Slack = Required - Arrive;
    BestArrive = pBestBefore ? pBestBefore->tArrive.Worst : MAP_FLOAT_LARGE;
    BestArea = pBestBefore ? pBestBefore->AreaFlow : MAP_FLOAT_LARGE;
    s_nStmap80CandidateCutRows++;
    if ( fViable )
        s_nStmap80CandidateCutViable++;
    if ( fSelectedUpdate )
        s_nStmap80CandidateCutAccepted++;
    if ( ChildPhase == 0 )
        s_nStmap80CandidateChildPhase0++;
    else if ( ChildPhase == 1 )
        s_nStmap80CandidateChildPhase1++;
    else if ( ChildLeaf >= 0 )
        s_nStmap80CandidateChildUnknown++;
    else
        s_nStmap80CandidateChildMissing++;
    switch ( Reason )
    {
    case MAP_STMAP80_REASON_SKIP_FANOUT:
        s_nStmap80CandidateSkipFanout++;
        break;
    case MAP_STMAP80_REASON_NO_SUPERS:
        s_nStmap80CandidateNoSupers++;
        break;
    case MAP_STMAP80_REASON_NO_SUPER_BEST:
        s_nStmap80CandidateNoSuperBest++;
        break;
    case MAP_STMAP80_REASON_REQUIRED:
        s_nStmap80CandidateRequired++;
        break;
    case MAP_STMAP80_REASON_AREA_SENSITIVE:
        s_nStmap80CandidateAreaSensitive++;
        break;
    default:
        break;
    }
    printf( "%s candidate-cut: index = %d  parent-aig = %d  child-aig = %d  mapper-mode = %d  phase = %d  cut-ordinal = %d  reason = %s  viable = %d  selected-update = %d  node = %d  level = %d  refs = %d  gate = %s  leaves = %d  u-phase-best = %u  fanout-limit = %d  arrival = %.3f  required = %.3f  slack = %.3f  area-flow = %.3f  best-gate = %s  best-arrival = %.3f  best-area-flow = %.3f  child-leaf-index = %d  child-requested-phase = %d  child-inverted-pin = %d  leaf0-aig-id = %d  leaf0-phase = %d  leaf1-aig-id = %d  leaf1-phase = %d  leaf2-aig-id = %d  leaf2-phase = %d  leaf3-aig-id = %d  leaf3-phase = %d  leaf4-aig-id = %d  leaf4-phase = %d  leaf5-aig-id = %d  leaf5-phase = %d\n",
        s_pStmap80CandidateCutLabel, s_nStmap80CandidateCutRows,
        s_Stmap80ParentAigId, s_Stmap80ChildAigId, p->fMappingMode, fPhase,
        CutOrdinal, Map_Stmap80ReasonName( Reason ), fViable, fSelectedUpdate,
        pNodeRegular->Num, pNodeRegular->Level, pNodeRegular->nRefs,
        pGate ? Mio_GateReadName(pGate) : "?", nLeaves, uPhaseBest,
        pSuperBest ? (int)pSuperBest->nFanLimit : -1, Arrive, Required, Slack,
        AreaFlow, pGateBefore ? Mio_GateReadName(pGateBefore) : "?",
        BestArrive, BestArea, ChildLeaf, ChildPhase, ChildInverted,
        LeafAigId[0], LeafPhase[0], LeafAigId[1], LeafPhase[1],
        LeafAigId[2], LeafPhase[2], LeafAigId[3], LeafPhase[3],
        LeafAigId[4], LeafPhase[4], LeafAigId[5], LeafPhase[5] );
}

void Map_Stmap80PrintCandidateCutSummary( void )
{
    printf( "%s candidate-cut stats: parent-aig = %d  child-aig = %d  rows = %d  viable = %d  accepted-updates = %d  child-phase0 = %d  child-phase1 = %d  child-unknown = %d  child-missing = %d  skip-fanout = %d  no-supers = %d  no-super-best = %d  violates-required = %d  area-sensitive-skip = %d\n",
        s_pStmap80CandidateCutLabel, s_Stmap80ParentAigId, s_Stmap80ChildAigId,
        s_nStmap80CandidateCutRows, s_nStmap80CandidateCutViable,
        s_nStmap80CandidateCutAccepted, s_nStmap80CandidateChildPhase0,
        s_nStmap80CandidateChildPhase1, s_nStmap80CandidateChildUnknown,
        s_nStmap80CandidateChildMissing, s_nStmap80CandidateSkipFanout,
        s_nStmap80CandidateNoSupers, s_nStmap80CandidateNoSuperBest,
        s_nStmap80CandidateRequired, s_nStmap80CandidateAreaSensitive );
}

static int s_fStmap81ParentPhaseBias = 0;
static int s_fStmap81ParentPhaseBiasActive = 0;
static const char * s_pStmap81ParentPhaseBiasLabel = "stmap81";
static int s_Stmap81ParentAigId = -1;
static int s_Stmap81ChildAigId = -1;
static int s_nStmap81ParentPhaseRows = 0;
static int s_nStmap81ParentPhaseChild0 = 0;
static int s_nStmap81ParentPhaseEligible = 0;
static int s_nStmap81ParentPhaseOverrides = 0;
static int s_nStmap81ParentPhaseAlready = 0;
static int s_nStmap81ParentPhaseBlockedMode = 0;
static int s_nStmap81ParentPhaseBlockedBest = 0;
static int s_nStmap81ParentPhaseBlockedWindow = 0;
static int s_nStmap81ParentPhaseBlockedChild = 0;

static void Map_Stmap81ResetParentPhaseBiasCounters( void )
{
    s_nStmap81ParentPhaseRows = 0;
    s_nStmap81ParentPhaseChild0 = 0;
    s_nStmap81ParentPhaseEligible = 0;
    s_nStmap81ParentPhaseOverrides = 0;
    s_nStmap81ParentPhaseAlready = 0;
    s_nStmap81ParentPhaseBlockedMode = 0;
    s_nStmap81ParentPhaseBlockedBest = 0;
    s_nStmap81ParentPhaseBlockedWindow = 0;
    s_nStmap81ParentPhaseBlockedChild = 0;
}

static void Map_Stmap81ClearParentPhaseBias( void )
{
    s_fStmap81ParentPhaseBias = 0;
    s_fStmap81ParentPhaseBiasActive = 0;
    s_pStmap81ParentPhaseBiasLabel = "stmap81";
    s_Stmap81ParentAigId = -1;
    s_Stmap81ChildAigId = -1;
    Map_Stmap81ResetParentPhaseBiasCounters();
}

void Map_Stmap81SetParentPhaseBias( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId )
{
    Map_Stmap81ClearParentPhaseBias();
    s_pStmap81ParentPhaseBiasLabel = pLabel && pLabel[0] ? pLabel : "stmap81";
    if ( !fEnable || ParentAigId < 0 || ChildAigId < 0 )
        return;
    s_fStmap81ParentPhaseBias = 1;
    s_Stmap81ParentAigId = ParentAigId;
    s_Stmap81ChildAigId = ChildAigId;
}

int Map_Stmap81ParentPhaseBiasConfigured( void )
{
    return s_fStmap81ParentPhaseBias;
}

void Map_Stmap81SetParentPhaseBiasActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap81ParentPhaseBias )
        return;
    if ( fActive )
        Map_Stmap81ResetParentPhaseBiasCounters();
    s_fStmap81ParentPhaseBiasActive = fActive;
    (void)pPassLabel;
}

static int Map_Stmap81ReadChildPhase( Map_Cut_t * pCut, Map_Match_t * pMatch, int ChildAigId, int * pChildLeaf, int * pChildPhase )
{
    int i, LeafAigId;
    unsigned uPhaseBest;
    *pChildLeaf = -1;
    *pChildPhase = -1;
    if ( pCut == NULL || pMatch == NULL || pMatch->pSuperBest == NULL )
        return 0;
    uPhaseBest = pMatch->uPhaseBest;
    for ( i = 0; i < pCut->nLeaves && i < 6; i++ )
    {
        LeafAigId = Map_NodeReadAigId( Map_Regular(pCut->ppLeaves[i]) );
        if ( LeafAigId != ChildAigId )
            continue;
        *pChildLeaf = i;
        *pChildPhase = ((uPhaseBest & (1 << i)) > 0) ? 0 : 1;
        return 1;
    }
    return 0;
}

static int Map_Stmap81MaybeBiasParentPhase( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, int CutOrdinal, Map_Match_t * pMatch, Map_Match_t * pBestBefore, int fAccepted )
{
    Map_Node_t * pNodeRegular;
    Mio_Gate_t * pGate, * pGateBefore;
    int AigId, ChildLeaf, ChildPhase, fEligible, fOverride;
    float ArrivalDelta, AreaPremium, AreaRatio, ArrivalWindow, AreaPremiumWindow;
    const char * pAction;
    if ( !s_fStmap81ParentPhaseBias || !s_fStmap81ParentPhaseBiasActive || p == NULL || pNode == NULL || pCut == NULL || pMatch == NULL )
        return fAccepted;
    pNodeRegular = Map_Regular( pNode );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return fAccepted;
    AigId = Map_NodeReadAigId( pNodeRegular );
    if ( AigId != s_Stmap81ParentAigId )
        return fAccepted;

    s_nStmap81ParentPhaseRows++;
    Map_Stmap81ReadChildPhase( pCut, pMatch, s_Stmap81ChildAigId, &ChildLeaf, &ChildPhase );
    if ( ChildPhase == 0 )
        s_nStmap81ParentPhaseChild0++;
    else
        s_nStmap81ParentPhaseBlockedChild++;

    ArrivalWindow = 0.75f;
    AreaPremiumWindow = 0.75f;
    ArrivalDelta = pBestBefore ? pMatch->tArrive.Worst - pBestBefore->tArrive.Worst : MAP_FLOAT_LARGE;
    AreaPremium = pBestBefore ? pMatch->AreaFlow - pBestBefore->AreaFlow : MAP_FLOAT_LARGE;
    AreaRatio = (pBestBefore && pBestBefore->AreaFlow > p->fEpsilon) ? pMatch->AreaFlow / pBestBefore->AreaFlow : MAP_FLOAT_LARGE;
    fEligible = 0;
    fOverride = 0;
    pAction = "blocked-child-phase";

    if ( ChildPhase == 0 )
    {
        if ( p->fMappingMode != 2 && p->fMappingMode != 3 )
        {
            s_nStmap81ParentPhaseBlockedMode++;
            pAction = "blocked-mode";
        }
        else if ( pBestBefore == NULL || pBestBefore->pSuperBest == NULL )
        {
            s_nStmap81ParentPhaseBlockedBest++;
            pAction = "blocked-no-best";
        }
        else if ( ArrivalDelta > ArrivalWindow + p->fEpsilon || AreaPremium > AreaPremiumWindow + p->fEpsilon || AreaRatio > 2.00f + p->fEpsilon )
        {
            s_nStmap81ParentPhaseBlockedWindow++;
            pAction = "blocked-window";
        }
        else
        {
            s_nStmap81ParentPhaseEligible++;
            fEligible = 1;
            if ( fAccepted )
            {
                s_nStmap81ParentPhaseAlready++;
                pAction = "already-selected";
            }
            else
            {
                s_nStmap81ParentPhaseOverrides++;
                fOverride = 1;
                pAction = "override";
                fAccepted = 1;
            }
        }
    }

    if ( ChildPhase == 0 || fOverride )
    {
        pGate = pMatch && pMatch->pSuperBest ? pMatch->pSuperBest->pRoot : NULL;
        pGateBefore = pBestBefore && pBestBefore->pSuperBest ? pBestBefore->pSuperBest->pRoot : NULL;
        printf( "%s parent-phase-bias: index = %d  parent-aig = %d  child-aig = %d  mapper-mode = %d  phase = %d  cut-ordinal = %d  action = %s  eligible = %d  override = %d  accepted-before = %d  child-leaf-index = %d  child-requested-phase = %d  gate = %s  best-gate = %s  arrival = %.3f  best-arrival = %.3f  arrival-delta = %.3f  area-flow = %.3f  best-area-flow = %.3f  area-premium = %.3f  area-ratio = %.3f  arrival-window = %.3f  area-premium-window = %.3f\n",
            s_pStmap81ParentPhaseBiasLabel, s_nStmap81ParentPhaseRows,
            s_Stmap81ParentAigId, s_Stmap81ChildAigId, p->fMappingMode, fPhase,
            CutOrdinal, pAction, fEligible, fOverride, fAccepted && !fOverride,
            ChildLeaf, ChildPhase, pGate ? Mio_GateReadName(pGate) : "?",
            pGateBefore ? Mio_GateReadName(pGateBefore) : "?",
            pMatch->tArrive.Worst, pBestBefore ? pBestBefore->tArrive.Worst : MAP_FLOAT_LARGE,
            ArrivalDelta, pMatch->AreaFlow, pBestBefore ? pBestBefore->AreaFlow : MAP_FLOAT_LARGE,
            AreaPremium, AreaRatio, ArrivalWindow, AreaPremiumWindow );
    }
    return fAccepted;
}

void Map_Stmap81PrintParentPhaseBiasSummary( void )
{
    printf( "%s parent-phase-bias stats: parent-aig = %d  child-aig = %d  rows = %d  child-phase0 = %d  eligible = %d  overrides = %d  already-selected = %d  blocked-mode = %d  blocked-best = %d  blocked-window = %d  blocked-child = %d  arrival-window = %.3f  area-premium-window = %.3f  area-ratio-window = %.3f\n",
        s_pStmap81ParentPhaseBiasLabel, s_Stmap81ParentAigId, s_Stmap81ChildAigId,
        s_nStmap81ParentPhaseRows, s_nStmap81ParentPhaseChild0,
        s_nStmap81ParentPhaseEligible, s_nStmap81ParentPhaseOverrides,
        s_nStmap81ParentPhaseAlready, s_nStmap81ParentPhaseBlockedMode,
        s_nStmap81ParentPhaseBlockedBest, s_nStmap81ParentPhaseBlockedWindow,
        s_nStmap81ParentPhaseBlockedChild, 0.75f, 0.75f, 2.00f );
}

static int s_fStmap82StickyParentPhase = 0;
static int s_fStmap82StickyParentPhaseActive = 0;
static const char * s_pStmap82StickyParentPhaseLabel = "stmap82";
static int s_Stmap82ParentAigId = -1;
static int s_Stmap82ChildAigId = -1;
static int s_nStmap82StickyRows = 0;
static int s_nStmap82StickySet = 0;
static int s_nStmap82StickyBlocked = 0;
static int s_nStmap82StickyAllowedTiming = 0;
static int s_nStmap82StickyAllowedChild0 = 0;
static int s_nStmap82StickyIgnored = 0;
static float s_Stmap82StickyTimingHoldWindow = 3.00f;

static void Map_Stmap82ResetStickyParentPhaseCounters( void )
{
    s_nStmap82StickyRows = 0;
    s_nStmap82StickySet = 0;
    s_nStmap82StickyBlocked = 0;
    s_nStmap82StickyAllowedTiming = 0;
    s_nStmap82StickyAllowedChild0 = 0;
    s_nStmap82StickyIgnored = 0;
}

static void Map_Stmap82ClearStickyParentPhase( void )
{
    s_fStmap82StickyParentPhase = 0;
    s_fStmap82StickyParentPhaseActive = 0;
    s_pStmap82StickyParentPhaseLabel = "stmap82";
    s_Stmap82ParentAigId = -1;
    s_Stmap82ChildAigId = -1;
    s_Stmap82StickyTimingHoldWindow = 3.00f;
    Map_Stmap82ResetStickyParentPhaseCounters();
}

void Map_Stmap82SetStickyParentPhase( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId )
{
    Map_Stmap82ClearStickyParentPhase();
    s_pStmap82StickyParentPhaseLabel = pLabel && pLabel[0] ? pLabel : "stmap82";
    if ( !fEnable || ParentAigId < 0 || ChildAigId < 0 )
        return;
    s_fStmap82StickyParentPhase = 1;
    s_Stmap82ParentAigId = ParentAigId;
    s_Stmap82ChildAigId = ChildAigId;
}

void Map_Stmap82SetStickyParentPhaseTimingWindow( float TimingHoldWindow )
{
    s_Stmap82StickyTimingHoldWindow = TimingHoldWindow > 0.0f ? TimingHoldWindow : 3.00f;
}

int Map_Stmap82StickyParentPhaseConfigured( void )
{
    return s_fStmap82StickyParentPhase;
}

void Map_Stmap82SetStickyParentPhaseActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap82StickyParentPhase )
        return;
    if ( fActive )
        Map_Stmap82ResetStickyParentPhaseCounters();
    s_fStmap82StickyParentPhaseActive = fActive;
    (void)pPassLabel;
}

static int Map_Stmap82StickyParentPhaseApplies( Map_Man_t * p, Map_Node_t * pNode, int fPhase )
{
    Map_Node_t * pNodeRegular;
    if ( !s_fStmap82StickyParentPhase || !s_fStmap82StickyParentPhaseActive || p == NULL || pNode == NULL )
        return 0;
    if ( p->fMappingMode != 2 && p->fMappingMode != 3 )
        return 0;
    pNodeRegular = Map_Regular( pNode );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return 0;
    (void)fPhase;
    return Map_NodeReadAigId( pNodeRegular ) == s_Stmap82ParentAigId;
}

static void Map_Stmap82RememberStickyParentPhase( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, int CutOrdinal, Map_Match_t * pMatch, int fAccepted, int * pfSticky, Map_Match_t * pStickyMatch, Map_Cut_t ** ppStickyCut, int * pStickyCutOrdinal )
{
    int ChildLeaf, ChildPhase;
    if ( !fAccepted || !Map_Stmap82StickyParentPhaseApplies( p, pNode, fPhase ) )
        return;
    if ( !Map_Stmap81ReadChildPhase( pCut, pMatch, s_Stmap82ChildAigId, &ChildLeaf, &ChildPhase ) || ChildPhase != 0 )
        return;
    *pfSticky = 1;
    *pStickyMatch = *pMatch;
    *ppStickyCut = pCut;
    *pStickyCutOrdinal = CutOrdinal;
    s_nStmap82StickySet++;
    printf( "%s sticky-parent-phase: index = %d  parent-aig = %d  child-aig = %d  mapper-mode = %d  phase = %d  action = set-sticky  cut-ordinal = %d  child-leaf-index = %d  child-requested-phase = %d  gate = %s  arrival = %.3f  area-flow = %.3f  sticky-cut-ordinal = %d  sticky-arrival = %.3f  sticky-area-flow = %.3f  timing-hold-window = %.3f\n",
        s_pStmap82StickyParentPhaseLabel, ++s_nStmap82StickyRows,
        s_Stmap82ParentAigId, s_Stmap82ChildAigId, p->fMappingMode, fPhase,
        CutOrdinal, ChildLeaf, ChildPhase,
        pMatch->pSuperBest ? Mio_GateReadName(pMatch->pSuperBest->pRoot) : "?",
        pMatch->tArrive.Worst, pMatch->AreaFlow,
        *pStickyCutOrdinal, pStickyMatch->tArrive.Worst, pStickyMatch->AreaFlow, s_Stmap82StickyTimingHoldWindow );
}

static int Map_Stmap82MaybeBlockStickyReplacement( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, int CutOrdinal, Map_Match_t * pMatch, int fAccepted, int fSticky, Map_Match_t * pStickyMatch, Map_Cut_t * pStickyCut, int StickyCutOrdinal )
{
    int ChildLeaf, ChildPhase;
    float TimingGain;
    const char * pAction;
    if ( !fAccepted || !fSticky || !Map_Stmap82StickyParentPhaseApplies( p, pNode, fPhase ) )
        return fAccepted;
    Map_Stmap81ReadChildPhase( pCut, pMatch, s_Stmap82ChildAigId, &ChildLeaf, &ChildPhase );
    TimingGain = pStickyMatch->tArrive.Worst - pMatch->tArrive.Worst;
    if ( ChildPhase == 0 )
    {
        s_nStmap82StickyAllowedChild0++;
        pAction = "allow-child-phase0";
    }
    else if ( TimingGain > s_Stmap82StickyTimingHoldWindow + p->fEpsilon )
    {
        s_nStmap82StickyAllowedTiming++;
        pAction = "allow-timing";
    }
    else
    {
        s_nStmap82StickyBlocked++;
        pAction = "block-child-missing";
        fAccepted = 0;
    }
    if ( pAction[0] == 'a' )
        s_nStmap82StickyIgnored++;
    printf( "%s sticky-parent-phase: index = %d  parent-aig = %d  child-aig = %d  mapper-mode = %d  phase = %d  action = %s  cut-ordinal = %d  child-leaf-index = %d  child-requested-phase = %d  gate = %s  arrival = %.3f  area-flow = %.3f  sticky-cut-ordinal = %d  sticky-gate = %s  sticky-arrival = %.3f  sticky-area-flow = %.3f  timing-gain = %.3f  timing-hold-window = %.3f\n",
        s_pStmap82StickyParentPhaseLabel, ++s_nStmap82StickyRows,
        s_Stmap82ParentAigId, s_Stmap82ChildAigId, p->fMappingMode, fPhase,
        pAction, CutOrdinal, ChildLeaf, ChildPhase,
        pMatch->pSuperBest ? Mio_GateReadName(pMatch->pSuperBest->pRoot) : "?",
        pMatch->tArrive.Worst, pMatch->AreaFlow,
        StickyCutOrdinal,
        pStickyMatch->pSuperBest ? Mio_GateReadName(pStickyMatch->pSuperBest->pRoot) : "?",
        pStickyMatch->tArrive.Worst, pStickyMatch->AreaFlow, TimingGain, s_Stmap82StickyTimingHoldWindow );
    (void)pStickyCut;
    return fAccepted;
}

void Map_Stmap82PrintStickyParentPhaseSummary( void )
{
    printf( "%s sticky-parent-phase stats: parent-aig = %d  child-aig = %d  rows = %d  sticky-set = %d  blocked-replacements = %d  allowed-timing = %d  allowed-child-phase0 = %d  allowed-total = %d  timing-hold-window = %.3f\n",
        s_pStmap82StickyParentPhaseLabel, s_Stmap82ParentAigId, s_Stmap82ChildAigId,
        s_nStmap82StickyRows, s_nStmap82StickySet, s_nStmap82StickyBlocked,
        s_nStmap82StickyAllowedTiming, s_nStmap82StickyAllowedChild0,
        s_nStmap82StickyIgnored, s_Stmap82StickyTimingHoldWindow );
}

static int s_fStmap87ParentPhaseTarget = 0;
static int s_fStmap87ParentPhaseTargetActive = 0;
static const char * s_pStmap87ParentPhaseTargetLabel = "stmap87";
static int s_Stmap87ParentAigId = -1;
static int s_Stmap87ChildAigId = -1;
static int s_Stmap87ParentPhase = 0;
static int s_Stmap87ChildPhase = 1;
static int s_nStmap87TargetRows = 0;
static int s_nStmap87TargetChildHits = 0;
static int s_nStmap87TargetEligible = 0;
static int s_nStmap87TargetOverrides = 0;
static int s_nStmap87TargetAlready = 0;
static int s_nStmap87TargetBlockedMode = 0;
static int s_nStmap87TargetBlockedPhase = 0;
static int s_nStmap87TargetBlockedBest = 0;
static int s_nStmap87TargetBlockedWindow = 0;
static int s_nStmap87TargetBlockedChild = 0;

static void Map_Stmap87ResetParentPhaseTargetCounters( void )
{
    s_nStmap87TargetRows = 0;
    s_nStmap87TargetChildHits = 0;
    s_nStmap87TargetEligible = 0;
    s_nStmap87TargetOverrides = 0;
    s_nStmap87TargetAlready = 0;
    s_nStmap87TargetBlockedMode = 0;
    s_nStmap87TargetBlockedPhase = 0;
    s_nStmap87TargetBlockedBest = 0;
    s_nStmap87TargetBlockedWindow = 0;
    s_nStmap87TargetBlockedChild = 0;
}

static void Map_Stmap87ClearParentPhaseTarget( void )
{
    s_fStmap87ParentPhaseTarget = 0;
    s_fStmap87ParentPhaseTargetActive = 0;
    s_pStmap87ParentPhaseTargetLabel = "stmap87";
    s_Stmap87ParentAigId = -1;
    s_Stmap87ChildAigId = -1;
    s_Stmap87ParentPhase = 0;
    s_Stmap87ChildPhase = 1;
    Map_Stmap87ResetParentPhaseTargetCounters();
}

void Map_Stmap87SetParentPhaseTarget( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId, int ParentPhase, int ChildPhase )
{
    Map_Stmap87ClearParentPhaseTarget();
    s_pStmap87ParentPhaseTargetLabel = pLabel && pLabel[0] ? pLabel : "stmap87";
    if ( !fEnable || ParentAigId < 0 || ChildAigId < 0 || ParentPhase < 0 || ParentPhase > 1 || ChildPhase < 0 || ChildPhase > 1 )
        return;
    s_fStmap87ParentPhaseTarget = 1;
    s_Stmap87ParentAigId = ParentAigId;
    s_Stmap87ChildAigId = ChildAigId;
    s_Stmap87ParentPhase = ParentPhase;
    s_Stmap87ChildPhase = ChildPhase;
}

int Map_Stmap87ParentPhaseTargetConfigured( void )
{
    return s_fStmap87ParentPhaseTarget;
}

void Map_Stmap87SetParentPhaseTargetActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap87ParentPhaseTarget )
        return;
    if ( fActive )
        Map_Stmap87ResetParentPhaseTargetCounters();
    s_fStmap87ParentPhaseTargetActive = fActive;
    (void)pPassLabel;
}

static int Map_Stmap87MaybeTargetParentPhase( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, int CutOrdinal, Map_Match_t * pMatch, Map_Match_t * pBestBefore, int fAccepted )
{
    Map_Node_t * pNodeRegular;
    Mio_Gate_t * pGate, * pGateBefore;
    int AigId, ChildLeaf, ChildPhase, fEligible, fOverride;
    float ArrivalDelta, AreaPremium, AreaRatio, ArrivalWindow, AreaPremiumWindow, AreaRatioWindow;
    const char * pAction;
    if ( !s_fStmap87ParentPhaseTarget || !s_fStmap87ParentPhaseTargetActive || p == NULL || pNode == NULL || pCut == NULL || pMatch == NULL )
        return fAccepted;
    pNodeRegular = Map_Regular( pNode );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return fAccepted;
    AigId = Map_NodeReadAigId( pNodeRegular );
    if ( AigId != s_Stmap87ParentAigId )
        return fAccepted;

    s_nStmap87TargetRows++;
    Map_Stmap81ReadChildPhase( pCut, pMatch, s_Stmap87ChildAigId, &ChildLeaf, &ChildPhase );
    if ( ChildPhase == s_Stmap87ChildPhase )
        s_nStmap87TargetChildHits++;
    else
        s_nStmap87TargetBlockedChild++;

    ArrivalWindow = 0.20f;
    AreaPremiumWindow = 0.60f;
    AreaRatioWindow = 1.75f;
    ArrivalDelta = pBestBefore ? pMatch->tArrive.Worst - pBestBefore->tArrive.Worst : MAP_FLOAT_LARGE;
    AreaPremium = pBestBefore ? pMatch->AreaFlow - pBestBefore->AreaFlow : MAP_FLOAT_LARGE;
    AreaRatio = (pBestBefore && pBestBefore->AreaFlow > p->fEpsilon) ? pMatch->AreaFlow / pBestBefore->AreaFlow : MAP_FLOAT_LARGE;
    fEligible = 0;
    fOverride = 0;
    pAction = "blocked-child-phase";

    if ( fPhase != s_Stmap87ParentPhase )
    {
        s_nStmap87TargetBlockedPhase++;
        pAction = "blocked-parent-phase";
    }
    else if ( ChildPhase == s_Stmap87ChildPhase )
    {
        if ( p->fMappingMode != 2 && p->fMappingMode != 3 )
        {
            s_nStmap87TargetBlockedMode++;
            pAction = "blocked-mode";
        }
        else if ( pBestBefore == NULL || pBestBefore->pSuperBest == NULL )
        {
            s_nStmap87TargetBlockedBest++;
            pAction = "blocked-no-best";
        }
        else if ( ArrivalDelta > ArrivalWindow + p->fEpsilon || AreaPremium > AreaPremiumWindow + p->fEpsilon || AreaRatio > AreaRatioWindow + p->fEpsilon )
        {
            s_nStmap87TargetBlockedWindow++;
            pAction = "blocked-window";
        }
        else
        {
            s_nStmap87TargetEligible++;
            fEligible = 1;
            if ( fAccepted )
            {
                s_nStmap87TargetAlready++;
                pAction = "already-selected";
            }
            else
            {
                s_nStmap87TargetOverrides++;
                fOverride = 1;
                pAction = "override";
                fAccepted = 1;
            }
        }
    }

    if ( ChildPhase == s_Stmap87ChildPhase || fOverride )
    {
        pGate = pMatch && pMatch->pSuperBest ? pMatch->pSuperBest->pRoot : NULL;
        pGateBefore = pBestBefore && pBestBefore->pSuperBest ? pBestBefore->pSuperBest->pRoot : NULL;
        printf( "%s parent-phase-target: index = %d  parent-aig = %d  child-aig = %d  target-parent-phase = %d  target-child-phase = %d  mapper-mode = %d  phase = %d  cut-ordinal = %d  action = %s  eligible = %d  override = %d  accepted-before = %d  child-leaf-index = %d  child-requested-phase = %d  gate = %s  best-gate = %s  arrival = %.3f  best-arrival = %.3f  arrival-delta = %.3f  area-flow = %.3f  best-area-flow = %.3f  area-premium = %.3f  area-ratio = %.3f  arrival-window = %.3f  area-premium-window = %.3f  area-ratio-window = %.3f\n",
            s_pStmap87ParentPhaseTargetLabel, s_nStmap87TargetRows,
            s_Stmap87ParentAigId, s_Stmap87ChildAigId, s_Stmap87ParentPhase,
            s_Stmap87ChildPhase, p->fMappingMode, fPhase, CutOrdinal, pAction,
            fEligible, fOverride, fAccepted && !fOverride, ChildLeaf, ChildPhase,
            pGate ? Mio_GateReadName(pGate) : "?",
            pGateBefore ? Mio_GateReadName(pGateBefore) : "?",
            pMatch->tArrive.Worst, pBestBefore ? pBestBefore->tArrive.Worst : MAP_FLOAT_LARGE,
            ArrivalDelta, pMatch->AreaFlow, pBestBefore ? pBestBefore->AreaFlow : MAP_FLOAT_LARGE,
            AreaPremium, AreaRatio, ArrivalWindow, AreaPremiumWindow, AreaRatioWindow );
    }
    return fAccepted;
}

void Map_Stmap87PrintParentPhaseTargetSummary( void )
{
    printf( "%s parent-phase-target stats: parent-aig = %d  child-aig = %d  target-parent-phase = %d  target-child-phase = %d  rows = %d  target-child-hits = %d  eligible = %d  overrides = %d  already-selected = %d  blocked-mode = %d  blocked-parent-phase = %d  blocked-best = %d  blocked-window = %d  blocked-child = %d  arrival-window = %.3f  area-premium-window = %.3f  area-ratio-window = %.3f\n",
        s_pStmap87ParentPhaseTargetLabel, s_Stmap87ParentAigId, s_Stmap87ChildAigId,
        s_Stmap87ParentPhase, s_Stmap87ChildPhase, s_nStmap87TargetRows,
        s_nStmap87TargetChildHits, s_nStmap87TargetEligible, s_nStmap87TargetOverrides,
        s_nStmap87TargetAlready, s_nStmap87TargetBlockedMode, s_nStmap87TargetBlockedPhase,
        s_nStmap87TargetBlockedBest, s_nStmap87TargetBlockedWindow, s_nStmap87TargetBlockedChild,
        0.20f, 0.60f, 1.75f );
}

static int s_fStmap88ChildPhaseTarget = 0;
static int s_fStmap88ChildPhaseTargetActive = 0;
static const char * s_pStmap88ChildPhaseTargetLabel = "stmap88";
static int s_Stmap88ChildAigId = -1;
static int s_Stmap88TargetPhase = 1;
static int s_nStmap88ChildTargetRows = 0;
static int s_nStmap88ChildTargetPhaseHits = 0;
static int s_nStmap88ChildTargetEligible = 0;
static int s_nStmap88ChildTargetOverrides = 0;
static int s_nStmap88ChildTargetAlready = 0;
static int s_nStmap88ChildTargetBlockedMode = 0;
static int s_nStmap88ChildTargetBlockedPhase = 0;
static int s_nStmap88ChildTargetBlockedBest = 0;
static int s_nStmap88ChildTargetBlockedWindow = 0;
static int s_nStmap88ChildTargetBlockedSpeed = 0;

static void Map_Stmap88ResetChildPhaseTargetCounters( void )
{
    s_nStmap88ChildTargetRows = 0;
    s_nStmap88ChildTargetPhaseHits = 0;
    s_nStmap88ChildTargetEligible = 0;
    s_nStmap88ChildTargetOverrides = 0;
    s_nStmap88ChildTargetAlready = 0;
    s_nStmap88ChildTargetBlockedMode = 0;
    s_nStmap88ChildTargetBlockedPhase = 0;
    s_nStmap88ChildTargetBlockedBest = 0;
    s_nStmap88ChildTargetBlockedWindow = 0;
    s_nStmap88ChildTargetBlockedSpeed = 0;
}

static void Map_Stmap88ClearChildPhaseTarget( void )
{
    s_fStmap88ChildPhaseTarget = 0;
    s_fStmap88ChildPhaseTargetActive = 0;
    s_pStmap88ChildPhaseTargetLabel = "stmap88";
    s_Stmap88ChildAigId = -1;
    s_Stmap88TargetPhase = 1;
    Map_Stmap88ResetChildPhaseTargetCounters();
}

void Map_Stmap88SetChildPhaseTarget( int fEnable, const char * pLabel, int ChildAigId, int TargetPhase )
{
    Map_Stmap88ClearChildPhaseTarget();
    s_pStmap88ChildPhaseTargetLabel = pLabel && pLabel[0] ? pLabel : "stmap88";
    if ( !fEnable || ChildAigId < 0 || TargetPhase < 0 || TargetPhase > 1 )
        return;
    s_fStmap88ChildPhaseTarget = 1;
    s_Stmap88ChildAigId = ChildAigId;
    s_Stmap88TargetPhase = TargetPhase;
}

int Map_Stmap88ChildPhaseTargetConfigured( void )
{
    return s_fStmap88ChildPhaseTarget;
}

void Map_Stmap88SetChildPhaseTargetActive( int fActive, const char * pPassLabel )
{
    if ( !s_fStmap88ChildPhaseTarget )
        return;
    if ( fActive )
        Map_Stmap88ResetChildPhaseTargetCounters();
    s_fStmap88ChildPhaseTargetActive = fActive;
    (void)pPassLabel;
}

static int Map_Stmap88MaybeTargetChildPhase( Map_Man_t * p, Map_Node_t * pNode, int fPhase, int CutOrdinal, Map_Match_t * pMatch, Map_Match_t * pBestBefore, int fAccepted )
{
    Map_Node_t * pNodeRegular;
    Mio_Gate_t * pGate, * pGateBefore;
    int AigId, fEligible, fOverride;
    float ArrivalGain, AreaPremium, AreaRatio, MinArrivalGain, AreaPremiumWindow, AreaRatioWindow;
    const char * pAction;
    if ( !s_fStmap88ChildPhaseTarget || !s_fStmap88ChildPhaseTargetActive || p == NULL || pNode == NULL || pMatch == NULL )
        return fAccepted;
    pNodeRegular = Map_Regular( pNode );
    if ( Map_NodeIsConst( pNodeRegular ) )
        return fAccepted;
    AigId = Map_NodeReadAigId( pNodeRegular );
    if ( AigId != s_Stmap88ChildAigId )
        return fAccepted;

    s_nStmap88ChildTargetRows++;
    if ( fPhase == s_Stmap88TargetPhase )
        s_nStmap88ChildTargetPhaseHits++;

    MinArrivalGain = 0.02f;
    AreaPremiumWindow = 1.20f;
    AreaRatioWindow = 2.00f;
    ArrivalGain = pBestBefore ? pBestBefore->tArrive.Worst - pMatch->tArrive.Worst : -MAP_FLOAT_LARGE;
    AreaPremium = pBestBefore ? pMatch->AreaFlow - pBestBefore->AreaFlow : MAP_FLOAT_LARGE;
    AreaRatio = (pBestBefore && pBestBefore->AreaFlow > p->fEpsilon) ? pMatch->AreaFlow / pBestBefore->AreaFlow : MAP_FLOAT_LARGE;
    fEligible = 0;
    fOverride = 0;
    pAction = "blocked-phase";

    if ( fPhase != s_Stmap88TargetPhase )
    {
        s_nStmap88ChildTargetBlockedPhase++;
        pAction = "blocked-phase";
    }
    else if ( p->fMappingMode != 2 && p->fMappingMode != 3 )
    {
        s_nStmap88ChildTargetBlockedMode++;
        pAction = "blocked-mode";
    }
    else if ( pBestBefore == NULL || pBestBefore->pSuperBest == NULL )
    {
        s_nStmap88ChildTargetBlockedBest++;
        pAction = "blocked-no-best";
    }
    else if ( ArrivalGain < MinArrivalGain - p->fEpsilon )
    {
        s_nStmap88ChildTargetBlockedSpeed++;
        pAction = "blocked-speed";
    }
    else if ( AreaPremium > AreaPremiumWindow + p->fEpsilon || AreaRatio > AreaRatioWindow + p->fEpsilon )
    {
        s_nStmap88ChildTargetBlockedWindow++;
        pAction = "blocked-window";
    }
    else
    {
        s_nStmap88ChildTargetEligible++;
        fEligible = 1;
        if ( fAccepted )
        {
            s_nStmap88ChildTargetAlready++;
            pAction = "already-selected";
        }
        else
        {
            s_nStmap88ChildTargetOverrides++;
            fOverride = 1;
            pAction = "override";
            fAccepted = 1;
        }
    }

    if ( fPhase == s_Stmap88TargetPhase || fOverride )
    {
        pGate = pMatch && pMatch->pSuperBest ? pMatch->pSuperBest->pRoot : NULL;
        pGateBefore = pBestBefore && pBestBefore->pSuperBest ? pBestBefore->pSuperBest->pRoot : NULL;
        printf( "%s child-phase-target: index = %d  child-aig = %d  target-phase = %d  mapper-mode = %d  phase = %d  cut-ordinal = %d  action = %s  eligible = %d  override = %d  accepted-before = %d  gate = %s  best-gate = %s  arrival = %.3f  best-arrival = %.3f  arrival-gain = %.3f  area-flow = %.3f  best-area-flow = %.3f  area-premium = %.3f  area-ratio = %.3f  min-arrival-gain = %.3f  area-premium-window = %.3f  area-ratio-window = %.3f\n",
            s_pStmap88ChildPhaseTargetLabel, s_nStmap88ChildTargetRows,
            s_Stmap88ChildAigId, s_Stmap88TargetPhase, p->fMappingMode, fPhase,
            CutOrdinal, pAction, fEligible, fOverride, fAccepted && !fOverride,
            pGate ? Mio_GateReadName(pGate) : "?",
            pGateBefore ? Mio_GateReadName(pGateBefore) : "?",
            pMatch->tArrive.Worst, pBestBefore ? pBestBefore->tArrive.Worst : MAP_FLOAT_LARGE,
            ArrivalGain, pMatch->AreaFlow, pBestBefore ? pBestBefore->AreaFlow : MAP_FLOAT_LARGE,
            AreaPremium, AreaRatio, MinArrivalGain, AreaPremiumWindow, AreaRatioWindow );
    }
    return fAccepted;
}

void Map_Stmap88PrintChildPhaseTargetSummary( void )
{
    printf( "%s child-phase-target stats: child-aig = %d  target-phase = %d  rows = %d  phase-hits = %d  eligible = %d  overrides = %d  already-selected = %d  blocked-mode = %d  blocked-phase = %d  blocked-best = %d  blocked-window = %d  blocked-speed = %d  min-arrival-gain = %.3f  area-premium-window = %.3f  area-ratio-window = %.3f\n",
        s_pStmap88ChildPhaseTargetLabel, s_Stmap88ChildAigId, s_Stmap88TargetPhase,
        s_nStmap88ChildTargetRows, s_nStmap88ChildTargetPhaseHits,
        s_nStmap88ChildTargetEligible, s_nStmap88ChildTargetOverrides,
        s_nStmap88ChildTargetAlready, s_nStmap88ChildTargetBlockedMode,
        s_nStmap88ChildTargetBlockedPhase, s_nStmap88ChildTargetBlockedBest,
        s_nStmap88ChildTargetBlockedWindow, s_nStmap88ChildTargetBlockedSpeed,
        0.02f, 1.20f, 2.00f );
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the cut is a high-fanout wide-cut risk.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchCutHasStmapFanoutRisk( Map_Node_t * pNode, Map_Cut_t * pCut )
{
    return (pNode->nRefs > 6 && pCut->nLeaves > 2) || (pNode->nRefs > 3 && pCut->nLeaves > 3);
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the cut is in the highest stmap fanout bucket.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchCutHasStmapHighestFanoutRisk( Map_Node_t * pNode, Map_Cut_t * pCut )
{
    return pNode->nRefs > 6 && pCut->nLeaves > 2;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the cut is in the lower moderate fanout bucket.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchCutHasStmapLowerModerateFanoutRisk( Map_Node_t * pNode, Map_Cut_t * pCut )
{
    return pNode->nRefs > 3 && pNode->nRefs <= 5 && pCut->nLeaves > 3;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the cut is in the upper moderate fanout bucket.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchCutHasStmapUpperModerateFanoutRisk( Map_Node_t * pNode, Map_Cut_t * pCut )
{
    return pNode->nRefs == 6 && pCut->nLeaves > 3;
}

/**Function*************************************************************

  Synopsis    [Records stmap13+ exact-area guard statistics.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Map_MatchStmap13CountRisk( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut )
{
    if ( p->fSkipFanout < 14 || p->fSkipFanout > 57 )
        return;
    p->nStmap13ExactRisk++;
    if ( Map_MatchCutHasStmapHighestFanoutRisk( pNode, pCut ) )
        p->nStmap13HighestRisk++;
    else if ( Map_MatchCutHasStmapLowerModerateFanoutRisk( pNode, pCut ) )
        p->nStmap13LowerModRisk++;
    else if ( Map_MatchCutHasStmapUpperModerateFanoutRisk( pNode, pCut ) )
        p->nStmap13UpperModRisk++;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the node is in the stmap10 relief depth window.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchNodeHasStmapReliefDepth( Map_Node_t * pNode )
{
    return pNode->Level <= 96;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the node is in the stmap11 relief depth window.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchNodeHasStmapTightReliefDepth( Map_Node_t * pNode )
{
    return pNode->Level <= 64;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the stmap mode allows middle-slack relief here.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchNodeHasStmapMiddleSlackRelief( int Mode, Map_Node_t * pNode )
{
    if ( Mode == 10 )
        return 1;
    if ( Mode == 11 )
        return Map_MatchNodeHasStmapReliefDepth( pNode );
    if ( Mode == 12 )
        return Map_MatchNodeHasStmapTightReliefDepth( pNode );
    if ( Mode >= 13 && Mode <= 57 )
        return Map_MatchNodeHasStmapTightReliefDepth( pNode );
    return 0;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the run profile is large enough for stmap15.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap15ReliefProfile( Map_Man_t * p )
{
    return p->nStmap13LowerModRisk >= 20000 && p->nStmap13MiddleSlack >= 128;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the node is shallow enough for a stmap20 seed.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchNodeHasStmap20EarlySeedDepth( Map_Node_t * pNode )
{
    return pNode->Level <= 24;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the arrival gain is large enough for stmap21.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap21DeepSeedGain( float ArrivalDelta, float ArrivalGainMargin, float Epsilon )
{
    return ArrivalDelta <= -5.0 * ArrivalGainMargin - Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the arrival gain is large enough for stmap22.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap22ModerateDeepSeedGain( float ArrivalDelta, float ArrivalGainMargin, float Epsilon )
{
    return ArrivalDelta <= -2.0 * ArrivalGainMargin - Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if a moderate deep seed is critical enough for stmap23.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap23CriticalModerateDeepSeed( float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( Map_MatchHasStmap21DeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) )
        return 1;
    return Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.2 * SlackMargin + Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if a moderate deep seed is tight-critical for stmap24.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap24TightCriticalModerateDeepSeed( float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( Map_MatchHasStmap21DeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) )
        return 1;
    return Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.1 * SlackMargin + Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if stmap25 should admit its single moderate ablation.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchIsStmap25ModerateAblationSeed( Map_Man_t * p, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0;
    return Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.25 * SlackMargin + Epsilon &&
        p->nStmap25ModerateAblationSeed == 0;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if stmap25 allows a deep pre-profile seed.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap25SingleModerateDeepSeed( Map_Man_t * p, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    return Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) ||
        Map_MatchIsStmap25ModerateAblationSeed( p, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
}

/**Function*************************************************************

  Synopsis    [Returns 1 if this is an stmap26 moderate ablation candidate.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchIsStmap26ModerateAblationCandidate( float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0;
    return Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.25 * SlackMargin + Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if stmap26 should admit its rank-2 ablation.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchIsStmap26RankTwoModerateAblationSeed( Map_Man_t * p, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    return Map_MatchIsStmap26ModerateAblationCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) &&
        p->nStmap26ModerateAblationSeen == 1 &&
        p->nStmap26ModerateAblationSeed == 0;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if stmap26 allows a deep pre-profile seed.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap26RankTwoModerateDeepSeed( Map_Man_t * p, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    return Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) ||
        Map_MatchIsStmap26RankTwoModerateAblationSeed( p, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
}

/**Function*************************************************************

  Synopsis    [Returns 1 if this candidate matches the stmap27 block shape.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchIsStmap27BlockedModerateSignature( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0;
    return Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        pNode->nRefs >= 5 &&
        Slack <= 1.15 * SlackMargin + Epsilon &&
        ArrivalDelta <= -2.25 * ArrivalGainMargin - Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if stmap27 should admit a signature-safe seed.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchIsStmap27SignatureSafeModerateSeed( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0;
    return Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.25 * SlackMargin + Epsilon &&
        !Map_MatchIsStmap27BlockedModerateSignature( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
}

/**Function*************************************************************

  Synopsis    [Returns 1 if stmap27 allows a deep pre-profile seed.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap27SignatureSafeModerateDeepSeed( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    return Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) ||
        Map_MatchIsStmap27SignatureSafeModerateSeed( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
}

/**Function*************************************************************

  Synopsis    [Returns 1 if this is an stmap28 soft-penalty candidate.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchIsStmap28ModeratePenaltyCandidate( float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0;
    return Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.25 * SlackMargin + Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns the stmap28 extra area margin in inverter-area units.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap28ModeratePenaltyFactor( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    float Penalty = 0.0;
    if ( !Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    if ( pNode->nRefs < 5 )
        return 0.0;
    Penalty += 0.15 * (float)(pNode->nRefs - 4);
    if ( Slack <= 1.15 * SlackMargin + Epsilon )
        Penalty += 0.15;
    if ( ArrivalDelta <= -2.25 * ArrivalGainMargin - Epsilon )
        Penalty += 0.15;
    return Penalty > 1.0 ? 1.0 : Penalty;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if stmap28 allows a deep pre-profile seed.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    return Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) ||
        Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
}

/**Function*************************************************************

  Synopsis    [Returns the stmap29 continuous extra area margin.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap29ModeratePenaltyFactor( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    float Penalty, SlackRatio, GainRatio;
    if ( !Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    if ( pNode->nRefs < 5 || SlackMargin <= Epsilon || ArrivalGainMargin <= Epsilon )
        return 0.0;
    SlackRatio = Slack / SlackMargin;
    GainRatio = -ArrivalDelta / ArrivalGainMargin;
    Penalty = 0.22 * (float)(pNode->nRefs - 4);
    if ( SlackRatio < 1.25 )
        Penalty += 0.20 * (1.25 - SlackRatio) / 0.25;
    if ( GainRatio > 2.0 )
        Penalty += 0.10 * (GainRatio - 2.0);
    return Penalty > 1.0 ? 1.0 : Penalty;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if this is an stmap30 strong-seed penalty candidate.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchIsStmap30StrongPenaltyCandidate( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( pNode->nRefs < 5 )
        return 0;
    return Map_MatchHasStmap21DeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.25 * SlackMargin + Epsilon;
}

/**Function*************************************************************

  Synopsis    [Returns the stmap30 strong-seed extra area margin.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap30StrongPenaltyFactor( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    float Penalty, SlackRatio, GainRatio;
    if ( !Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    if ( SlackMargin <= Epsilon || ArrivalGainMargin <= Epsilon )
        return 0.0;
    SlackRatio = Slack / SlackMargin;
    GainRatio = -ArrivalDelta / ArrivalGainMargin;
    Penalty = 0.18 * (float)(pNode->nRefs - 4);
    if ( SlackRatio < 1.25 )
        Penalty += 0.16 * (1.25 - SlackRatio) / 0.25;
    if ( GainRatio > 5.0 )
        Penalty += 0.09 * (GainRatio - 5.0);
    return Penalty > 0.75 ? 0.75 : Penalty;
}

/**Function*************************************************************

  Synopsis    [Returns the stmap31 damped strong-seed extra area margin.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap31StrongPenaltyFactor( Map_Node_t * pNode, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    float Penalty, SlackRatio, GainRatio;
    if ( !Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    if ( SlackMargin <= Epsilon || ArrivalGainMargin <= Epsilon )
        return 0.0;
    SlackRatio = Slack / SlackMargin;
    GainRatio = -ArrivalDelta / ArrivalGainMargin;
    Penalty = 0.18 * (float)(pNode->nRefs - 4);
    if ( SlackRatio < 1.25 )
        Penalty += 0.16 * (1.25 - SlackRatio) / 0.25;
    if ( GainRatio > 5.0 )
        Penalty += 0.05 * (GainRatio - 5.0);
    return Penalty > 0.75 ? 0.75 : Penalty;
}

/**Function*************************************************************

  Synopsis    [Returns the average reference count across cut leaves.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap32CutLeafLoadAvg( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Load = 0.0;
    int i;
    if ( pCut == NULL || pCut->nLeaves == 0 )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf != NULL && pLeaf->nRefs > 0 )
            Load += (float)pLeaf->nRefs;
    }
    return Load / (float)pCut->nLeaves;
}

/**Function*************************************************************

  Synopsis    [Returns the stmap32 cut-leaf strong-seed extra area margin.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap32StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    float Penalty, LeafLoadAvg;
    if ( !Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    Penalty = Map_MatchStmap31StrongPenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
    LeafLoadAvg = Map_MatchStmap32CutLeafLoadAvg( pCut );
    if ( LeafLoadAvg > 1.0 )
        Penalty += 0.04 * (LeafLoadAvg - 1.0);
    return Penalty > 0.75 ? 0.75 : Penalty;
}

/**Function*************************************************************

  Synopsis    [Returns the stmap33 drive-normalized strong-seed margin.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap33StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit )
{
    float Penalty, LeafLoadAvg, LoadDriveRatio;
    int FanLimit;
    if ( pLeafLoadAvg )
        *pLeafLoadAvg = 0.0;
    if ( pLoadDriveRatio )
        *pLoadDriveRatio = 0.0;
    if ( pFanLimit )
        *pFanLimit = 0;
    if ( !Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    Penalty = Map_MatchStmap31StrongPenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
    LeafLoadAvg = Map_MatchStmap32CutLeafLoadAvg( pCut );
    FanLimit = pMatch && pMatch->pSuperBest ? (int)pMatch->pSuperBest->nFanLimit : 0;
    LoadDriveRatio = FanLimit > 0 ? LeafLoadAvg / (float)FanLimit : LeafLoadAvg;
    if ( pLeafLoadAvg )
        *pLeafLoadAvg = LeafLoadAvg;
    if ( pLoadDriveRatio )
        *pLoadDriveRatio = LoadDriveRatio;
    if ( pFanLimit )
        *pFanLimit = FanLimit;
    if ( LoadDriveRatio > 1.0 )
        Penalty += FanLimit > 0 ? 0.18 * (LoadDriveRatio - 1.0) : 0.04 * (LeafLoadAvg - 1.0);
    return Penalty > 0.75 ? 0.75 : Penalty;
}

/**Function*************************************************************

  Synopsis    [Returns the stmap34 bounded drive-normalized strong margin.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Map_MatchStmap34StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit )
{
    float Penalty, LeafLoadAvg, LoadDriveRatio;
    int FanLimit;
    if ( pLeafLoadAvg )
        *pLeafLoadAvg = 0.0;
    if ( pLoadDriveRatio )
        *pLoadDriveRatio = 0.0;
    if ( pFanLimit )
        *pFanLimit = 0;
    if ( !Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    Penalty = Map_MatchStmap31StrongPenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
    LeafLoadAvg = Map_MatchStmap32CutLeafLoadAvg( pCut );
    FanLimit = pMatch && pMatch->pSuperBest ? (int)pMatch->pSuperBest->nFanLimit : 0;
    LoadDriveRatio = FanLimit > 0 ? LeafLoadAvg / (float)FanLimit : LeafLoadAvg;
    if ( pLeafLoadAvg )
        *pLeafLoadAvg = LeafLoadAvg;
    if ( pLoadDriveRatio )
        *pLoadDriveRatio = LoadDriveRatio;
    if ( pFanLimit )
        *pFanLimit = FanLimit;
    if ( LoadDriveRatio > 1.0 )
        Penalty += 0.02 * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap36SclMaxLoadRatio = 0.0;
static float s_Stmap36SclOverFrac = 0.0;
static float s_Stmap36SclFeedback = 0.0;

void Map_Stmap36SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity )
{
    s_Stmap36SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap36SclOverFrac = OverFrac;
    s_Stmap36SclFeedback = Severity;
}

static float Map_MatchStmap36StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit )
{
    float Penalty, LoadDriveRatio;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    if ( Penalty > 0.0 && s_Stmap36SclFeedback > 0.0 && LoadDriveRatio > 1.0 )
        Penalty += 0.03 * s_Stmap36SclFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap37SclMaxLoadRatio = 0.0;
static float s_Stmap37SclOverFrac = 0.0;
static float s_Stmap37SclFeedback = 0.0;
static int * s_pStmap37SclHotspotIds = NULL;
static float * s_pStmap37SclHotspotRatios = NULL;
static int s_nStmap37SclHotspots = 0;

void Map_Stmap37SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, int * pAigIds, float * pRatios, int nHotspots )
{
    s_Stmap37SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap37SclOverFrac = OverFrac;
    s_Stmap37SclFeedback = Severity;
    s_pStmap37SclHotspotIds = pAigIds;
    s_pStmap37SclHotspotRatios = pRatios;
    s_nStmap37SclHotspots = nHotspots;
}

static float Map_MatchStmap37HotspotRatio( Map_Node_t * pNode )
{
    int AigId, i;
    if ( s_Stmap37SclFeedback <= 0.0 || s_pStmap37SclHotspotIds == NULL || s_pStmap37SclHotspotRatios == NULL )
        return 0.0;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( AigId < 0 )
        return 0.0;
    for ( i = 0; i < s_nStmap37SclHotspots; i++ )
        if ( s_pStmap37SclHotspotIds[i] == AigId )
            return s_pStmap37SclHotspotRatios[i];
    return 0.0;
}

static float Map_MatchStmap37StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pHotspotRatio )
{
    float Penalty, LoadDriveRatio, HotspotRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    HotspotRatio = Map_MatchStmap37HotspotRatio( pNode );
    if ( pHotspotRatio )
        *pHotspotRatio = HotspotRatio;
    if ( Penalty > 0.0 && s_Stmap37SclFeedback > 0.0 && HotspotRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (HotspotRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.05 * s_Stmap37SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap38SclMaxLoadRatio = 0.0;
static float s_Stmap38SclOverFrac = 0.0;
static float s_Stmap38SclFeedback = 0.0;
static int * s_pStmap38SclHotspotIds = NULL;
static float * s_pStmap38SclHotspotRatios = NULL;
static int s_nStmap38SclHotspots = 0;

void Map_Stmap38SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, int * pAigIds, float * pRatios, int nHotspots )
{
    s_Stmap38SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap38SclOverFrac = OverFrac;
    s_Stmap38SclFeedback = Severity;
    s_pStmap38SclHotspotIds = pAigIds;
    s_pStmap38SclHotspotRatios = pRatios;
    s_nStmap38SclHotspots = nHotspots;
}

static float Map_MatchStmap38HotspotLookup( int AigId )
{
    int i;
    if ( AigId < 0 || s_Stmap38SclFeedback <= 0.0 || s_pStmap38SclHotspotIds == NULL || s_pStmap38SclHotspotRatios == NULL )
        return 0.0;
    for ( i = 0; i < s_nStmap38SclHotspots; i++ )
        if ( s_pStmap38SclHotspotIds[i] == AigId )
            return s_pStmap38SclHotspotRatios[i];
    return 0.0;
}

static float Map_MatchStmap38NodeHotspotRatio( Map_Node_t * pNode )
{
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    return Map_MatchStmap38HotspotLookup( Map_NodeReadAigId( pNode ) );
}

static float Map_MatchStmap38CutHotspotRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap38HotspotLookup( Map_NodeReadAigId( pLeaf ) );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static float Map_MatchStmap38StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodeHotspotRatio, float * pCutHotspotRatio )
{
    float Penalty, LoadDriveRatio, NodeHotspotRatio, CutHotspotRatio, HotspotRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodeHotspotRatio = Map_MatchStmap38NodeHotspotRatio( pNode );
    CutHotspotRatio = Map_MatchStmap38CutHotspotRatio( pCut );
    HotspotRatio = NodeHotspotRatio > CutHotspotRatio ? NodeHotspotRatio : CutHotspotRatio;
    if ( pNodeHotspotRatio )
        *pNodeHotspotRatio = NodeHotspotRatio;
    if ( pCutHotspotRatio )
        *pCutHotspotRatio = CutHotspotRatio;
    if ( Penalty > 0.0 && s_Stmap38SclFeedback > 0.0 && HotspotRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (HotspotRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.05 * s_Stmap38SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap39SclMaxLoadRatio = 0.0;
static float s_Stmap39SclOverFrac = 0.0;
static float s_Stmap39SclFeedback = 0.0;
static int * s_pStmap39SclPressureIds = NULL;
static float * s_pStmap39SclPressureRatios = NULL;
static int s_nStmap39SclPressures = 0;

void Map_Stmap39SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, int * pAigIds, float * pRatios, int nHotspots )
{
    s_Stmap39SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap39SclOverFrac = OverFrac;
    s_Stmap39SclFeedback = Severity;
    s_pStmap39SclPressureIds = pAigIds;
    s_pStmap39SclPressureRatios = pRatios;
    s_nStmap39SclPressures = nHotspots;
}

static float Map_MatchStmap39PressureLookup( int AigId )
{
    int i;
    if ( AigId < 0 || s_Stmap39SclFeedback <= 0.0 || s_pStmap39SclPressureIds == NULL || s_pStmap39SclPressureRatios == NULL )
        return 0.0;
    for ( i = 0; i < s_nStmap39SclPressures; i++ )
        if ( s_pStmap39SclPressureIds[i] == AigId )
            return s_pStmap39SclPressureRatios[i];
    return 0.0;
}

static float Map_MatchStmap39NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    float Ratio;
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    Ratio = Map_MatchStmap39PressureLookup( AigId );
    if ( Ratio > 0.0 )
        return Ratio;
    return 0.0;
}

static float Map_MatchStmap39CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap39NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static float Map_MatchStmap39StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap39NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap39CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap39SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.05 * s_Stmap39SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap40SclMaxLoadRatio = 0.0;
static float s_Stmap40SclOverFrac = 0.0;
static float s_Stmap40SclFeedback = 0.0;
static float * s_pStmap40SclPressureRatios = NULL;
static int s_nStmap40SclPressureRatios = 0;

void Map_Stmap40SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios )
{
    s_Stmap40SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap40SclOverFrac = OverFrac;
    s_Stmap40SclFeedback = Severity;
    s_pStmap40SclPressureRatios = pAigPressureRatios;
    s_nStmap40SclPressureRatios = nAigPressureRatios;
}

static float Map_MatchStmap40PressureLookup( int AigId )
{
    if ( AigId < 0 || AigId >= s_nStmap40SclPressureRatios || s_Stmap40SclFeedback <= 0.0 || s_pStmap40SclPressureRatios == NULL )
        return 0.0;
    return s_pStmap40SclPressureRatios[AigId];
}

static float Map_MatchStmap40NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    return Map_MatchStmap40PressureLookup( AigId );
}

static float Map_MatchStmap40CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap40NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static float Map_MatchStmap40StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap40NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap40CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap40SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.065 * s_Stmap40SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap41SclMaxLoadRatio = 0.0;
static float s_Stmap41SclOverFrac = 0.0;
static float s_Stmap41SclFeedback = 0.0;
static float * s_pStmap41SclPressureRatios = NULL;
static int s_nStmap41SclPressureRatios = 0;

void Map_Stmap41SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios )
{
    s_Stmap41SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap41SclOverFrac = OverFrac;
    s_Stmap41SclFeedback = Severity;
    s_pStmap41SclPressureRatios = pAigPressureRatios;
    s_nStmap41SclPressureRatios = nAigPressureRatios;
}

static float Map_MatchStmap41PressureLookup( int AigId )
{
    if ( AigId < 0 || AigId >= s_nStmap41SclPressureRatios || s_Stmap41SclFeedback <= 0.0 || s_pStmap41SclPressureRatios == NULL )
        return 0.0;
    return s_pStmap41SclPressureRatios[AigId];
}

static float Map_MatchStmap41NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    return Map_MatchStmap41PressureLookup( AigId );
}

static float Map_MatchStmap41CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap41NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static float Map_MatchStmap41StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap41NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap41CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap41SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.065 * s_Stmap41SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap42SclMaxLoadRatio = 0.0;
static float s_Stmap42SclOverFrac = 0.0;
static float s_Stmap42SclFeedback = 0.0;
static float * s_pStmap42SclPressureRatios = NULL;
static int s_nStmap42SclPressureRatios = 0;

void Map_Stmap42SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios )
{
    s_Stmap42SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap42SclOverFrac = OverFrac;
    s_Stmap42SclFeedback = Severity;
    s_pStmap42SclPressureRatios = pAigPressureRatios;
    s_nStmap42SclPressureRatios = nAigPressureRatios;
}

static float Map_MatchStmap42PressureLookup( int AigId )
{
    if ( AigId < 0 || AigId >= s_nStmap42SclPressureRatios || s_Stmap42SclFeedback <= 0.0 || s_pStmap42SclPressureRatios == NULL )
        return 0.0;
    return s_pStmap42SclPressureRatios[AigId];
}

static float Map_MatchStmap42NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    return Map_MatchStmap42PressureLookup( AigId );
}

static float Map_MatchStmap42CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap42NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static float Map_MatchStmap42StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap42NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap42CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap42SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.16 * s_Stmap42SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap43SclMaxLoadRatio = 0.0;
static float s_Stmap43SclOverFrac = 0.0;
static float s_Stmap43SclFeedback = 0.0;
static float * s_pStmap43SclPressureRatios = NULL;
static int s_nStmap43SclPressureRatios = 0;

void Map_Stmap43SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios )
{
    s_Stmap43SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap43SclOverFrac = OverFrac;
    s_Stmap43SclFeedback = Severity;
    s_pStmap43SclPressureRatios = pAigPressureRatios;
    s_nStmap43SclPressureRatios = nAigPressureRatios;
}

static float Map_MatchStmap43PressureLookup( int AigId )
{
    if ( AigId < 0 || AigId >= s_nStmap43SclPressureRatios || s_Stmap43SclFeedback <= 0.0 || s_pStmap43SclPressureRatios == NULL )
        return 0.0;
    return s_pStmap43SclPressureRatios[AigId];
}

static float Map_MatchStmap43NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    return Map_MatchStmap43PressureLookup( AigId );
}

static float Map_MatchStmap43CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap43NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static float Map_MatchStmap43ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
    NodePressureRatio = Map_MatchStmap43NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap43CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty <= 0.0 || s_Stmap43SclFeedback <= 0.0 || PressureRatio <= 1.50 )
        return 0.0;
    LocalFeedback = (PressureRatio - 1.50) / 3.0;
    if ( LocalFeedback > 1.0 )
        LocalFeedback = 1.0;
    Penalty *= 0.45 + 0.55 * s_Stmap43SclFeedback * LocalFeedback;
    return Penalty > 1.0 ? 1.0 : Penalty;
}

static float Map_MatchStmap43StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap43NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap43CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap43SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.16 * s_Stmap43SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap44SclMaxLoadRatio = 0.0;
static float s_Stmap44SclOverFrac = 0.0;
static float s_Stmap44SclFeedback = 0.0;
static float * s_pStmap44SclPressureRatios = NULL;
static int s_nStmap44SclPressureRatios = 0;

void Map_Stmap44SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios )
{
    s_Stmap44SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap44SclOverFrac = OverFrac;
    s_Stmap44SclFeedback = Severity;
    s_pStmap44SclPressureRatios = pAigPressureRatios;
    s_nStmap44SclPressureRatios = nAigPressureRatios;
}

static float Map_MatchStmap44PressureLookup( int AigId )
{
    if ( AigId < 0 || AigId >= s_nStmap44SclPressureRatios || s_Stmap44SclFeedback <= 0.0 || s_pStmap44SclPressureRatios == NULL )
        return 0.0;
    return s_pStmap44SclPressureRatios[AigId];
}

static float Map_MatchStmap44NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    return Map_MatchStmap44PressureLookup( AigId );
}

static float Map_MatchStmap44CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap44NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static float Map_MatchStmap44ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, NodePressureRatio, CutPressureRatio, PressureAgreement, PressureSpread, LocalFeedback;
    Penalty = Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
    NodePressureRatio = Map_MatchStmap44NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap44CutPressureRatio( pCut );
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty <= 0.0 || s_Stmap44SclFeedback <= 0.0 || NodePressureRatio <= 1.75 || CutPressureRatio <= 1.75 )
        return 0.0;
    PressureAgreement = NodePressureRatio < CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    PressureSpread = NodePressureRatio > CutPressureRatio ? NodePressureRatio / CutPressureRatio : CutPressureRatio / NodePressureRatio;
    if ( PressureSpread > 1.25 )
        return 0.0;
    LocalFeedback = (PressureAgreement - 1.75) / 3.0;
    if ( LocalFeedback > 1.0 )
        LocalFeedback = 1.0;
    Penalty *= 0.40 + 0.60 * s_Stmap44SclFeedback * LocalFeedback;
    return Penalty > 1.0 ? 1.0 : Penalty;
}

static float Map_MatchStmap44StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap44NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap44CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap44SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.16 * s_Stmap44SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float s_Stmap45SclMaxLoadRatio = 0.0;
static float s_Stmap45SclOverFrac = 0.0;
static float s_Stmap45SclFeedback = 0.0;
static float * s_pStmap45SclPressureRatios = NULL;
static int s_nStmap45SclPressureRatios = 0;
static int s_nStmap45SclPressureEntries = 0;
static int s_fStmap60NearMissLeafDiag = 0;
static int s_Stmap60NearMissLeafDiagTarget = -1;
static int s_fStmap61CutOnlyGateDiag = 0;
static int s_Stmap61CutOnlyGateDiagTarget = -1;
static int s_fStmap62CutOnlyBeforeModerate = 0;
static int s_fStmap63CutOnlyLoadDropGuard = 0;
static int s_fStmap65StrongNodeLoadDropGuard = 0;
static int s_fStmap66NearStrongNodeLoadDropDiag = 0;
static int s_fStmap67NearStrongNodeWitnessDiag = 0;
static int s_fStmap68BlockedStrongWitnessDiag = 0;
static int s_nStmap68BlockedStrongWitnessSources = 0;
static int s_fStmap69PressureNearWitnessDiag = 0;
static int s_nStmap69PressureNearWitnessSources = 0;
static int s_fStmap71ModeratePenaltyWitnessDiag = 0;
static int s_nStmap71ModeratePenaltyWitnessSources = 0;
static int s_fStmap72PathProximityWitnessDiag = 0;
static int s_fStmap73AcceptedPressureWitnessDiag = 0;
static int s_nStmap73AcceptedPressureWitnessSources = 0;
#define MAP_STMAP75_WATCH_AIGS 4
static int s_fStmap75FinalCriticalAigDiag = 0;
static int s_nStmap75ActiveWatchAigs = 0;
static int s_nStmap75SelectedMatchRows = 0;
static int s_Stmap75WatchAigIds[MAP_STMAP75_WATCH_AIGS] = { -1, -1, -1, -1 };
static int s_Stmap75WatchHitCounts[MAP_STMAP75_WATCH_AIGS];
static const char * s_pStmap75FinalCriticalAigDiagLabel = "stmap75";
#define MAP_STMAP64_MAX_WITNESSES 16
static int s_fStmap64CutOnlyWitnessDiag = 0;
static int s_nStmap64CutOnlyWitnesses = 0;
static int s_Stmap64WitnessAigIds[MAP_STMAP64_MAX_WITNESSES];
static int s_Stmap64WitnessPhases[MAP_STMAP64_MAX_WITNESSES];
static int s_Stmap64WitnessNodes[MAP_STMAP64_MAX_WITNESSES];
static float s_Stmap64WitnessNodePressureRatios[MAP_STMAP64_MAX_WITNESSES];
static float s_Stmap64WitnessCutPressureRatios[MAP_STMAP64_MAX_WITNESSES];
static float s_Stmap64WitnessSlacks[MAP_STMAP64_MAX_WITNESSES];
static float s_Stmap64WitnessAreaSaves[MAP_STMAP64_MAX_WITNESSES];
static float s_Stmap64WitnessArrivalDeltas[MAP_STMAP64_MAX_WITNESSES];
static float s_Stmap64WitnessArrivalGainMargins[MAP_STMAP64_MAX_WITNESSES];
static float s_Stmap64WitnessFeedbacks[MAP_STMAP64_MAX_WITNESSES];

void Map_Stmap45SetSclLoadFeedbackWithEntries( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios, int nPressureEntries )
{
    s_Stmap45SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap45SclOverFrac = OverFrac;
    s_Stmap45SclFeedback = Severity;
    s_pStmap45SclPressureRatios = pAigPressureRatios;
    s_nStmap45SclPressureRatios = nAigPressureRatios;
    s_nStmap45SclPressureEntries = nPressureEntries;
}

void Map_Stmap45SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios )
{
    Map_Stmap45SetSclLoadFeedbackWithEntries( MaxLoadRatio, OverFrac, Severity, pAigPressureRatios, nAigPressureRatios, 0 );
}

void Map_Stmap60SetNearMissLeafDiag( int fEnable, int TrackNode )
{
    s_fStmap60NearMissLeafDiag = fEnable;
    s_Stmap60NearMissLeafDiagTarget = fEnable ? TrackNode : -1;
}

int Map_Stmap60NearMissLeafDiagEnabled( void )
{
    return s_fStmap60NearMissLeafDiag;
}

int Map_Stmap60NearMissLeafDiagTarget( void )
{
    return s_Stmap60NearMissLeafDiagTarget;
}

void Map_Stmap61SetCutOnlyGateDiag( int fEnable, int TrackNode )
{
    s_fStmap61CutOnlyGateDiag = fEnable;
    s_Stmap61CutOnlyGateDiagTarget = fEnable ? TrackNode : -1;
}

int Map_Stmap61CutOnlyGateDiagEnabled( void )
{
    return s_fStmap61CutOnlyGateDiag;
}

int Map_Stmap61CutOnlyGateDiagTarget( void )
{
    return s_Stmap61CutOnlyGateDiagTarget;
}

void Map_Stmap62SetCutOnlyOrdering( int fEnable )
{
    s_fStmap62CutOnlyBeforeModerate = fEnable;
}

void Map_Stmap63SetCutOnlyLoadDropGuard( int fEnable )
{
    s_fStmap63CutOnlyLoadDropGuard = fEnable;
}

void Map_Stmap65SetStrongNodeLoadDropGuard( int fEnable )
{
    s_fStmap65StrongNodeLoadDropGuard = fEnable;
}

void Map_Stmap66SetNearStrongNodeLoadDropDiag( int fEnable )
{
    s_fStmap66NearStrongNodeLoadDropDiag = fEnable;
}

int Map_Stmap66NearStrongNodeLoadDropDiagEnabled( void )
{
    return s_fStmap66NearStrongNodeLoadDropDiag;
}

void Map_Stmap67SetNearStrongNodeWitnessDiag( int fEnable )
{
    if ( fEnable )
        s_nStmap64CutOnlyWitnesses = 0;
    s_fStmap67NearStrongNodeWitnessDiag = fEnable;
}

void Map_Stmap68SetBlockedStrongWitnessDiag( int fEnable )
{
    if ( fEnable )
    {
        s_nStmap64CutOnlyWitnesses = 0;
        s_nStmap68BlockedStrongWitnessSources = 0;
    }
    s_fStmap68BlockedStrongWitnessDiag = fEnable;
}

void Map_Stmap69SetPressureNearWitnessDiag( int fEnable )
{
    if ( fEnable )
    {
        s_nStmap64CutOnlyWitnesses = 0;
        s_nStmap69PressureNearWitnessSources = 0;
    }
    s_fStmap69PressureNearWitnessDiag = fEnable;
}

void Map_Stmap71SetModeratePenaltyWitnessDiag( int fEnable )
{
    if ( fEnable )
    {
        s_nStmap64CutOnlyWitnesses = 0;
        s_nStmap71ModeratePenaltyWitnessSources = 0;
    }
    s_fStmap71ModeratePenaltyWitnessDiag = fEnable;
}

void Map_Stmap72SetPathProximityWitnessDiag( int fEnable )
{
    if ( fEnable )
    {
        s_nStmap64CutOnlyWitnesses = 0;
        s_nStmap68BlockedStrongWitnessSources = 0;
        s_nStmap69PressureNearWitnessSources = 0;
        s_nStmap71ModeratePenaltyWitnessSources = 0;
    }
    s_fStmap72PathProximityWitnessDiag = fEnable;
}

void Map_Stmap73SetAcceptedPressureWitnessDiag( int fEnable )
{
    if ( fEnable )
    {
        s_nStmap64CutOnlyWitnesses = 0;
        s_nStmap73AcceptedPressureWitnessSources = 0;
    }
    s_fStmap73AcceptedPressureWitnessDiag = fEnable;
}

static void Map_Stmap75ResetFinalCriticalAigDiagState( void )
{
    int i;
    s_nStmap75ActiveWatchAigs = 0;
    s_nStmap75SelectedMatchRows = 0;
    for ( i = 0; i < MAP_STMAP75_WATCH_AIGS; i++ )
    {
        s_Stmap75WatchAigIds[i] = -1;
        s_Stmap75WatchHitCounts[i] = 0;
    }
}

void Map_Stmap75SetFinalCriticalAigDiag( int fEnable, int WatchAigId )
{
    if ( fEnable )
    {
        Map_Stmap75ResetFinalCriticalAigDiagState();
        s_nStmap75ActiveWatchAigs = WatchAigId >= 0 ? 1 : 0;
        if ( WatchAigId >= 0 )
            s_Stmap75WatchAigIds[0] = WatchAigId;
    }
    else
    {
        Map_Stmap75ResetFinalCriticalAigDiagState();
        s_pStmap75FinalCriticalAigDiagLabel = "stmap75";
    }
    s_fStmap75FinalCriticalAigDiag = fEnable;
}

void Map_Stmap75SetFinalCriticalAigDiagArray( int fEnable, int * pWatchAigIds, int nWatchAigs )
{
    int i;
    Map_Stmap75ResetFinalCriticalAigDiagState();
    if ( !fEnable )
    {
        s_pStmap75FinalCriticalAigDiagLabel = "stmap75";
        s_fStmap75FinalCriticalAigDiag = 0;
        return;
    }
    for ( i = 0; i < nWatchAigs && s_nStmap75ActiveWatchAigs < MAP_STMAP75_WATCH_AIGS; i++ )
        if ( pWatchAigIds != NULL && pWatchAigIds[i] >= 0 )
            s_Stmap75WatchAigIds[s_nStmap75ActiveWatchAigs++] = pWatchAigIds[i];
    s_fStmap75FinalCriticalAigDiag = s_nStmap75ActiveWatchAigs > 0;
}

void Map_Stmap75SetFinalCriticalAigDiagLabel( const char * pLabel )
{
    s_pStmap75FinalCriticalAigDiagLabel = pLabel && pLabel[0] ? pLabel : "stmap75";
}

void Map_Stmap75PrintFinalCriticalAigSummary( void )
{
    printf( "%s selected-match stats: watched-aigs = %d  rows = %d  watch0-aig-id = %d  watch0-rows = %d  watch1-aig-id = %d  watch1-rows = %d  watch2-aig-id = %d  watch2-rows = %d  watch3-aig-id = %d  watch3-rows = %d\n",
        s_pStmap75FinalCriticalAigDiagLabel, s_nStmap75ActiveWatchAigs, s_nStmap75SelectedMatchRows,
        s_Stmap75WatchAigIds[0], s_Stmap75WatchHitCounts[0],
        s_Stmap75WatchAigIds[1], s_Stmap75WatchHitCounts[1],
        s_Stmap75WatchAigIds[2], s_Stmap75WatchHitCounts[2],
        s_Stmap75WatchAigIds[3], s_Stmap75WatchHitCounts[3] );
}

void Map_Stmap64ClearCutOnlyWitnesses( void )
{
    s_fStmap64CutOnlyWitnessDiag = 0;
    s_nStmap64CutOnlyWitnesses = 0;
}

void Map_Stmap64SetCutOnlyWitnessDiag( int fEnable )
{
    if ( fEnable )
        Map_Stmap64ClearCutOnlyWitnesses();
    s_fStmap64CutOnlyWitnessDiag = fEnable;
}

int Map_Stmap64CutOnlyWitnessCount( void )
{
    return s_nStmap64CutOnlyWitnesses;
}

int Map_Stmap64ReadCutOnlyWitness( int i, int * pAigId, int * pPhase, int * pNode, float * pNodePressureRatio, float * pCutPressureRatio, float * pSlack, float * pAreaSave, float * pArrivalDelta, float * pArrivalGainMargin, float * pFeedback )
{
    if ( i < 0 || i >= s_nStmap64CutOnlyWitnesses )
        return 0;
    if ( pAigId )
        *pAigId = s_Stmap64WitnessAigIds[i];
    if ( pPhase )
        *pPhase = s_Stmap64WitnessPhases[i];
    if ( pNode )
        *pNode = s_Stmap64WitnessNodes[i];
    if ( pNodePressureRatio )
        *pNodePressureRatio = s_Stmap64WitnessNodePressureRatios[i];
    if ( pCutPressureRatio )
        *pCutPressureRatio = s_Stmap64WitnessCutPressureRatios[i];
    if ( pSlack )
        *pSlack = s_Stmap64WitnessSlacks[i];
    if ( pAreaSave )
        *pAreaSave = s_Stmap64WitnessAreaSaves[i];
    if ( pArrivalDelta )
        *pArrivalDelta = s_Stmap64WitnessArrivalDeltas[i];
    if ( pArrivalGainMargin )
        *pArrivalGainMargin = s_Stmap64WitnessArrivalGainMargins[i];
    if ( pFeedback )
        *pFeedback = s_Stmap64WitnessFeedbacks[i];
    return 1;
}

static void Map_Stmap64WriteWitnessSlot( int i, Map_Node_t * pNode, int AigId, int fPhase, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin )
{
    s_Stmap64WitnessAigIds[i] = AigId;
    s_Stmap64WitnessPhases[i] = fPhase ? 1 : 0;
    s_Stmap64WitnessNodes[i] = pNode->Num;
    s_Stmap64WitnessNodePressureRatios[i] = NodePressureRatio;
    s_Stmap64WitnessCutPressureRatios[i] = CutPressureRatio;
    s_Stmap64WitnessSlacks[i] = Slack;
    s_Stmap64WitnessAreaSaves[i] = AreaSave;
    s_Stmap64WitnessArrivalDeltas[i] = ArrivalDelta;
    s_Stmap64WitnessArrivalGainMargins[i] = ArrivalGainMargin;
    s_Stmap64WitnessFeedbacks[i] = s_Stmap45SclFeedback;
}

static void Map_Stmap64RecordCutOnlyWitnessRaw( Map_Node_t * pNode, int AigId, int fPhase, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin )
{
    int i;
    if ( pNode == NULL || AigId < 0 )
        return;
    fPhase = fPhase ? 1 : 0;
    for ( i = 0; i < s_nStmap64CutOnlyWitnesses; i++ )
    {
        if ( s_Stmap64WitnessAigIds[i] != AigId || s_Stmap64WitnessPhases[i] != fPhase )
            continue;
        Map_Stmap64WriteWitnessSlot( i, pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
        return;
    }
    if ( s_nStmap64CutOnlyWitnesses >= MAP_STMAP64_MAX_WITNESSES )
        return;
    i = s_nStmap64CutOnlyWitnesses++;
    Map_Stmap64WriteWitnessSlot( i, pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
}

static void Map_Stmap64RecordCutOnlyWitness( Map_Node_t * pNode, int AigId, int fPhase, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin )
{
    if ( !s_fStmap64CutOnlyWitnessDiag )
        return;
    Map_Stmap64RecordCutOnlyWitnessRaw( pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
}

static void Map_Stmap67RecordNearStrongNodeWitness( Map_Node_t * pNode, int AigId, int fPhase, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin )
{
    if ( !s_fStmap67NearStrongNodeWitnessDiag && !s_fStmap72PathProximityWitnessDiag )
        return;
    Map_Stmap64RecordCutOnlyWitnessRaw( pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
}

static void Map_Stmap68RecordBlockedStrongWitness( Map_Node_t * pNode, Map_Cut_t * pCut, int AigId, int fPhase, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin, float PenaltyFactor, float AreaMargin, float CutLeafLoadAvg, int FanLimit, float LoadDriveRatio )
{
    const char * pLabel;
    int i, Before, After, fDuplicate = 0;
    if ( (!s_fStmap68BlockedStrongWitnessDiag && !s_fStmap72PathProximityWitnessDiag) || pNode == NULL || AigId < 0 )
        return;
    pLabel = s_fStmap72PathProximityWitnessDiag ? "stmap72" : "stmap68";
    fPhase = fPhase ? 1 : 0;
    s_nStmap68BlockedStrongWitnessSources++;
    for ( i = 0; i < s_nStmap64CutOnlyWitnesses; i++ )
    {
        if ( s_Stmap64WitnessAigIds[i] == AigId && s_Stmap64WitnessPhases[i] == fPhase )
        {
            fDuplicate = 1;
            break;
        }
    }
    Before = s_nStmap64CutOnlyWitnesses;
    if ( !fDuplicate )
        Map_Stmap64RecordCutOnlyWitnessRaw( pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
    After = s_nStmap64CutOnlyWitnesses;
    if ( s_nStmap68BlockedStrongWitnessSources <= 64 )
        printf( "%s blocked-strong witness source: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  stored = %d  duplicate = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
            pLabel, s_nStmap68BlockedStrongWitnessSources, pNode->Num, AigId, pNode->Level, pNode->nRefs,
            pCut ? (int)pCut->nLeaves : 0, fPhase, After > Before, fDuplicate, Slack, AreaSave,
            ArrivalDelta, ArrivalGainMargin, PenaltyFactor, AreaMargin, CutLeafLoadAvg, FanLimit,
            LoadDriveRatio, NodePressureRatio, CutPressureRatio, s_Stmap45SclFeedback );
}

static void Map_Stmap69RecordPressureNearWitness( Map_Node_t * pNode, Map_Cut_t * pCut, int AigId, int fPhase, int fAccepted, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin, float AreaMargin )
{
    const char * pLabel;
    int i, Before, After, fDuplicate = 0;
    if ( (!s_fStmap69PressureNearWitnessDiag && !s_fStmap72PathProximityWitnessDiag) || pNode == NULL || AigId < 0 )
        return;
    pLabel = s_fStmap72PathProximityWitnessDiag ? "stmap72" : "stmap69";
    fPhase = fPhase ? 1 : 0;
    s_nStmap69PressureNearWitnessSources++;
    for ( i = 0; i < s_nStmap64CutOnlyWitnesses; i++ )
    {
        if ( s_Stmap64WitnessAigIds[i] == AigId && s_Stmap64WitnessPhases[i] == fPhase )
        {
            fDuplicate = 1;
            break;
        }
    }
    Before = s_nStmap64CutOnlyWitnesses;
    if ( !fDuplicate )
        Map_Stmap64RecordCutOnlyWitnessRaw( pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
    After = s_nStmap64CutOnlyWitnesses;
    if ( s_nStmap69PressureNearWitnessSources <= 64 )
        printf( "%s pressure-near witness source: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  accepted = %d  stored = %d  duplicate = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
            pLabel, s_nStmap69PressureNearWitnessSources, pNode->Num, AigId, pNode->Level, pNode->nRefs,
            pCut ? (int)pCut->nLeaves : 0, fPhase, fAccepted, After > Before, fDuplicate, Slack,
            AreaSave, ArrivalDelta, ArrivalGainMargin, AreaMargin, NodePressureRatio,
            CutPressureRatio, s_Stmap45SclFeedback );
}

static void Map_Stmap71RecordModeratePenaltyWitness( Map_Node_t * pNode, Map_Cut_t * pCut, int AigId, int fPhase, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin, float PenaltyFactor, float AreaMargin, int fAreaCap )
{
    const char * pLabel;
    int i, Before, After, fDuplicate = 0;
    if ( (!s_fStmap71ModeratePenaltyWitnessDiag && !s_fStmap72PathProximityWitnessDiag) || pNode == NULL || AigId < 0 )
        return;
    pLabel = s_fStmap72PathProximityWitnessDiag ? "stmap72" : "stmap71";
    fPhase = fPhase ? 1 : 0;
    s_nStmap71ModeratePenaltyWitnessSources++;
    for ( i = 0; i < s_nStmap64CutOnlyWitnesses; i++ )
    {
        if ( s_Stmap64WitnessAigIds[i] == AigId && s_Stmap64WitnessPhases[i] == fPhase )
        {
            fDuplicate = 1;
            break;
        }
    }
    Before = s_nStmap64CutOnlyWitnesses;
    if ( !fDuplicate )
        Map_Stmap64RecordCutOnlyWitnessRaw( pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
    After = s_nStmap64CutOnlyWitnesses;
    if ( s_nStmap71ModeratePenaltyWitnessSources <= 64 )
        printf( "%s moderate-penalty witness source: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  stored = %d  duplicate = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
            pLabel, s_nStmap71ModeratePenaltyWitnessSources, pNode->Num, AigId, pNode->Level, pNode->nRefs,
            pCut ? (int)pCut->nLeaves : 0, fPhase, After > Before, fDuplicate, Slack, AreaSave,
            ArrivalDelta, ArrivalGainMargin, PenaltyFactor, AreaMargin, NodePressureRatio,
            CutPressureRatio, fAreaCap, s_Stmap45SclFeedback );
}

static float Map_Stmap73AcceptedWitnessScore( float NodePressureRatio, float CutPressureRatio, float Slack, float ArrivalDelta, float ArrivalGainMargin )
{
    float Pressure = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    float Tightness = Slack > 0.0f ? 1.0f / (1.0f + Slack) : 1.0f;
    float SpeedGain = ArrivalDelta < 0.0f ? -ArrivalDelta / 20.0f : 0.0f;
    float MarginGain = ArrivalGainMargin > 0.0f ? ArrivalGainMargin / 20.0f : 0.0f;
    if ( SpeedGain > 1.0f )
        SpeedGain = 1.0f;
    if ( MarginGain > 1.0f )
        MarginGain = 1.0f;
    return Pressure + 0.25f * Tightness + 0.10f * SpeedGain + 0.05f * MarginGain;
}

static float Map_Stmap73AcceptedWitnessScoreSlot( int i )
{
    return Map_Stmap73AcceptedWitnessScore(
        s_Stmap64WitnessNodePressureRatios[i],
        s_Stmap64WitnessCutPressureRatios[i],
        s_Stmap64WitnessSlacks[i],
        s_Stmap64WitnessArrivalDeltas[i],
        s_Stmap64WitnessArrivalGainMargins[i] );
}

static void Map_Stmap73RecordAcceptedPressureWitness( Map_Node_t * pNode, Map_Cut_t * pCut, int AigId, int fPhase, const char * pClass, float NodePressureRatio, float CutPressureRatio, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin, float PenaltyFactor, float AreaMargin, int fPressureAgreement, int fPressureNear, int fCutOnlyPressure, int fAreaCap )
{
    int i, Slot = -1, fDuplicate = 0, fStored = 0, fReplaced = 0;
    float Score, WorstScore;
    if ( !s_fStmap73AcceptedPressureWitnessDiag || pNode == NULL || AigId < 0 )
        return;
    if ( NodePressureRatio < 1.0f && CutPressureRatio < 1.0f )
        return;
    fPhase = fPhase ? 1 : 0;
    Score = Map_Stmap73AcceptedWitnessScore( NodePressureRatio, CutPressureRatio, Slack, ArrivalDelta, ArrivalGainMargin );
    s_nStmap73AcceptedPressureWitnessSources++;
    for ( i = 0; i < s_nStmap64CutOnlyWitnesses; i++ )
    {
        if ( s_Stmap64WitnessAigIds[i] == AigId && s_Stmap64WitnessPhases[i] == fPhase )
        {
            fDuplicate = 1;
            if ( Score > Map_Stmap73AcceptedWitnessScoreSlot( i ) + 0.0001f )
            {
                Slot = i;
                fReplaced = 1;
            }
            break;
        }
    }
    if ( Slot < 0 && !fDuplicate )
    {
        if ( s_nStmap64CutOnlyWitnesses < MAP_STMAP64_MAX_WITNESSES )
            Slot = s_nStmap64CutOnlyWitnesses++;
        else
        {
            Slot = 0;
            WorstScore = Map_Stmap73AcceptedWitnessScoreSlot( 0 );
            for ( i = 1; i < s_nStmap64CutOnlyWitnesses; i++ )
            {
                if ( Map_Stmap73AcceptedWitnessScoreSlot( i ) < WorstScore )
                {
                    Slot = i;
                    WorstScore = Map_Stmap73AcceptedWitnessScoreSlot( i );
                }
            }
            if ( Score <= WorstScore + 0.0001f )
                Slot = -1;
            else
                fReplaced = 1;
        }
    }
    if ( Slot >= 0 )
    {
        Map_Stmap64WriteWitnessSlot( Slot, pNode, AigId, fPhase, NodePressureRatio, CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
        fStored = 1;
    }
    if ( s_nStmap73AcceptedPressureWitnessSources <= 64 || fStored || fReplaced )
        printf( "stmap73 accepted-pressure witness source: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  class = %s  stored = %d  replaced = %d  duplicate = %d  witness-score = %.6f  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  pressure-agreement = %d  pressure-near = %d  cut-only = %d  area-cap = %d  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
            s_nStmap73AcceptedPressureWitnessSources, pNode->Num, AigId, pNode->Level, pNode->nRefs,
            pCut ? (int)pCut->nLeaves : 0, fPhase, pClass ? pClass : "unknown", fStored, fReplaced,
            fDuplicate, Score, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin, PenaltyFactor,
            AreaMargin, fPressureAgreement, fPressureNear, fCutOnlyPressure, fAreaCap,
            NodePressureRatio, CutPressureRatio, s_Stmap45SclFeedback );
}

static float Map_MatchStmap45PressureLookup( int AigId )
{
    if ( AigId < 0 || AigId >= s_nStmap45SclPressureRatios || s_Stmap45SclFeedback <= 0.0 || s_pStmap45SclPressureRatios == NULL )
        return 0.0;
    return s_pStmap45SclPressureRatios[AigId];
}

static float Map_MatchStmap45NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    return Map_MatchStmap45PressureLookup( AigId );
}

static float Map_MatchStmap45CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap45NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static int Map_Stmap75WatchIndex( int AigId )
{
    int i;
    if ( !s_fStmap75FinalCriticalAigDiag )
        return -1;
    for ( i = 0; i < s_nStmap75ActiveWatchAigs; i++ )
        if ( s_Stmap75WatchAigIds[i] == AigId )
            return i;
    return -1;
}

static void Map_Stmap75ReadLeaf( Map_Cut_t * pCut, int iLeaf, int * pLeafNode, int * pLeafAigId )
{
    Map_Node_t * pLeaf;
    if ( pLeafNode )
        *pLeafNode = -1;
    if ( pLeafAigId )
        *pLeafAigId = -1;
    if ( pCut == NULL || iLeaf < 0 || iLeaf >= (int)pCut->nLeaves )
        return;
    pLeaf = Map_Regular( pCut->ppLeaves[iLeaf] );
    if ( pLeaf == NULL )
        return;
    if ( pLeafNode )
        *pLeafNode = pLeaf->Num;
    if ( pLeafAigId )
        *pLeafAigId = Map_NodeReadAigId( pLeaf );
}

static void Map_Stmap75PrintSelectedMatches( Map_Man_t * p, Map_Node_t * pNode )
{
    Map_Cut_t * pCut;
    Map_Match_t * pMatch;
    Mio_Gate_t * pGate;
    float NodePressureRatio, CutPressureRatio, LeafLoadAvg, LoadDriveRatio;
    float Arrival, Required, Slack, AreaFlow;
    int Phase, AigId, WatchIndex, FanLimit, Leaves, LeafNode[6], LeafAigId[6], i;

    if ( !s_fStmap75FinalCriticalAigDiag || p == NULL || pNode == NULL )
        return;
    NodePressureRatio = Map_MatchStmap45NodePressureRatio( pNode, &AigId );
    WatchIndex = Map_Stmap75WatchIndex( AigId );
    if ( WatchIndex < 0 )
        return;
    for ( Phase = 0; Phase < 2; Phase++ )
    {
        pCut = pNode->pCutBest[Phase];
        if ( pCut == NULL )
            continue;
        pMatch = pCut->M + Phase;
        if ( pMatch->pSuperBest == NULL )
            continue;
        pGate = pMatch->pSuperBest->pRoot;
        CutPressureRatio = Map_MatchStmap45CutPressureRatio( pCut );
        LeafLoadAvg = Map_MatchStmap32CutLeafLoadAvg( pCut );
        FanLimit = (int)pMatch->pSuperBest->nFanLimit;
        LoadDriveRatio = FanLimit > 0 ? LeafLoadAvg / (float)FanLimit : LeafLoadAvg;
        Arrival = pMatch->tArrive.Worst;
        Required = pNode->tRequired[Phase].Worst;
        if ( Required > MAP_FLOAT_LARGE / 2 )
        {
            Required = -1.0;
            Slack = 0.0;
        }
        else
            Slack = Required - Arrival;
        AreaFlow = pMatch->AreaFlow;
        Leaves = (int)pCut->nLeaves;
        for ( i = 0; i < 6; i++ )
            Map_Stmap75ReadLeaf( pCut, i, LeafNode + i, LeafAigId + i );
        s_nStmap75SelectedMatchRows++;
        s_Stmap75WatchHitCounts[WatchIndex]++;
        printf( "%s selected-match: index = %d  watch-index = %d  mapper-mode = %d  node = %d  aig-id = %d  level = %u  refs = %d  phase = %d  gate = %s  leaves = %d  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  arrival = %.6f  required = %.6f  slack = %.6f  area-flow = %.6f  u-phase-best = %u  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f  leaf0-node = %d  leaf0-aig-id = %d  leaf1-node = %d  leaf1-aig-id = %d  leaf2-node = %d  leaf2-aig-id = %d  leaf3-node = %d  leaf3-aig-id = %d  leaf4-node = %d  leaf4-aig-id = %d  leaf5-node = %d  leaf5-aig-id = %d\n",
            s_pStmap75FinalCriticalAigDiagLabel, s_nStmap75SelectedMatchRows, WatchIndex, p->fMappingMode, pNode->Num, AigId,
            pNode->Level, pNode->nRefs, Phase, pGate ? Mio_GateReadName(pGate) : "?",
            Leaves, LeafLoadAvg, FanLimit, LoadDriveRatio, Arrival, Required, Slack, AreaFlow,
            pMatch->uPhaseBest, NodePressureRatio, CutPressureRatio, s_Stmap45SclFeedback,
            LeafNode[0], LeafAigId[0], LeafNode[1], LeafAigId[1], LeafNode[2], LeafAigId[2],
            LeafNode[3], LeafAigId[3], LeafNode[4], LeafAigId[4], LeafNode[5], LeafAigId[5] );
    }
}

static void Map_MatchStmap60PrintNearMissLeafDiag( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, int NearMissIndex, const char * pReason, float Slack, float AreaSave, float ArrivalDelta, float ArrivalGainMargin, float AreaMargin, float NodePressureRatio, float CutPressureRatio )
{
    Map_Node_t * pLeaf;
    float LeafRatio, MaxLeafRatio = 0.0;
    int i, NodeAigId, LeafAigId, LeafNode, MaxLeafAigId = -1, MaxLeafNode = -1, nLeaves = 0;
    if ( !s_fStmap60NearMissLeafDiag || p == NULL || pNode == NULL || pCut == NULL || pNode->Num != s_Stmap60NearMissLeafDiagTarget )
        return;
    p->nStmap60NearMissLeafDiag++;
    NodeAigId = Map_NodeReadAigId( pNode );
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        LeafAigId = Map_NodeReadAigId( pLeaf );
        LeafNode = pLeaf->Num;
        LeafRatio = Map_MatchStmap45PressureLookup( LeafAigId );
        if ( nLeaves == 0 || LeafRatio > MaxLeafRatio )
        {
            MaxLeafRatio = LeafRatio;
            MaxLeafAigId = LeafAigId;
            MaxLeafNode = LeafNode;
        }
        nLeaves++;
        printf( "stmap60 near-miss leaf diag: index = %d  near-miss-index = %d  tracked-node = %d  node-aig-id = %d  phase = %d  leaf-index = %d  leaf-node = %d  leaf-aig-id = %d  leaf-pressure-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  reason = %s\n",
            p->nStmap60NearMissLeafDiag, NearMissIndex, pNode->Num, NodeAigId, fPhase, i,
            LeafNode, LeafAigId, LeafRatio, NodePressureRatio, CutPressureRatio, pReason );
    }
    p->nStmap60NearMissLeafDiagLeaves += nLeaves;
    if ( p->nStmap60NearMissLeafDiag == 1 || MaxLeafRatio > p->Stmap60NearMissMaxLeafRatio )
    {
        p->Stmap60NearMissMaxLeafRatio = MaxLeafRatio;
        p->Stmap60NearMissMaxLeafAigId = MaxLeafAigId;
        p->Stmap60NearMissMaxLeafNode = MaxLeafNode;
    }
    printf( "stmap60 near-miss leaf summary: index = %d  near-miss-index = %d  tracked-node = %d  node-aig-id = %d  phase = %d  leaves = %d  max-leaf-node = %d  max-leaf-aig-id = %d  max-leaf-pressure-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  scl-feedback = %.3f\n",
        p->nStmap60NearMissLeafDiag, NearMissIndex, pNode->Num, NodeAigId, fPhase, nLeaves,
        MaxLeafNode, MaxLeafAigId, MaxLeafRatio, NodePressureRatio, CutPressureRatio, Slack,
        AreaSave, ArrivalDelta, ArrivalGainMargin, AreaMargin, s_Stmap45SclFeedback );
}

static int Map_MatchStmap45HasPressureAgreement( float NodePressureRatio, float CutPressureRatio )
{
    float PressureSpread;
    if ( s_Stmap45SclFeedback <= 0.0 || NodePressureRatio <= 1.75 || CutPressureRatio <= 1.75 )
        return 0;
    PressureSpread = NodePressureRatio > CutPressureRatio ? NodePressureRatio / CutPressureRatio : CutPressureRatio / NodePressureRatio;
    return PressureSpread <= 1.25;
}

static const char * Map_MatchStmap61CutOnlyFirstBlocker( int fModerateSoftSeed, int fModeratePenaltyCandidate, int fPressureAgreement, int fPressureNear, int fFeedbackGate, int fEntryGate, int fModerateGainGate, int fNodeZeroGate, int fCutBandGate, int fArrivalStrongGate, int fSlackGate, int fAreaCapGate )
{
    if ( !fModerateSoftSeed )
        return "soft-seed";
    if ( fPressureAgreement )
        return "pressure-agreement";
    if ( !fModeratePenaltyCandidate )
        return "moderate-candidate";
    if ( fPressureNear )
        return "pressure-near";
    if ( !fFeedbackGate )
        return "feedback";
    if ( !fEntryGate )
        return "pressure-entries";
    if ( !fModerateGainGate )
        return "moderate-gain";
    if ( !fNodeZeroGate )
        return "node-pressure";
    if ( !fCutBandGate )
        return "cut-band";
    if ( !fArrivalStrongGate )
        return "arrival-strength";
    if ( !fSlackGate )
        return "slack";
    if ( !fAreaCapGate )
        return "area-cap";
    return "none";
}

static void Map_MatchStmap61PrintCutOnlyGateDiag( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, int NearMissIndex, const char * pReason, int fProfileOpen, int fModerateSoftSeed, int fModeratePenaltyCandidate, int fPressureAgreement, int fPressureNear, int fCutOnlyRaw, int fCutOnlyAreaCapBlocked, int fCutOnlyPressure, float Slack, float SlackMargin, float AreaSave, float OneInvArea, float ArrivalDelta, float ArrivalGainMargin, float AreaMargin, float NodePressureRatio, float CutPressureRatio )
{
    int NodeAigId, fEarlyDepth, fTightCritical, fSlack125Gate;
    int fFeedbackGate, fEntryGate, fModerateGainGate, fNodeZeroGate, fCutBandGate, fArrivalStrongGate, fSlackGate, fAreaCapGate;
    int fPrimitivePass, fLoadDropGate, fRawExpected;
    const char * pFirstBlocker;
    if ( !s_fStmap61CutOnlyGateDiag || p == NULL || pNode == NULL || pCut == NULL || pNode->Num != s_Stmap61CutOnlyGateDiagTarget )
        return;
    NodeAigId = Map_NodeReadAigId( pNode );
    fEarlyDepth = Map_MatchNodeHasStmap20EarlySeedDepth( pNode );
    fTightCritical = Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
    fSlack125Gate = Slack <= 1.25 * SlackMargin + p->fEpsilon;
    fFeedbackGate = s_Stmap45SclFeedback >= 0.85;
    fEntryGate = s_nStmap45SclPressureEntries >= 8000;
    fModerateGainGate = Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon );
    fNodeZeroGate = NodePressureRatio <= 0.0;
    fCutBandGate = CutPressureRatio >= 1.60 && CutPressureRatio <= 2.20;
    fArrivalStrongGate = ArrivalDelta <= -2.0 * ArrivalGainMargin - p->fEpsilon;
    fSlackGate = Slack >= SlackMargin + p->fEpsilon;
    fAreaCapGate = OneInvArea > 0.0 && AreaSave <= 1.35 * OneInvArea + p->fEpsilon;
    fPrimitivePass = fFeedbackGate && fEntryGate && !fPressureAgreement && fModerateGainGate && fNodeZeroGate && fCutBandGate && fArrivalStrongGate && fSlackGate;
    fLoadDropGate = fFeedbackGate && fEntryGate && !fPressureAgreement &&
        fModerateGainGate && CutPressureRatio >= 1.95 && CutPressureRatio <= 2.20 &&
        fArrivalStrongGate && fSlackGate &&
        ((s_fStmap63CutOnlyLoadDropGuard && NodePressureRatio >= 1.05 && NodePressureRatio <= 1.55) ||
         (s_fStmap65StrongNodeLoadDropGuard && NodePressureRatio >= 1.25 && NodePressureRatio <= 1.55));
    fRawExpected = ((fModeratePenaltyCandidate || s_fStmap62CutOnlyBeforeModerate) && fPrimitivePass) || fLoadDropGate;
    pFirstBlocker = fCutOnlyPressure ? "none" : Map_MatchStmap61CutOnlyFirstBlocker( fModerateSoftSeed, fModeratePenaltyCandidate, fPressureAgreement, fPressureNear, fFeedbackGate, fEntryGate, fModerateGainGate, fNodeZeroGate, fCutBandGate, fArrivalStrongGate, fSlackGate, fAreaCapGate );
    p->nStmap61CutOnlyGateDiag++;
    if ( fPrimitivePass )
        p->nStmap61CutOnlyPrimitivePass++;
    if ( fCutOnlyRaw )
        p->nStmap61CutOnlyRawPass++;
    if ( fPrimitivePass && !fModeratePenaltyCandidate && !fCutOnlyPressure )
        p->nStmap61CutOnlyRawBlockedByModerate++;
    if ( fModeratePenaltyCandidate && !fPrimitivePass )
        p->nStmap61CutOnlyRawBlockedByPrimitive++;
    if ( fCutOnlyRaw && fAreaCapGate )
        p->nStmap61CutOnlyAreaCapPass++;
    if ( fCutOnlyAreaCapBlocked )
        p->nStmap61CutOnlyAreaCapBlocked++;
    if ( fCutOnlyPressure )
        p->nStmap61CutOnlyAccepted++;
    if ( !fModerateSoftSeed )
        p->nStmap61CutOnlySoftSeedFail++;
    if ( !fModeratePenaltyCandidate )
        p->nStmap61CutOnlyModerateCandidateFail++;
    if ( !fFeedbackGate )
        p->nStmap61CutOnlyFeedbackFail++;
    if ( !fEntryGate )
        p->nStmap61CutOnlyEntryFail++;
    if ( fPressureAgreement )
        p->nStmap61CutOnlyAgreementBlocked++;
    if ( !fNodeZeroGate )
        p->nStmap61CutOnlyNodeZeroFail++;
    if ( !fCutBandGate )
        p->nStmap61CutOnlyCutBandFail++;
    if ( !fArrivalStrongGate )
        p->nStmap61CutOnlyArrivalFail++;
    if ( !fSlackGate )
        p->nStmap61CutOnlySlackFail++;
    printf( "stmap61 cut-only gate diag: index = %d  near-miss-index = %d  tracked-node = %d  node-aig-id = %d  phase = %d  reason = %s  first-blocker = %s  profile-open = %d  early-depth = %d  soft-seed = %d  moderate-candidate = %d  tight-critical = %d  slack-1p25-pass = %d  pressure-agreement = %d  pressure-near = %d  primitive-pass = %d  raw-expected = %d  raw-pass = %d  area-cap-pass = %d  area-cap-blocked = %d  accepted = %d  severe-feedback-pass = %d  pressure-entries = %d  pressure-entries-pass = %d  moderate-gain-pass = %d  node-zero-pass = %d  cut-band-pass = %d  arrival-strength-pass = %d  slack-pass = %d  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  slack = %.6f  slack-margin = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  one-inv-area = %.6f  scl-feedback = %.3f\n",
        p->nStmap61CutOnlyGateDiag, NearMissIndex, pNode->Num, NodeAigId, fPhase,
        pReason, pFirstBlocker, fProfileOpen, fEarlyDepth, fModerateSoftSeed,
        fModeratePenaltyCandidate, fTightCritical, fSlack125Gate, fPressureAgreement,
        fPressureNear, fPrimitivePass, fRawExpected, fCutOnlyRaw, fAreaCapGate,
        fCutOnlyAreaCapBlocked, fCutOnlyPressure, fFeedbackGate, s_nStmap45SclPressureEntries,
        fEntryGate, fModerateGainGate, fNodeZeroGate, fCutBandGate, fArrivalStrongGate,
        fSlackGate, NodePressureRatio, CutPressureRatio, Slack, SlackMargin, AreaSave,
        1.35 * OneInvArea, ArrivalDelta, ArrivalGainMargin, AreaMargin, OneInvArea,
        s_Stmap45SclFeedback );
}

static float Map_MatchStmap45ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, NodePressureRatio, CutPressureRatio, PressureAgreement, LocalFeedback;
    Penalty = Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
    NodePressureRatio = Map_MatchStmap45NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap45CutPressureRatio( pCut );
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty <= 0.0 || !Map_MatchStmap45HasPressureAgreement( NodePressureRatio, CutPressureRatio ) )
        return 0.0;
    PressureAgreement = NodePressureRatio < CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    LocalFeedback = (PressureAgreement - 1.75) / 3.0;
    if ( LocalFeedback > 1.0 )
        LocalFeedback = 1.0;
    Penalty *= 0.40 + 0.60 * s_Stmap45SclFeedback * LocalFeedback;
    return Penalty > 1.0 ? 1.0 : Penalty;
}

static float Map_MatchStmap45StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap45NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap45CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap45SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.16 * s_Stmap45SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

static float Map_MatchStmap52ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId, int * pAreaCap )
{
    float Penalty, NodePressureRatio, CutPressureRatio, PressureAgreement;
    if ( pAreaCap )
        *pAreaCap = 0;
    Penalty = Map_MatchStmap45ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pNodePressureRatio, pCutPressureRatio, pNodeAigId );
    if ( Penalty <= 0.0 )
        return 0.0;
    NodePressureRatio = pNodePressureRatio ? *pNodePressureRatio : 0.0;
    CutPressureRatio = pCutPressureRatio ? *pCutPressureRatio : 0.0;
    PressureAgreement = NodePressureRatio < CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( PressureAgreement < 2.35 || (Slack > 1.35 * SlackMargin + Epsilon && ArrivalDelta <= Epsilon) )
    {
        if ( pAreaCap )
            *pAreaCap = 1;
        Penalty *= 0.65;
    }
    return Penalty;
}

static int Map_MatchStmap53HasPressureNearException( float NodePressureRatio, float CutPressureRatio, float ArrivalDelta, float ArrivalGainMargin, float Epsilon )
{
    float PressureMax, PressureMin, PressureSpread;
    if ( s_Stmap45SclFeedback < 0.85 || s_nStmap45SclPressureEntries < 8000 )
        return 0;
    if ( Map_MatchStmap45HasPressureAgreement( NodePressureRatio, CutPressureRatio ) )
        return 0;
    if ( !Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) )
        return 0;
    PressureMax = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    PressureMin = NodePressureRatio < CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( PressureMin < 1.25 || PressureMax > 2.75 )
        return 0;
    PressureSpread = PressureMax / PressureMin;
    if ( PressureSpread > 2.25 )
        return 0;
    return 1;
}

static int Map_MatchStmap54HasCutOnlyPressureException( float NodePressureRatio, float CutPressureRatio, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( s_Stmap45SclFeedback < 0.85 || s_nStmap45SclPressureEntries < 8000 )
        return 0;
    if ( Map_MatchStmap45HasPressureAgreement( NodePressureRatio, CutPressureRatio ) )
        return 0;
    if ( !Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) )
        return 0;
    if ( NodePressureRatio > 0.0 )
        return 0;
    if ( CutPressureRatio < 1.60 || CutPressureRatio > 2.20 )
        return 0;
    if ( ArrivalDelta > -2.0 * ArrivalGainMargin - Epsilon )
        return 0;
    if ( Slack < SlackMargin + Epsilon )
        return 0;
    return 1;
}

static int Map_MatchStmap63HasLoadDropCutOnlyPressureException( float NodePressureRatio, float CutPressureRatio, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( s_Stmap45SclFeedback < 0.85 || s_nStmap45SclPressureEntries < 8000 )
        return 0;
    if ( Map_MatchStmap45HasPressureAgreement( NodePressureRatio, CutPressureRatio ) )
        return 0;
    if ( !Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) )
        return 0;
    if ( NodePressureRatio < 1.05 || NodePressureRatio > 1.55 )
        return 0;
    if ( CutPressureRatio < 1.95 || CutPressureRatio > 2.20 )
        return 0;
    if ( ArrivalDelta > -2.0 * ArrivalGainMargin - Epsilon )
        return 0;
    if ( Slack < SlackMargin + Epsilon )
        return 0;
    return 1;
}

static int Map_MatchStmap65HasStrongNodeLoadDropCutOnlyPressureException( float NodePressureRatio, float CutPressureRatio, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( s_Stmap45SclFeedback < 0.85 || s_nStmap45SclPressureEntries < 8000 )
        return 0;
    if ( Map_MatchStmap45HasPressureAgreement( NodePressureRatio, CutPressureRatio ) )
        return 0;
    if ( !Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) )
        return 0;
    if ( NodePressureRatio < 1.25 || NodePressureRatio > 1.55 )
        return 0;
    if ( CutPressureRatio < 1.95 || CutPressureRatio > 2.20 )
        return 0;
    if ( ArrivalDelta > -2.0 * ArrivalGainMargin - Epsilon )
        return 0;
    if ( Slack < SlackMargin + Epsilon )
        return 0;
    return 1;
}

static int Map_MatchStmap66HasNearStrongNodeLoadDropCandidate( float NodePressureRatio, float CutPressureRatio, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    if ( s_Stmap45SclFeedback < 0.85 || s_nStmap45SclPressureEntries < 8000 )
        return 0;
    if ( Map_MatchStmap45HasPressureAgreement( NodePressureRatio, CutPressureRatio ) )
        return 0;
    if ( !Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) )
        return 0;
    if ( NodePressureRatio < 1.15 || NodePressureRatio >= 1.25 )
        return 0;
    if ( CutPressureRatio < 1.95 || CutPressureRatio > 2.20 )
        return 0;
    if ( ArrivalDelta > -2.0 * ArrivalGainMargin - Epsilon )
        return 0;
    if ( Slack < SlackMargin + Epsilon )
        return 0;
    return 1;
}

static float Map_MatchStmap53ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId, int * pAreaCap, int * pPressureNear )
{
    float Penalty;
    if ( pPressureNear )
        *pPressureNear = 0;
    Penalty = Map_MatchStmap52ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pNodePressureRatio, pCutPressureRatio, pNodeAigId, pAreaCap );
    if ( Penalty > 0.0 )
        return Penalty;
    if ( !Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    if ( Map_MatchStmap53HasPressureNearException( pNodePressureRatio ? *pNodePressureRatio : 0.0, pCutPressureRatio ? *pCutPressureRatio : 0.0, ArrivalDelta, ArrivalGainMargin, Epsilon ) )
    {
        if ( pPressureNear )
            *pPressureNear = 1;
    }
    return 0.0;
}

static float Map_MatchStmap54ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId, int * pAreaCap, int * pPressureNear, int * pCutOnlyPressure )
{
    float Penalty;
    if ( pPressureNear )
        *pPressureNear = 0;
    if ( pCutOnlyPressure )
        *pCutOnlyPressure = 0;
    Penalty = Map_MatchStmap53ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pNodePressureRatio, pCutPressureRatio, pNodeAigId, pAreaCap, pPressureNear );
    if ( Penalty > 0.0 )
        return Penalty;
    if ( pPressureNear && *pPressureNear )
        return 0.0;
    if ( s_fStmap62CutOnlyBeforeModerate &&
         Map_MatchStmap54HasCutOnlyPressureException( pNodePressureRatio ? *pNodePressureRatio : 0.0, pCutPressureRatio ? *pCutPressureRatio : 0.0, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
    {
        if ( pCutOnlyPressure )
            *pCutOnlyPressure = 1;
        return 0.0;
    }
    if ( s_fStmap63CutOnlyLoadDropGuard &&
         Map_MatchStmap63HasLoadDropCutOnlyPressureException( pNodePressureRatio ? *pNodePressureRatio : 0.0, pCutPressureRatio ? *pCutPressureRatio : 0.0, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
    {
        if ( pCutOnlyPressure )
            *pCutOnlyPressure = 1;
        return 0.0;
    }
    if ( s_fStmap65StrongNodeLoadDropGuard &&
         Map_MatchStmap65HasStrongNodeLoadDropCutOnlyPressureException( pNodePressureRatio ? *pNodePressureRatio : 0.0, pCutPressureRatio ? *pCutPressureRatio : 0.0, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
    {
        if ( pCutOnlyPressure )
            *pCutOnlyPressure = 1;
        return 0.0;
    }
    if ( !Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
        return 0.0;
    if ( Map_MatchStmap54HasCutOnlyPressureException( pNodePressureRatio ? *pNodePressureRatio : 0.0, pCutPressureRatio ? *pCutPressureRatio : 0.0, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon ) )
    {
        if ( pCutOnlyPressure )
            *pCutOnlyPressure = 1;
    }
    return 0.0;
}

static int Map_MatchStmap55CutOnlyAreaCapPass( float AreaSave, float OneInvArea, float Epsilon )
{
    return OneInvArea > 0.0 && AreaSave <= 1.25 * OneInvArea + Epsilon;
}

static float Map_MatchStmap55ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId, int * pAreaCap, int * pPressureNear, int * pCutOnlyPressure )
{
    return Map_MatchStmap54ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pNodePressureRatio, pCutPressureRatio, pNodeAigId, pAreaCap, pPressureNear, pCutOnlyPressure );
}

static int Map_MatchStmap56CutOnlyAreaCapPass( float AreaSave, float OneInvArea, float Epsilon )
{
    return OneInvArea > 0.0 && AreaSave <= 1.35 * OneInvArea + Epsilon;
}

static float Map_MatchStmap56ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId, int * pAreaCap, int * pPressureNear, int * pCutOnlyPressure )
{
    return Map_MatchStmap54ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pNodePressureRatio, pCutPressureRatio, pNodeAigId, pAreaCap, pPressureNear, pCutOnlyPressure );
}

static float s_Stmap46SclMaxLoadRatio = 0.0;
static float s_Stmap46SclOverFrac = 0.0;
static float s_Stmap46SclFeedback = 0.0;
static float * s_pStmap46SclPressureRatios = NULL;
static int s_nStmap46SclPressureRatios = 0;

void Map_Stmap46SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity, float * pAigPressureRatios, int nAigPressureRatios )
{
    s_Stmap46SclMaxLoadRatio = MaxLoadRatio;
    s_Stmap46SclOverFrac = OverFrac;
    s_Stmap46SclFeedback = Severity;
    s_pStmap46SclPressureRatios = pAigPressureRatios;
    s_nStmap46SclPressureRatios = nAigPressureRatios;
}

static float Map_MatchStmap46PressureLookup( int AigId )
{
    if ( AigId < 0 || AigId >= s_nStmap46SclPressureRatios || s_Stmap46SclFeedback <= 0.0 || s_pStmap46SclPressureRatios == NULL )
        return 0.0;
    return s_pStmap46SclPressureRatios[AigId];
}

static float Map_MatchStmap46NodePressureRatio( Map_Node_t * pNode, int * pAigId )
{
    int AigId;
    if ( pAigId )
        *pAigId = -1;
    if ( pNode == NULL || pNode->Num < 0 )
        return 0.0;
    AigId = Map_NodeReadAigId( pNode );
    if ( pAigId )
        *pAigId = AigId;
    return Map_MatchStmap46PressureLookup( AigId );
}

static float Map_MatchStmap46CutPressureRatio( Map_Cut_t * pCut )
{
    Map_Node_t * pLeaf;
    float Ratio, RatioMax = 0.0;
    int i;
    if ( pCut == NULL )
        return 0.0;
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pLeaf = Map_Regular( pCut->ppLeaves[i] );
        if ( pLeaf == NULL )
            continue;
        Ratio = Map_MatchStmap46NodePressureRatio( pLeaf, NULL );
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
    }
    return RatioMax;
}

static int Map_MatchStmap46HasPressureAgreement( float NodePressureRatio, float CutPressureRatio )
{
    float PressureSpread;
    if ( s_Stmap46SclFeedback <= 0.0 || NodePressureRatio <= 1.75 || CutPressureRatio <= 1.75 )
        return 0;
    PressureSpread = NodePressureRatio > CutPressureRatio ? NodePressureRatio / CutPressureRatio : CutPressureRatio / NodePressureRatio;
    return PressureSpread <= 1.25;
}

static int Map_MatchHasStmap46TightException( float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon )
{
    return Map_MatchHasStmap21DeepSeedGain( ArrivalDelta, ArrivalGainMargin, Epsilon ) &&
        Slack <= 1.10 * SlackMargin + Epsilon;
}

static float Map_MatchStmap46ModeratePenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, NodePressureRatio, CutPressureRatio, PressureAgreement, LocalFeedback;
    Penalty = Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon );
    NodePressureRatio = Map_MatchStmap46NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap46CutPressureRatio( pCut );
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty <= 0.0 || !Map_MatchStmap46HasPressureAgreement( NodePressureRatio, CutPressureRatio ) )
        return 0.0;
    PressureAgreement = NodePressureRatio < CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    LocalFeedback = (PressureAgreement - 1.75) / 3.0;
    if ( LocalFeedback > 1.0 )
        LocalFeedback = 1.0;
    Penalty *= 0.40 + 0.60 * s_Stmap46SclFeedback * LocalFeedback;
    return Penalty > 1.0 ? 1.0 : Penalty;
}

static float Map_MatchStmap46StrongPenaltyFactor( Map_Node_t * pNode, Map_Cut_t * pCut, Map_Match_t * pMatch, float ArrivalDelta, float ArrivalGainMargin, float Slack, float SlackMargin, float Epsilon, float * pLeafLoadAvg, float * pLoadDriveRatio, int * pFanLimit, float * pNodePressureRatio, float * pCutPressureRatio, int * pNodeAigId )
{
    float Penalty, LoadDriveRatio, NodePressureRatio, CutPressureRatio, PressureRatio, LocalFeedback;
    Penalty = Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, Epsilon, pLeafLoadAvg, pLoadDriveRatio, pFanLimit );
    LoadDriveRatio = pLoadDriveRatio ? *pLoadDriveRatio : 0.0;
    NodePressureRatio = Map_MatchStmap46NodePressureRatio( pNode, pNodeAigId );
    CutPressureRatio = Map_MatchStmap46CutPressureRatio( pCut );
    PressureRatio = NodePressureRatio > CutPressureRatio ? NodePressureRatio : CutPressureRatio;
    if ( pNodePressureRatio )
        *pNodePressureRatio = NodePressureRatio;
    if ( pCutPressureRatio )
        *pCutPressureRatio = CutPressureRatio;
    if ( Penalty > 0.0 && s_Stmap46SclFeedback > 0.0 && PressureRatio > 1.0 && LoadDriveRatio > 1.0 )
    {
        LocalFeedback = (PressureRatio - 1.0) / 4.0;
        if ( LocalFeedback > 1.0 )
            LocalFeedback = 1.0;
        Penalty += 0.16 * s_Stmap46SclFeedback * LocalFeedback * (LoadDriveRatio - 1.0) / LoadDriveRatio;
    }
    return Penalty > 0.75 ? 0.75 : Penalty;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if a fanout/load proxy should reject this cut.]

  Description [Mode 1 is the classic map -f guard. Mode 2 is the stmap1
  selective guard, which only rejects wider cuts when the node has higher
  estimated fanout. Mode 3 is the stmap2 guard, which uses the same fanout
  thresholds only during area recovery and only when the current phase has
  slack over the mapper required time. Mode 4 is the stmap3 guard, which
  narrows the intervention to high-fanout nodes with only shallow positive
  slack. Mode 5 is the stmap4 split-phase guard: stmap1-style guarding in
  delay matching and stmap2-style slack-aware guarding in area recovery.
  Mode 6 is the stmap5 shape-gated split-phase guard: the delay pass only
  uses stmap1-style guarding in very large mapper graphs, while area recovery
  keeps the stmap2 slack-aware guard. Mode 7 is the stmap6 area-sensitive
  shape-gated guard: the delay pass keeps the stmap5 graph-size gate, small
  mapper graphs keep the stmap2 slack-aware recovery guard, and exact-area
  recovery on larger graphs checks the cut after matching so materially cheaper
  cuts can override the fanout/load proxy. Mode 8 is the stmap7 criticality-
  sensitive version of mode 7: exact-area recovery on larger graphs allows the
  override only with deeper mapper slack and outside the highest fanout bucket.
  Mode 9 is the stmap8 arrival-bounded version of mode 8: the area-saving
  override also requires the candidate match to stay within one quarter of an
  inverter delay of the existing mapper arrival. Mode 10 is the stmap9
  middle-slack area-relief version of mode 9: it keeps the highest fanout
  bucket protected, but in the one-to-two inverter slack bucket it allows a
  wide cut only when it is arrival-neutral and saves at least one inverter
  area. Mode 11 is the stmap10 depth-qualified version of mode 10: the
  middle-slack relief is allowed only for shallow and medium-depth nodes.
  Mode 12 is the stmap11 tight-depth version of mode 11: the middle-slack
  relief is allowed only for shallower nodes. Mode 13 is the stmap12
  fanout-bucket split version of mode 12: the tight-depth middle-slack relief
  is allowed only in the lower moderate fanout bucket. Mode 14 is stmap13,
  which keeps mode 13 behavior while recording guard-hit counters. Mode 15 is
  stmap14, which keeps the same bucket and counters but requires a stronger
  middle-slack area-flow saving. Mode 16 is stmap15, which restores the
  one-inverter relief threshold only after the current run has observed a high
  lower-moderate/middle-slack exact-area profile. Mode 17 is stmap16, which
  keeps mode 16 behavior and prints per-relief diagnostics. Mode 18 is stmap17,
  which uses the one-inverter post-profile threshold only for candidates with
  materially better mapper arrival. Mode 19 is stmap18, which keeps mode 18
  decisions and adds near-miss diagnostics for candidates rejected by the
  timing-quality/profile fallback. Mode 20 is stmap19, which allows the
  one-inverter threshold before profile-open only for candidates with the same
  material mapper-arrival improvement. Mode 21 is stmap20, which keeps the
  mode 20 pre-profile seed but restricts it to shallow nodes. Mode 22 is
  stmap21, which keeps the shallow seed and admits deeper pre-profile seeds
  only when the arrival gain is unusually large. Mode 23 is stmap22, which
  relaxes the deep pre-profile seed boundary to a moderate arrival gain. Mode
  24 is stmap23, which keeps the strong deep seeds but allows moderate deep
  seeds only when the existing mapper slack is near the critical side of the
  middle-slack window. Mode 25 is stmap24, which tightens the moderate deep
  slack gate to the lower edge of that critical window. Mode 26 is stmap25,
  which keeps stmap24 and admits at most one moderate deep seed from the next
  slack band as an ablation diagnostic. Mode 27 is stmap26, which skips the
  first such moderate seed and admits only the second qualifying seed. Mode 28
  is stmap27, which reopens the next slack band except for the high-reference,
  low-slack, stronger-moderate signature isolated by stmap26. Mode 29 is
  stmap28, which replaces that hard signature blocker with a high-reference
  moderate-seed area penalty that scales with reference count, slack risk, and
  arrival-gain risk. Mode 30 is stmap29, which keeps the same seed band but
  makes the penalty a continuous load-risk gradient. Mode 31 is stmap30,
  which keeps that moderate-seed gradient and adds a smaller continuous
  penalty to high-reference strong deep seeds. Mode 32 is stmap31, which keeps
  mode 31's moderate penalty but dampens the strong-seed gain slope. Mode 33
  is stmap32, which adds a cut-leaf load proxy to the strong-seed penalty. Mode
  34 is stmap33, which normalizes that load proxy by supergate fanout limit.
  Mode 35 is stmap34, which uses a bounded sublinear load/drive curve. Mode
  36 is stmap35, which preserves that mapper curve while command-level SCL
  load/max-cap diagnostics are emitted. Mode 37 is stmap36, which strengthens
  the bounded curve according to direct SCL over-cap severity measured by a
  command-layer first pass. Mode 38 is stmap37, which applies that feedback
  only when the current mapper node appears in the first-pass over-cap
  hotspot set. Mode 39 is stmap38, which broadens the local association to
  command-collected fanout users and candidate cut leaves. Mode 40 is stmap39,
  which replaces the static hotspot neighborhood with command-collected
  downstream consumer pressure. Mode 41 is stmap40, which preserves those
  pressure ratios in a dense AIG-ID lookup so medium-pressure reconstruction
  IDs are not discarded by a capped table. Mode 42 is stmap41, which uses the
  same dense transfer but weights pressure by first-pass SCL timing criticality
  before it can increase the strong-seed area margin. Mode 43 is stmap42, which
  propagates sink criticality from over-cap consumers into their fanin pressure
  cones with a bounded raw-pressure floor. Mode 44 is stmap43, which keeps the
  sink-pressure strong-seed protection but gates moderate deep area penalties
  by the same sink-pressure vector so unpressured paths can recover area. Mode
  45 is stmap44, which keeps the strong-seed sink-pressure rule but requires
  node and cut-leaf pressure agreement before applying the moderate penalty.
  Mode 46 is stmap45, which also requires that pressure agreement before
  moderate deep recovery can use the one-inverter admission margin. Mode 47
  is stmap46, which preserves the pressure-agreement rule for broad moderate
  candidates but admits high-arrival-gain tight-class seeds through a narrow
  exception. Mode 53 is stmap52, which returns to the stmap45
  pressure-agreement rule but caps the moderate pressure penalty for
  area-saving candidates that still have local pressure agreement. Mode 54 is
  stmap53, which keeps the stmap52 shape and adds a severe-feedback-only
  pressure-near moderate exception. Mode 55 is stmap54, which keeps the
  reviewed stmap53 branch and adds a cut-only one-sided pressure exception with
  stronger arrival-gain gating. Mode 56 is stmap55, which keeps that branch but
  caps cut-only exceptions to at most 1.25 inverter areas of local area-save.
  Mode 57 is stmap56, which relaxes only that cap to 1.35 inverter areas.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchSkipCutForFanout( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase )
{
    Map_Cut_t * pCutBest;
    Map_Match_t * pMatchBest;
    float Slack, SlackMargin, SlackLimit;

    if ( !p->fSkipFanout )
        return 0;
    if ( p->fSkipFanout == 2 )
        return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
    if ( p->fSkipFanout == 3 )
    {
        if ( p->fMappingMode < 1 || p->fMappingMode > 3 )
            return 0;
        pCutBest = pNode->pCutBest[fPhase];
        if ( pCutBest == NULL )
            return 0;
        pMatchBest = pCutBest->M + fPhase;
        if ( pMatchBest->pSuperBest == NULL )
            return 0;
        Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
        SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
        if ( Slack <= SlackMargin + p->fEpsilon )
            return 0;
        return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
    }
    if ( p->fSkipFanout == 4 )
    {
        if ( p->fMappingMode < 1 || p->fMappingMode > 3 )
            return 0;
        pCutBest = pNode->pCutBest[fPhase];
        if ( pCutBest == NULL )
            return 0;
        pMatchBest = pCutBest->M + fPhase;
        if ( pMatchBest->pSuperBest == NULL || p->pSuperLib == NULL )
            return 0;
        Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
        SlackMargin = p->pSuperLib->tDelayInv.Worst;
        SlackLimit = 2.0 * SlackMargin;
        if ( Slack <= p->fEpsilon || Slack > SlackLimit + p->fEpsilon )
            return 0;
        return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
    }
    if ( p->fSkipFanout == 5 )
    {
        if ( p->fMappingMode == 0 )
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        if ( p->fMappingMode < 1 || p->fMappingMode > 3 )
            return 0;
        pCutBest = pNode->pCutBest[fPhase];
        if ( pCutBest == NULL )
            return 0;
        pMatchBest = pCutBest->M + fPhase;
        if ( pMatchBest->pSuperBest == NULL )
            return 0;
        Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
        SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
        if ( Slack <= SlackMargin + p->fEpsilon )
            return 0;
        return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
    }
    if ( p->fSkipFanout == 6 )
    {
        if ( p->fMappingMode == 0 )
        {
            if ( p->vMapObjs->nSize <= 20000 )
                return 0;
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        }
        if ( p->fMappingMode < 1 || p->fMappingMode > 3 )
            return 0;
        pCutBest = pNode->pCutBest[fPhase];
        if ( pCutBest == NULL )
            return 0;
        pMatchBest = pCutBest->M + fPhase;
        if ( pMatchBest->pSuperBest == NULL )
            return 0;
        Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
        SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
        if ( Slack <= SlackMargin + p->fEpsilon )
            return 0;
        return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
    }
    if ( p->fSkipFanout == 7 )
    {
        if ( p->fMappingMode == 0 )
        {
            if ( p->vMapObjs->nSize <= 20000 )
                return 0;
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        }
        if ( p->fMappingMode >= 1 && p->fMappingMode <= 3 && p->vMapObjs->nSize <= 8000 )
        {
            pCutBest = pNode->pCutBest[fPhase];
            if ( pCutBest == NULL )
                return 0;
            pMatchBest = pCutBest->M + fPhase;
            if ( pMatchBest->pSuperBest == NULL )
                return 0;
            Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
            SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
            if ( Slack <= SlackMargin + p->fEpsilon )
                return 0;
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        }
        if ( p->fMappingMode == 1 )
        {
            pCutBest = pNode->pCutBest[fPhase];
            if ( pCutBest == NULL )
                return 0;
            pMatchBest = pCutBest->M + fPhase;
            if ( pMatchBest->pSuperBest == NULL )
                return 0;
            Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
            SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
            if ( Slack <= SlackMargin + p->fEpsilon )
                return 0;
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        }
        return 0;
    }
    if ( p->fSkipFanout >= 8 && p->fSkipFanout <= 57 )
    {
        if ( p->fMappingMode == 0 )
        {
            if ( p->vMapObjs->nSize <= 20000 )
                return 0;
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        }
        if ( p->fMappingMode >= 1 && p->fMappingMode <= 3 && p->vMapObjs->nSize <= 8000 )
        {
            pCutBest = pNode->pCutBest[fPhase];
            if ( pCutBest == NULL )
                return 0;
            pMatchBest = pCutBest->M + fPhase;
            if ( pMatchBest->pSuperBest == NULL )
                return 0;
            Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
            SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
            if ( Slack <= SlackMargin + p->fEpsilon )
                return 0;
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        }
        if ( p->fMappingMode == 1 )
        {
            pCutBest = pNode->pCutBest[fPhase];
            if ( pCutBest == NULL )
                return 0;
            pMatchBest = pCutBest->M + fPhase;
            if ( pMatchBest->pSuperBest == NULL )
                return 0;
            Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
            SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
            if ( Slack <= SlackMargin + p->fEpsilon )
                return 0;
            return Map_MatchCutHasStmapFanoutRisk( pNode, pCut );
        }
        return 0;
    }
    return (pNode->nRefs > 3 && pCut->nLeaves > 2) || (pNode->nRefs > 1 && pCut->nLeaves > 3);
}

/**Function*************************************************************

  Synopsis    [Returns 1 if an stmap area-sensitive mode should reject a cut.]

  Description [The area-sensitive guards are evaluated after matching the
  candidate cut because their hypotheses need the candidate's actual mapper
  area. In stmap6 exact-area recovery, a wide high-fanout cut is rejected only
  when the existing match has slack and the candidate does not save at least
  half an inverter area. In stmap7, the area-saving override additionally
  requires more than two inverter delays of slack and excludes the highest
  fanout-risk bucket. In stmap8, the override is further rejected when the
  candidate match worsens mapper arrival by more than one quarter of an
  inverter delay. In stmap9, middle-slack candidates can recover area only
  when they are outside the highest fanout bucket, do not worsen mapper
  arrival, and save at least one inverter area. In stmap10, that middle-slack
  relief also requires the mapper node to be no deeper than level 96. In
  stmap11, that depth window is tightened to level 64. In stmap12, the same
  tight-depth relief is allowed only in the lower moderate fanout bucket. In
  stmap13, the same decision path also records exact-area guard statistics. In
  stmap14, the lower-moderate middle-slack relief must save two inverter areas
  instead of one. In stmap15, one-inverter relief is restored only after the
  mapper observes a high lower-moderate/middle-slack profile. In stmap16, the
  same behavior also reports each admitted relief choice. In stmap17, the
  adaptive one-inverter relief threshold also requires a material mapper-arrival
  improvement; arrival-neutral choices keep the stricter two-inverter fallback.
  In stmap18, the same decisions are preserved while near-miss rejected
  middle-slack candidates are logged. In stmap19, pre-profile one-inverter
  relief is seeded only by candidates with the same material arrival gain. In
  stmap20, the pre-profile seed is further limited to shallow nodes. In
  stmap21, deeper pre-profile seeds are allowed only with a much larger mapper
  arrival gain. In stmap22, the same deep-seed path is relaxed to a moderate
  mapper-arrival gain. In stmap23, moderate deep seeds also need near-critical
  existing mapper slack. In stmap24, the moderate deep slack gate is tightened
  to the lower edge of the critical slack window. In stmap25, the stmap24 rule
  is kept, and one moderate deep seed from the next slack band is admitted for
  ablation. In stmap26, the first such moderate seed is skipped and only the
  second qualifying seed is admitted. In stmap27, moderate deep candidates in
  the next slack band are admitted unless they match the high-reference,
  low-slack, stronger-moderate signature isolated by stmap26. In stmap28, that
  hard blocker is replaced by a soft area-margin penalty on high-reference
  moderate seeds whose slack and arrival-gain ratios resemble load-risky
  choices. In stmap29, the high-reference penalty is made continuous over the
  same seed band. In stmap30, the same load-risk surface is extended to
  high-reference strong deep seeds with a smaller continuous penalty. In
  stmap31, the strong-seed gain slope is damped to probe the penalty threshold.
  In stmap32, a cut-leaf load proxy adds a candidate-cut fanout-shape term to
  the strong-seed penalty. In stmap33, the cut-leaf term is normalized by the
  selected supergate fanout limit to test a library-derived drive/load signal.
  In stmap34, the drive-normalized term uses a bounded sublinear curve so high
  load/drive ratios do not automatically saturate the area margin. Stmap35
  preserves the stmap34 mapper decision surface and adds direct SCL
  mapped-load diagnostics in the command layer. Stmap36 feeds the aggregate
  direct SCL load/max-cap severity from a first pass back into the same bounded
  strong-seed area margin. Stmap37 restricts that SCL feedback to first-pass
  over-cap hotspot AIG IDs and their immediate mapped fanin cone. Stmap38
  broadens the first-pass association and checks candidate cut leaves against
  that hotspot set before applying the local SCL feedback term. Stmap39 carries
  first-pass SCL load pressure onto downstream consumers and their fanin cones,
  then lets strong-seed candidates use the maximum node/cut consumer pressure
  as the local SCL feedback term. Stmap40 keeps the same pressure source but
  passes it as a dense AIG-ID vector so medium-pressure reconstructed IDs survive
  the command-to-mapper boundary. Stmap41 keeps dense pressure but gates its
  strength by first-pass SCL criticality, reducing non-critical over-cap hits
  before they affect mapper area recovery. Stmap42 instead propagates the
  over-cap consumer's criticality into the fanin cone and keeps a bounded
  raw-pressure floor for sink-correlated load pressure. Stmap43 keeps the
  strong-seed sink-pressure rule but applies moderate deep penalties only when
  the candidate or its leaves have material sink-pressure, letting unpressured
  area-recovery candidates use the lighter one-inverter margin. Stmap44 keeps
  the same strong-seed pressure protection and tightens moderate recovery by
  requiring direct node pressure and cut-leaf pressure to agree. Stmap45 moves
  that agreement check into the moderate-admission rule, so disagreement keeps
  the stricter two-inverter fallback margin. Stmap46 keeps the agreement rule
  for broad moderate candidates but admits high-arrival-gain tight-class
  candidates through a separate exception. Stmap52 returns to the stmap45
  pressure-agreement rule but caps pressure-agreed moderate penalties when the
  local pressure is below the severe band or the area candidate remains
  arrival-neutral. Stmap53 keeps that path and adds a bounded pressure-near
  exception for severe-feedback runs. Stmap54 keeps the reviewed stmap53
  severe branch but admits only a separately labelled cut-only one-sided
  pressure exception with stronger arrival-gain checks. Stmap55 keeps that
  exception but adds a local area-save cap so one-sided pressure cannot admit
  area-recovery choices larger than 1.25 inverter areas. Stmap56 relaxes only
  that cap to 1.35 inverter areas to test the boundary around the one known
  severe-pressure candidate.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchSkipAreaSensitiveFanout( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, Map_Match_t * pMatchBest, Map_Match_t * pMatch )
{
    float Slack, SlackMargin, SlackGate, ArrivalMargin, ArrivalDelta, ArrivalGainMargin, AreaMargin, AreaSave, OneInvArea;
    float Stmap28PenaltyFactor, Stmap29PenaltyFactor, Stmap30ModeratePenaltyFactor, Stmap30StrongPenaltyFactor, Stmap30PenaltyFactor;
    float Stmap31ModeratePenaltyFactor, Stmap31StrongPenaltyFactor, Stmap31PenaltyFactor;
    float Stmap32ModeratePenaltyFactor, Stmap32StrongPenaltyFactor, Stmap32PenaltyFactor, Stmap32CutLeafLoadAvg;
    float Stmap33ModeratePenaltyFactor, Stmap33StrongPenaltyFactor, Stmap33PenaltyFactor, Stmap33CutLeafLoadAvg, Stmap33LoadDriveRatio;
    float Stmap34ModeratePenaltyFactor, Stmap34StrongPenaltyFactor, Stmap34PenaltyFactor, Stmap34CutLeafLoadAvg, Stmap34LoadDriveRatio;
    float Stmap35ModeratePenaltyFactor, Stmap35StrongPenaltyFactor, Stmap35PenaltyFactor, Stmap35CutLeafLoadAvg, Stmap35LoadDriveRatio;
    float Stmap36ModeratePenaltyFactor, Stmap36StrongPenaltyFactor, Stmap36PenaltyFactor, Stmap36CutLeafLoadAvg, Stmap36LoadDriveRatio;
    float Stmap37ModeratePenaltyFactor, Stmap37StrongPenaltyFactor, Stmap37PenaltyFactor, Stmap37CutLeafLoadAvg, Stmap37LoadDriveRatio, Stmap37HotspotRatio;
    float Stmap38ModeratePenaltyFactor, Stmap38StrongPenaltyFactor, Stmap38PenaltyFactor, Stmap38CutLeafLoadAvg, Stmap38LoadDriveRatio, Stmap38NodeHotspotRatio, Stmap38CutHotspotRatio;
    float Stmap39ModeratePenaltyFactor, Stmap39StrongPenaltyFactor, Stmap39PenaltyFactor, Stmap39CutLeafLoadAvg, Stmap39LoadDriveRatio, Stmap39NodePressureRatio, Stmap39CutPressureRatio;
    float Stmap40ModeratePenaltyFactor, Stmap40StrongPenaltyFactor, Stmap40PenaltyFactor, Stmap40CutLeafLoadAvg, Stmap40LoadDriveRatio, Stmap40NodePressureRatio, Stmap40CutPressureRatio;
    float Stmap41ModeratePenaltyFactor, Stmap41StrongPenaltyFactor, Stmap41PenaltyFactor, Stmap41CutLeafLoadAvg, Stmap41LoadDriveRatio, Stmap41NodePressureRatio, Stmap41CutPressureRatio;
    float Stmap42ModeratePenaltyFactor, Stmap42StrongPenaltyFactor, Stmap42PenaltyFactor, Stmap42CutLeafLoadAvg, Stmap42LoadDriveRatio, Stmap42NodePressureRatio, Stmap42CutPressureRatio;
    float Stmap43ModeratePenaltyFactor, Stmap43StrongPenaltyFactor, Stmap43PenaltyFactor, Stmap43CutLeafLoadAvg, Stmap43LoadDriveRatio, Stmap43NodePressureRatio, Stmap43CutPressureRatio;
    float Stmap44ModeratePenaltyFactor, Stmap44StrongPenaltyFactor, Stmap44PenaltyFactor, Stmap44CutLeafLoadAvg, Stmap44LoadDriveRatio, Stmap44NodePressureRatio, Stmap44CutPressureRatio;
    float Stmap45ModeratePenaltyFactor, Stmap45StrongPenaltyFactor, Stmap45PenaltyFactor, Stmap45CutLeafLoadAvg, Stmap45LoadDriveRatio, Stmap45NodePressureRatio, Stmap45CutPressureRatio;
    float Stmap46ModeratePenaltyFactor, Stmap46StrongPenaltyFactor, Stmap46PenaltyFactor, Stmap46CutLeafLoadAvg, Stmap46LoadDriveRatio, Stmap46NodePressureRatio, Stmap46CutPressureRatio;
    float Stmap52ModeratePenaltyFactor, Stmap52StrongPenaltyFactor, Stmap52PenaltyFactor, Stmap52CutLeafLoadAvg, Stmap52LoadDriveRatio, Stmap52NodePressureRatio, Stmap52CutPressureRatio;
    float Stmap53ModeratePenaltyFactor, Stmap53StrongPenaltyFactor, Stmap53PenaltyFactor, Stmap53CutLeafLoadAvg, Stmap53LoadDriveRatio, Stmap53NodePressureRatio, Stmap53CutPressureRatio;
    float Stmap54ModeratePenaltyFactor, Stmap54StrongPenaltyFactor, Stmap54PenaltyFactor, Stmap54CutLeafLoadAvg, Stmap54LoadDriveRatio, Stmap54NodePressureRatio, Stmap54CutPressureRatio;
    float Stmap55ModeratePenaltyFactor, Stmap55StrongPenaltyFactor, Stmap55PenaltyFactor, Stmap55CutLeafLoadAvg, Stmap55LoadDriveRatio, Stmap55NodePressureRatio, Stmap55CutPressureRatio;
    float Stmap56ModeratePenaltyFactor, Stmap56StrongPenaltyFactor, Stmap56PenaltyFactor, Stmap56CutLeafLoadAvg, Stmap56LoadDriveRatio, Stmap56NodePressureRatio, Stmap56CutPressureRatio;
    const char * pReason;
    int fMiddleReliefWindow, fStmap13, fProfileOpen, fArrivalQuality, fStrictFallback;
    int fStmap25ModerateAblation, fStmap26ModerateAblationCandidate, fStmap26ModerateAblation;
    int fStmap27ModerateSignatureSeed, fStmap27ModerateSignatureBlocked;
    int fStmap28ModeratePenaltyCandidate, fStmap29ModeratePenaltyCandidate;
    int fStmap30ModeratePenaltyCandidate, fStmap30StrongPenaltyCandidate;
    int fStmap31ModeratePenaltyCandidate, fStmap31StrongPenaltyCandidate;
    int fStmap32ModeratePenaltyCandidate, fStmap32StrongPenaltyCandidate;
    int fStmap33ModeratePenaltyCandidate, fStmap33StrongPenaltyCandidate, Stmap33FanLimit;
    int fStmap34ModeratePenaltyCandidate, fStmap34StrongPenaltyCandidate, Stmap34FanLimit;
    int fStmap35ModeratePenaltyCandidate, fStmap35StrongPenaltyCandidate, Stmap35FanLimit;
    int fStmap36ModeratePenaltyCandidate, fStmap36StrongPenaltyCandidate, Stmap36FanLimit;
    int fStmap37ModeratePenaltyCandidate, fStmap37StrongPenaltyCandidate, Stmap37FanLimit;
    int fStmap38ModeratePenaltyCandidate, fStmap38StrongPenaltyCandidate, Stmap38FanLimit;
    int fStmap39ModeratePenaltyCandidate, fStmap39StrongPenaltyCandidate, Stmap39FanLimit, Stmap39NodeAigId;
    int fStmap40ModeratePenaltyCandidate, fStmap40StrongPenaltyCandidate, Stmap40FanLimit, Stmap40NodeAigId;
    int fStmap41ModeratePenaltyCandidate, fStmap41StrongPenaltyCandidate, Stmap41FanLimit, Stmap41NodeAigId;
    int fStmap42ModeratePenaltyCandidate, fStmap42StrongPenaltyCandidate, Stmap42FanLimit, Stmap42NodeAigId;
    int fStmap43ModeratePenaltyCandidate, fStmap43StrongPenaltyCandidate, Stmap43FanLimit, Stmap43NodeAigId;
    int fStmap44ModeratePenaltyCandidate, fStmap44StrongPenaltyCandidate, Stmap44FanLimit, Stmap44NodeAigId;
    int fStmap45ModerateSoftSeed, fStmap45ModeratePenaltyCandidate, fStmap45ModeratePressureAgreement, fStmap45StrongPenaltyCandidate, Stmap45FanLimit, Stmap45NodeAigId;
    int fStmap46ModerateSoftSeed, fStmap46ModeratePenaltyCandidate, fStmap46ModeratePressureAgreement, fStmap46TightException, fStmap46StrongPenaltyCandidate, Stmap46FanLimit, Stmap46NodeAigId;
    int fStmap52ModerateSoftSeed, fStmap52ModeratePenaltyCandidate, fStmap52ModeratePressureAgreement, fStmap52ModerateAreaCap, fStmap52StrongPenaltyCandidate, Stmap52FanLimit, Stmap52NodeAigId;
    int fStmap53ModerateSoftSeed, fStmap53ModeratePenaltyCandidate, fStmap53ModeratePressureAgreement, fStmap53ModerateAreaCap, fStmap53ModeratePressureNear, fStmap53StrongPenaltyCandidate, Stmap53FanLimit, Stmap53NodeAigId;
    int fStmap54ModerateSoftSeed, fStmap54ModeratePenaltyCandidate, fStmap54ModeratePressureAgreement, fStmap54ModerateAreaCap, fStmap54ModeratePressureNear, fStmap54CutOnlyPressure, fStmap54StrongPenaltyCandidate, Stmap54FanLimit, Stmap54NodeAigId;
    int fStmap55ModerateSoftSeed, fStmap55ModeratePenaltyCandidate, fStmap55ModeratePressureAgreement, fStmap55ModerateAreaCap, fStmap55ModeratePressureNear, fStmap55CutOnlyPressureRaw, fStmap55CutOnlyPressure, fStmap55CutOnlyAreaCapBlocked, fStmap55StrongPenaltyCandidate, Stmap55FanLimit, Stmap55NodeAigId;
    int fStmap56ModerateSoftSeed, fStmap56ModeratePenaltyCandidate, fStmap56ModeratePressureAgreement, fStmap56ModerateAreaCap, fStmap56ModeratePressureNear, fStmap56CutOnlyPressureRaw, fStmap56CutOnlyPressure, fStmap56CutOnlyAreaCapBlocked, fStmap56StrongPenaltyCandidate, Stmap56FanLimit, Stmap56NodeAigId;
    int fStmap66NearStrongNodeLoadDrop;

    if ( p->fSkipFanout < 7 || p->fSkipFanout > 57 )
        return 0;
    if ( p->fMappingMode < 2 || p->fMappingMode > 3 )
        return 0;
    if ( p->vMapObjs->nSize <= 8000 )
        return 0;
    if ( !Map_MatchCutHasStmapFanoutRisk( pNode, pCut ) )
        return 0;
    fStmap13 = (p->fSkipFanout >= 14 && p->fSkipFanout <= 57);
    Map_MatchStmap13CountRisk( p, pNode, pCut );
    if ( pMatchBest == NULL || pMatchBest->pSuperBest == NULL || pMatch == NULL || pMatch->pSuperBest == NULL )
        return 0;
    Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
    SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
    ArrivalDelta = pMatch->tArrive.Worst - pMatchBest->tArrive.Worst;
    ArrivalGainMargin = 0.25 * SlackMargin;
    fArrivalQuality = ArrivalDelta <= -ArrivalGainMargin - p->fEpsilon;
    if ( p->fSkipFanout >= 8 && p->fSkipFanout <= 57 )
    {
        SlackGate = 2.0 * SlackMargin;
        fMiddleReliefWindow =
             (p->fSkipFanout >= 10 && p->fSkipFanout <= 57) &&
             Slack > SlackMargin + p->fEpsilon &&
             Slack <= SlackGate + p->fEpsilon &&
             !Map_MatchCutHasStmapHighestFanoutRisk( pNode, pCut ) &&
             Map_MatchNodeHasStmapMiddleSlackRelief( p->fSkipFanout, pNode ) &&
             (p->fSkipFanout < 13 || Map_MatchCutHasStmapLowerModerateFanoutRisk( pNode, pCut )) &&
             pMatch->tArrive.Worst <= pMatchBest->tArrive.Worst + p->fEpsilon;
        if ( fMiddleReliefWindow )
        {
            if ( fStmap13 )
                p->nStmap13MiddleSlack++;
            fProfileOpen = Map_MatchHasStmap15ReliefProfile( p );
            fStmap25ModerateAblation =
                p->fSkipFanout == 26 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap25ModerateAblationSeed( p, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap26ModerateAblationCandidate =
                p->fSkipFanout == 27 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap26ModerateAblationCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap26ModerateAblation =
                fStmap26ModerateAblationCandidate &&
                Map_MatchIsStmap26RankTwoModerateAblationSeed( p, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap27ModerateSignatureSeed =
                p->fSkipFanout == 28 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap27SignatureSafeModerateSeed( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap27ModerateSignatureBlocked =
                p->fSkipFanout == 28 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap27BlockedModerateSignature( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap28ModeratePenaltyCandidate =
                p->fSkipFanout == 29 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap28PenaltyFactor = fStmap28ModeratePenaltyCandidate ?
                Map_MatchStmap28ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap29ModeratePenaltyCandidate =
                p->fSkipFanout == 30 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap29PenaltyFactor = fStmap29ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap30ModeratePenaltyCandidate =
                p->fSkipFanout == 31 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap30ModeratePenaltyFactor = fStmap30ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap30StrongPenaltyCandidate =
                p->fSkipFanout == 31 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap30StrongPenaltyFactor = fStmap30StrongPenaltyCandidate ?
                Map_MatchStmap30StrongPenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            Stmap30PenaltyFactor = Stmap30ModeratePenaltyFactor > Stmap30StrongPenaltyFactor ? Stmap30ModeratePenaltyFactor : Stmap30StrongPenaltyFactor;
            fStmap31ModeratePenaltyCandidate =
                p->fSkipFanout == 32 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap31ModeratePenaltyFactor = fStmap31ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap31StrongPenaltyCandidate =
                p->fSkipFanout == 32 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap31StrongPenaltyFactor = fStmap31StrongPenaltyCandidate ?
                Map_MatchStmap31StrongPenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            Stmap31PenaltyFactor = Stmap31ModeratePenaltyFactor > Stmap31StrongPenaltyFactor ? Stmap31ModeratePenaltyFactor : Stmap31StrongPenaltyFactor;
            fStmap32ModeratePenaltyCandidate =
                p->fSkipFanout == 33 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap32ModeratePenaltyFactor = fStmap32ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap32StrongPenaltyCandidate =
                p->fSkipFanout == 33 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap32StrongPenaltyFactor = fStmap32StrongPenaltyCandidate ?
                Map_MatchStmap32StrongPenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            Stmap32PenaltyFactor = Stmap32ModeratePenaltyFactor > Stmap32StrongPenaltyFactor ? Stmap32ModeratePenaltyFactor : Stmap32StrongPenaltyFactor;
            Stmap32CutLeafLoadAvg = fStmap32StrongPenaltyCandidate ? Map_MatchStmap32CutLeafLoadAvg( pCut ) : 0.0;
            fStmap33ModeratePenaltyCandidate =
                p->fSkipFanout == 34 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap33ModeratePenaltyFactor = fStmap33ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap33StrongPenaltyCandidate =
                p->fSkipFanout == 34 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap33CutLeafLoadAvg = 0.0;
            Stmap33LoadDriveRatio = 0.0;
            Stmap33FanLimit = 0;
            Stmap33StrongPenaltyFactor = fStmap33StrongPenaltyCandidate ?
                Map_MatchStmap33StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap33CutLeafLoadAvg, &Stmap33LoadDriveRatio, &Stmap33FanLimit ) : 0.0;
            Stmap33PenaltyFactor = Stmap33ModeratePenaltyFactor > Stmap33StrongPenaltyFactor ? Stmap33ModeratePenaltyFactor : Stmap33StrongPenaltyFactor;
            fStmap34ModeratePenaltyCandidate =
                p->fSkipFanout == 35 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap34ModeratePenaltyFactor = fStmap34ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap34StrongPenaltyCandidate =
                p->fSkipFanout == 35 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap34CutLeafLoadAvg = 0.0;
            Stmap34LoadDriveRatio = 0.0;
            Stmap34FanLimit = 0;
            Stmap34StrongPenaltyFactor = fStmap34StrongPenaltyCandidate ?
                Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap34CutLeafLoadAvg, &Stmap34LoadDriveRatio, &Stmap34FanLimit ) : 0.0;
            Stmap34PenaltyFactor = Stmap34ModeratePenaltyFactor > Stmap34StrongPenaltyFactor ? Stmap34ModeratePenaltyFactor : Stmap34StrongPenaltyFactor;
            fStmap35ModeratePenaltyCandidate =
                p->fSkipFanout == 36 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap35ModeratePenaltyFactor = fStmap35ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap35StrongPenaltyCandidate =
                p->fSkipFanout == 36 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap35CutLeafLoadAvg = 0.0;
            Stmap35LoadDriveRatio = 0.0;
            Stmap35FanLimit = 0;
            Stmap35StrongPenaltyFactor = fStmap35StrongPenaltyCandidate ?
                Map_MatchStmap34StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap35CutLeafLoadAvg, &Stmap35LoadDriveRatio, &Stmap35FanLimit ) : 0.0;
            Stmap35PenaltyFactor = Stmap35ModeratePenaltyFactor > Stmap35StrongPenaltyFactor ? Stmap35ModeratePenaltyFactor : Stmap35StrongPenaltyFactor;
            fStmap36ModeratePenaltyCandidate =
                p->fSkipFanout == 37 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap36ModeratePenaltyFactor = fStmap36ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap36StrongPenaltyCandidate =
                p->fSkipFanout == 37 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap36CutLeafLoadAvg = 0.0;
            Stmap36LoadDriveRatio = 0.0;
            Stmap36FanLimit = 0;
            Stmap36StrongPenaltyFactor = fStmap36StrongPenaltyCandidate ?
                Map_MatchStmap36StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap36CutLeafLoadAvg, &Stmap36LoadDriveRatio, &Stmap36FanLimit ) : 0.0;
            Stmap36PenaltyFactor = Stmap36ModeratePenaltyFactor > Stmap36StrongPenaltyFactor ? Stmap36ModeratePenaltyFactor : Stmap36StrongPenaltyFactor;
            fStmap37ModeratePenaltyCandidate =
                p->fSkipFanout == 38 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap37ModeratePenaltyFactor = fStmap37ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap37StrongPenaltyCandidate =
                p->fSkipFanout == 38 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap37CutLeafLoadAvg = 0.0;
            Stmap37LoadDriveRatio = 0.0;
            Stmap37HotspotRatio = 0.0;
            Stmap37FanLimit = 0;
            Stmap37StrongPenaltyFactor = fStmap37StrongPenaltyCandidate ?
                Map_MatchStmap37StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap37CutLeafLoadAvg, &Stmap37LoadDriveRatio, &Stmap37FanLimit, &Stmap37HotspotRatio ) : 0.0;
            Stmap37PenaltyFactor = Stmap37ModeratePenaltyFactor > Stmap37StrongPenaltyFactor ? Stmap37ModeratePenaltyFactor : Stmap37StrongPenaltyFactor;
            fStmap38ModeratePenaltyCandidate =
                p->fSkipFanout == 39 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap38ModeratePenaltyFactor = fStmap38ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap38StrongPenaltyCandidate =
                p->fSkipFanout == 39 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap38CutLeafLoadAvg = 0.0;
            Stmap38LoadDriveRatio = 0.0;
            Stmap38NodeHotspotRatio = 0.0;
            Stmap38CutHotspotRatio = 0.0;
            Stmap38FanLimit = 0;
            Stmap38StrongPenaltyFactor = fStmap38StrongPenaltyCandidate ?
                Map_MatchStmap38StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap38CutLeafLoadAvg, &Stmap38LoadDriveRatio, &Stmap38FanLimit, &Stmap38NodeHotspotRatio, &Stmap38CutHotspotRatio ) : 0.0;
            Stmap38PenaltyFactor = Stmap38ModeratePenaltyFactor > Stmap38StrongPenaltyFactor ? Stmap38ModeratePenaltyFactor : Stmap38StrongPenaltyFactor;
            fStmap39ModeratePenaltyCandidate =
                p->fSkipFanout == 40 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap39ModeratePenaltyFactor = fStmap39ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap39StrongPenaltyCandidate =
                p->fSkipFanout == 40 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap39CutLeafLoadAvg = 0.0;
            Stmap39LoadDriveRatio = 0.0;
            Stmap39NodePressureRatio = 0.0;
            Stmap39CutPressureRatio = 0.0;
            Stmap39FanLimit = 0;
            Stmap39NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap39NodeAigId = Map_NodeReadAigId( pNode );
            Stmap39StrongPenaltyFactor = fStmap39StrongPenaltyCandidate ?
                Map_MatchStmap39StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap39CutLeafLoadAvg, &Stmap39LoadDriveRatio, &Stmap39FanLimit, &Stmap39NodePressureRatio, &Stmap39CutPressureRatio, &Stmap39NodeAigId ) : 0.0;
            Stmap39PenaltyFactor = Stmap39ModeratePenaltyFactor > Stmap39StrongPenaltyFactor ? Stmap39ModeratePenaltyFactor : Stmap39StrongPenaltyFactor;
            fStmap40ModeratePenaltyCandidate =
                p->fSkipFanout == 41 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap40ModeratePenaltyFactor = fStmap40ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap40StrongPenaltyCandidate =
                p->fSkipFanout == 41 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap40CutLeafLoadAvg = 0.0;
            Stmap40LoadDriveRatio = 0.0;
            Stmap40NodePressureRatio = 0.0;
            Stmap40CutPressureRatio = 0.0;
            Stmap40FanLimit = 0;
            Stmap40NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap40NodeAigId = Map_NodeReadAigId( pNode );
            Stmap40StrongPenaltyFactor = fStmap40StrongPenaltyCandidate ?
                Map_MatchStmap40StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap40CutLeafLoadAvg, &Stmap40LoadDriveRatio, &Stmap40FanLimit, &Stmap40NodePressureRatio, &Stmap40CutPressureRatio, &Stmap40NodeAigId ) : 0.0;
            Stmap40PenaltyFactor = Stmap40ModeratePenaltyFactor > Stmap40StrongPenaltyFactor ? Stmap40ModeratePenaltyFactor : Stmap40StrongPenaltyFactor;
            fStmap41ModeratePenaltyCandidate =
                p->fSkipFanout == 42 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap41ModeratePenaltyFactor = fStmap41ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap41StrongPenaltyCandidate =
                p->fSkipFanout == 42 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap41CutLeafLoadAvg = 0.0;
            Stmap41LoadDriveRatio = 0.0;
            Stmap41NodePressureRatio = 0.0;
            Stmap41CutPressureRatio = 0.0;
            Stmap41FanLimit = 0;
            Stmap41NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap41NodeAigId = Map_NodeReadAigId( pNode );
            Stmap41StrongPenaltyFactor = fStmap41StrongPenaltyCandidate ?
                Map_MatchStmap41StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap41CutLeafLoadAvg, &Stmap41LoadDriveRatio, &Stmap41FanLimit, &Stmap41NodePressureRatio, &Stmap41CutPressureRatio, &Stmap41NodeAigId ) : 0.0;
            Stmap41PenaltyFactor = Stmap41ModeratePenaltyFactor > Stmap41StrongPenaltyFactor ? Stmap41ModeratePenaltyFactor : Stmap41StrongPenaltyFactor;
            fStmap42ModeratePenaltyCandidate =
                p->fSkipFanout == 43 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap42ModeratePenaltyFactor = fStmap42ModeratePenaltyCandidate ?
                Map_MatchStmap29ModeratePenaltyFactor( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ) : 0.0;
            fStmap42StrongPenaltyCandidate =
                p->fSkipFanout == 43 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap42CutLeafLoadAvg = 0.0;
            Stmap42LoadDriveRatio = 0.0;
            Stmap42NodePressureRatio = 0.0;
            Stmap42CutPressureRatio = 0.0;
            Stmap42FanLimit = 0;
            Stmap42NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap42NodeAigId = Map_NodeReadAigId( pNode );
            Stmap42StrongPenaltyFactor = fStmap42StrongPenaltyCandidate ?
                Map_MatchStmap42StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap42CutLeafLoadAvg, &Stmap42LoadDriveRatio, &Stmap42FanLimit, &Stmap42NodePressureRatio, &Stmap42CutPressureRatio, &Stmap42NodeAigId ) : 0.0;
            Stmap42PenaltyFactor = Stmap42ModeratePenaltyFactor > Stmap42StrongPenaltyFactor ? Stmap42ModeratePenaltyFactor : Stmap42StrongPenaltyFactor;
            fStmap43ModeratePenaltyCandidate =
                p->fSkipFanout == 44 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap43NodePressureRatio = 0.0;
            Stmap43CutPressureRatio = 0.0;
            Stmap43NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap43NodeAigId = Map_NodeReadAigId( pNode );
            Stmap43ModeratePenaltyFactor = fStmap43ModeratePenaltyCandidate ?
                Map_MatchStmap43ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap43NodePressureRatio, &Stmap43CutPressureRatio, &Stmap43NodeAigId ) : 0.0;
            fStmap43StrongPenaltyCandidate =
                p->fSkipFanout == 44 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap43CutLeafLoadAvg = 0.0;
            Stmap43LoadDriveRatio = 0.0;
            Stmap43FanLimit = 0;
            Stmap43StrongPenaltyFactor = fStmap43StrongPenaltyCandidate ?
                Map_MatchStmap43StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap43CutLeafLoadAvg, &Stmap43LoadDriveRatio, &Stmap43FanLimit, &Stmap43NodePressureRatio, &Stmap43CutPressureRatio, &Stmap43NodeAigId ) : 0.0;
            Stmap43PenaltyFactor = Stmap43ModeratePenaltyFactor > Stmap43StrongPenaltyFactor ? Stmap43ModeratePenaltyFactor : Stmap43StrongPenaltyFactor;
            fStmap44ModeratePenaltyCandidate =
                p->fSkipFanout == 45 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap44NodePressureRatio = 0.0;
            Stmap44CutPressureRatio = 0.0;
            Stmap44NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap44NodeAigId = Map_NodeReadAigId( pNode );
            Stmap44ModeratePenaltyFactor = fStmap44ModeratePenaltyCandidate ?
                Map_MatchStmap44ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap44NodePressureRatio, &Stmap44CutPressureRatio, &Stmap44NodeAigId ) : 0.0;
            fStmap44StrongPenaltyCandidate =
                p->fSkipFanout == 45 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap44CutLeafLoadAvg = 0.0;
            Stmap44LoadDriveRatio = 0.0;
            Stmap44FanLimit = 0;
            Stmap44StrongPenaltyFactor = fStmap44StrongPenaltyCandidate ?
                Map_MatchStmap44StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap44CutLeafLoadAvg, &Stmap44LoadDriveRatio, &Stmap44FanLimit, &Stmap44NodePressureRatio, &Stmap44CutPressureRatio, &Stmap44NodeAigId ) : 0.0;
            Stmap44PenaltyFactor = Stmap44ModeratePenaltyFactor > Stmap44StrongPenaltyFactor ? Stmap44ModeratePenaltyFactor : Stmap44StrongPenaltyFactor;
            fStmap45ModerateSoftSeed =
                p->fSkipFanout == 46 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap45ModeratePenaltyCandidate =
                fStmap45ModerateSoftSeed &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap45NodePressureRatio = 0.0;
            Stmap45CutPressureRatio = 0.0;
            Stmap45NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap45NodeAigId = Map_NodeReadAigId( pNode );
            Stmap45ModeratePenaltyFactor = fStmap45ModerateSoftSeed ?
                Map_MatchStmap45ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap45NodePressureRatio, &Stmap45CutPressureRatio, &Stmap45NodeAigId ) : 0.0;
            fStmap45ModeratePressureAgreement =
                fStmap45ModerateSoftSeed &&
                Map_MatchStmap45HasPressureAgreement( Stmap45NodePressureRatio, Stmap45CutPressureRatio );
            fStmap45StrongPenaltyCandidate =
                p->fSkipFanout == 46 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap45CutLeafLoadAvg = 0.0;
            Stmap45LoadDriveRatio = 0.0;
            Stmap45FanLimit = 0;
            Stmap45StrongPenaltyFactor = fStmap45StrongPenaltyCandidate ?
                Map_MatchStmap45StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap45CutLeafLoadAvg, &Stmap45LoadDriveRatio, &Stmap45FanLimit, &Stmap45NodePressureRatio, &Stmap45CutPressureRatio, &Stmap45NodeAigId ) : 0.0;
            Stmap45PenaltyFactor = Stmap45ModeratePenaltyFactor > Stmap45StrongPenaltyFactor ? Stmap45ModeratePenaltyFactor : Stmap45StrongPenaltyFactor;
            fStmap46ModerateSoftSeed =
                p->fSkipFanout == 47 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap46ModeratePenaltyCandidate =
                fStmap46ModerateSoftSeed &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap46TightException =
                fStmap46ModerateSoftSeed &&
                !fStmap46ModeratePenaltyCandidate &&
                Map_MatchHasStmap46TightException( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap46NodePressureRatio = 0.0;
            Stmap46CutPressureRatio = 0.0;
            Stmap46NodeAigId = -1;
            if ( pNode && pNode->Num >= 0 )
                Stmap46NodeAigId = Map_NodeReadAigId( pNode );
            Stmap46ModeratePenaltyFactor = fStmap46ModeratePenaltyCandidate ?
                Map_MatchStmap46ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap46NodePressureRatio, &Stmap46CutPressureRatio, &Stmap46NodeAigId ) : 0.0;
            fStmap46ModeratePressureAgreement =
                fStmap46ModerateSoftSeed &&
                Map_MatchStmap46HasPressureAgreement( Stmap46NodePressureRatio, Stmap46CutPressureRatio );
            fStmap46StrongPenaltyCandidate =
                p->fSkipFanout == 47 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap46CutLeafLoadAvg = 0.0;
            Stmap46LoadDriveRatio = 0.0;
            Stmap46FanLimit = 0;
            Stmap46StrongPenaltyFactor = fStmap46StrongPenaltyCandidate ?
                Map_MatchStmap46StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap46CutLeafLoadAvg, &Stmap46LoadDriveRatio, &Stmap46FanLimit, &Stmap46NodePressureRatio, &Stmap46CutPressureRatio, &Stmap46NodeAigId ) : 0.0;
            Stmap46PenaltyFactor = Stmap46ModeratePenaltyFactor > Stmap46StrongPenaltyFactor ? Stmap46ModeratePenaltyFactor : Stmap46StrongPenaltyFactor;
            fStmap52ModerateSoftSeed =
                p->fSkipFanout == 53 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap52ModeratePenaltyCandidate =
                fStmap52ModerateSoftSeed &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap52NodePressureRatio = 0.0;
            Stmap52CutPressureRatio = 0.0;
            Stmap52NodeAigId = -1;
            fStmap52ModerateAreaCap = 0;
            if ( pNode && pNode->Num >= 0 )
                Stmap52NodeAigId = Map_NodeReadAigId( pNode );
            Stmap52ModeratePenaltyFactor = fStmap52ModerateSoftSeed ?
                Map_MatchStmap52ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap52NodePressureRatio, &Stmap52CutPressureRatio, &Stmap52NodeAigId, &fStmap52ModerateAreaCap ) : 0.0;
            fStmap52ModeratePressureAgreement =
                fStmap52ModerateSoftSeed &&
                Map_MatchStmap45HasPressureAgreement( Stmap52NodePressureRatio, Stmap52CutPressureRatio );
            fStmap52StrongPenaltyCandidate =
                p->fSkipFanout == 53 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap52CutLeafLoadAvg = 0.0;
            Stmap52LoadDriveRatio = 0.0;
            Stmap52FanLimit = 0;
            Stmap52StrongPenaltyFactor = fStmap52StrongPenaltyCandidate ?
                Map_MatchStmap45StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap52CutLeafLoadAvg, &Stmap52LoadDriveRatio, &Stmap52FanLimit, &Stmap52NodePressureRatio, &Stmap52CutPressureRatio, &Stmap52NodeAigId ) : 0.0;
            Stmap52PenaltyFactor = Stmap52ModeratePenaltyFactor > Stmap52StrongPenaltyFactor ? Stmap52ModeratePenaltyFactor : Stmap52StrongPenaltyFactor;
            fStmap53ModerateSoftSeed =
                p->fSkipFanout == 54 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap53ModeratePenaltyCandidate =
                fStmap53ModerateSoftSeed &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap53NodePressureRatio = 0.0;
            Stmap53CutPressureRatio = 0.0;
            Stmap53NodeAigId = -1;
            fStmap53ModerateAreaCap = 0;
            fStmap53ModeratePressureNear = 0;
            if ( pNode && pNode->Num >= 0 )
                Stmap53NodeAigId = Map_NodeReadAigId( pNode );
            Stmap53ModeratePenaltyFactor = fStmap53ModerateSoftSeed ?
                Map_MatchStmap53ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap53NodePressureRatio, &Stmap53CutPressureRatio, &Stmap53NodeAigId, &fStmap53ModerateAreaCap, &fStmap53ModeratePressureNear ) : 0.0;
            fStmap53ModeratePressureAgreement =
                fStmap53ModerateSoftSeed &&
                Map_MatchStmap45HasPressureAgreement( Stmap53NodePressureRatio, Stmap53CutPressureRatio );
            fStmap53StrongPenaltyCandidate =
                p->fSkipFanout == 54 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap53CutLeafLoadAvg = 0.0;
            Stmap53LoadDriveRatio = 0.0;
            Stmap53FanLimit = 0;
            Stmap53StrongPenaltyFactor = fStmap53StrongPenaltyCandidate ?
                Map_MatchStmap45StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap53CutLeafLoadAvg, &Stmap53LoadDriveRatio, &Stmap53FanLimit, &Stmap53NodePressureRatio, &Stmap53CutPressureRatio, &Stmap53NodeAigId ) : 0.0;
            Stmap53PenaltyFactor = Stmap53ModeratePenaltyFactor > Stmap53StrongPenaltyFactor ? Stmap53ModeratePenaltyFactor : Stmap53StrongPenaltyFactor;
            fStmap54ModerateSoftSeed =
                p->fSkipFanout == 55 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap54ModeratePenaltyCandidate =
                fStmap54ModerateSoftSeed &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap54NodePressureRatio = 0.0;
            Stmap54CutPressureRatio = 0.0;
            Stmap54NodeAigId = -1;
            fStmap54ModerateAreaCap = 0;
            fStmap54ModeratePressureNear = 0;
            fStmap54CutOnlyPressure = 0;
            if ( pNode && pNode->Num >= 0 )
                Stmap54NodeAigId = Map_NodeReadAigId( pNode );
            Stmap54ModeratePenaltyFactor = fStmap54ModerateSoftSeed ?
                Map_MatchStmap54ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap54NodePressureRatio, &Stmap54CutPressureRatio, &Stmap54NodeAigId, &fStmap54ModerateAreaCap, &fStmap54ModeratePressureNear, &fStmap54CutOnlyPressure ) : 0.0;
            fStmap54ModeratePressureAgreement =
                fStmap54ModerateSoftSeed &&
                Map_MatchStmap45HasPressureAgreement( Stmap54NodePressureRatio, Stmap54CutPressureRatio );
            fStmap54StrongPenaltyCandidate =
                p->fSkipFanout == 55 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap54CutLeafLoadAvg = 0.0;
            Stmap54LoadDriveRatio = 0.0;
            Stmap54FanLimit = 0;
            Stmap54StrongPenaltyFactor = fStmap54StrongPenaltyCandidate ?
                Map_MatchStmap45StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap54CutLeafLoadAvg, &Stmap54LoadDriveRatio, &Stmap54FanLimit, &Stmap54NodePressureRatio, &Stmap54CutPressureRatio, &Stmap54NodeAigId ) : 0.0;
            Stmap54PenaltyFactor = Stmap54ModeratePenaltyFactor > Stmap54StrongPenaltyFactor ? Stmap54ModeratePenaltyFactor : Stmap54StrongPenaltyFactor;
            fStmap55ModerateSoftSeed =
                p->fSkipFanout == 56 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap55ModeratePenaltyCandidate =
                fStmap55ModerateSoftSeed &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap55NodePressureRatio = 0.0;
            Stmap55CutPressureRatio = 0.0;
            Stmap55NodeAigId = -1;
            fStmap55ModerateAreaCap = 0;
            fStmap55ModeratePressureNear = 0;
            fStmap55CutOnlyPressureRaw = 0;
            fStmap55CutOnlyPressure = 0;
            fStmap55CutOnlyAreaCapBlocked = 0;
            if ( pNode && pNode->Num >= 0 )
                Stmap55NodeAigId = Map_NodeReadAigId( pNode );
            Stmap55ModeratePenaltyFactor = fStmap55ModerateSoftSeed ?
                Map_MatchStmap55ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap55NodePressureRatio, &Stmap55CutPressureRatio, &Stmap55NodeAigId, &fStmap55ModerateAreaCap, &fStmap55ModeratePressureNear, &fStmap55CutOnlyPressureRaw ) : 0.0;
            fStmap55ModeratePressureAgreement =
                fStmap55ModerateSoftSeed &&
                Map_MatchStmap45HasPressureAgreement( Stmap55NodePressureRatio, Stmap55CutPressureRatio );
            fStmap55StrongPenaltyCandidate =
                p->fSkipFanout == 56 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap55CutLeafLoadAvg = 0.0;
            Stmap55LoadDriveRatio = 0.0;
            Stmap55FanLimit = 0;
            Stmap55StrongPenaltyFactor = fStmap55StrongPenaltyCandidate ?
                Map_MatchStmap45StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap55CutLeafLoadAvg, &Stmap55LoadDriveRatio, &Stmap55FanLimit, &Stmap55NodePressureRatio, &Stmap55CutPressureRatio, &Stmap55NodeAigId ) : 0.0;
            Stmap55PenaltyFactor = Stmap55ModeratePenaltyFactor > Stmap55StrongPenaltyFactor ? Stmap55ModeratePenaltyFactor : Stmap55StrongPenaltyFactor;
            AreaSave = pMatchBest->AreaFlow - pMatch->AreaFlow;
            OneInvArea = p->pSuperLib ? p->pSuperLib->AreaInv : 0.0;
            if ( fStmap55CutOnlyPressureRaw )
            {
                if ( Map_MatchStmap55CutOnlyAreaCapPass( AreaSave, OneInvArea, p->fEpsilon ) )
                    fStmap55CutOnlyPressure = 1;
                else
                    fStmap55CutOnlyAreaCapBlocked = 1;
            }
            fStmap56ModerateSoftSeed =
                p->fSkipFanout == 57 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            fStmap56ModeratePenaltyCandidate =
                fStmap56ModerateSoftSeed &&
                Map_MatchIsStmap28ModeratePenaltyCandidate( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap56NodePressureRatio = 0.0;
            Stmap56CutPressureRatio = 0.0;
            Stmap56NodeAigId = -1;
            fStmap56ModerateAreaCap = 0;
            fStmap56ModeratePressureNear = 0;
            fStmap56CutOnlyPressureRaw = 0;
            fStmap56CutOnlyPressure = 0;
            fStmap56CutOnlyAreaCapBlocked = 0;
            if ( pNode && pNode->Num >= 0 )
                Stmap56NodeAigId = Map_NodeReadAigId( pNode );
            Stmap56ModeratePenaltyFactor = fStmap56ModerateSoftSeed ?
                Map_MatchStmap56ModeratePenaltyFactor( pNode, pCut, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap56NodePressureRatio, &Stmap56CutPressureRatio, &Stmap56NodeAigId, &fStmap56ModerateAreaCap, &fStmap56ModeratePressureNear, &fStmap56CutOnlyPressureRaw ) : 0.0;
            fStmap56ModeratePressureAgreement =
                fStmap56ModerateSoftSeed &&
                Map_MatchStmap45HasPressureAgreement( Stmap56NodePressureRatio, Stmap56CutPressureRatio );
            fStmap56StrongPenaltyCandidate =
                p->fSkipFanout == 57 &&
                !fProfileOpen &&
                !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                Map_MatchIsStmap30StrongPenaltyCandidate( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            Stmap56CutLeafLoadAvg = 0.0;
            Stmap56LoadDriveRatio = 0.0;
            Stmap56FanLimit = 0;
            Stmap56StrongPenaltyFactor = fStmap56StrongPenaltyCandidate ?
                Map_MatchStmap45StrongPenaltyFactor( pNode, pCut, pMatch, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon, &Stmap56CutLeafLoadAvg, &Stmap56LoadDriveRatio, &Stmap56FanLimit, &Stmap56NodePressureRatio, &Stmap56CutPressureRatio, &Stmap56NodeAigId ) : 0.0;
            Stmap56PenaltyFactor = Stmap56ModeratePenaltyFactor > Stmap56StrongPenaltyFactor ? Stmap56ModeratePenaltyFactor : Stmap56StrongPenaltyFactor;
            if ( fStmap56CutOnlyPressureRaw )
            {
                if ( Map_MatchStmap56CutOnlyAreaCapPass( AreaSave, OneInvArea, p->fEpsilon ) )
                    fStmap56CutOnlyPressure = 1;
                else
                    fStmap56CutOnlyAreaCapBlocked = 1;
            }
            fStmap66NearStrongNodeLoadDrop =
                (s_fStmap66NearStrongNodeLoadDropDiag || s_fStmap72PathProximityWitnessDiag) &&
                p->fSkipFanout == 57 &&
                fStmap56ModerateSoftSeed &&
                !fStmap56CutOnlyPressureRaw &&
                Map_MatchStmap66HasNearStrongNodeLoadDropCandidate( Stmap56NodePressureRatio, Stmap56CutPressureRatio, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon );
            if ( fStmap66NearStrongNodeLoadDrop )
            {
                int fAreaCapPass = Map_MatchStmap56CutOnlyAreaCapPass( AreaSave, OneInvArea, p->fEpsilon );
                p->nStmap66NearStrongNodeLoadDrop++;
                if ( fAreaCapPass )
                {
                    p->nStmap66NearStrongNodeAreaCapPass++;
                    Map_Stmap67RecordNearStrongNodeWitness( pNode, Stmap56NodeAigId, fPhase, Stmap56NodePressureRatio, Stmap56CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
                }
                if ( p->nStmap66NearStrongNodeLoadDrop <= 64 )
                    printf( "stmap66 near-strong-node load-drop diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  area-cap-pass = %d  slack = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  pressure-entries = %d  scl-feedback = %.3f\n",
                        p->nStmap66NearStrongNodeLoadDrop, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, fAreaCapPass, Slack, AreaSave, 1.35 * OneInvArea,
                        ArrivalDelta, ArrivalGainMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio,
                        s_nStmap45SclPressureEntries, s_Stmap45SclFeedback );
            }
            fStrictFallback =
                p->fSkipFanout == 15 ||
                ((p->fSkipFanout == 16 || p->fSkipFanout == 17) && !fProfileOpen) ||
                ((p->fSkipFanout == 18 || p->fSkipFanout == 19) && (!fProfileOpen || !fArrivalQuality)) ||
                (p->fSkipFanout == 20 && !fArrivalQuality) ||
                (p->fSkipFanout == 21 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode )))) ||
                (p->fSkipFanout == 22 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap21DeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 23 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 24 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap23CriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 25 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap24TightCriticalModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 26 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap25SingleModerateDeepSeed( p, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 27 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap26RankTwoModerateDeepSeed( p, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 28 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap27SignatureSafeModerateDeepSeed( pNode, ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 29 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 30 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 31 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 32 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 33 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 34 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 35 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 36 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 37 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 38 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 39 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 40 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 41 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 42 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 43 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 44 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                (p->fSkipFanout == 45 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon )))) ||
                ((p->fSkipFanout == 46) &&
                    (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                    (!fStmap45ModerateSoftSeed || !fStmap45ModeratePressureAgreement)))) ||
                ((p->fSkipFanout == 47) &&
                    (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                    (!fStmap46ModerateSoftSeed || (!fStmap46ModeratePressureAgreement && !fStmap46TightException))))) ||
                ((p->fSkipFanout == 53) &&
                    (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                    (!fStmap52ModerateSoftSeed || !fStmap52ModeratePressureAgreement)))) ||
                ((p->fSkipFanout == 54) &&
                    (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                    (!fStmap53ModerateSoftSeed || (!fStmap53ModeratePressureAgreement && !fStmap53ModeratePressureNear))))) ||
                ((p->fSkipFanout == 55) &&
                    (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                    (!fStmap54ModerateSoftSeed || (!fStmap54ModeratePressureAgreement && !fStmap54ModeratePressureNear && !fStmap54CutOnlyPressure))))) ||
                ((p->fSkipFanout == 56) &&
                    (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                    (!fStmap55ModerateSoftSeed || (!fStmap55ModeratePressureAgreement && !fStmap55ModeratePressureNear && !fStmap55CutOnlyPressure))))) ||
                ((p->fSkipFanout == 57) &&
                    (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) &&
                    (!fStmap56ModerateSoftSeed || (!fStmap56ModeratePressureAgreement && !fStmap56ModeratePressureNear && !fStmap56CutOnlyPressure)))));
            AreaMargin = p->pSuperLib ? (fStrictFallback ? 2.0 : 1.0) * p->pSuperLib->AreaInv : 0.0;
            if ( p->fSkipFanout == 29 && fStmap28ModeratePenaltyCandidate && Stmap28PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap28PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 30 && fStmap29ModeratePenaltyCandidate && Stmap29PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap29PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 31 && Stmap30PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap30PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 32 && Stmap31PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap31PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 33 && Stmap32PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap32PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 34 && Stmap33PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap33PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 35 && Stmap34PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap34PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 36 && Stmap35PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap35PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 37 && Stmap36PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap36PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 38 && Stmap37PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap37PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 39 && Stmap38PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap38PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 40 && Stmap39PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap39PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 41 && Stmap40PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap40PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 42 && Stmap41PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap41PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 43 && Stmap42PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap42PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 44 && Stmap43PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap43PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 45 && Stmap44PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap44PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 46 && Stmap45PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap45PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 47 && Stmap46PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap46PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 53 && Stmap52PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap52PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 54 && Stmap53PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap53PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 55 && Stmap54PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap54PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 56 && Stmap55PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap55PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 57 && Stmap56PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap56PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( AreaSave > AreaMargin + p->fEpsilon )
            {
                if ( fStmap13 )
                    p->nStmap13MiddleRelief++;
                if ( p->fSkipFanout == 17 )
                    printf( "stmap16 relief diag: index = %d  profile-open = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  area-margin = %.6f\n",
                        p->nStmap13MiddleRelief, fProfileOpen, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, pMatchBest->AreaFlow - pMatch->AreaFlow,
                        pMatch->tArrive.Worst - pMatchBest->tArrive.Worst, AreaMargin );
                if ( p->fSkipFanout >= 18 && p->fSkipFanout <= 57 )
                    printf( "stmap%d relief diag: index = %d  profile-open = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f\n",
                        p->fSkipFanout - 1, p->nStmap13MiddleRelief, fProfileOpen, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, pMatchBest->AreaFlow - pMatch->AreaFlow,
                        ArrivalDelta, ArrivalGainMargin, AreaMargin );
                if ( p->fSkipFanout == 26 && fStmap25ModerateAblation )
                {
                    p->nStmap25ModerateAblationSeed++;
                    printf( "stmap25 ablation diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f\n",
                        p->nStmap25ModerateAblationSeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
                }
                if ( p->fSkipFanout == 27 && fStmap26ModerateAblation )
                {
                    p->nStmap26ModerateAblationSeen++;
                    p->nStmap26ModerateAblationSeed++;
                    printf( "stmap26 ablation diag: index = %d  rank = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f\n",
                        p->nStmap26ModerateAblationSeed, p->nStmap26ModerateAblationSeen, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
                }
                if ( p->fSkipFanout == 28 && fStmap27ModerateSignatureSeed )
                {
                    p->nStmap27ModerateSignatureSeed++;
                    printf( "stmap27 signature seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f\n",
                        p->nStmap27ModerateSignatureSeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
                }
                if ( p->fSkipFanout == 29 && fStmap28ModeratePenaltyCandidate && Stmap28PenaltyFactor > 0.0 )
                {
                    p->nStmap28ModeratePenaltySeed++;
                    printf( "stmap28 penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap28ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap28PenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 30 && fStmap29ModeratePenaltyCandidate && Stmap29PenaltyFactor > 0.0 )
                {
                    p->nStmap29ModeratePenaltySeed++;
                    printf( "stmap29 penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap29ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap29PenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 31 && fStmap30ModeratePenaltyCandidate && Stmap30ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap30ModeratePenaltySeed++;
                    printf( "stmap30 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap30ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap30ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 31 && fStmap30StrongPenaltyCandidate && Stmap30StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap30StrongPenaltySeed++;
                    printf( "stmap30 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap30StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap30StrongPenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 32 && fStmap31ModeratePenaltyCandidate && Stmap31ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap31ModeratePenaltySeed++;
                    printf( "stmap31 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap31ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap31ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 32 && fStmap31StrongPenaltyCandidate && Stmap31StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap31StrongPenaltySeed++;
                    printf( "stmap31 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap31StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap31StrongPenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 33 && fStmap32ModeratePenaltyCandidate && Stmap32ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap32ModeratePenaltySeed++;
                    printf( "stmap32 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap32ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap32ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 33 && fStmap32StrongPenaltyCandidate && Stmap32StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap32StrongPenaltySeed++;
                    printf( "stmap32 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f\n",
                        p->nStmap32StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap32StrongPenaltyFactor, AreaMargin, Stmap32CutLeafLoadAvg );
                }
                if ( p->fSkipFanout == 34 && fStmap33ModeratePenaltyCandidate && Stmap33ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap33ModeratePenaltySeed++;
                    printf( "stmap33 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap33ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap33ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 34 && fStmap33StrongPenaltyCandidate && Stmap33StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap33StrongPenaltySeed++;
                    printf( "stmap33 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f\n",
                        p->nStmap33StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap33StrongPenaltyFactor, AreaMargin, Stmap33CutLeafLoadAvg,
                        Stmap33FanLimit, Stmap33LoadDriveRatio );
                }
                if ( p->fSkipFanout == 35 && fStmap34ModeratePenaltyCandidate && Stmap34ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap34ModeratePenaltySeed++;
                    printf( "stmap34 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap34ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap34ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 35 && fStmap34StrongPenaltyCandidate && Stmap34StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap34StrongPenaltySeed++;
                    printf( "stmap34 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f\n",
                        p->nStmap34StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap34StrongPenaltyFactor, AreaMargin, Stmap34CutLeafLoadAvg,
                        Stmap34FanLimit, Stmap34LoadDriveRatio );
                }
                if ( p->fSkipFanout == 36 && fStmap35ModeratePenaltyCandidate && Stmap35ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap35ModeratePenaltySeed++;
                    printf( "stmap35 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap35ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap35ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 36 && fStmap35StrongPenaltyCandidate && Stmap35StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap35StrongPenaltySeed++;
                    printf( "stmap35 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f\n",
                        p->nStmap35StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap35StrongPenaltyFactor, AreaMargin, Stmap35CutLeafLoadAvg,
                        Stmap35FanLimit, Stmap35LoadDriveRatio );
                }
                if ( p->fSkipFanout == 37 && fStmap36ModeratePenaltyCandidate && Stmap36ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap36ModeratePenaltySeed++;
                    printf( "stmap36 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap36ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap36ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 37 && fStmap36StrongPenaltyCandidate && Stmap36StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap36StrongPenaltySeed++;
                    printf( "stmap36 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap36StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap36StrongPenaltyFactor, AreaMargin, Stmap36CutLeafLoadAvg,
                        Stmap36FanLimit, Stmap36LoadDriveRatio, s_Stmap36SclFeedback );
                }
                if ( p->fSkipFanout == 38 && fStmap37ModeratePenaltyCandidate && Stmap37ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap37ModeratePenaltySeed++;
                    printf( "stmap37 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap37ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap37ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 38 && fStmap37StrongPenaltyCandidate && Stmap37StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap37StrongPenaltySeed++;
                    printf( "stmap37 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  hotspot-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap37StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap37StrongPenaltyFactor, AreaMargin, Stmap37CutLeafLoadAvg,
                        Stmap37FanLimit, Stmap37LoadDriveRatio, Stmap37HotspotRatio, s_Stmap37SclFeedback );
                }
                if ( p->fSkipFanout == 39 && fStmap38ModeratePenaltyCandidate && Stmap38ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap38ModeratePenaltySeed++;
                    printf( "stmap38 moderate penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap38ModeratePenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap38ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 39 && fStmap38StrongPenaltyCandidate && Stmap38StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap38StrongPenaltySeed++;
                    printf( "stmap38 strong penalty seed diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-hotspot-ratio = %.3f  cut-hotspot-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap38StrongPenaltySeed, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap38StrongPenaltyFactor, AreaMargin, Stmap38CutLeafLoadAvg,
                        Stmap38FanLimit, Stmap38LoadDriveRatio, Stmap38NodeHotspotRatio,
                        Stmap38CutHotspotRatio, s_Stmap38SclFeedback );
                }
                if ( p->fSkipFanout == 40 && fStmap39ModeratePenaltyCandidate && Stmap39ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap39ModeratePenaltySeed++;
                    printf( "stmap39 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap39ModeratePenaltySeed, pNode->Num, Stmap39NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap39ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 40 && fStmap39StrongPenaltyCandidate && Stmap39StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap39StrongPenaltySeed++;
                    printf( "stmap39 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-pressure-ratio = %.3f  cut-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap39StrongPenaltySeed, pNode->Num, Stmap39NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap39StrongPenaltyFactor, AreaMargin, Stmap39CutLeafLoadAvg,
                        Stmap39FanLimit, Stmap39LoadDriveRatio, Stmap39NodePressureRatio,
                        Stmap39CutPressureRatio, s_Stmap39SclFeedback );
                }
                if ( p->fSkipFanout == 41 && fStmap40ModeratePenaltyCandidate && Stmap40ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap40ModeratePenaltySeed++;
                    printf( "stmap40 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap40ModeratePenaltySeed, pNode->Num, Stmap40NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap40ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 41 && fStmap40StrongPenaltyCandidate && Stmap40StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap40StrongPenaltySeed++;
                    printf( "stmap40 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-pressure-ratio = %.3f  cut-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap40StrongPenaltySeed, pNode->Num, Stmap40NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap40StrongPenaltyFactor, AreaMargin, Stmap40CutLeafLoadAvg,
                        Stmap40FanLimit, Stmap40LoadDriveRatio, Stmap40NodePressureRatio,
                        Stmap40CutPressureRatio, s_Stmap40SclFeedback );
                }
                if ( p->fSkipFanout == 42 && fStmap41ModeratePenaltyCandidate && Stmap41ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap41ModeratePenaltySeed++;
                    printf( "stmap41 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap41ModeratePenaltySeed, pNode->Num, Stmap41NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap41ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 42 && fStmap41StrongPenaltyCandidate && Stmap41StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap41StrongPenaltySeed++;
                    printf( "stmap41 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-critical-pressure-ratio = %.3f  cut-critical-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap41StrongPenaltySeed, pNode->Num, Stmap41NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap41StrongPenaltyFactor, AreaMargin, Stmap41CutLeafLoadAvg,
                        Stmap41FanLimit, Stmap41LoadDriveRatio, Stmap41NodePressureRatio,
                        Stmap41CutPressureRatio, s_Stmap41SclFeedback );
                }
                if ( p->fSkipFanout == 43 && fStmap42ModeratePenaltyCandidate && Stmap42ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap42ModeratePenaltySeed++;
                    printf( "stmap42 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap42ModeratePenaltySeed, pNode->Num, Stmap42NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap42ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 43 && fStmap42StrongPenaltyCandidate && Stmap42StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap42StrongPenaltySeed++;
                    printf( "stmap42 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap42StrongPenaltySeed, pNode->Num, Stmap42NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap42StrongPenaltyFactor, AreaMargin, Stmap42CutLeafLoadAvg,
                        Stmap42FanLimit, Stmap42LoadDriveRatio, Stmap42NodePressureRatio,
                        Stmap42CutPressureRatio, s_Stmap42SclFeedback );
                }
                if ( p->fSkipFanout == 44 && fStmap43ModeratePenaltyCandidate && Stmap43ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap43ModeratePenaltySeed++;
                    printf( "stmap43 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap43ModeratePenaltySeed, pNode->Num, Stmap43NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap43ModeratePenaltyFactor, AreaMargin, Stmap43NodePressureRatio,
                        Stmap43CutPressureRatio, s_Stmap43SclFeedback );
                }
                if ( p->fSkipFanout == 44 && fStmap43StrongPenaltyCandidate && Stmap43StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap43StrongPenaltySeed++;
                    printf( "stmap43 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap43StrongPenaltySeed, pNode->Num, Stmap43NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap43StrongPenaltyFactor, AreaMargin, Stmap43CutLeafLoadAvg,
                        Stmap43FanLimit, Stmap43LoadDriveRatio, Stmap43NodePressureRatio,
                        Stmap43CutPressureRatio, s_Stmap43SclFeedback );
                }
                if ( p->fSkipFanout == 45 && fStmap44ModeratePenaltyCandidate && Stmap44ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap44ModeratePenaltySeed++;
                    printf( "stmap44 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap44ModeratePenaltySeed, pNode->Num, Stmap44NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap44ModeratePenaltyFactor, AreaMargin, Stmap44NodePressureRatio,
                        Stmap44CutPressureRatio, s_Stmap44SclFeedback );
                }
                if ( p->fSkipFanout == 45 && fStmap44StrongPenaltyCandidate && Stmap44StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap44StrongPenaltySeed++;
                    printf( "stmap44 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap44StrongPenaltySeed, pNode->Num, Stmap44NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap44StrongPenaltyFactor, AreaMargin, Stmap44CutLeafLoadAvg,
                        Stmap44FanLimit, Stmap44LoadDriveRatio, Stmap44NodePressureRatio,
                        Stmap44CutPressureRatio, s_Stmap44SclFeedback );
                }
                if ( p->fSkipFanout == 46 && fStmap45ModeratePenaltyCandidate && Stmap45ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap45ModeratePenaltySeed++;
                    printf( "stmap45 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap45ModeratePenaltySeed, pNode->Num, Stmap45NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap45ModeratePenaltyFactor, AreaMargin, Stmap45NodePressureRatio,
                        Stmap45CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 46 && fStmap45StrongPenaltyCandidate && Stmap45StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap45StrongPenaltySeed++;
                    printf( "stmap45 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap45StrongPenaltySeed, pNode->Num, Stmap45NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap45StrongPenaltyFactor, AreaMargin, Stmap45CutLeafLoadAvg,
                        Stmap45FanLimit, Stmap45LoadDriveRatio, Stmap45NodePressureRatio,
                        Stmap45CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 47 && fStmap46ModeratePenaltyCandidate && Stmap46ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap46ModeratePenaltySeed++;
                    printf( "stmap46 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap46ModeratePenaltySeed, pNode->Num, Stmap46NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap46ModeratePenaltyFactor, AreaMargin, Stmap46NodePressureRatio,
                        Stmap46CutPressureRatio, s_Stmap46SclFeedback );
                }
                if ( p->fSkipFanout == 47 && fStmap46TightException )
                {
                    p->nStmap46TightExceptionSeed++;
                    printf( "stmap46 tight exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap46TightExceptionSeed, pNode->Num, Stmap46NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap46NodePressureRatio, Stmap46CutPressureRatio, s_Stmap46SclFeedback );
                }
                if ( p->fSkipFanout == 47 && fStmap46StrongPenaltyCandidate && Stmap46StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap46StrongPenaltySeed++;
                    printf( "stmap46 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap46StrongPenaltySeed, pNode->Num, Stmap46NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap46StrongPenaltyFactor, AreaMargin, Stmap46CutLeafLoadAvg,
                        Stmap46FanLimit, Stmap46LoadDriveRatio, Stmap46NodePressureRatio,
                        Stmap46CutPressureRatio, s_Stmap46SclFeedback );
                }
                if ( p->fSkipFanout == 53 && fStmap52ModeratePenaltyCandidate && Stmap52ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap52ModeratePenaltySeed++;
                    printf( "stmap52 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap52ModeratePenaltySeed, pNode->Num, Stmap52NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap52ModeratePenaltyFactor, AreaMargin, Stmap52NodePressureRatio,
                        Stmap52CutPressureRatio, fStmap52ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 53 && fStmap52StrongPenaltyCandidate && Stmap52StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap52StrongPenaltySeed++;
                    printf( "stmap52 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap52StrongPenaltySeed, pNode->Num, Stmap52NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap52StrongPenaltyFactor, AreaMargin, Stmap52CutLeafLoadAvg,
                        Stmap52FanLimit, Stmap52LoadDriveRatio, Stmap52NodePressureRatio,
                        Stmap52CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 54 && fStmap53ModeratePressureNear )
                {
                    p->nStmap53ModerateExceptionSeed++;
                    printf( "stmap53 moderate exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap53ModerateExceptionSeed, pNode->Num, Stmap53NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap53NodePressureRatio, Stmap53CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 54 && fStmap53ModeratePenaltyCandidate && Stmap53ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap53ModeratePenaltySeed++;
                    printf( "stmap53 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap53ModeratePenaltySeed, pNode->Num, Stmap53NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap53ModeratePenaltyFactor, AreaMargin, Stmap53NodePressureRatio,
                        Stmap53CutPressureRatio, fStmap53ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 54 && fStmap53StrongPenaltyCandidate && Stmap53StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap53StrongPenaltySeed++;
                    printf( "stmap53 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap53StrongPenaltySeed, pNode->Num, Stmap53NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap53StrongPenaltyFactor, AreaMargin, Stmap53CutLeafLoadAvg,
                        Stmap53FanLimit, Stmap53LoadDriveRatio, Stmap53NodePressureRatio,
                        Stmap53CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54ModeratePressureNear )
                {
                    p->nStmap54PressureNearExceptionSeed++;
                    printf( "stmap54 pressure-near exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap54PressureNearExceptionSeed, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap54NodePressureRatio, Stmap54CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54CutOnlyPressure )
                {
                    p->nStmap54CutOnlyExceptionSeed++;
                    printf( "stmap54 cut-only exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap54CutOnlyExceptionSeed, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap54NodePressureRatio, Stmap54CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54ModeratePenaltyCandidate && Stmap54ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap54ModeratePenaltySeed++;
                    printf( "stmap54 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap54ModeratePenaltySeed, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap54ModeratePenaltyFactor, AreaMargin, Stmap54NodePressureRatio,
                        Stmap54CutPressureRatio, fStmap54ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54StrongPenaltyCandidate && Stmap54StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap54StrongPenaltySeed++;
                    printf( "stmap54 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap54StrongPenaltySeed, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap54StrongPenaltyFactor, AreaMargin, Stmap54CutLeafLoadAvg,
                        Stmap54FanLimit, Stmap54LoadDriveRatio, Stmap54NodePressureRatio,
                        Stmap54CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55ModeratePressureNear )
                {
                    p->nStmap55PressureNearExceptionSeed++;
                    printf( "stmap55 pressure-near exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap55PressureNearExceptionSeed, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap55NodePressureRatio, Stmap55CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55CutOnlyPressure )
                {
                    p->nStmap55CutOnlyExceptionSeed++;
                    printf( "stmap55 cut-only exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap55CutOnlyExceptionSeed, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, 1.25 * OneInvArea, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap55NodePressureRatio, Stmap55CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55ModeratePenaltyCandidate && Stmap55ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap55ModeratePenaltySeed++;
                    printf( "stmap55 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap55ModeratePenaltySeed, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap55ModeratePenaltyFactor, AreaMargin, Stmap55NodePressureRatio,
                        Stmap55CutPressureRatio, fStmap55ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55StrongPenaltyCandidate && Stmap55StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap55StrongPenaltySeed++;
                    printf( "stmap55 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap55StrongPenaltySeed, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap55StrongPenaltyFactor, AreaMargin, Stmap55CutLeafLoadAvg,
                        Stmap55FanLimit, Stmap55LoadDriveRatio, Stmap55NodePressureRatio,
                        Stmap55CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56ModeratePressureNear )
                {
                    p->nStmap56PressureNearExceptionSeed++;
                    Map_Stmap69RecordPressureNearWitness( pNode, pCut, Stmap56NodeAigId, fPhase, 1, Stmap56NodePressureRatio, Stmap56CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin, AreaMargin );
                    printf( "stmap56 pressure-near exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap56PressureNearExceptionSeed, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56CutOnlyPressure )
                {
                    p->nStmap56CutOnlyExceptionSeed++;
                    Map_Stmap64RecordCutOnlyWitness( pNode, Stmap56NodeAigId, fPhase, Stmap56NodePressureRatio, Stmap56CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
                    printf( "stmap56 cut-only exception seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap56CutOnlyExceptionSeed, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, 1.35 * OneInvArea, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56ModeratePenaltyCandidate && Stmap56ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap56ModeratePenaltySeed++;
                    printf( "stmap56 moderate penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap56ModeratePenaltySeed, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap56ModeratePenaltyFactor, AreaMargin, Stmap56NodePressureRatio,
                        Stmap56CutPressureRatio, fStmap56ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56StrongPenaltyCandidate && Stmap56StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap56StrongPenaltySeed++;
                    printf( "stmap56 strong penalty seed diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap56StrongPenaltySeed, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap56StrongPenaltyFactor, AreaMargin, Stmap56CutLeafLoadAvg,
                        Stmap56FanLimit, Stmap56LoadDriveRatio, Stmap56NodePressureRatio,
                        Stmap56CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && !fStrictFallback &&
                     (fStmap56ModeratePressureAgreement || fStmap56ModeratePressureNear ||
                      fStmap56CutOnlyPressure ||
                      (fStmap56ModeratePenaltyCandidate && Stmap56ModeratePenaltyFactor > 0.0) ||
                      (fStmap56StrongPenaltyCandidate && Stmap56StrongPenaltyFactor > 0.0)) )
                {
                    const char * pStmap73Class = "pressure-agreement";
                    if ( fStmap56CutOnlyPressure )
                        pStmap73Class = "cut-only";
                    else if ( fStmap56ModeratePressureNear )
                        pStmap73Class = "pressure-near";
                    else if ( fStmap56StrongPenaltyCandidate && Stmap56StrongPenaltyFactor > 0.0 )
                        pStmap73Class = "strong-penalty";
                    else if ( fStmap56ModeratePenaltyCandidate && Stmap56ModeratePenaltyFactor > 0.0 )
                        pStmap73Class = fStmap56ModerateAreaCap ? "moderate-area-cap" : "moderate-penalty";
                    Map_Stmap73RecordAcceptedPressureWitness( pNode, pCut, Stmap56NodeAigId, fPhase, pStmap73Class,
                        Stmap56NodePressureRatio, Stmap56CutPressureRatio, Slack, AreaSave, ArrivalDelta,
                        ArrivalGainMargin, Stmap56PenaltyFactor, AreaMargin, fStmap56ModeratePressureAgreement,
                        fStmap56ModeratePressureNear, fStmap56CutOnlyPressure, fStmap56ModerateAreaCap );
                }
                if ( p->fSkipFanout >= 20 && p->fSkipFanout <= 57 && !fProfileOpen && !fStrictFallback )
                    p->nStmap19EarlySeed++;
                return 0;
            }
            if ( p->fSkipFanout >= 19 && p->fSkipFanout <= 57 &&
                 (fStrictFallback || (p->fSkipFanout == 29 && fStmap28ModeratePenaltyCandidate && Stmap28PenaltyFactor > 0.0) || (p->fSkipFanout == 30 && fStmap29ModeratePenaltyCandidate && Stmap29PenaltyFactor > 0.0) || (p->fSkipFanout == 31 && Stmap30PenaltyFactor > 0.0) || (p->fSkipFanout == 32 && Stmap31PenaltyFactor > 0.0) || (p->fSkipFanout == 33 && Stmap32PenaltyFactor > 0.0) || (p->fSkipFanout == 34 && Stmap33PenaltyFactor > 0.0) || (p->fSkipFanout == 35 && Stmap34PenaltyFactor > 0.0) || (p->fSkipFanout == 36 && Stmap35PenaltyFactor > 0.0) || (p->fSkipFanout == 37 && Stmap36PenaltyFactor > 0.0) || (p->fSkipFanout == 38 && Stmap37PenaltyFactor > 0.0) || (p->fSkipFanout == 39 && Stmap38PenaltyFactor > 0.0) || (p->fSkipFanout == 40 && Stmap39PenaltyFactor > 0.0) || (p->fSkipFanout == 41 && Stmap40PenaltyFactor > 0.0) || (p->fSkipFanout == 42 && Stmap41PenaltyFactor > 0.0) || (p->fSkipFanout == 43 && Stmap42PenaltyFactor > 0.0) || (p->fSkipFanout == 44 && Stmap43PenaltyFactor > 0.0) || (p->fSkipFanout == 45 && Stmap44PenaltyFactor > 0.0) || (p->fSkipFanout == 46 && Stmap45PenaltyFactor > 0.0) || (p->fSkipFanout == 47 && Stmap46PenaltyFactor > 0.0) || (p->fSkipFanout == 53 && Stmap52PenaltyFactor > 0.0) || (p->fSkipFanout == 54 && (Stmap53PenaltyFactor > 0.0 || fStmap53ModeratePressureNear)) || (p->fSkipFanout == 55 && (Stmap54PenaltyFactor > 0.0 || fStmap54ModeratePressureNear || fStmap54CutOnlyPressure)) || (p->fSkipFanout == 56 && (Stmap55PenaltyFactor > 0.0 || fStmap55ModeratePressureNear || fStmap55CutOnlyPressure || fStmap55CutOnlyAreaCapBlocked)) || (p->fSkipFanout == 57 && (Stmap56PenaltyFactor > 0.0 || fStmap56ModeratePressureNear || fStmap56CutOnlyPressure || fStmap56CutOnlyAreaCapBlocked))) &&
                 AreaSave > OneInvArea + p->fEpsilon )
            {
                if ( p->fSkipFanout == 19 )
                    pReason = fProfileOpen ? "timing-gate" : "profile-closed";
                else if ( p->fSkipFanout == 20 )
                    pReason = "arrival-gate";
                else if ( p->fSkipFanout == 22 )
                    pReason = fArrivalQuality ? "deep-gain-gate" : "arrival-gate";
                else if ( p->fSkipFanout == 23 )
                    pReason = fArrivalQuality ? "moderate-gain-gate" : "arrival-gate";
                else if ( p->fSkipFanout == 24 )
                    pReason = fArrivalQuality ? (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? "moderate-slack-gate" : "moderate-gain-gate") : "arrival-gate";
                else if ( p->fSkipFanout == 25 )
                    pReason = fArrivalQuality ? (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? "tight-moderate-slack-gate" : "moderate-gain-gate") : "arrival-gate";
                else if ( p->fSkipFanout == 26 )
                    pReason = fArrivalQuality ? (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? "single-ablation-gate" : "moderate-gain-gate") : "arrival-gate";
                else if ( p->fSkipFanout == 27 )
                    pReason = fArrivalQuality ? (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? "rank2-ablation-gate" : "moderate-gain-gate") : "arrival-gate";
                else if ( p->fSkipFanout == 28 )
                    pReason = fArrivalQuality ? (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap27ModerateSignatureBlocked ? "signature-block-gate" : "signature-band-gate") : "moderate-gain-gate") : "arrival-gate";
                else if ( p->fSkipFanout == 29 )
                    pReason = fArrivalQuality ? (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap28ModeratePenaltyCandidate && Stmap28PenaltyFactor > 0.0 ? "soft-penalty-area-gate" : "soft-penalty-band-gate") : "moderate-gain-gate") : "arrival-gate";
                else if ( p->fSkipFanout == 30 )
                    pReason = fArrivalQuality ? (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap29ModeratePenaltyCandidate && Stmap29PenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate") : "arrival-gate";
                else if ( p->fSkipFanout == 31 )
                    pReason = fArrivalQuality ? (fStmap30StrongPenaltyCandidate && Stmap30StrongPenaltyFactor > 0.0 ? "strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap30ModeratePenaltyCandidate && Stmap30ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 32 )
                    pReason = fArrivalQuality ? (fStmap31StrongPenaltyCandidate && Stmap31StrongPenaltyFactor > 0.0 ? "damped-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap31ModeratePenaltyCandidate && Stmap31ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 33 )
                    pReason = fArrivalQuality ? (fStmap32StrongPenaltyCandidate && Stmap32StrongPenaltyFactor > 0.0 ? "cut-leaf-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap32ModeratePenaltyCandidate && Stmap32ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 34 )
                    pReason = fArrivalQuality ? (fStmap33StrongPenaltyCandidate && Stmap33StrongPenaltyFactor > 0.0 ? "drive-normalized-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap33ModeratePenaltyCandidate && Stmap33ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 35 )
                    pReason = fArrivalQuality ? (fStmap34StrongPenaltyCandidate && Stmap34StrongPenaltyFactor > 0.0 ? "bounded-drive-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap34ModeratePenaltyCandidate && Stmap34ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 36 )
                    pReason = fArrivalQuality ? (fStmap35StrongPenaltyCandidate && Stmap35StrongPenaltyFactor > 0.0 ? "scl-load-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap35ModeratePenaltyCandidate && Stmap35ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 37 )
                    pReason = fArrivalQuality ? (fStmap36StrongPenaltyCandidate && Stmap36StrongPenaltyFactor > 0.0 ? "scl-feedback-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap36ModeratePenaltyCandidate && Stmap36ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 38 )
                    pReason = fArrivalQuality ? (fStmap37StrongPenaltyCandidate && Stmap37StrongPenaltyFactor > 0.0 ? "local-hotspot-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap37ModeratePenaltyCandidate && Stmap37ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 39 )
                    pReason = fArrivalQuality ? (fStmap38StrongPenaltyCandidate && Stmap38StrongPenaltyFactor > 0.0 ? "broadened-hotspot-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap38ModeratePenaltyCandidate && Stmap38ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 40 )
                    pReason = fArrivalQuality ? (fStmap39StrongPenaltyCandidate && Stmap39StrongPenaltyFactor > 0.0 ? "consumer-pressure-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap39ModeratePenaltyCandidate && Stmap39ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 41 )
                    pReason = fArrivalQuality ? (fStmap40StrongPenaltyCandidate && Stmap40StrongPenaltyFactor > 0.0 ? "dense-pressure-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap40ModeratePenaltyCandidate && Stmap40ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 42 )
                    pReason = fArrivalQuality ? (fStmap41StrongPenaltyCandidate && Stmap41StrongPenaltyFactor > 0.0 ? "critical-pressure-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap41ModeratePenaltyCandidate && Stmap41ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 43 )
                    pReason = fArrivalQuality ? (fStmap42StrongPenaltyCandidate && Stmap42StrongPenaltyFactor > 0.0 ? "sink-pressure-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap42ModeratePenaltyCandidate && Stmap42ModeratePenaltyFactor > 0.0 ? "continuous-penalty-area-gate" : "continuous-penalty-band-gate") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 44 )
                    pReason = fArrivalQuality ? (fStmap43StrongPenaltyCandidate && Stmap43StrongPenaltyFactor > 0.0 ? "sink-gated-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap43ModeratePenaltyCandidate && Stmap43ModeratePenaltyFactor > 0.0 ? "sink-gated-moderate-area-gate" : "sink-gated-moderate-open") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 45 )
                    pReason = fArrivalQuality ? (fStmap44StrongPenaltyCandidate && Stmap44StrongPenaltyFactor > 0.0 ? "agreement-gated-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap44ModeratePenaltyCandidate && Stmap44ModeratePenaltyFactor > 0.0 ? "agreement-gated-moderate-area-gate" : "agreement-gated-moderate-open") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 46 )
                    pReason = fArrivalQuality ? (fStmap45StrongPenaltyCandidate && Stmap45StrongPenaltyFactor > 0.0 ? "agreement-admitted-strong-penalty-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap45ModeratePenaltyCandidate && Stmap45ModeratePenaltyFactor > 0.0 ? "agreement-admitted-moderate-area-gate" : "agreement-admitted-moderate-block") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 47 )
                    pReason = fArrivalQuality ? (fStmap46StrongPenaltyCandidate && Stmap46StrongPenaltyFactor > 0.0 ? "tight-exception-strong-penalty-area-gate" : (fStmap46TightException ? "tight-exception-area-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap46ModeratePenaltyCandidate && Stmap46ModeratePenaltyFactor > 0.0 ? "pressure-agreed-moderate-area-gate" : "pressure-agreed-moderate-block") : "moderate-gain-gate"))) : "arrival-gate";
                else if ( p->fSkipFanout == 53 )
                    pReason = fArrivalQuality ? (fStmap52StrongPenaltyCandidate && Stmap52StrongPenaltyFactor > 0.0 ? "area-cap-strong-penalty-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap52ModeratePenaltyCandidate && Stmap52ModeratePenaltyFactor > 0.0 ? (fStmap52ModerateAreaCap ? "area-cap-moderate-gate" : "pressure-agreed-moderate-area-gate") : "pressure-agreed-moderate-block") : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 54 )
                    pReason = fArrivalQuality ? (fStmap53StrongPenaltyCandidate && Stmap53StrongPenaltyFactor > 0.0 ? "pressure-exception-strong-penalty-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap53ModeratePressureNear ? "pressure-near-exception-area-gate" : (fStmap53ModeratePenaltyCandidate && Stmap53ModeratePenaltyFactor > 0.0 ? (fStmap53ModerateAreaCap ? "area-cap-moderate-gate" : "pressure-agreed-moderate-area-gate") : "pressure-near-moderate-block")) : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 55 )
                    pReason = fArrivalQuality ? (fStmap54StrongPenaltyCandidate && Stmap54StrongPenaltyFactor > 0.0 ? "cut-only-strong-penalty-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap54ModeratePressureNear ? "pressure-near-exception-area-gate" : (fStmap54CutOnlyPressure ? "cut-only-exception-area-gate" : (fStmap54ModeratePenaltyCandidate && Stmap54ModeratePenaltyFactor > 0.0 ? (fStmap54ModerateAreaCap ? "area-cap-moderate-gate" : "pressure-agreed-moderate-area-gate") : "cut-only-moderate-block"))) : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 56 )
                    pReason = fArrivalQuality ? (fStmap55StrongPenaltyCandidate && Stmap55StrongPenaltyFactor > 0.0 ? "area-capped-cut-only-strong-penalty-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap55ModeratePressureNear ? "pressure-near-exception-area-gate" : (fStmap55CutOnlyAreaCapBlocked ? "cut-only-area-cap-gate" : (fStmap55CutOnlyPressure ? "cut-only-exception-area-gate" : (fStmap55ModeratePenaltyCandidate && Stmap55ModeratePenaltyFactor > 0.0 ? (fStmap55ModerateAreaCap ? "area-cap-moderate-gate" : "pressure-agreed-moderate-area-gate") : "cut-only-moderate-block")))) : "moderate-gain-gate")) : "arrival-gate";
                else if ( p->fSkipFanout == 57 )
                    pReason = fArrivalQuality ? (fStmap56StrongPenaltyCandidate && Stmap56StrongPenaltyFactor > 0.0 ? "relaxed-cap-cut-only-strong-penalty-gate" : (Map_MatchHasStmap22ModerateDeepSeedGain( ArrivalDelta, ArrivalGainMargin, p->fEpsilon ) ? (fStmap56ModeratePressureNear ? "pressure-near-exception-area-gate" : (fStmap56CutOnlyAreaCapBlocked ? "cut-only-area-cap-gate" : (fStmap56CutOnlyPressure ? "cut-only-exception-area-gate" : (fStmap56ModeratePenaltyCandidate && Stmap56ModeratePenaltyFactor > 0.0 ? (fStmap56ModerateAreaCap ? "area-cap-moderate-gate" : "pressure-agreed-moderate-area-gate") : "cut-only-moderate-block")))) : "moderate-gain-gate")) : "arrival-gate";
                else
                    pReason = fArrivalQuality ? "depth-gate" : "arrival-gate";
                if ( p->fSkipFanout == 27 && fStmap26ModerateAblationCandidate && p->nStmap26ModerateAblationSeen == 0 )
                {
                    p->nStmap26ModerateAblationSeen++;
                    printf( "stmap26 ablation skip diag: rank = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f\n",
                        p->nStmap26ModerateAblationSeen, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
                }
                if ( p->fSkipFanout == 28 && fStmap27ModerateSignatureBlocked )
                {
                    p->nStmap27ModerateSignatureBlocked++;
                    printf( "stmap27 signature block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f\n",
                        p->nStmap27ModerateSignatureBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin );
                }
                if ( p->fSkipFanout == 29 && fStmap28ModeratePenaltyCandidate && Stmap28PenaltyFactor > 0.0 )
                {
                    p->nStmap28ModeratePenaltyBlocked++;
                    printf( "stmap28 penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap28ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap28PenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 30 && fStmap29ModeratePenaltyCandidate && Stmap29PenaltyFactor > 0.0 )
                {
                    p->nStmap29ModeratePenaltyBlocked++;
                    printf( "stmap29 penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap29ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap29PenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 31 && fStmap30ModeratePenaltyCandidate && Stmap30ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap30ModeratePenaltyBlocked++;
                    printf( "stmap30 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap30ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap30ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 31 && fStmap30StrongPenaltyCandidate && Stmap30StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap30StrongPenaltyBlocked++;
                    printf( "stmap30 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap30StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap30StrongPenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 32 && fStmap31ModeratePenaltyCandidate && Stmap31ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap31ModeratePenaltyBlocked++;
                    printf( "stmap31 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap31ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap31ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 32 && fStmap31StrongPenaltyCandidate && Stmap31StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap31StrongPenaltyBlocked++;
                    printf( "stmap31 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap31StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap31StrongPenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 33 && fStmap32ModeratePenaltyCandidate && Stmap32ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap32ModeratePenaltyBlocked++;
                    printf( "stmap32 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap32ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap32ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 33 && fStmap32StrongPenaltyCandidate && Stmap32StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap32StrongPenaltyBlocked++;
                    printf( "stmap32 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f\n",
                        p->nStmap32StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap32StrongPenaltyFactor, AreaMargin, Stmap32CutLeafLoadAvg );
                }
                if ( p->fSkipFanout == 34 && fStmap33ModeratePenaltyCandidate && Stmap33ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap33ModeratePenaltyBlocked++;
                    printf( "stmap33 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap33ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap33ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 34 && fStmap33StrongPenaltyCandidate && Stmap33StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap33StrongPenaltyBlocked++;
                    printf( "stmap33 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f\n",
                        p->nStmap33StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap33StrongPenaltyFactor, AreaMargin, Stmap33CutLeafLoadAvg,
                        Stmap33FanLimit, Stmap33LoadDriveRatio );
                }
                if ( p->fSkipFanout == 35 && fStmap34ModeratePenaltyCandidate && Stmap34ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap34ModeratePenaltyBlocked++;
                    printf( "stmap34 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap34ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap34ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 35 && fStmap34StrongPenaltyCandidate && Stmap34StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap34StrongPenaltyBlocked++;
                    printf( "stmap34 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f\n",
                        p->nStmap34StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap34StrongPenaltyFactor, AreaMargin, Stmap34CutLeafLoadAvg,
                        Stmap34FanLimit, Stmap34LoadDriveRatio );
                }
                if ( p->fSkipFanout == 36 && fStmap35ModeratePenaltyCandidate && Stmap35ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap35ModeratePenaltyBlocked++;
                    printf( "stmap35 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap35ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap35ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 36 && fStmap35StrongPenaltyCandidate && Stmap35StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap35StrongPenaltyBlocked++;
                    printf( "stmap35 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f\n",
                        p->nStmap35StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap35StrongPenaltyFactor, AreaMargin, Stmap35CutLeafLoadAvg,
                        Stmap35FanLimit, Stmap35LoadDriveRatio );
                }
                if ( p->fSkipFanout == 37 && fStmap36ModeratePenaltyCandidate && Stmap36ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap36ModeratePenaltyBlocked++;
                    printf( "stmap36 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap36ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap36ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 37 && fStmap36StrongPenaltyCandidate && Stmap36StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap36StrongPenaltyBlocked++;
                    printf( "stmap36 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap36StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap36StrongPenaltyFactor, AreaMargin, Stmap36CutLeafLoadAvg,
                        Stmap36FanLimit, Stmap36LoadDriveRatio, s_Stmap36SclFeedback );
                }
                if ( p->fSkipFanout == 38 && fStmap37ModeratePenaltyCandidate && Stmap37ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap37ModeratePenaltyBlocked++;
                    printf( "stmap37 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap37ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap37ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 38 && fStmap37StrongPenaltyCandidate && Stmap37StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap37StrongPenaltyBlocked++;
                    printf( "stmap37 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  hotspot-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap37StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap37StrongPenaltyFactor, AreaMargin, Stmap37CutLeafLoadAvg,
                        Stmap37FanLimit, Stmap37LoadDriveRatio, Stmap37HotspotRatio, s_Stmap37SclFeedback );
                }
                if ( p->fSkipFanout == 39 && fStmap38ModeratePenaltyCandidate && Stmap38ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap38ModeratePenaltyBlocked++;
                    printf( "stmap38 moderate penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap38ModeratePenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap38ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 39 && fStmap38StrongPenaltyCandidate && Stmap38StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap38StrongPenaltyBlocked++;
                    printf( "stmap38 strong penalty block diag: index = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-hotspot-ratio = %.3f  cut-hotspot-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap38StrongPenaltyBlocked, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap38StrongPenaltyFactor, AreaMargin, Stmap38CutLeafLoadAvg,
                        Stmap38FanLimit, Stmap38LoadDriveRatio, Stmap38NodeHotspotRatio,
                        Stmap38CutHotspotRatio, s_Stmap38SclFeedback );
                }
                if ( p->fSkipFanout == 40 && fStmap39ModeratePenaltyCandidate && Stmap39ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap39ModeratePenaltyBlocked++;
                    printf( "stmap39 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap39ModeratePenaltyBlocked, pNode->Num, Stmap39NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap39ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 40 && fStmap39StrongPenaltyCandidate && Stmap39StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap39StrongPenaltyBlocked++;
                    printf( "stmap39 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-pressure-ratio = %.3f  cut-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap39StrongPenaltyBlocked, pNode->Num, Stmap39NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap39StrongPenaltyFactor, AreaMargin, Stmap39CutLeafLoadAvg,
                        Stmap39FanLimit, Stmap39LoadDriveRatio, Stmap39NodePressureRatio,
                        Stmap39CutPressureRatio, s_Stmap39SclFeedback );
                }
                if ( p->fSkipFanout == 41 && fStmap40ModeratePenaltyCandidate && Stmap40ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap40ModeratePenaltyBlocked++;
                    printf( "stmap40 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap40ModeratePenaltyBlocked, pNode->Num, Stmap40NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap40ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 41 && fStmap40StrongPenaltyCandidate && Stmap40StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap40StrongPenaltyBlocked++;
                    printf( "stmap40 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-pressure-ratio = %.3f  cut-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap40StrongPenaltyBlocked, pNode->Num, Stmap40NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap40StrongPenaltyFactor, AreaMargin, Stmap40CutLeafLoadAvg,
                        Stmap40FanLimit, Stmap40LoadDriveRatio, Stmap40NodePressureRatio,
                        Stmap40CutPressureRatio, s_Stmap40SclFeedback );
                }
                if ( p->fSkipFanout == 42 && fStmap41ModeratePenaltyCandidate && Stmap41ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap41ModeratePenaltyBlocked++;
                    printf( "stmap41 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap41ModeratePenaltyBlocked, pNode->Num, Stmap41NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap41ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 42 && fStmap41StrongPenaltyCandidate && Stmap41StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap41StrongPenaltyBlocked++;
                    printf( "stmap41 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-critical-pressure-ratio = %.3f  cut-critical-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap41StrongPenaltyBlocked, pNode->Num, Stmap41NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap41StrongPenaltyFactor, AreaMargin, Stmap41CutLeafLoadAvg,
                        Stmap41FanLimit, Stmap41LoadDriveRatio, Stmap41NodePressureRatio,
                        Stmap41CutPressureRatio, s_Stmap41SclFeedback );
                }
                if ( p->fSkipFanout == 43 && fStmap42ModeratePenaltyCandidate && Stmap42ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap42ModeratePenaltyBlocked++;
                    printf( "stmap42 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f\n",
                        p->nStmap42ModeratePenaltyBlocked, pNode->Num, Stmap42NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap42ModeratePenaltyFactor, AreaMargin );
                }
                if ( p->fSkipFanout == 43 && fStmap42StrongPenaltyCandidate && Stmap42StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap42StrongPenaltyBlocked++;
                    printf( "stmap42 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap42StrongPenaltyBlocked, pNode->Num, Stmap42NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap42StrongPenaltyFactor, AreaMargin, Stmap42CutLeafLoadAvg,
                        Stmap42FanLimit, Stmap42LoadDriveRatio, Stmap42NodePressureRatio,
                        Stmap42CutPressureRatio, s_Stmap42SclFeedback );
                }
                if ( p->fSkipFanout == 44 && fStmap43ModeratePenaltyCandidate && Stmap43ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap43ModeratePenaltyBlocked++;
                    printf( "stmap43 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap43ModeratePenaltyBlocked, pNode->Num, Stmap43NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap43ModeratePenaltyFactor, AreaMargin, Stmap43NodePressureRatio,
                        Stmap43CutPressureRatio, s_Stmap43SclFeedback );
                }
                if ( p->fSkipFanout == 44 && fStmap43StrongPenaltyCandidate && Stmap43StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap43StrongPenaltyBlocked++;
                    printf( "stmap43 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap43StrongPenaltyBlocked, pNode->Num, Stmap43NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap43StrongPenaltyFactor, AreaMargin, Stmap43CutLeafLoadAvg,
                        Stmap43FanLimit, Stmap43LoadDriveRatio, Stmap43NodePressureRatio,
                        Stmap43CutPressureRatio, s_Stmap43SclFeedback );
                }
                if ( p->fSkipFanout == 45 && fStmap44ModeratePenaltyCandidate && Stmap44ModeratePenaltyFactor > 0.0 )
                {
                    p->nStmap44ModeratePenaltyBlocked++;
                    printf( "stmap44 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap44ModeratePenaltyBlocked, pNode->Num, Stmap44NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap44ModeratePenaltyFactor, AreaMargin, Stmap44NodePressureRatio,
                        Stmap44CutPressureRatio, s_Stmap44SclFeedback );
                }
                if ( p->fSkipFanout == 45 && fStmap44StrongPenaltyCandidate && Stmap44StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap44StrongPenaltyBlocked++;
                    printf( "stmap44 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap44StrongPenaltyBlocked, pNode->Num, Stmap44NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap44StrongPenaltyFactor, AreaMargin, Stmap44CutLeafLoadAvg,
                        Stmap44FanLimit, Stmap44LoadDriveRatio, Stmap44NodePressureRatio,
                        Stmap44CutPressureRatio, s_Stmap44SclFeedback );
                }
                if ( p->fSkipFanout == 46 && fStmap45ModerateSoftSeed )
                {
                    p->nStmap45ModeratePenaltyBlocked++;
                    printf( "stmap45 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap45ModeratePenaltyBlocked, pNode->Num, Stmap45NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap45ModeratePenaltyFactor, AreaMargin, Stmap45NodePressureRatio,
                        Stmap45CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 46 && fStmap45StrongPenaltyCandidate && Stmap45StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap45StrongPenaltyBlocked++;
                    printf( "stmap45 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap45StrongPenaltyBlocked, pNode->Num, Stmap45NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap45StrongPenaltyFactor, AreaMargin, Stmap45CutLeafLoadAvg,
                        Stmap45FanLimit, Stmap45LoadDriveRatio, Stmap45NodePressureRatio,
                        Stmap45CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 47 && fStmap46ModeratePenaltyCandidate )
                {
                    p->nStmap46ModeratePenaltyBlocked++;
                    printf( "stmap46 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap46ModeratePenaltyBlocked, pNode->Num, Stmap46NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap46ModeratePenaltyFactor, AreaMargin, Stmap46NodePressureRatio,
                        Stmap46CutPressureRatio, s_Stmap46SclFeedback );
                }
                if ( p->fSkipFanout == 47 && fStmap46TightException )
                {
                    p->nStmap46TightExceptionBlocked++;
                    printf( "stmap46 tight exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap46TightExceptionBlocked, pNode->Num, Stmap46NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap46NodePressureRatio, Stmap46CutPressureRatio, s_Stmap46SclFeedback );
                }
                if ( p->fSkipFanout == 47 && fStmap46StrongPenaltyCandidate && Stmap46StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap46StrongPenaltyBlocked++;
                    printf( "stmap46 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap46StrongPenaltyBlocked, pNode->Num, Stmap46NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap46StrongPenaltyFactor, AreaMargin, Stmap46CutLeafLoadAvg,
                        Stmap46FanLimit, Stmap46LoadDriveRatio, Stmap46NodePressureRatio,
                        Stmap46CutPressureRatio, s_Stmap46SclFeedback );
                }
                if ( p->fSkipFanout == 53 && fStmap52ModerateSoftSeed )
                {
                    p->nStmap52ModeratePenaltyBlocked++;
                    printf( "stmap52 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap52ModeratePenaltyBlocked, pNode->Num, Stmap52NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap52ModeratePenaltyFactor, AreaMargin, Stmap52NodePressureRatio,
                        Stmap52CutPressureRatio, fStmap52ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 53 && fStmap52StrongPenaltyCandidate && Stmap52StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap52StrongPenaltyBlocked++;
                    printf( "stmap52 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap52StrongPenaltyBlocked, pNode->Num, Stmap52NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap52StrongPenaltyFactor, AreaMargin, Stmap52CutLeafLoadAvg,
                        Stmap52FanLimit, Stmap52LoadDriveRatio, Stmap52NodePressureRatio,
                        Stmap52CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 54 && fStmap53ModeratePressureNear )
                {
                    p->nStmap53ModerateExceptionBlocked++;
                    printf( "stmap53 moderate exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap53ModerateExceptionBlocked, pNode->Num, Stmap53NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap53NodePressureRatio, Stmap53CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 54 && fStmap53ModeratePenaltyCandidate && !fStmap53ModeratePressureNear )
                {
                    p->nStmap53ModeratePenaltyBlocked++;
                    printf( "stmap53 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap53ModeratePenaltyBlocked, pNode->Num, Stmap53NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap53ModeratePenaltyFactor, AreaMargin, Stmap53NodePressureRatio,
                        Stmap53CutPressureRatio, fStmap53ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 54 && fStmap53StrongPenaltyCandidate && Stmap53StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap53StrongPenaltyBlocked++;
                    printf( "stmap53 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap53StrongPenaltyBlocked, pNode->Num, Stmap53NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap53StrongPenaltyFactor, AreaMargin, Stmap53CutLeafLoadAvg,
                        Stmap53FanLimit, Stmap53LoadDriveRatio, Stmap53NodePressureRatio,
                        Stmap53CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54ModeratePressureNear )
                {
                    p->nStmap54PressureNearExceptionBlocked++;
                    printf( "stmap54 pressure-near exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap54PressureNearExceptionBlocked, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap54NodePressureRatio, Stmap54CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54CutOnlyPressure )
                {
                    p->nStmap54CutOnlyExceptionBlocked++;
                    printf( "stmap54 cut-only exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap54CutOnlyExceptionBlocked, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap54NodePressureRatio, Stmap54CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54ModeratePenaltyCandidate && !fStmap54ModeratePressureNear && !fStmap54CutOnlyPressure )
                {
                    p->nStmap54ModeratePenaltyBlocked++;
                    printf( "stmap54 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap54ModeratePenaltyBlocked, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap54ModeratePenaltyFactor, AreaMargin, Stmap54NodePressureRatio,
                        Stmap54CutPressureRatio, fStmap54ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 55 && fStmap54StrongPenaltyCandidate && Stmap54StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap54StrongPenaltyBlocked++;
                    printf( "stmap54 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap54StrongPenaltyBlocked, pNode->Num, Stmap54NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap54StrongPenaltyFactor, AreaMargin, Stmap54CutLeafLoadAvg,
                        Stmap54FanLimit, Stmap54LoadDriveRatio, Stmap54NodePressureRatio,
                        Stmap54CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55ModeratePressureNear )
                {
                    p->nStmap55PressureNearExceptionBlocked++;
                    printf( "stmap55 pressure-near exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap55PressureNearExceptionBlocked, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap55NodePressureRatio, Stmap55CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55CutOnlyAreaCapBlocked )
                {
                    p->nStmap55CutOnlyAreaCapBlocked++;
                    printf( "stmap55 cut-only area-cap block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap55CutOnlyAreaCapBlocked, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, 1.25 * OneInvArea, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap55NodePressureRatio, Stmap55CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55CutOnlyPressure )
                {
                    p->nStmap55CutOnlyExceptionBlocked++;
                    printf( "stmap55 cut-only exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap55CutOnlyExceptionBlocked, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, 1.25 * OneInvArea, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap55NodePressureRatio, Stmap55CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55ModeratePenaltyCandidate && !fStmap55ModeratePressureNear && !fStmap55CutOnlyPressureRaw )
                {
                    p->nStmap55ModeratePenaltyBlocked++;
                    printf( "stmap55 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap55ModeratePenaltyBlocked, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap55ModeratePenaltyFactor, AreaMargin, Stmap55NodePressureRatio,
                        Stmap55CutPressureRatio, fStmap55ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 56 && fStmap55StrongPenaltyCandidate && Stmap55StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap55StrongPenaltyBlocked++;
                    printf( "stmap55 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap55StrongPenaltyBlocked, pNode->Num, Stmap55NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap55StrongPenaltyFactor, AreaMargin, Stmap55CutLeafLoadAvg,
                        Stmap55FanLimit, Stmap55LoadDriveRatio, Stmap55NodePressureRatio,
                        Stmap55CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56ModeratePressureNear )
                {
                    p->nStmap56PressureNearExceptionBlocked++;
                    Map_Stmap69RecordPressureNearWitness( pNode, pCut, Stmap56NodeAigId, fPhase, 0, Stmap56NodePressureRatio, Stmap56CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin, AreaMargin );
                    printf( "stmap56 pressure-near exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap56PressureNearExceptionBlocked, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56CutOnlyAreaCapBlocked )
                {
                    p->nStmap56CutOnlyAreaCapBlocked++;
                    printf( "stmap56 cut-only area-cap block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap56CutOnlyAreaCapBlocked, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, 1.35 * OneInvArea, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56CutOnlyPressure )
                {
                    p->nStmap56CutOnlyExceptionBlocked++;
                    printf( "stmap56 cut-only exception block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  area-save-cap = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap56CutOnlyExceptionBlocked, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, 1.35 * OneInvArea, ArrivalDelta, ArrivalGainMargin,
                        AreaMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56ModeratePenaltyCandidate && !fStmap56ModeratePressureNear && !fStmap56CutOnlyPressureRaw )
                {
                    p->nStmap56ModeratePenaltyBlocked++;
                    Map_Stmap71RecordModeratePenaltyWitness( pNode, pCut, Stmap56NodeAigId, fPhase, Stmap56NodePressureRatio, Stmap56CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin, Stmap56ModeratePenaltyFactor, AreaMargin, fStmap56ModerateAreaCap );
                    printf( "stmap56 moderate penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  area-cap = %d  scl-feedback = %.3f\n",
                        p->nStmap56ModeratePenaltyBlocked, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap56ModeratePenaltyFactor, AreaMargin, Stmap56NodePressureRatio,
                        Stmap56CutPressureRatio, fStmap56ModerateAreaCap, s_Stmap45SclFeedback );
                }
                if ( p->fSkipFanout == 57 && fStmap56StrongPenaltyCandidate && Stmap56StrongPenaltyFactor > 0.0 )
                {
                    p->nStmap56StrongPenaltyBlocked++;
                    Map_Stmap68RecordBlockedStrongWitness( pNode, pCut, Stmap56NodeAigId, fPhase, Stmap56NodePressureRatio, Stmap56CutPressureRatio, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin, Stmap56StrongPenaltyFactor, AreaMargin, Stmap56CutLeafLoadAvg, Stmap56FanLimit, Stmap56LoadDriveRatio );
                    printf( "stmap56 strong penalty block diag: index = %d  node = %d  aig-id = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  penalty-factor = %.3f  area-margin = %.6f  cut-leaf-load-avg = %.3f  fanout-limit = %d  load-drive-ratio = %.3f  node-sink-pressure-ratio = %.3f  cut-sink-pressure-ratio = %.3f  scl-feedback = %.3f\n",
                        p->nStmap56StrongPenaltyBlocked, pNode->Num, Stmap56NodeAigId, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin,
                        Stmap56StrongPenaltyFactor, AreaMargin, Stmap56CutLeafLoadAvg,
                        Stmap56FanLimit, Stmap56LoadDriveRatio, Stmap56NodePressureRatio,
                        Stmap56CutPressureRatio, s_Stmap45SclFeedback );
                }
                p->nStmap18NearMiss++;
                if ( p->fSkipFanout == 57 )
                    Map_MatchStmap61PrintCutOnlyGateDiag( p, pNode, pCut, fPhase, p->nStmap18NearMiss, pReason, fProfileOpen, fStmap56ModerateSoftSeed, fStmap56ModeratePenaltyCandidate, fStmap56ModeratePressureAgreement, fStmap56ModeratePressureNear, fStmap56CutOnlyPressureRaw, fStmap56CutOnlyAreaCapBlocked, fStmap56CutOnlyPressure, Slack, SlackMargin, AreaSave, OneInvArea, ArrivalDelta, ArrivalGainMargin, AreaMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio );
                if ( p->fSkipFanout == 57 )
                    Map_MatchStmap60PrintNearMissLeafDiag( p, pNode, pCut, fPhase, p->nStmap18NearMiss, pReason, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin, AreaMargin, Stmap56NodePressureRatio, Stmap56CutPressureRatio );
                if ( p->nStmap18NearMiss <= 128 )
                    printf( "stmap%d near-miss diag: index = %d  reason = %s  profile-open = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  arrival-gain-margin = %.6f  area-margin = %.6f  one-inv-area = %.6f\n",
                        p->fSkipFanout - 1, p->nStmap18NearMiss, pReason,
                        fProfileOpen, pNode->Num, pNode->Level, pNode->nRefs, pCut->nLeaves,
                        fPhase, Slack, AreaSave, ArrivalDelta, ArrivalGainMargin, AreaMargin, OneInvArea );
            }
            if ( fStmap13 )
                p->nStmap13RejectArea++;
            return 1;
        }
        if ( Slack <= SlackGate + p->fEpsilon )
        {
            if ( fStmap13 )
                p->nStmap13RejectSlack++;
            return 1;
        }
        if ( Map_MatchCutHasStmapHighestFanoutRisk( pNode, pCut ) )
        {
            if ( fStmap13 )
                p->nStmap13RejectHighest++;
            return 1;
        }
    }
    if ( Slack <= SlackMargin + p->fEpsilon )
        return 0;
    if ( p->fSkipFanout >= 9 && p->fSkipFanout <= 57 )
    {
        ArrivalMargin = 0.25 * SlackMargin;
        if ( pMatch->tArrive.Worst > pMatchBest->tArrive.Worst + ArrivalMargin + p->fEpsilon )
        {
            if ( fStmap13 )
                p->nStmap13RejectArrival++;
            return 1;
        }
    }
    AreaMargin = p->pSuperLib ? 0.5 * p->pSuperLib->AreaInv : 0.0;
    if ( pMatch->AreaFlow < pMatchBest->AreaFlow - AreaMargin - p->fEpsilon )
        return 0;
    if ( fStmap13 )
        p->nStmap13RejectArea++;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Compares two matches.]

  Description [Returns 1 if the second match is better. Otherwise returns 0.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Map_MatchCompare( Map_Man_t * pMan, Map_Match_t * pM1, Map_Match_t * pM2, int fDoingArea )
{
//    if ( pM1->pSuperBest == pM2->pSuperBest )
//        return 0;
    if ( !fDoingArea )
    {
        // compare the arrival times
        if ( pM1->tArrive.Worst < pM2->tArrive.Worst - pMan->fEpsilon )
            return 0;
        if ( pM1->tArrive.Worst > pM2->tArrive.Worst + pMan->fEpsilon )
            return 1;
        // compare the areas or area flows
        if ( pM1->AreaFlow < pM2->AreaFlow - pMan->fEpsilon )
            return 0;
        if ( pM1->AreaFlow > pM2->AreaFlow + pMan->fEpsilon )
            return 1;
        // compare the fanout limits
        if ( pM1->pSuperBest->nFanLimit > pM2->pSuperBest->nFanLimit )
            return 0;
        if ( pM1->pSuperBest->nFanLimit < pM2->pSuperBest->nFanLimit )
            return 1;
        // compare the number of leaves
        if ( pM1->pSuperBest->nFanins < pM2->pSuperBest->nFanins )
            return 0;
        if ( pM1->pSuperBest->nFanins > pM2->pSuperBest->nFanins )
            return 1;
        // otherwise prefer the old cut
        return 0;
    }
    else
    {
        // compare the areas or area flows
        if ( pM1->AreaFlow < pM2->AreaFlow - pMan->fEpsilon )
            return 0;
        if ( pM1->AreaFlow > pM2->AreaFlow + pMan->fEpsilon )
            return 1;

        // make decision based on cell profile
        if ( pMan->fUseProfile && pM1->pSuperBest && pM1->pSuperBest )
        {
            int M1req = Mio_GateReadProfile(pM1->pSuperBest->pRoot);
            int M2req = Mio_GateReadProfile(pM2->pSuperBest->pRoot);
            int M1act = Mio_GateReadProfile2(pM1->pSuperBest->pRoot);
            int M2act = Mio_GateReadProfile2(pM2->pSuperBest->pRoot);
            //printf( "%d %d  ", M1req, M2req );
            if ( M1act < M1req && M2act > M2req )
                return 0;
            if ( M2act < M2req && M1act > M1req )
                return 1;
        }

        // compare the arrival times
        if ( pM1->tArrive.Worst < pM2->tArrive.Worst - pMan->fEpsilon )
            return 0;
        if ( pM1->tArrive.Worst > pM2->tArrive.Worst + pMan->fEpsilon )
            return 1;
        // compare the fanout limits
        if ( pM1->pSuperBest->nFanLimit > pM2->pSuperBest->nFanLimit )
            return 0;
        if ( pM1->pSuperBest->nFanLimit < pM2->pSuperBest->nFanLimit )
            return 1;
        // compare the number of leaves
        if ( pM1->pSuperBest->nFanins < pM2->pSuperBest->nFanins )
            return 0;
        if ( pM1->pSuperBest->nFanins > pM2->pSuperBest->nFanins )
            return 1;
        // otherwise prefer the old cut
        return 0;
    }
}

/**Function*************************************************************

  Synopsis    [Find the best matching of the cut.]

  Description [The parameters: the node (pNode), the cut (pCut), the phase to be matched 
  (fPhase), and the upper bound on the arrival times of the cut (fWorstLimit). This 
  procedure goes through the matching supergates up to the phase assignment, and selects the
  best supergate, which will be used to map the cut. As a result of calling this procedure
  the matching information is written into pMatch.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Map_MatchNodeCut( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, float fWorstLimit )
{
    Map_Match_t MatchBest, * pMatch = pCut->M + fPhase;
    Map_Super_t * pSuper;
    int i, Counter;

    // save the current match of the cut
    MatchBest = *pMatch;
    // go through the supergates
    for ( pSuper = pMatch->pSupers, Counter = 0; pSuper; pSuper = pSuper->pNext, Counter++ )
    {
        p->nMatches++;
        // this is an attempt to reduce the runtime of matching and area 
        // at the cost of rare and very minor increase in delay
        // (the supergates are sorted by increasing area)
        if ( Counter == 30 )
           break;

        // go through different phases of the given match and supergate
        pMatch->pSuperBest = pSuper;
        for ( i = 0; i < (int)pSuper->nPhases; i++ )
        {
            p->nPhases++;
            // find the overall phase of this match
            pMatch->uPhaseBest = pMatch->uPhase ^ pSuper->uPhases[i];
            if ( p->fMappingMode == 0 )
            {
                // get the arrival time
                Map_TimeCutComputeArrival( pNode, pCut, fPhase, fWorstLimit );
                // skip the cut if the arrival times exceed the required times
                if ( pMatch->tArrive.Worst > fWorstLimit + p->fEpsilon )
                    continue;
                // get the area (area flow)
                pMatch->AreaFlow = Map_CutGetAreaFlow( pCut, fPhase );
            }
            else
            {
                // get the area (area flow)
                if ( p->fMappingMode == 2 || p->fMappingMode == 3 )
                    pMatch->AreaFlow = Map_CutGetAreaDerefed( pCut, fPhase );
                else if ( p->fMappingMode == 4 )
                    pMatch->AreaFlow = Map_SwitchCutGetDerefed( pNode, pCut, fPhase );
                else 
                    pMatch->AreaFlow = Map_CutGetAreaFlow( pCut, fPhase );
                // skip if the cut is too large
                if ( pMatch->AreaFlow > MatchBest.AreaFlow + p->fEpsilon )
                    continue;
                // get the arrival time
                Map_TimeCutComputeArrival( pNode, pCut, fPhase, fWorstLimit );
                // skip the cut if the arrival times exceed the required times
                if ( pMatch->tArrive.Worst > fWorstLimit + p->fEpsilon )
                    continue;
            }

            // if the cut is non-trivial, compare it
            if ( Map_MatchCompare( p, &MatchBest, pMatch, p->fMappingMode ) )
            {
                MatchBest = *pMatch;
                // if we are mapping for delay, the worst-case limit should be reduced
                if ( p->fMappingMode == 0 )
                    fWorstLimit = MatchBest.tArrive.Worst;
            }
        }
    }
    // set the best match
    *pMatch = MatchBest;

    // recompute the arrival time and area (area flow) of this cut
    if ( pMatch->pSuperBest )
    {
        Map_TimeCutComputeArrival( pNode, pCut, fPhase, MAP_FLOAT_LARGE );
        if ( p->fMappingMode == 2 || p->fMappingMode == 3 )
            pMatch->AreaFlow = Map_CutGetAreaDerefed( pCut, fPhase );
        else if ( p->fMappingMode == 4 )
            pMatch->AreaFlow = Map_SwitchCutGetDerefed( pNode, pCut, fPhase );
        else 
            pMatch->AreaFlow = Map_CutGetAreaFlow( pCut, fPhase );
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Find the matching of one polarity of the node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Map_MatchNodePhase( Map_Man_t * p, Map_Node_t * pNode, int fPhase )
{
    Map_Match_t MatchBest, Stmap82StickyMatch, * pMatch;
    Map_Cut_t * pCut, * pCutBest, * pStmap82StickyCut = NULL;
    float Area1 = 0.0; // Suppress "might be used uninitialized
    float Area2, fWorstLimit;
    int CutOrdinal, fSkipFanout, fAreaSkip, fAccepted, fStmap82Sticky = 0, Stmap82StickyCutOrdinal = -1;

    // skip the cuts that have been unassigned during area recovery
    pCutBest = pNode->pCutBest[fPhase];
    if ( p->fMappingMode != 0 && pCutBest == NULL )
        return 1;

    // recompute the arrival times of the current best match 
    // because the arrival times of the fanins may have changed 
    // as a result of remapping fanins in the topological order
    if ( p->fMappingMode != 0 )
    {
        Map_TimeCutComputeArrival( pNode, pCutBest, fPhase, MAP_FLOAT_LARGE );
        // make sure that the required times are met
//        assert( pCutBest->M[fPhase].tArrive.Rise < pNode->tRequired[fPhase].Rise + p->fEpsilon );
//        assert( pCutBest->M[fPhase].tArrive.Fall < pNode->tRequired[fPhase].Fall + p->fEpsilon );
    }

    // recompute the exact area of the current best match
    // because the exact area of the fanins may have changed
    // as a result of remapping fanins in the topological order
    if ( p->fMappingMode == 2 || p->fMappingMode == 3 )
    {
        pMatch = pCutBest->M + fPhase;
        if ( pNode->nRefAct[fPhase] > 0 || 
            (pNode->pCutBest[!fPhase] == NULL && pNode->nRefAct[!fPhase] > 0) )
            pMatch->AreaFlow = Area1 = Map_CutDeref( pCutBest, fPhase, p->fUseProfile );
        else
            pMatch->AreaFlow = Area1 = Map_CutGetAreaDerefed( pCutBest, fPhase );
    }
    else if ( p->fMappingMode == 4 )
    {
        pMatch = pCutBest->M + fPhase;
        if ( pNode->nRefAct[fPhase] > 0 || 
            (pNode->pCutBest[!fPhase] == NULL && pNode->nRefAct[!fPhase] > 0) )
            pMatch->AreaFlow = Area1 = Map_SwitchCutDeref( pNode, pCutBest, fPhase );
        else
            pMatch->AreaFlow = Area1 = Map_SwitchCutGetDerefed( pNode, pCutBest, fPhase );
    }

    // save the old mapping
    if ( pCutBest )
        MatchBest = pCutBest->M[fPhase];
    else
        Map_MatchClean( &MatchBest );
    Map_MatchClean( &Stmap82StickyMatch );
    Map_Stmap82RememberStickyParentPhase( p, pNode, pCutBest, fPhase, -1, &MatchBest, pCutBest != NULL, &fStmap82Sticky, &Stmap82StickyMatch, &pStmap82StickyCut, &Stmap82StickyCutOrdinal );
 
    // select the new best cut
    fWorstLimit = pNode->tRequired[fPhase].Worst;
    for ( pCut = pNode->pCuts->pNext, CutOrdinal = 0; pCut; pCut = pCut->pNext, CutOrdinal++ )
    {
        // limit gate sizes based on fanout count
        fSkipFanout = Map_MatchSkipCutForFanout( p, pNode, pCut, fPhase );
        if ( fSkipFanout )
        {
            Map_Stmap80RecordCandidateCut( p, pNode, pCut, fPhase, CutOrdinal, pCut->M + fPhase, &MatchBest, MAP_STMAP80_REASON_SKIP_FANOUT, 0, 0 );
            continue;
        }
        pMatch = pCut->M + fPhase;
        if ( pMatch->pSupers == NULL )
        {
            Map_Stmap80RecordCandidateCut( p, pNode, pCut, fPhase, CutOrdinal, pMatch, &MatchBest, MAP_STMAP80_REASON_NO_SUPERS, 0, 0 );
            continue;
        }

        // find the matches for the cut
        Map_MatchNodeCut( p, pNode, pCut, fPhase, fWorstLimit );
        if ( pMatch->pSuperBest == NULL || pMatch->tArrive.Worst > fWorstLimit + p->fEpsilon )
        {
            Map_Stmap80RecordCandidateCut( p, pNode, pCut, fPhase, CutOrdinal, pMatch, &MatchBest, pMatch->pSuperBest == NULL ? MAP_STMAP80_REASON_NO_SUPER_BEST : MAP_STMAP80_REASON_REQUIRED, 0, 0 );
            continue;
        }
        fAreaSkip = Map_MatchSkipAreaSensitiveFanout( p, pNode, pCut, fPhase, &MatchBest, pMatch );
        if ( fAreaSkip )
        {
            Map_Stmap80RecordCandidateCut( p, pNode, pCut, fPhase, CutOrdinal, pMatch, &MatchBest, MAP_STMAP80_REASON_AREA_SENSITIVE, 0, 0 );
            continue;
        }

        // if the cut can be matched compare the matchings
        fAccepted = Map_MatchCompare( p, &MatchBest, pMatch, p->fMappingMode );
        fAccepted = Map_Stmap81MaybeBiasParentPhase( p, pNode, pCut, fPhase, CutOrdinal, pMatch, &MatchBest, fAccepted );
        fAccepted = Map_Stmap87MaybeTargetParentPhase( p, pNode, pCut, fPhase, CutOrdinal, pMatch, &MatchBest, fAccepted );
        fAccepted = Map_Stmap88MaybeTargetChildPhase( p, pNode, fPhase, CutOrdinal, pMatch, &MatchBest, fAccepted );
        fAccepted = Map_Stmap82MaybeBlockStickyReplacement( p, pNode, pCut, fPhase, CutOrdinal, pMatch, fAccepted, fStmap82Sticky, &Stmap82StickyMatch, pStmap82StickyCut, Stmap82StickyCutOrdinal );
        Map_Stmap80RecordCandidateCut( p, pNode, pCut, fPhase, CutOrdinal, pMatch, &MatchBest, fAccepted ? MAP_STMAP80_REASON_ACCEPTED_BEST : MAP_STMAP80_REASON_NONSELECTED, fAccepted, 1 );
        if ( fAccepted )
        {
            pCutBest  =  pCut;
            MatchBest = *pMatch;
            Map_Stmap82RememberStickyParentPhase( p, pNode, pCut, fPhase, CutOrdinal, pMatch, fAccepted, &fStmap82Sticky, &Stmap82StickyMatch, &pStmap82StickyCut, &Stmap82StickyCutOrdinal );
            // if we are mapping for delay, the worst-case limit should be tightened
            if ( p->fMappingMode == 0 )
                fWorstLimit = MatchBest.tArrive.Worst;
        }
    }

    if ( pCutBest == NULL )
        return 1;

    // set the new mapping
    pNode->pCutBest[fPhase] = pCutBest;
    pCutBest->M[fPhase]     = MatchBest;

    // reference the new cut if it used
    if ( p->fMappingMode >= 2 && 
         (pNode->nRefAct[fPhase] > 0 || 
         (pNode->pCutBest[!fPhase] == NULL && pNode->nRefAct[!fPhase] > 0)) )
    {
        if ( p->fMappingMode == 2 || p->fMappingMode == 3 )
            Area2 = Map_CutRef( pNode->pCutBest[fPhase], fPhase, p->fUseProfile );
        else if ( p->fMappingMode == 4 )
            Area2 = Map_SwitchCutRef( pNode, pNode->pCutBest[fPhase], fPhase );
        else 
            assert( 0 );
//        assert( Area2 < Area1 + p->fEpsilon );
    }

    // make sure that the requited times are met
//    assert( MatchBest.tArrive.Rise < pNode->tRequired[fPhase].Rise + p->fEpsilon );
//    assert( MatchBest.tArrive.Fall < pNode->tRequired[fPhase].Fall + p->fEpsilon );
    return 1;
}

/**Function*************************************************************

  Synopsis    [Sets the PI arrival times.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Map_MappingSetPiArrivalTimes( Map_Man_t * p )
{
    Map_Node_t * pNode;
    int i;
    for ( i = 0; i < p->nInputs; i++ )
    {
        pNode = p->pInputs[i];
        // set the arrival time of the positive phase
        if ( Scl_ConIsRunning() )
        {
            float Time = Scl_ConGetInArrFloat( i );
            pNode->tArrival[1].Fall  = Time;
            pNode->tArrival[1].Rise  = Time;
            pNode->tArrival[1].Worst = Time;
        }
        else
            pNode->tArrival[1] = p->pInputArrivals[i];
        pNode->tArrival[1].Rise  += p->pNodeDelays ? p->pNodeDelays[pNode->Num] : 0;
        pNode->tArrival[1].Fall  += p->pNodeDelays ? p->pNodeDelays[pNode->Num] : 0;
        pNode->tArrival[1].Worst += p->pNodeDelays ? p->pNodeDelays[pNode->Num] : 0;
        // set the arrival time of the negative phase
        pNode->tArrival[0].Rise  = pNode->tArrival[1].Fall + p->pSuperLib->tDelayInv.Rise;
        pNode->tArrival[0].Fall  = pNode->tArrival[1].Rise + p->pSuperLib->tDelayInv.Fall;
        pNode->tArrival[0].Worst = MAP_MAX(pNode->tArrival[0].Rise, pNode->tArrival[0].Fall);
    }
}

/**function*************************************************************

  synopsis    [Computes the exact area associated with the cut.]

  description []
               
  sideeffects []

  seealso     []

***********************************************************************/
float Map_TimeMatchWithInverter( Map_Man_t * p, Map_Match_t * pMatch )
{
    Map_Time_t tArrInv;
    tArrInv.Fall  = pMatch->tArrive.Rise + p->pSuperLib->tDelayInv.Fall;
    tArrInv.Rise  = pMatch->tArrive.Fall + p->pSuperLib->tDelayInv.Rise;
    tArrInv.Worst = MAP_MAX( tArrInv.Rise, tArrInv.Fall ); 
    return tArrInv.Worst;
}

/**Function*************************************************************

  Synopsis    [Attempts dropping one phase of the node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Map_NodeTryDroppingOnePhase( Map_Man_t * p, Map_Node_t * pNode )
{
    Map_Match_t * pMatchBest0, * pMatchBest1;
    float tWorst0Using1, tWorst1Using0; 
    int fUsePhase1, fUsePhase0;

    // nothing to do if one of the phases is already dropped
    if ( pNode->pCutBest[0] == NULL || pNode->pCutBest[1] == NULL )
        return;

    // do not drop while recovering area flow
    if ( p->fMappingMode == 1 )//|| p->fMappingMode == 2 )
        return;

    // get the pointers to the matches of the best cuts
    pMatchBest0 = pNode->pCutBest[0]->M + 0;
    pMatchBest1 = pNode->pCutBest[1]->M + 1;

    // get the worst arrival times of each phase
    // implemented using the other phase with inverter added
    tWorst0Using1 = Map_TimeMatchWithInverter( p, pMatchBest1 );
    tWorst1Using0 = Map_TimeMatchWithInverter( p, pMatchBest0 );

    // consider the case of mapping for delay
    if ( p->fMappingMode == 0 && p->DelayTarget < ABC_INFINITY )
    { 
        // if the arrival time of a phase is larger than the arrival time 
        // of the opposite phase plus the inverter, drop this phase
        if ( pMatchBest0->tArrive.Worst > tWorst0Using1 + p->fEpsilon ) 
            pNode->pCutBest[0] = NULL;
        else if ( pMatchBest1->tArrive.Worst > tWorst1Using0 + p->fEpsilon ) 
            pNode->pCutBest[1] = NULL;
        return;
    }

    // do not perform replacement if one of the phases is unused
    if ( pNode->nRefAct[0] == 0 || pNode->nRefAct[1] == 0 )
        return;
 
    // check if replacement of each phase is possible using required times
    fUsePhase0 = fUsePhase1 = 0;
    if ( p->fMappingMode == 2 )
    {
        fUsePhase0 = (pNode->tRequired[1].Worst > tWorst1Using0 + 3*p->pSuperLib->tDelayInv.Worst + p->fEpsilon);
        fUsePhase1 = (pNode->tRequired[0].Worst > tWorst0Using1 + 3*p->pSuperLib->tDelayInv.Worst + p->fEpsilon);
    }
    else if ( p->fMappingMode == 3 || p->fMappingMode == 4 )
    {
        fUsePhase0 = (pNode->tRequired[1].Worst > tWorst1Using0 + p->fEpsilon);
        fUsePhase1 = (pNode->tRequired[0].Worst > tWorst0Using1 + p->fEpsilon);
    }
    if ( !fUsePhase0 && !fUsePhase1 )
        return;

    // if replacement is possible both ways, use the one that works better
    if ( fUsePhase0 && fUsePhase1 )
    {
        if ( pMatchBest0->AreaFlow < pMatchBest1->AreaFlow )
            fUsePhase1 = 0;
        else
            fUsePhase0 = 0;
    }
    // only one phase should be used
    assert( fUsePhase0 ^ fUsePhase1 );

    // set the corresponding cut to NULL
    if ( fUsePhase0 )
    {
        // deref phase 1 cut if necessary
        if ( p->fMappingMode >= 2 && pNode->nRefAct[1] > 0 )
            Map_CutDeref( pNode->pCutBest[1], 1, p->fUseProfile );
        // get rid of the cut
        pNode->pCutBest[1] = NULL;
        // ref phase 0 cut if necessary
        if ( p->fMappingMode >= 2 && pNode->nRefAct[0] == 0 )
            Map_CutRef( pNode->pCutBest[0], 0, p->fUseProfile );
    }
    else
    {
        // deref phase 0 cut if necessary
        if ( p->fMappingMode >= 2 && pNode->nRefAct[0] > 0 )
            Map_CutDeref( pNode->pCutBest[0], 0, p->fUseProfile );
        // get rid of the cut
        pNode->pCutBest[0] = NULL;
        // ref phase 1 cut if necessary
        if ( p->fMappingMode >= 2 && pNode->nRefAct[1] == 0 )
            Map_CutRef( pNode->pCutBest[1], 1, p->fUseProfile );
    }
}


/**Function*************************************************************

  Synopsis    [Transfers the arrival times from the best cuts to the node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Map_NodeTransferArrivalTimes( Map_Man_t * p, Map_Node_t * pNode )
{
    // if both phases are available, set their arrival times
    if ( pNode->pCutBest[0] && pNode->pCutBest[1] )
    {
        pNode->tArrival[0] = pNode->pCutBest[0]->M[0].tArrive;
        pNode->tArrival[1] = pNode->pCutBest[1]->M[1].tArrive;
    }
    // if only one phase is available, compute the arrival time of other phase
    else if ( pNode->pCutBest[0] )
    {
        pNode->tArrival[0] = pNode->pCutBest[0]->M[0].tArrive;
        pNode->tArrival[1].Rise  = pNode->tArrival[0].Fall + p->pSuperLib->tDelayInv.Rise;
        pNode->tArrival[1].Fall  = pNode->tArrival[0].Rise + p->pSuperLib->tDelayInv.Fall;
        pNode->tArrival[1].Worst = MAP_MAX(pNode->tArrival[1].Rise, pNode->tArrival[1].Fall);
    }
    else if ( pNode->pCutBest[1] )
    {
        pNode->tArrival[1] = pNode->pCutBest[1]->M[1].tArrive;
        pNode->tArrival[0].Rise  = pNode->tArrival[1].Fall + p->pSuperLib->tDelayInv.Rise;
        pNode->tArrival[0].Fall  = pNode->tArrival[1].Rise + p->pSuperLib->tDelayInv.Fall;
        pNode->tArrival[0].Worst = MAP_MAX(pNode->tArrival[0].Rise, pNode->tArrival[0].Fall);
    }
    else 
    {
        assert( 0 );
    }

//    assert( pNode->tArrival[0].Rise < pNode->tRequired[0].Rise + p->fEpsilon );
//    assert( pNode->tArrival[0].Fall < pNode->tRequired[0].Fall + p->fEpsilon );

//    assert( pNode->tArrival[1].Rise < pNode->tRequired[1].Rise + p->fEpsilon );
//    assert( pNode->tArrival[1].Fall < pNode->tRequired[1].Fall + p->fEpsilon );
}

/**Function*************************************************************

  Synopsis    [Computes the best matches of the nodes.]

  Description [Uses parameter p->fMappingMode to decide how to assign
  the matches for both polarities of the node. While the matches are 
  being assigned, one of them may turn out to be better than the other 
  (in terms of delay, for example). In this case, the worse match can 
  be permanently dropped, and the corresponding pointer set to NULL.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Map_MappingMatches( Map_Man_t * p )
{
    ProgressBar * pProgress;
    Map_Node_t * pNode;
    int i;

    assert( p->fMappingMode >= 0 && p->fMappingMode <= 4 );

    // use the externally given PI arrival times
    if ( p->fMappingMode == 0 )
        Map_MappingSetPiArrivalTimes( p );

    // estimate the fanouts
    if ( p->fMappingMode == 0 )
        Map_MappingEstimateRefsInit( p );
    else if ( p->fMappingMode == 1 )
        Map_MappingEstimateRefs( p );

    // the PI cuts are matched in the cut computation package
    // in the loop below we match the internal nodes
    pProgress = Extra_ProgressBarStart( stdout, p->vMapObjs->nSize );
    for ( i = 0; i < p->vMapObjs->nSize; i++ )
    {
        pNode = p->vMapObjs->pArray[i];
        if ( Map_NodeIsBuf(pNode) )
        {
            assert( pNode->p2 == NULL );
            pNode->tArrival[0] = Map_Regular(pNode->p1)->tArrival[ Map_IsComplement(pNode->p1)];
            pNode->tArrival[1] = Map_Regular(pNode->p1)->tArrival[!Map_IsComplement(pNode->p1)];
            continue;
        }

        // skip primary inputs and secondary nodes if mapping with choices
        if ( !Map_NodeIsAnd( pNode ) || pNode->pRepr )
            continue;

        // make sure that at least one non-trival cut is present
        if ( pNode->pCuts->pNext == NULL )
        {
            Extra_ProgressBarStop( pProgress );
            printf( "\nError: A node in the mapping graph does not have feasible cuts.\n" );
            return 0;
        }

        // match negative phase
        if ( !Map_MatchNodePhase( p, pNode, 0 ) )
        {
            Extra_ProgressBarStop( pProgress );
            return 0;
        }
        // match positive phase
        if ( !Map_MatchNodePhase( p, pNode, 1 ) )
        {
            Extra_ProgressBarStop( pProgress );
            return 0;
        }

        // make sure that at least one phase is mapped
        if ( pNode->pCutBest[0] == NULL && pNode->pCutBest[1] == NULL )
        {
            printf( "\nError: Could not match both phases of AIG node %d.\n", pNode->Num );
            printf( "Please make sure that the supergate library has equivalents of AND2 or NAND2.\n" );
            printf( "If such supergates exist in the library, report a bug.\n" );
            Extra_ProgressBarStop( pProgress );
            return 0;
        }

        // if both phases are assigned, check if one of them can be dropped
        Map_NodeTryDroppingOnePhase( p, pNode );
        // set the arrival times of the node using the best cuts
        Map_NodeTransferArrivalTimes( p, pNode );
        // command-scoped stmap75 diagnostics; no mapper decisions depend on this.
        Map_Stmap75PrintSelectedMatches( p, pNode );

        // update the progress bar
        Extra_ProgressBarUpdate( pProgress, i, "Matches ..." );
    }
    Extra_ProgressBarStop( pProgress );
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
ABC_NAMESPACE_IMPL_END
