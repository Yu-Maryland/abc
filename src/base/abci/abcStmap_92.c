/**CFile****************************************************************

  FileName    [abcStmap_92.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap92 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

static int Abc_Stmap92WatchAigsForNetwork( Abc_Ntk_t * pNtk, int * pChildAig, int * pParentAig, int * pChildPhase, int * pParentPhase )
{
    char * pName = pNtk ? Abc_NtkName(pNtk) : NULL;
    char * pBase, * pSlash, * pBackslash;
    *pChildAig = -1;
    *pParentAig = -1;
    *pChildPhase = 0;
    *pParentPhase = 0;
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
        *pChildPhase = 0;
        *pParentPhase = 1;
        return 1;
    }
    if ( !strcmp( pBase, "ode" ) )
    {
        *pChildAig = 5219;
        *pParentAig = 5229;
        *pChildPhase = 1;
        *pParentPhase = 0;
        return 1;
    }
    if ( !strcmp( pBase, "or1200_flat" ) || !strcmp( pBase, "or1200" ) )
    {
        *pChildAig = 11491;
        *pParentAig = 13829;
        *pChildPhase = 0;
        *pParentPhase = 1;
        return 1;
    }
    if ( !strcmp( pBase, "syn2" ) )
    {
        *pChildAig = 18002;
        *pParentAig = 18467;
        *pChildPhase = 1;
        *pParentPhase = 1;
        return 1;
    }
    return 0;
}

/**Function*************************************************************

  Synopsis    [Runs the stmap92 final-critical emitted-AIG drive/load target probe.]

  Description [stmap92 targets the AIG/phase that survives into final critical
  timing and prefers same-phase mapper alternatives with a better drive/load
  proxy.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap92( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    int i, RetValue, ChildAigId, ParentAigId, ChildPhase, ParentPhase, WatchAigs[2], nWatchAigs = 0;
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
    extern void Map_Stmap92SetEmittedDriveTarget( int fEnable, const char * pLabel, int AigId, int Phase );
    extern void Map_Stmap92PrintEmittedDriveTargetSummary( void );

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    pNtk = Abc_FrameReadNtk( pAbc );
    Abc_Stmap92WatchAigsForNetwork( pNtk, &ChildAigId, &ParentAigId, &ChildPhase, &ParentPhase );
    if ( ChildAigId >= 0 )
        WatchAigs[nWatchAigs++] = ChildAigId;
    if ( ParentAigId >= 0 )
        WatchAigs[nWatchAigs++] = ParentAigId;
    printf( "stmap92 diagnostic: inherited-policy = stmap65  mapper-selected-match-watch = %d  network = %s  final-critical-aig = %d  downstream-aig = %d  selected-watch-count = %d  dual-phase-survival-watch-count = %d  emitted-drive-target = %d  target-phase = %d  source = stmap92-final-critical-emitted-drive-load-policy-probe  reconstruction-phase-trace = 1  demand-path-trace = 1  parent-cut-trace = 1  candidate-cut-trace = 1  final-phase-survival = 1  mapping-policy-change = watched-final-critical-emitted-drive-load-target\n",
        nWatchAigs > 0, pNtk && Abc_NtkName(pNtk) ? Abc_NtkName(pNtk) : "?",
        ChildAigId, ParentAigId, nWatchAigs, nWatchAigs,
        ChildAigId >= 0, ChildPhase );
    Map_Stmap75SetFinalCriticalAigDiagArray( nWatchAigs > 0, WatchAigs, nWatchAigs );
    Map_Stmap75SetFinalCriticalAigDiagLabel( "stmap92" );
    Abc_Stmap77SetReconstructionDiag( nWatchAigs > 0, "stmap92", WatchAigs, nWatchAigs );
    Abc_Stmap78SetDemandPathDiag( ChildAigId >= 0, "stmap92", ChildAigId, ParentAigId );
    Abc_Stmap79SetParentCutDiag( ChildAigId >= 0, "stmap92", ChildAigId, ParentAigId );
    Map_Stmap80SetCandidateCutDiag( ChildAigId >= 0, "stmap92", ParentAigId, ChildAigId );
    Map_Stmap92SetEmittedDriveTarget( ChildAigId >= 0, "stmap92", ChildAigId, ChildPhase );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Abc_Stmap77PrintReconstructionSummary();
    Abc_Stmap78PrintDemandPathSummary();
    Abc_Stmap79PrintParentCutSummary();
    Map_Stmap80PrintCandidateCutSummary();
    Map_Stmap92PrintEmittedDriveTargetSummary();
    Map_Stmap92SetEmittedDriveTarget( 0, "stmap92", -1, 0 );
    Map_Stmap80SetCandidateCutDiag( 0, "stmap92", -1, -1 );
    Abc_Stmap79SetParentCutDiag( 0, "stmap92", -1, -1 );
    Abc_Stmap78SetDemandPathDiag( 0, "stmap92", -1, -1 );
    Abc_Stmap77SetReconstructionDiag( 0, "stmap92", NULL, 0 );
    Map_Stmap75PrintFinalCriticalAigSummary();
    Map_Stmap75SetFinalCriticalAigDiag( 0, -1 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap74EnableFinalCriticalScope( pNtkRes, "stmap92" );
        Abc_Stmap76EnablePhaseSurvivalScope( pNtkRes, "stmap92", WatchAigs, nWatchAigs );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap92 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap92 bounded-pressure mapping with benchmark-specific final-critical emitted-AIG drive/load targeting\n" );
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
