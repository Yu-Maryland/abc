/**CFile****************************************************************

  FileName    [abcStmap_71.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap71 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

/**Function*************************************************************

  Synopsis    [Runs the stmap71 moderate-penalty witness diagnostic.]

  Description [stmap71 keeps the reviewed stmap65 mapping policy unchanged
  and records mode-57 moderate-penalty block candidates as downstream final
  witnesses.  The diagnostic asks whether the remaining moderate blocker class
  has final criticality at least 0.50 after topo, buffer, upsize, dnsize, and
  stime before reopening any mapper admission rule.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap71( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtkRes;
    int i, RetValue;
    extern int Abc_CommandStmap65( Abc_Frame_t * pAbc, int argc, char ** argv );
    extern void Map_Stmap71SetModeratePenaltyWitnessDiag( int fEnable );
    extern void Abc_Stmap64EnableFinalWitnessScope( Abc_Ntk_t * pNtk, const char * pLabel );

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    printf( "stmap71 diagnostic: inherited-policy = stmap65  moderate-penalty-witness = 1  source = stmap56-mode57  final-criticality-gate = 0.50  mapping-policy-change = 0\n" );
    Map_Stmap71SetModeratePenaltyWitnessDiag( 1 );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Map_Stmap71SetModeratePenaltyWitnessDiag( 0 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap64EnableFinalWitnessScope( pNtkRes, "stmap71" );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap71 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap71 bounded-pressure mapping with moderate-penalty downstream witness diagnostics\n" );
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
