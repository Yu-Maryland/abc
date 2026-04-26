/**CFile****************************************************************

  FileName    [abcStmap_78.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap78 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

static int Abc_Stmap78WatchAigsForNetwork( Abc_Ntk_t * pNtk, int * pWatchAig, int * pDownstreamAig )
{
    char * pName = pNtk ? Abc_NtkName(pNtk) : NULL;
    char * pBase, * pSlash, * pBackslash;
    *pWatchAig = -1;
    *pDownstreamAig = -1;
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
        *pWatchAig = 855;
        *pDownstreamAig = 861;
        return 1;
    }
    if ( !strcmp( pBase, "ode" ) )
    {
        *pWatchAig = 5219;
        *pDownstreamAig = 5229;
        return 1;
    }
    if ( !strcmp( pBase, "or1200_flat" ) || !strcmp( pBase, "or1200" ) )
    {
        *pWatchAig = 11491;
        *pDownstreamAig = 13829;
        return 1;
    }
    if ( !strcmp( pBase, "syn2" ) )
    {
        *pWatchAig = 18002;
        *pDownstreamAig = 18467;
        return 1;
    }
    return 0;
}

/**Function*************************************************************

  Synopsis    [Runs the stmap78 final-demand path diagnostic.]

  Description [stmap78 keeps the reviewed stmap65 mapping policy unchanged,
  reuses stmap77 reconstruction and phase-survival tracing, and adds a
  final-remap demand-path trace from CO roots to the watched AIG. For ode,
  this identifies whether downstream AIG 5229 is the parent that requests
  phase 1 of watched AIG 5219.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap78( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    int i, RetValue, WatchAigId, DownstreamAigId, WatchAigs[1];
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

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    pNtk = Abc_FrameReadNtk( pAbc );
    Abc_Stmap78WatchAigsForNetwork( pNtk, &WatchAigId, &DownstreamAigId );
    WatchAigs[0] = WatchAigId;
    printf( "stmap78 diagnostic: inherited-policy = stmap65  mapper-selected-match-watch = %d  network = %s  watch-aig = %d  downstream-aig = %d  source = stmap77-final-demand-hypothesis  reconstruction-phase-trace = 1  demand-path-trace = 1  final-phase-survival = 1  mapping-policy-change = 0\n",
        WatchAigId >= 0, pNtk && Abc_NtkName(pNtk) ? Abc_NtkName(pNtk) : "?",
        WatchAigId, DownstreamAigId );
    Map_Stmap75SetFinalCriticalAigDiag( WatchAigId >= 0, WatchAigId );
    Map_Stmap75SetFinalCriticalAigDiagLabel( "stmap78" );
    Abc_Stmap77SetReconstructionDiag( WatchAigId >= 0, "stmap78", WatchAigs, WatchAigId >= 0 ? 1 : 0 );
    Abc_Stmap78SetDemandPathDiag( WatchAigId >= 0, "stmap78", WatchAigId, DownstreamAigId );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Abc_Stmap77PrintReconstructionSummary();
    Abc_Stmap78PrintDemandPathSummary();
    Abc_Stmap78SetDemandPathDiag( 0, "stmap78", -1, -1 );
    Abc_Stmap77SetReconstructionDiag( 0, "stmap78", NULL, 0 );
    Map_Stmap75PrintFinalCriticalAigSummary();
    Map_Stmap75SetFinalCriticalAigDiag( 0, -1 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap74EnableFinalCriticalScope( pNtkRes, "stmap78" );
        Abc_Stmap76EnablePhaseSurvivalScope( pNtkRes, "stmap78", WatchAigs, WatchAigId >= 0 ? 1 : 0 );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap78 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap78 bounded-pressure mapping with final demand-path diagnostics\n" );
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
