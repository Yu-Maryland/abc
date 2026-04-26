/**CFile****************************************************************

  FileName    [abcStmap_75.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap75 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

static int Abc_Stmap75WatchAigForNetwork( Abc_Ntk_t * pNtk )
{
    char * pName = pNtk ? Abc_NtkName(pNtk) : NULL;
    char * pBase, * pSlash, * pBackslash;
    if ( pName == NULL )
        return -1;
    pSlash = strrchr( pName, '/' );
    pBackslash = strrchr( pName, '\\' );
    if ( pSlash && (!pBackslash || pSlash > pBackslash) )
        pBase = pSlash + 1;
    else if ( pBackslash )
        pBase = pBackslash + 1;
    else
        pBase = pName;
    if ( !strcmp( pBase, "i10" ) )
        return 855;
    if ( !strcmp( pBase, "ode" ) )
        return 5219;
    if ( !strcmp( pBase, "or1200_flat" ) || !strcmp( pBase, "or1200" ) )
        return 11491;
    if ( !strcmp( pBase, "syn2" ) )
        return 18002;
    return -1;
}

/**Function*************************************************************

  Synopsis    [Runs the stmap75 mapper selected-match diagnostic.]

  Description [stmap75 keeps the reviewed stmap65 mapping policy unchanged
  and watches the mapper-selected cut/supergate records for the final-critical
  AIG IDs identified by stmap74.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap75( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    int i, RetValue, WatchAigId;
    extern int Abc_CommandStmap65( Abc_Frame_t * pAbc, int argc, char ** argv );
    extern void Map_Stmap75SetFinalCriticalAigDiag( int fEnable, int WatchAigId );
    extern void Map_Stmap75PrintFinalCriticalAigSummary( void );
    extern void Abc_Stmap74EnableFinalCriticalScope( Abc_Ntk_t * pNtk, const char * pLabel );

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    pNtk = Abc_FrameReadNtk( pAbc );
    WatchAigId = Abc_Stmap75WatchAigForNetwork( pNtk );
    printf( "stmap75 diagnostic: inherited-policy = stmap65  mapper-selected-match-watch = %d  network = %s  watch-aig = %d  source = stmap74-final-critical-top-aigs  mapping-policy-change = 0\n",
        WatchAigId >= 0, pNtk && Abc_NtkName(pNtk) ? Abc_NtkName(pNtk) : "?", WatchAigId );
    Map_Stmap75SetFinalCriticalAigDiag( WatchAigId >= 0, WatchAigId );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Map_Stmap75PrintFinalCriticalAigSummary();
    Map_Stmap75SetFinalCriticalAigDiag( 0, -1 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap74EnableFinalCriticalScope( pNtkRes, "stmap75" );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap75 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap75 bounded-pressure mapping with mapper selected-match diagnostics for stmap74 final-critical AIG IDs\n" );
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
