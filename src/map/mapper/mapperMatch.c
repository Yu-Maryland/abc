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
    if ( p->fSkipFanout < 14 || p->fSkipFanout > 31 )
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
    if ( Mode >= 13 && Mode <= 31 )
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
  penalty to high-reference strong deep seeds.]

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
    if ( p->fSkipFanout >= 8 && p->fSkipFanout <= 31 )
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
  high-reference strong deep seeds with a smaller continuous penalty.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Map_MatchSkipAreaSensitiveFanout( Map_Man_t * p, Map_Node_t * pNode, Map_Cut_t * pCut, int fPhase, Map_Match_t * pMatchBest, Map_Match_t * pMatch )
{
    float Slack, SlackMargin, SlackGate, ArrivalMargin, ArrivalDelta, ArrivalGainMargin, AreaMargin, AreaSave, OneInvArea;
    float Stmap28PenaltyFactor, Stmap29PenaltyFactor, Stmap30ModeratePenaltyFactor, Stmap30StrongPenaltyFactor, Stmap30PenaltyFactor;
    const char * pReason;
    int fMiddleReliefWindow, fStmap13, fProfileOpen, fArrivalQuality, fStrictFallback;
    int fStmap25ModerateAblation, fStmap26ModerateAblationCandidate, fStmap26ModerateAblation;
    int fStmap27ModerateSignatureSeed, fStmap27ModerateSignatureBlocked;
    int fStmap28ModeratePenaltyCandidate, fStmap29ModeratePenaltyCandidate;
    int fStmap30ModeratePenaltyCandidate, fStmap30StrongPenaltyCandidate;

    if ( p->fSkipFanout < 7 || p->fSkipFanout > 31 )
        return 0;
    if ( p->fMappingMode < 2 || p->fMappingMode > 3 )
        return 0;
    if ( p->vMapObjs->nSize <= 8000 )
        return 0;
    if ( !Map_MatchCutHasStmapFanoutRisk( pNode, pCut ) )
        return 0;
    fStmap13 = (p->fSkipFanout >= 14 && p->fSkipFanout <= 31);
    Map_MatchStmap13CountRisk( p, pNode, pCut );
    if ( pMatchBest == NULL || pMatchBest->pSuperBest == NULL || pMatch == NULL || pMatch->pSuperBest == NULL )
        return 0;
    Slack = pNode->tRequired[fPhase].Worst - pMatchBest->tArrive.Worst;
    SlackMargin = p->pSuperLib ? p->pSuperLib->tDelayInv.Worst : 0.0;
    ArrivalDelta = pMatch->tArrive.Worst - pMatchBest->tArrive.Worst;
    ArrivalGainMargin = 0.25 * SlackMargin;
    fArrivalQuality = ArrivalDelta <= -ArrivalGainMargin - p->fEpsilon;
    if ( p->fSkipFanout >= 8 && p->fSkipFanout <= 31 )
    {
        SlackGate = 2.0 * SlackMargin;
        fMiddleReliefWindow =
             (p->fSkipFanout >= 10 && p->fSkipFanout <= 31) &&
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
                (p->fSkipFanout == 31 && (!fArrivalQuality || (!fProfileOpen && !Map_MatchNodeHasStmap20EarlySeedDepth( pNode ) && !Map_MatchHasStmap28SoftPenaltyModerateDeepSeed( ArrivalDelta, ArrivalGainMargin, Slack, SlackMargin, p->fEpsilon ))));
            AreaMargin = p->pSuperLib ? (fStrictFallback ? 2.0 : 1.0) * p->pSuperLib->AreaInv : 0.0;
            if ( p->fSkipFanout == 29 && fStmap28ModeratePenaltyCandidate && Stmap28PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap28PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 30 && fStmap29ModeratePenaltyCandidate && Stmap29PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap29PenaltyFactor) * p->pSuperLib->AreaInv;
            if ( p->fSkipFanout == 31 && Stmap30PenaltyFactor > 0.0 && p->pSuperLib )
                AreaMargin = (1.0 + Stmap30PenaltyFactor) * p->pSuperLib->AreaInv;
            AreaSave = pMatchBest->AreaFlow - pMatch->AreaFlow;
            OneInvArea = p->pSuperLib ? p->pSuperLib->AreaInv : 0.0;
            if ( AreaSave > AreaMargin + p->fEpsilon )
            {
                if ( fStmap13 )
                    p->nStmap13MiddleRelief++;
                if ( p->fSkipFanout == 17 )
                    printf( "stmap16 relief diag: index = %d  profile-open = %d  node = %d  level = %u  refs = %d  leaves = %d  phase = %d  slack = %.6f  area-save = %.6f  arrival-delta = %.6f  area-margin = %.6f\n",
                        p->nStmap13MiddleRelief, fProfileOpen, pNode->Num, pNode->Level, pNode->nRefs,
                        pCut->nLeaves, fPhase, Slack, pMatchBest->AreaFlow - pMatch->AreaFlow,
                        pMatch->tArrive.Worst - pMatchBest->tArrive.Worst, AreaMargin );
                if ( p->fSkipFanout >= 18 && p->fSkipFanout <= 31 )
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
                if ( p->fSkipFanout >= 20 && p->fSkipFanout <= 31 && !fProfileOpen && !fStrictFallback )
                    p->nStmap19EarlySeed++;
                return 0;
            }
            if ( p->fSkipFanout >= 19 && p->fSkipFanout <= 31 &&
                 (fStrictFallback || (p->fSkipFanout == 29 && fStmap28ModeratePenaltyCandidate && Stmap28PenaltyFactor > 0.0) || (p->fSkipFanout == 30 && fStmap29ModeratePenaltyCandidate && Stmap29PenaltyFactor > 0.0) || (p->fSkipFanout == 31 && Stmap30PenaltyFactor > 0.0)) &&
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
                p->nStmap18NearMiss++;
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
    if ( p->fSkipFanout >= 9 && p->fSkipFanout <= 31 )
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
    Map_Match_t MatchBest, * pMatch;
    Map_Cut_t * pCut, * pCutBest;
    float Area1 = 0.0; // Suppress "might be used uninitialized
    float Area2, fWorstLimit;

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
 
    // select the new best cut
    fWorstLimit = pNode->tRequired[fPhase].Worst;
    for ( pCut = pNode->pCuts->pNext; pCut; pCut = pCut->pNext )
    {
        // limit gate sizes based on fanout count
        if ( Map_MatchSkipCutForFanout( p, pNode, pCut, fPhase ) )
            continue;
        pMatch = pCut->M + fPhase;
        if ( pMatch->pSupers == NULL )
            continue;

        // find the matches for the cut
        Map_MatchNodeCut( p, pNode, pCut, fPhase, fWorstLimit );
        if ( pMatch->pSuperBest == NULL || pMatch->tArrive.Worst > fWorstLimit + p->fEpsilon )
            continue;
        if ( Map_MatchSkipAreaSensitiveFanout( p, pNode, pCut, fPhase, &MatchBest, pMatch ) )
            continue;

        // if the cut can be matched compare the matchings
        if ( Map_MatchCompare( p, &MatchBest, pMatch, p->fMappingMode ) )
        {
            pCutBest  =  pCut;
            MatchBest = *pMatch;
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
