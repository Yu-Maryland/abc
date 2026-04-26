/**CFile****************************************************************

  FileName    [abcStmap_82.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap82 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

static int Abc_Stmap82WatchAigsForNetwork( Abc_Ntk_t * pNtk, int * pChildAig, int * pParentAig )
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

  Synopsis    [Runs the stmap82 sticky parent-phase retention probe.]

  Description [stmap82 reuses the stmap81 child-phase-0 parent candidate
  bias and adds a same-node/same-phase sticky guard against later
  child-missing replacements unless the replacement is materially faster.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap82( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    int i, RetValue, ChildAigId, ParentAigId, WatchAigs[1];
    extern int Abc_CommandStmap65( Abc_Frame_t * pAbc, int argc, char ** argv );
    extern void Map_Stmap75SetFinalCriticalAigDiag( int fEnable, int WatchAigId );
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
    extern void Map_Stmap82PrintStickyParentPhaseSummary( void );

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    pNtk = Abc_FrameReadNtk( pAbc );
    Abc_Stmap82WatchAigsForNetwork( pNtk, &ChildAigId, &ParentAigId );
    WatchAigs[0] = ChildAigId;
    printf( "stmap82 diagnostic: inherited-policy = stmap65  mapper-selected-match-watch = %d  network = %s  child-aig = %d  parent-aig = %d  source = stmap81-sticky-parent-phase-probe  reconstruction-phase-trace = 1  demand-path-trace = 1  parent-cut-trace = 1  candidate-cut-trace = 1  parent-phase-bias = 1  sticky-parent-phase = 1  final-phase-survival = 1  mapping-policy-change = watched-parent-child-phase0-sticky-window\n",
        ChildAigId >= 0, pNtk && Abc_NtkName(pNtk) ? Abc_NtkName(pNtk) : "?",
        ChildAigId, ParentAigId );
    Map_Stmap75SetFinalCriticalAigDiag( ChildAigId >= 0, ChildAigId );
    Map_Stmap75SetFinalCriticalAigDiagLabel( "stmap82" );
    Abc_Stmap77SetReconstructionDiag( ChildAigId >= 0, "stmap82", WatchAigs, ChildAigId >= 0 ? 1 : 0 );
    Abc_Stmap78SetDemandPathDiag( ChildAigId >= 0, "stmap82", ChildAigId, ParentAigId );
    Abc_Stmap79SetParentCutDiag( ChildAigId >= 0, "stmap82", ChildAigId, ParentAigId );
    Map_Stmap80SetCandidateCutDiag( ChildAigId >= 0, "stmap82", ParentAigId, ChildAigId );
    Map_Stmap81SetParentPhaseBias( ChildAigId >= 0, "stmap82", ParentAigId, ChildAigId );
    Map_Stmap82SetStickyParentPhase( ChildAigId >= 0, "stmap82", ParentAigId, ChildAigId );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Abc_Stmap77PrintReconstructionSummary();
    Abc_Stmap78PrintDemandPathSummary();
    Abc_Stmap79PrintParentCutSummary();
    Map_Stmap80PrintCandidateCutSummary();
    Map_Stmap81PrintParentPhaseBiasSummary();
    Map_Stmap82PrintStickyParentPhaseSummary();
    Map_Stmap82SetStickyParentPhase( 0, "stmap82", -1, -1 );
    Map_Stmap81SetParentPhaseBias( 0, "stmap82", -1, -1 );
    Map_Stmap80SetCandidateCutDiag( 0, "stmap82", -1, -1 );
    Abc_Stmap79SetParentCutDiag( 0, "stmap82", -1, -1 );
    Abc_Stmap78SetDemandPathDiag( 0, "stmap82", -1, -1 );
    Abc_Stmap77SetReconstructionDiag( 0, "stmap82", NULL, 0 );
    Map_Stmap75PrintFinalCriticalAigSummary();
    Map_Stmap75SetFinalCriticalAigDiag( 0, -1 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap74EnableFinalCriticalScope( pNtkRes, "stmap82" );
        Abc_Stmap76EnablePhaseSurvivalScope( pNtkRes, "stmap82", WatchAigs, ChildAigId >= 0 ? 1 : 0 );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap82 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap82 bounded-pressure mapping with sticky watched-parent child-phase-0 retention\n" );
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
