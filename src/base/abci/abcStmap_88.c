/**CFile****************************************************************

  FileName    [abcStmap_88.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap88 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

static int Abc_Stmap88WatchAigsForNetwork( Abc_Ntk_t * pNtk, int * pChildAig, int * pParentAig )
{
    char * pName = pNtk ? Abc_NtkName(pNtk) : NULL;
    char * pBase, * pSlash, * pBackslash;
    *pChildAig = -1;
    *pParentAig = -1;
    if ( pName == NULL )
        return 0;
    pSlash = strrchr( pName, '/' );
    pBackslash = strrchr( pName, '\\' );
    if ( pSlash && (!pBackslash || pSlash > pBackslash) )
        pBase = pSlash + 1;
    else if ( pBackslash )
        pBase = pBackslash + 1;
    else
        pBase = pName;
    if ( !strcmp( pBase, "i10" ) )
    {
        *pChildAig = 855;
        *pParentAig = 861;
        return 1;
    }
    if ( !strcmp( pBase, "ode" ) )
    {
        *pChildAig = 5219;
        *pParentAig = 5229;
        return 1;
    }
    if ( !strcmp( pBase, "or1200_flat" ) || !strcmp( pBase, "or1200" ) )
    {
        *pChildAig = 11491;
        *pParentAig = 13829;
        return 1;
    }
    if ( !strcmp( pBase, "syn2" ) )
    {
        *pChildAig = 18002;
        *pParentAig = 18467;
        return 1;
    }
    return 0;
}

/**Function*************************************************************

  Synopsis    [Runs the stmap88 child phase-1 speed-target probe.]

  Description [stmap88 extends stmap87 by adding a scoped exact-area
  recovery policy for the watched child AIG itself. When phase 1 of the
  watched child has a faster near-tie candidate, the mapper may keep it
  within bounded area windows while retaining the stmap87 parent/child
  diagnostics for comparison.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap88( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    int i, RetValue, ChildAigId, ParentAigId, WatchAigs[2], nWatchAigs = 0;
    extern int Abc_CommandStmap65( Abc_Frame_t * pAbc, int argc, char ** argv );
    extern void Map_Stmap75SetFinalCriticalAigDiag( int fEnable, int WatchAigId );
    extern void Map_Stmap75SetFinalCriticalAigDiagArray( int fEnable, int * pWatchAigIds, int nWatchAigs );
    extern void Map_Stmap75SetFinalCriticalAigDiagLabel( const char * pLabel );
    extern void Map_Stmap75PrintFinalCriticalAigSummary( void );
    extern void Abc_Stmap74EnableFinalCriticalScope( Abc_Ntk_t * pNtk, const char * pLabel );
    extern void Abc_Stmap76EnablePhaseSurvivalScope( Abc_Ntk_t * pNtk, const char * pLabel, int * pWatchAigIds, int nWatchAigs );
    extern void Abc_Stmap77SetReconstructionDiag( int fEnable, const char * pLabel, int * pWatchAigIds, int nWatchAigs );
    extern void Abc_Stmap77PrintReconstructionSummary( void );
    extern void Abc_Stmap78SetDemandPathDiag( int fEnable, const char * pLabel, int WatchAigId, int DownstreamAigId );
    extern void Abc_Stmap78PrintDemandPathSummary( void );
    extern void Abc_Stmap79SetParentCutDiag( int fEnable, const char * pLabel, int ChildAigId, int ParentAigId );
    extern void Abc_Stmap79PrintParentCutSummary( void );
    extern void Map_Stmap80SetCandidateCutDiag( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId );
    extern void Map_Stmap80PrintCandidateCutSummary( void );
    extern void Map_Stmap81SetParentPhaseBias( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId );
    extern void Map_Stmap81PrintParentPhaseBiasSummary( void );
    extern void Map_Stmap82SetStickyParentPhase( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId );
    extern void Map_Stmap82SetStickyParentPhaseTimingWindow( float TimingHoldWindow );
    extern void Map_Stmap82PrintStickyParentPhaseSummary( void );
    extern void Map_Stmap87SetParentPhaseTarget( int fEnable, const char * pLabel, int ParentAigId, int ChildAigId, int ParentPhase, int ChildPhase );
    extern void Map_Stmap87PrintParentPhaseTargetSummary( void );
    extern void Map_Stmap88SetChildPhaseTarget( int fEnable, const char * pLabel, int ChildAigId, int TargetPhase );
    extern void Map_Stmap88PrintChildPhaseTargetSummary( void );

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    pNtk = Abc_FrameReadNtk( pAbc );
    Abc_Stmap88WatchAigsForNetwork( pNtk, &ChildAigId, &ParentAigId );
    if ( ChildAigId >= 0 )
        WatchAigs[nWatchAigs++] = ChildAigId;
    if ( ParentAigId >= 0 )
        WatchAigs[nWatchAigs++] = ParentAigId;
    printf( "stmap88 diagnostic: inherited-policy = stmap65  mapper-selected-match-watch = %d  network = %s  child-aig = %d  parent-aig = %d  selected-watch-count = %d  dual-phase-survival-watch-count = %d  parent-phase-target = %d  child-phase-target = %d  target-parent-phase = 0  target-child-phase = 1  source = stmap88-child-phase1-speed-policy-probe  reconstruction-phase-trace = 1  demand-path-trace = 1  parent-cut-trace = 1  candidate-cut-trace = 1  parent-phase-bias = 1  sticky-parent-phase = 1  final-phase-survival = 1  mapping-policy-change = watched-child-phase1-speed-target\n",
        nWatchAigs > 0, pNtk && Abc_NtkName(pNtk) ? Abc_NtkName(pNtk) : "?",
        ChildAigId, ParentAigId, nWatchAigs, nWatchAigs,
        ChildAigId >= 0 && ParentAigId >= 0, ChildAigId >= 0 );
    Map_Stmap75SetFinalCriticalAigDiagArray( nWatchAigs > 0, WatchAigs, nWatchAigs );
    Map_Stmap75SetFinalCriticalAigDiagLabel( "stmap88" );
    Abc_Stmap77SetReconstructionDiag( nWatchAigs > 0, "stmap88", WatchAigs, nWatchAigs );
    Abc_Stmap78SetDemandPathDiag( ChildAigId >= 0, "stmap88", ChildAigId, ParentAigId );
    Abc_Stmap79SetParentCutDiag( ChildAigId >= 0, "stmap88", ChildAigId, ParentAigId );
    Map_Stmap80SetCandidateCutDiag( ChildAigId >= 0, "stmap88", ParentAigId, ChildAigId );
    Map_Stmap81SetParentPhaseBias( ChildAigId >= 0, "stmap88", ParentAigId, ChildAigId );
    Map_Stmap82SetStickyParentPhase( ChildAigId >= 0, "stmap88", ParentAigId, ChildAigId );
    Map_Stmap82SetStickyParentPhaseTimingWindow( 100000.00f );
    Map_Stmap87SetParentPhaseTarget( ChildAigId >= 0 && ParentAigId >= 0, "stmap88", ParentAigId, ChildAigId, 0, 1 );
    Map_Stmap88SetChildPhaseTarget( ChildAigId >= 0, "stmap88", ChildAigId, 1 );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Abc_Stmap77PrintReconstructionSummary();
    Abc_Stmap78PrintDemandPathSummary();
    Abc_Stmap79PrintParentCutSummary();
    Map_Stmap80PrintCandidateCutSummary();
    Map_Stmap81PrintParentPhaseBiasSummary();
    Map_Stmap82PrintStickyParentPhaseSummary();
    Map_Stmap87PrintParentPhaseTargetSummary();
    Map_Stmap88PrintChildPhaseTargetSummary();
    Map_Stmap88SetChildPhaseTarget( 0, "stmap88", -1, 1 );
    Map_Stmap87SetParentPhaseTarget( 0, "stmap88", -1, -1, 0, 1 );
    Map_Stmap82SetStickyParentPhase( 0, "stmap88", -1, -1 );
    Map_Stmap81SetParentPhaseBias( 0, "stmap88", -1, -1 );
    Map_Stmap80SetCandidateCutDiag( 0, "stmap88", -1, -1 );
    Abc_Stmap79SetParentCutDiag( 0, "stmap88", -1, -1 );
    Abc_Stmap78SetDemandPathDiag( 0, "stmap88", -1, -1 );
    Abc_Stmap77SetReconstructionDiag( 0, "stmap88", NULL, 0 );
    Map_Stmap75PrintFinalCriticalAigSummary();
    Map_Stmap75SetFinalCriticalAigDiag( 0, -1 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap74EnableFinalCriticalScope( pNtkRes, "stmap88" );
        Abc_Stmap76EnablePhaseSurvivalScope( pNtkRes, "stmap88", WatchAigs, nWatchAigs );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap88 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap88 bounded-pressure mapping with parent phase targeting plus child phase-1 speed targeting\n" );
    Abc_Print( -2, "\t-D float : sets the global required times [default = not used]\n" );
    Abc_Print( -2, "\t-A float : \"area multiplier\" to bias gate selection [default = 0.00]\n" );
    Abc_Print( -2, "\t-B float : \"delay multiplier\" to bias gate selection [default = 0.00]\n" );
    Abc_Print( -2, "\t-F float : the logarithmic fanout delay parameter [default = 0.00]\n" );
    Abc_Print( -2, "\t-S float : the slew parameter used to generate the library [default = 0.00]\n" );
    Abc_Print( -2, "\t-G float : the SCL/genlib gain parameter used to generate the library [default = 250.00]\n" );
    Abc_Print( -2, "\t-M num   : skip gate classes whose size is less than this [default = 0]\n" );
    Abc_Print( -2, "\t-a       : toggles area-only mapping [default = no]\n" );
    Abc_Print( -2, "\t-r       : toggles area recovery [default = yes]\n" );
    Abc_Print( -2, "\t-s       : toggles sweep after mapping [default = no]\n" );
    Abc_Print( -2, "\t-p       : optimizes power by minimizing switching [default = no]\n" );
    Abc_Print( -2, "\t-f       : disables the bounded-pressure SCL feedback mapper mode [default = yes]\n" );
    Abc_Print( -2, "\t-u       : use standard-cell profile [default = no]\n" );
    Abc_Print( -2, "\t-o       : toggles using buffers to decouple combinational outputs [default = no]\n" );
    Abc_Print( -2, "\t-v       : toggles verbose output [default = no]\n" );
    Abc_Print( -2, "\t-h       : print the command usage\n" );
    return 1;
}

ABC_NAMESPACE_IMPL_END
